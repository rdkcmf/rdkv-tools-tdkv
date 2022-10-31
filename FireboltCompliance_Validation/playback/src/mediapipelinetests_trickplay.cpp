/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2021 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
#include <stdio.h>
#include <unistd.h>
#include <iterator>
#include <string>
#include <vector>
#include <cmath>
#include <time.h>
#include <sstream>
#include <chrono>
extern "C"
{
#include <gst/check/gstcheck.h>
#include <gst/gst.h>
}
using namespace std;
#define RATE_SET_TIMEOUT                5
#define DEFAULT_TEST_SUITE_TIMEOUT      360
#define VIDEO_STATUS                    "/CheckVideoStatus.sh"
#define AUDIO_STATUS                    "/CheckAudioStatus.sh"
#define PLAYBIN_ELEMENT                 "playbin"
#define WESTEROS_SINK                   "westerossink"
#define BUFFER_SIZE_LONG                1024
#define BUFFER_SIZE_SHORT               264
#define NORMAL_PLAYBACK_RATE            1.0
#define FCS_MICROSECONDS                1000000
#define Sleep(RunSeconds)               start = std::chrono::high_resolution_clock::now(); \
                                        Runforseconds = RunSeconds; \
                                        while(1) { \
                                        if (std::chrono::high_resolution_clock::now() - start > std::chrono::seconds(Runforseconds)) \
                                             break; \
                                        }
#define WaitForOperation                Sleep(5)

char m_play_url[BUFFER_SIZE_LONG] = {'\0'};
char TDK_PATH[BUFFER_SIZE_SHORT] = {'\0'};
vector<string> operationsList;

/*
 * Default values for avstatus check flag and play_timeout if not received as input arguments
 */

bool checkAVStatus = false;
int play_timeout = 10;
int Runforseconds;
auto start = std::chrono::high_resolution_clock::now();
bool latency_check_test = false;
auto timestamp = std::chrono::high_resolution_clock::now(), time_elapsed = std::chrono::high_resolution_clock::now();
auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(time_elapsed - timestamp);
bool firstFrameReceived = false;
bool checkPTS = true;
gint64 currentposition;
bool trickplay = false;
bool pause_operation = false;
bool forward_events = true;
gint64 startPosition;
string audiosink;

/*
 * Playbin flags
 */

typedef enum {
  GST_PLAY_FLAG_VIDEO         = (1 << 0),
  GST_PLAY_FLAG_AUDIO         = (1 << 1),
  GST_PLAY_FLAG_TEXT          = (1 << 2)
} GstPlayFlags;

/*
 * Trickplay operations
 */
typedef enum {
  REWIND4x_RATE        = -4,
  REWIND3x_RATE         = -3,
  REWIND2x_RATE         = -2,
  FASTFORWARD2x_RATE    = 2,
  FASTFORWARD4x_RATE    = 4,
  FASTFORWARD3x_RATE   = 3
} PlaybackRates;

/*
 * Structure to pass arguments to/from the message handling method
 */
typedef struct CustomData {
    GstElement *playbin;                /* Playbin element handle */
    GstState cur_state;                 /* Variable to store the current state of pipeline */
    gint64 seekPosition;                /* Variable to store the position to be seeked */
    gdouble seekSeconds;                /* Variable to store the position to be seeked in seconds */
    gint64 currentPosition;             /* Variable to store the current position of pipeline */
    gint64 duration;                    /* Variable to store the duration  of the piepline */
    gboolean terminate;                 /* Variable to indicate whether execution should be terminated in case of an error */
    gboolean seeked;                    /* Variable to indicate if seek to requested position is completed */
    gboolean eosDetected;               /* Variable to indicate if EOS is detected */
    gboolean stateChanged;              /* Variable to indicate if stateChange is occured */
    gboolean streamStart;               /* Variable to indicate start of new stream */
    gboolean setRateOperation;          /* Variable which indicates setRate operation is carried out */
    gdouble setRate;                    /* Variable to indicate the playback rate to be set */
    gdouble currentRate;                /* Variable to store the current playback rate of the pipeline */
} MessageHandlerData;

/*
 * Methods
 */

static void handleMessage (MessageHandlerData *data, GstMessage *message);
static void trickplayOperation (MessageHandlerData *data);
static gdouble getRate (GstElement* playbin);
static void SetupStream (MessageHandlerData *data);

/*******************************************************************************************************************************************
Purpose:                To continue the state of the pipeline and check whether operation is being carried throughout the specified interval
Parameters:
playbin                   - The pipeline which is to be monitored
RunSeconds:               - The interval for which pipeline should be monitored
********************************************************************************************************************************************/
static void PlaySeconds(GstElement* playbin,int RunSeconds)
{
   gint64 currentPosition;
   gint difference;
   GstMessage *message;
   GstBus *bus;
   MessageHandlerData data;
   gint64 pts;
   gint64 old_pts=0;
   gint pts_buffer=5;
   gint RanForTime=0;
   gdouble current_rate;
   GstElement *videoSink;
   GstState cur_state;
   gint play_jump = 0;
   gint play_jump_previous = 99;
   gint previous_position = 0;
   gint jump_buffer = 1;
   gint jump_buffer_small_value = 3;
   GstStateChangeReturn state_change;


   /* Update data variables */
   data.playbin = playbin;
   data.setRateOperation = FALSE;
   data.terminate = FALSE;
   data.eosDetected = FALSE;

   g_object_get (playbin,"video-sink",&videoSink,NULL);

   gst_element_get_state (playbin, &cur_state, NULL, GST_SECOND);

   if (checkPTS)
   {	   
       g_object_get (videoSink,"video-pts",&pts,NULL);
       old_pts = pts;
   }

   printf("\nRunning for %d seconds, start Position is %lld\n",RunSeconds,startPosition/GST_SECOND);
   fail_unless (gst_element_query_position (playbin, GST_FORMAT_TIME, &currentPosition), "Failed to query the current playback position");
   previous_position = (currentPosition/GST_SECOND);

   if (pause_operation)
   {
        do
        {
            Sleep(1);
            printf("Current State is PAUSED , waiting for %d\n", RunSeconds);
            fail_unless (gst_element_query_position (playbin, GST_FORMAT_TIME, &currentPosition), "Failed to query the current playback position");
            play_jump = int(currentPosition/GST_SECOND) - previous_position;
            previous_position = (currentPosition/GST_SECOND);

	    if (play_jump != 0)
		jump_buffer -=1;

            fail_unless(jump_buffer != 0,"Playback is not PAUSED");

            if (checkPTS)
            {
                g_object_get (videoSink,"video-pts",&pts,NULL);
                printf("\nPTS: %lld \n",pts);
                if (old_pts != pts)
                {
                    pts_buffer -= 1;
                }
                fail_unless(pts_buffer != pts , "Video is not PAUSED according to video-pts check of westerosSink");
                fail_unless(old_pts != 0 , "Video is not playing according to video-pts check of westerosSink");
                old_pts = pts;
            }
            RanForTime += 1;
        }while(RanForTime < RunSeconds);
        return;	   
   }

   if (trickplay)
   {
	jump_buffer = 3;
   }
   current_rate = getRate (playbin);
   bus = gst_element_get_bus (playbin);
   do
   {
	Sleep(1);
        fail_unless (gst_element_query_position (playbin, GST_FORMAT_TIME, &currentPosition), "Failed to query the current playback position");
        difference = int(abs((currentPosition/GST_SECOND) - (startPosition/GST_SECOND)));
        printf("\nCurrent Position : %lld , Playing after operation for: %d",(currentPosition/GST_SECOND),difference);
	
	play_jump = int(currentPosition/GST_SECOND) - previous_position;
	printf("\nPlay jump = %d", play_jump);

	if(!trickplay)
	{
           /*
            * Ignore if first jump is 0
            */
            if ((play_jump != NORMAL_PLAYBACK_RATE) && !(((play_jump == 0) && (play_jump_previous == 99))))
                jump_buffer -=1;

           /*
            * For small jumps until 2 , jump_buffer is 2
            */
            if (((play_jump == 0) || (play_jump == 2) || (play_jump == -1)) && (jump_buffer == 0))
            {
                jump_buffer_small_value -=1;
                jump_buffer = jump_buffer_small_value;
            }

           /*
            * if playbin reports jump=0 and then jump=2 , then video has played fine only
            */
            if (((play_jump == 2) && (play_jump_previous == 0)) && (jump_buffer == 0))
            {
                jump_buffer = 1;
            }
        }
        else
	{	
            if (current_rate >0 && (play_jump < current_rate))
                jump_buffer -=1;
        }

        message = gst_bus_pop_filtered (bus, (GstMessageType) ((GstMessageType) GST_MESSAGE_STATE_CHANGED |
                                             (GstMessageType) GST_MESSAGE_ERROR | (GstMessageType) GST_MESSAGE_EOS |
                                             (GstMessageType) GST_MESSAGE_ASYNC_DONE ));
        /*
         * Parse message
         */
        if (NULL != message)
        {
            handleMessage (&data, message);
        }
	if (checkPTS)
        {
            g_object_get (videoSink,"video-pts",&pts,NULL);
            printf("\nPTS: %lld",pts);
            if ((pts ==0) || (old_pts >= pts))
            {
                pts_buffer -= 1;
            }

            fail_unless(pts_buffer != 0 , "Video is not playing according to video-pts check of westerosSink");
            fail_unless(jump_buffer != 0 , "Playback is not happening at the expected rate");
            old_pts = pts;
        }
	previous_position = (currentPosition/GST_SECOND);
        play_jump_previous = play_jump;
   
   }while((difference <= RunSeconds) && !data.terminate && !data.eosDetected);

   if(data.eosDetected)
   {
	RanForTime = difference;
	printf("\nEnd of stream was detected, calling PlaySeconds agaifor remaining time %d\n",(RunSeconds-RanForTime));
	PlaySeconds(playbin,RunSeconds-RanForTime);
   }
   printf("\nExiting from PlaySeconds, currentPosition is %lld\n",currentPosition/GST_SECOND);
   gst_object_unref (bus);
}

/********************************************************************************************************************
Purpose:               To get the current status of the AV running
Parameters:
Return:               - bool SUCCESS/FAILURE
*********************************************************************************************************************/
bool getstreamingstatus(char* script)
{
    char buffer[BUFFER_SIZE_SHORT]={'\0'};
    char result[BUFFER_SIZE_LONG]={'\0'};
    FILE* pipe = popen(script, "r");
    if (!pipe)
    {
            GST_ERROR("Error in opening pipe \n");
            return false;
    }
    while (!feof(pipe))
    {
        if (fgets(buffer, BUFFER_SIZE_SHORT, pipe) != NULL)
        {
            strcat(result, buffer);
        }
    }
    pclose(pipe);
    GST_LOG("Script Output: %s %s\n", script, result);
    if (strstr(result, "SUCCESS") != NULL)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/********************************************************************************************************************
Purpose:               To check the current status of the AV running
Parameters:
scriptname [IN]       - The input scriptname
Return:               - bool SUCCESS/FAILURE
*********************************************************************************************************************/
bool check_for_AV_status ()
{
    GST_LOG ("\nCheck_for_AV_status\n");
    char video_status[BUFFER_SIZE_SHORT] = {'\0'};
    char audio_status[BUFFER_SIZE_SHORT] = {'\0'};
    strcat (video_status, TDK_PATH);
    strcat (video_status, VIDEO_STATUS);
    strcat (audio_status, TDK_PATH);
    strcat (audio_status, AUDIO_STATUS);

    /*
     * VideoStatus Check, AudioStatus Check script execution
     */
    return (getstreamingstatus (video_status) && getstreamingstatus (audio_status));

}

static void parselatency()
{
    int latency_int;
    printf("\nTime measured: %.3lld milliseconds.\n", latency.count());
    latency_int = latency.count();
    /*
     * Writing to file
     */
    FILE *filePointer ;
    char latency_file[BUFFER_SIZE_SHORT] = {'\0'};
    strcat (latency_file, TDK_PATH);
    strcat (latency_file, "/latency_log");
    filePointer = fopen(latency_file, "w");
    if (filePointer != NULL)
    {
        fprintf(filePointer,"Latency = %d milliseconds\n", latency_int);
    }
    else
    {
	printf("\nLatency writing operation failed\n");
    }
    fclose(filePointer);
}

/********************************************************************************************************************
Purpose:               Callback function to set a variable to true on receiving first frame
*********************************************************************************************************************/
static void firstFrameCallback(GstElement *sink, guint size, void *context, gpointer data)
{
   bool *gotFirstFrameSignal = (bool*)data;
   printf ("Received first frame signal\n");
   fail_unless (gst_element_query_position (sink, GST_FORMAT_TIME, &currentposition), "Failed to query the current playback position");
   printf("\nCurrent Position %lld\n",currentposition/GST_SECOND);
   /*
    * Set the Value to global variable once the first frame signal is received
    */
   *gotFirstFrameSignal = true;
}

/********************************************************************************************************************
Purpose:               To get the current playback rate of pipeline
Parameters:
playbin [IN]          - GstElement* 'playbin' whose playback rate should be queried
Return:               - gdouble value for the current playback rate
*********************************************************************************************************************/
static gdouble getRate (GstElement* playbin)
{
    GstQuery *query;
    gdouble currentRate = 0.0;
    /*
     * Retrieve the current playback speed of the pipeline using gst_element_query()
     */
    /*
     * Create a GstQuery to retrieve the segment
     */
    query = gst_query_new_segment (GST_FORMAT_DEFAULT);
    /*
     * Query the playbin element
     */
    fail_unless (gst_element_query (playbin, query), "Failed to query the current playback rate");
    /*
     * Parse the GstQuery structure to get the current playback rate
     */
    gst_query_parse_segment (query, &currentRate, NULL, NULL, NULL);
    /*
     * Unreference the query structure
     */
    gst_query_unref (query);
    /*
     * The returned playback rate should be validated
     */
    return currentRate;
}

/********************************************************************************************************************
Purpose:               To check whether seek is successfull
Parameters:
playbin [IN]          - MessageHandleData contains all data of the pipeline
Return:               - NIL
*********************************************************************************************************************/

static void checkTrickplay(MessageHandlerData *Param)
{
    GstMessage *message;
    GstBus *bus;
    GstStateChangeReturn state_change;
    GstState cur_state;
    MessageHandlerData data;
    int Seek_time_threshold = 5;
    int wait_time = 0;
    data.playbin = Param->playbin;
    bus = gst_element_get_bus (data.playbin);
    /*
     * Set all the required variables before polling for the message
     */
    data.terminate = FALSE;
    data.seeked = FALSE;
    data.setRateOperation = FALSE;
    if (Param->setRateOperation)
    {
        data.setRateOperation = TRUE;
        data.setRate = Param->setRate;
    }
    data.currentRate = 0.0;    
    data.seekPosition = Param->seekPosition;
    data.currentPosition = GST_CLOCK_TIME_NONE;
    data.stateChanged = FALSE;
    data.eosDetected = FALSE;

    if(!data.setRateOperation)
    {
         start = std::chrono::high_resolution_clock::now();
         while(!data.terminate && !data.seeked)
         {
               //Check if seek had already happened
               fail_unless (gst_element_query_position (data.playbin, GST_FORMAT_TIME, &(data.currentPosition)),
                                                     "Failed to query the current playback position");
               //Added GST_SECOND buffer time between currentPosition and seekPosition
               if (abs( data.currentPosition - data.seekPosition) <= (GST_SECOND))
               {
                   data.seeked = TRUE;
                   time_elapsed = std::chrono::high_resolution_clock::now();
               }

	       if (std::chrono::high_resolution_clock::now() - start > std::chrono::seconds(Seek_time_threshold))
                   break;
         }
    }
    else
    {
	 start = std::chrono::high_resolution_clock::now();
         do
         {
               message = gst_bus_pop_filtered (bus,(GstMessageType) ((GstMessageType) GST_MESSAGE_STATE_CHANGED |
                                   (GstMessageType) GST_MESSAGE_ERROR | (GstMessageType) GST_MESSAGE_EOS |
                                   (GstMessageType) GST_MESSAGE_ASYNC_DONE ));
               if (message != NULL)
               {
		    handleMessage (&data, message);
               }
               if (std::chrono::high_resolution_clock::now() - start > std::chrono::seconds(RATE_SET_TIMEOUT))
                    break;
         }while(!data.terminate && !data.seeked);
    }

    if(data.eosDetected == TRUE)
    {
	printf("\nEOS was detected, pipeline was reset, checking for trickplay change again\n");
	trickplayOperation(Param);
    }
    Param->seeked = data.seeked;
    Param->terminate = data.terminate;
    Param->currentRate = data.currentRate;
    Param->currentPosition = data.currentPosition;
    gst_object_unref (bus);
}

/********************************************************************************************************************
Purpose:               Method to handle the different messages from gstreamer bus
Parameters:
message [IN]          - GstMessage* handle to the message recieved from bus
data [OUT]	      - MessageHandlerData* handle to the custom structure to pass arguments between calling function
Return:               - None
*********************************************************************************************************************/
static void handleMessage (MessageHandlerData *data, GstMessage *message) 
{
    GError *err;
    gchar *debug_info;
    switch (GST_MESSAGE_TYPE (message)) 
    {
        case GST_MESSAGE_ERROR:
            gst_message_parse_error (message, &err, &debug_info);
            printf ("Error received from element %s: %s\n", GST_OBJECT_NAME (message->src), err->message);
            printf ("Debugging information: %s\n", debug_info ? debug_info : "none");
            g_clear_error (&err);
            g_free (debug_info);
            data->terminate = TRUE;
            break;
        case GST_MESSAGE_EOS:
            printf ("End-Of-Stream reached.\n");
	    gst_element_set_state(data->playbin, GST_STATE_NULL);
            gst_object_unref (data->playbin);
            SetupStream (data);
            break;
        case GST_MESSAGE_STATE_CHANGED:
            data->stateChanged = TRUE;
        case GST_MESSAGE_ASYNC_DONE:
	    if (data->setRateOperation == TRUE)
            {
	       data->currentRate = getRate(data->playbin);	    
               if (data->setRate == data->currentRate)
               {
                   time_elapsed = std::chrono::high_resolution_clock::now();
                   data->seeked = TRUE;
               }
            }
            else
            {
               fail_unless (gst_element_query_position (data->playbin, GST_FORMAT_TIME, &(data->currentPosition)),
                                                     "Failed to querry the current playback position");
               //Added GST_SECOND buffer time between currentPosition and seekPosition
               if (abs( data->currentPosition - data->seekPosition) <= (GST_SECOND))
               {
                   data->seeked = TRUE;
                   time_elapsed = std::chrono::high_resolution_clock::now();
               }
            }
            break;
        case GST_MESSAGE_STREAM_START:
            data->streamStart = TRUE;
            break;
        default:
            break;
    }
    gst_message_unref (message);
}

/********************************************************************************************************************
Purpose:               To set the playback rate or to seek the positionof pieline
Parameters:
playbin [IN]          - MessageHandlerData element containing seekPosition or playback rate and playbin pipeline
Return:               - None
*********************************************************************************************************************/
static void trickplayOperation(MessageHandlerData *data)
{
    /*
     * Get the current playback position
     */
    fail_unless (gst_element_query_position (data->playbin, GST_FORMAT_TIME, &data->currentPosition), "Failed to query the current playback position");
    data->seeked = FALSE;
    if(!(data->setRateOperation))
    {
	data->seekPosition = GST_SECOND * (data->seekSeconds);
	timestamp = std::chrono::high_resolution_clock::now();
	fail_unless (gst_element_seek (data->playbin, NORMAL_PLAYBACK_RATE, GST_FORMAT_TIME,
                                   GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, data->seekPosition,
                                   GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE), "Failed to seek");
    }
    else
    {
	trickplay = true;
        GST_LOG ("Setting the playback rate to %f\n", data->setRate);
        /*
         * Playback rates can be positive or negative depending on whether we are fast forwarding or rewinding
         * Below are the Playback rates used in our test scenarios
         * Fastforward Rates:
         * 	1) 2.0
         * 	2) 3.0
         * 	3) 4.0
         * Rewind Rates:
         * 	1) -2.0
         * 	2) -3.0
         * 	3) -4.0
         */
        /*
         * Rewind the pipeline if rate is a negative number
         */ 
        timestamp = std::chrono::high_resolution_clock::now();
        if (data->setRate < 0)
	{
	     fail_unless (gst_element_seek (data->playbin, data->setRate, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
                  GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_SET, data->currentPosition), "Failed to set playback rate");
        }   
    	/*
         * Fast forward the pipeline if rate is a positive number
         */
        else
        {
             fail_unless (gst_element_seek(data->playbin, data->setRate, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, data->currentPosition,
    	        GST_SEEK_TYPE_SET, GST_CLOCK_TIME_NONE), "Failed to set playback rate");
        }    
    }

    checkTrickplay(data);

    if(!(data->setRateOperation))
    {
	//Convert time to seconds
        data->currentPosition /= GST_SECOND;
        data->seekPosition /= GST_SECOND;
	printf("\nCurrentPosition %lld seconds, SeekPosition %lld seconds\n", data->currentPosition, data->seekPosition);
        fail_unless (TRUE == data->seeked, "Seek Unsuccessfull\n");
        printf("\nSEEK SUCCESSFULL :  CurrentPosition %lld seconds, SeekPosition %lld seconds\n", data->currentPosition, data->seekPosition);
        GST_LOG ("\nSEEK SUCCESSFULL :  CurrentPosition %lld seconds, SeekPosition %lld seconds\n", data->currentPosition, data->seekPosition);
    }
    else
    {
        fail_unless (data->setRate == data->currentRate, "Failed to do set rate to %f correctly\nCurrent playback rate is: %f\n", data->setRate, data->currentRate);
        printf ("\nRate is set to %s %dx speed\n",(data->setRate > 0)?"fastforward":"rewind", (int)abs (data->setRate));
	if (data->setRate < 0)
        {
	    printf("\nIn negative rate handling");
	    bool reached_start = false;
	    gint64 previous_position;

	    if (currentposition/GST_SECOND == 0)
		reached_start = true;

            int time_to_reach_start = (currentposition/GST_SECOND)/abs(data->currentRate);
            start = std::chrono::high_resolution_clock::now();

	    printf("\nTime to reach start = %d",time_to_reach_start);
            while(!reached_start)
            {
	       previous_position = currentposition;
               if ((currentposition/GST_SECOND) == 0)
               {
                  reached_start = true;
               }
               if (std::chrono::high_resolution_clock::now() - start > std::chrono::seconds(time_to_reach_start))
                  break;
               fail_unless (gst_element_query_position (data->playbin, GST_FORMAT_TIME, &currentposition), "Failed to query the current playback position");
	       if (previous_position != currentposition)
		  printf("\nCurrentPosition %lld seconds",(currentposition/GST_SECOND));
	    }

	    if (reached_start)
	    {
	       printf("\nPipeline successfully rewinded to start\n");
	       gst_element_set_state(data->playbin, GST_STATE_NULL);
	       gst_object_unref (data->playbin);
	       SetupStream (data);
	    }
            fail_unless (true == reached_start, "Pipeline was not able to rewind to start");
        }
    }
}

/********************************************************************************************************************
Purpose:               Setup stream
*********************************************************************************************************************/
static void SetupStream (MessageHandlerData *data)
{
    GstElement *playbin, *playsink;
    GstElement *westerosSink;
    GstElement *audioSink;
    gint flags;
    /*
     * Create the playbin element
     */
    playbin = gst_element_factory_make(PLAYBIN_ELEMENT, NULL);
    fail_unless (playbin != NULL, "Failed to create 'playbin' element");
    /*
     * Set the url received from argument as the 'uri' for playbin
     */
    fail_unless (m_play_url != NULL, "Playback url should not be NULL"); 
    g_object_set (playbin, "uri", m_play_url, NULL);
    /*
     * Update the current playbin flags to enable Video and Audio Playback
     */
    g_object_get (playbin, "flags", &flags, NULL);
    flags |= GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO;
    g_object_set (playbin, "flags", flags, NULL);

    if (forward_events)
    {
         /* Forward all events to all sinks */
         playsink = gst_bin_get_by_name(GST_BIN(playbin), "playsink");
         g_object_set(playsink, "send-event-mode", 0, NULL);
    }

    /*
     * Set westros-sink
     */
    westerosSink = gst_element_factory_make(WESTEROS_SINK, NULL);
    fail_unless (westerosSink != NULL, "Failed to create 'westerossink' element");

    if (!audiosink.empty())
    {
	 printf("\nAudioSink is provided as %s",audiosink.c_str());
         audioSink = gst_element_factory_make(audiosink.c_str(), NULL);
	 if (audioSink == NULL)
	 {
	     printf("\nUnable to create %s element\nPlaybin will take autoaudiosink\n",audiosink.c_str());
	 }
	 else
	 {
	     g_object_set (playbin, "audio-sink", audioSink, NULL);
	 }
    }
    /*
     * Link the westeros-sink to playbin
     */
    g_object_set (playbin, "video-sink", westerosSink, NULL);
    g_object_set (playbin, "async-handling", true, NULL);
    /*
     * Set the first frame recieved callback
     */
    g_signal_connect( westerosSink, "first-video-frame-callback", G_CALLBACK(firstFrameCallback), &firstFrameReceived);
    /*
     * Set the firstFrameReceived variable as false before starting play
     */
    firstFrameReceived= false;
    /*
     * Set playbin to PLAYING
     */
    GST_FIXME( "Setting to Playing State\n");
    fail_unless (gst_element_set_state (playbin, GST_STATE_PLAYING) !=  GST_STATE_CHANGE_FAILURE);
    GST_FIXME( "Set to Playing State\n");

    Sleep(5);
    /*
     * Check if the first frame received flag is set
     */
    fail_unless (firstFrameReceived == true, "Failed to receive first video frame signal");

    PlaySeconds(playbin,5);
    data->playbin = playbin;
}

GST_START_TEST (trickplayTest)
{
    string operationString;
    double operationTimeout = 10.0;
    int seekSeconds = 0;
    bool seekOperation = false;
    gdouble rate = 1;
    GstBus *bus;
    GstStateChangeReturn state_change;
    GstState cur_state;
    MessageHandlerData data;
    int timeout = 0;
    bool is_av_playing = false;
    vector<string>::iterator operationItr;
    char* operationBuffer = NULL;

    SetupStream(&data);
    bus = gst_element_get_bus(data.playbin);
    gst_bus_add_watch(bus, (GstBusFunc) handleMessage, NULL);

    /*
     * Iterate through the list of operations recieved as input arguments and execute each of them for the requesed operation and timeperiod
     */
    for (operationItr = operationsList.begin(); operationItr != operationsList.end(); ++operationItr)
    {
	/*
	 * Operating string will be in operation:operationTimeout format,
	 * so split the string to retrieve operation string and timeout values
	 */
        operationBuffer = strdup ((*operationItr).c_str());
        operationString = strtok (operationBuffer, ":");
	operationTimeout = atof (strtok (NULL, ":"));
	if (!operationString.empty())
	{
	    timeout = operationTimeout;
        }
        if ("fastforward2x" == operationString)
        {
	    /*
	     * Fastforward the pipeline to 2
	     */
            rate = FASTFORWARD2x_RATE;
        }
	else if ("fastforward4x" == operationString)
        {
	    /*
	     * Fastforward the pipeline to 4
	     */
	    rate = FASTFORWARD4x_RATE;
	}
	else if ("fastforward3x" == operationString)
	{
	    /*
	     * Fastforward the pipeline to 3
	     */
	    rate = FASTFORWARD3x_RATE;	
	}
	else if ("rewind2x" == operationString)
	{
	    /*
	     * Rewind the pipeline to -2
	     */
	    rate = REWIND2x_RATE;
	}
	else if ("rewind4x" == operationString)
        {
	    /*
	     * Rewind the pipeline to -4
	     */
	    rate = REWIND4x_RATE;
	}
	else if ("rewind3x" == operationString)
	{
	    /*
	     * Rewind the pipeline to -3
	     */
	    rate = REWIND3x_RATE;
	}
	else if ("rate" == operationString)
	{
	    /*
	     * Set any rate mentioned
	     */
	     rate = atof(strtok(NULL, ":"));
	     printf("\nRequested playback rate is %f",rate);
	}
	else if ("seek" == operationString)
        {
	    /*
	     * Acquire seek seconds and set playabck rate to 1
	     */
	    seekSeconds = atoi(strtok(NULL, ":"));
	    seekOperation = true;
	    rate = 1;
	}
	else if ( ("play" == operationString) || ("pause" == operationString) )
        {
	    /*
	     * Playback rate is set to 1 if operation is play/pause
	     */
            rate = 1;
        }
	else
	{
	    GST_ERROR ("Invalid operation\n");
	}	
        
	pause_operation = false;

	data.currentRate = getRate(data.playbin);
        fail_unless (gst_element_query_position (data.playbin, GST_FORMAT_TIME, &data.currentPosition), "Failed to query the current playback position");

	if (rate != data.currentRate)
	{
	    if (rate < 0)
            {
                fail_unless (gst_element_query_duration (data.playbin, GST_FORMAT_TIME, &data.duration), "Failed to query the duration");
		printf("\nEntering negative rate operation\n");

	        /*Seek to  end of stream - 20 seconds for rewind testcases
		data.setRateOperation = FALSE;
	        data.seekSeconds = (abs(rate))*20;
		trickplayOperation(&data);*/
	    }
	    data.setRateOperation = TRUE;
	    data.setRate = rate;
            trickplayOperation(&data);
	    if (!(rate < 0))
	    {
		/* Playing for 20 seconds with 4x speed is equal to playing until position is 4*20 = 80 seconds */
                operationTimeout *= abs(rate);
            }
	    data.setRateOperation = FALSE;
	    WaitForOperation;
	    fail_unless (gst_element_query_position (data.playbin, GST_FORMAT_TIME, &startPosition), "Failed to query the current playback position");
	    if (latency_check_test)
            {
                latency = std::chrono::duration_cast<std::chrono::milliseconds>(time_elapsed - timestamp);
                parselatency();
            }

	}
        
    
	if (seekOperation)
	{
	    trickplay = false;
	    data.setRateOperation = FALSE;
	    data.seekSeconds = seekSeconds;
	    trickplayOperation(&data);
	    seekOperation = false;
	    startPosition = seekSeconds * GST_SECOND;
	    if (latency_check_test)
            {
                latency = std::chrono::duration_cast<std::chrono::milliseconds>(time_elapsed - timestamp);
                parselatency();
            }
	}

	if ("play" == operationString)
        {
	    trickplay = false;
	    /*
             * If pipeline is already in playing state with normal playback rate (1.0),
	     * just wait for operationTimeout seconds, instead os setting the pipeline to playing state again
	     */	
	    fail_unless_equals_int (gst_element_get_state (data.playbin, &cur_state,
                                                                  NULL, 0), GST_STATE_CHANGE_SUCCESS);
	    if ((cur_state != GST_STATE_PLAYING))
	    {
              	 /* Set the playbin state to GST_STATE_PLAYING
                  */
	         fail_unless (gst_element_set_state (data.playbin, GST_STATE_PLAYING) !=  GST_STATE_CHANGE_FAILURE);
		 do{
		 //Waiting for state change
                    /*
                     * Polling for the state change to reflect with 10 ms timeout
                     */
                    state_change = gst_element_get_state (data.playbin, &cur_state, NULL, 10000000);
                 } while (state_change == GST_STATE_CHANGE_ASYNC);
		 printf ("\n********Current state is: %s \n", gst_element_state_get_name(cur_state));
             }
	     fail_unless (gst_element_query_position (data.playbin, GST_FORMAT_TIME, &startPosition), "Failed to query the current playback position");
             /*
	      * Wait for the requested time
	      */
             printf ("Waiting for %f seconds\n", operationTimeout);
	 } 
	 else if ("pause" == operationString)
         {
	     trickplay = false;
	     pause_operation = true;
             /*
	      * Set the playbin state to GST_STATE_PAUSED
	      */	
	     fail_unless (gst_element_query_position (data.playbin, GST_FORMAT_TIME, &startPosition), "Failed to query the current playback position");
	     gst_element_set_state (data.playbin, GST_STATE_PAUSED);
	     PlaySeconds(data.playbin,5);
             fail_unless_equals_int (gst_element_get_state (data.playbin, &cur_state, NULL, 0), GST_STATE_CHANGE_SUCCESS);
	     fail_unless_equals_int (cur_state, GST_STATE_PAUSED);
	     printf("\n********Current state: %s\n",gst_element_state_get_name(cur_state));
             GST_LOG("\n********Current state: %s\n",gst_element_state_get_name(cur_state));
	     /*
	      * Sleep for the requested time
	      */
	     operationTimeout -= 5;
	 }
	 if (true == checkAVStatus)
         {
             is_av_playing = check_for_AV_status();
             fail_unless (is_av_playing == true, "Video is not playing in TV");
             printf ("DETAILS: SUCCESS, Video playing successfully \n");
 	 }

	 timeout=operationTimeout;
	 PlaySeconds(data.playbin,timeout);
    }
    if (data.playbin)
    {
       gst_element_set_state(data.playbin, GST_STATE_NULL);
    }
    /*
     * Cleanup after use
     */
    gst_object_unref (data.playbin);
    gst_object_unref(bus);
}
GST_END_TEST;

int main (int argc, char **argv)
{
    int returnValue = 0;
    int arg = 0;
    char *operationStr = NULL;
    char *operation = NULL;
    double timeout = 0;
    Suite *gstPluginsSuite;
    TCase *tc_chain;

    /*
     * Get TDK path
     */
    if (getenv ("TDK_PATH") != NULL)
    {
        strcpy (TDK_PATH, getenv ("TDK_PATH"));
    }
    else
    {
        GST_ERROR ("Environment variable TDK_PATH should be set!!!!");
        printf ("Environment variable TDK_PATH should be set!!!!");
        returnValue = 0;
        goto exit;
    }
    strcpy(m_play_url,argv[1]);
    for (arg = 2; arg < argc; arg++)
    {
        if (strstr (argv[arg], "operations=") != NULL)
        {
            /*
             * The trickplay operations can be given in
             * operations="" argument as coma separated string
             * eg: operations=play:play_timeout,fastforward2x:timeout,seek:timeout:seekvalue
             */
            strtok (argv[arg], "=");
            operationStr = strtok(NULL, "=");
            operation = strtok (operationStr, ",");
            while (operation != NULL)
            {
               operationsList.push_back(operation);
               operation = strtok (NULL, ",");
            }
         }
         if (strcmp ("checkavstatus=yes", argv[arg]) == 0)
         {
            checkAVStatus = true;
	 }
	 if (strcmp ("checkPTS=no", argv[arg]) == 0)
         {
            checkPTS = false;
         }
	 if (strcmp ("checkLatency", argv[arg]) == 0)
	 {
	    latency_check_test = true;
	 }
	 if (strstr (argv[arg], "audioSink=") != NULL)
         {
             strtok (argv[arg], "=");
             audiosink = (strtok (NULL, "="));
         }
	 if (strcmp ("forwardEvents=no", argv[arg]) == 0)
         {
            forward_events = false;
         }
    }
    gst_check_init (&argc, &argv);

    gstPluginsSuite = suite_create ("playbin_plugin_test");
    tc_chain = tcase_create ("general");
    /*
     * Set timeout to play_timeout if play_timeout > DEFAULT_TEST_SUITE_TIMEOUT(360) seconds
     */
    if (play_timeout > DEFAULT_TEST_SUITE_TIMEOUT)
    {
        timeout = play_timeout;
    }
    tcase_set_timeout (tc_chain, timeout);
    suite_add_tcase (gstPluginsSuite, tc_chain);
    tcase_add_test (tc_chain, trickplayTest);
    returnValue =  gst_check_run_suite (gstPluginsSuite, "playbin_plugin_test", __FILE__);
exit:
    return returnValue;
}
