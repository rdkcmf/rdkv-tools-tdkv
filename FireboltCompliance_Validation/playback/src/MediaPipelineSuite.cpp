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
#include <sstream>
#include <chrono>
extern "C"
{
#include <gst/check/gstcheck.h>
#include <gst/gst.h>
}

using namespace std;

#define EOS_TIMEOUT 			120
#define DEFAULT_TEST_SUITE_TIMEOUT	360
#define VIDEO_STATUS 			"/CheckVideoStatus.sh"
#define AUDIO_STATUS 			"/CheckAudioStatus.sh"
#define PLAYBIN_ELEMENT 		"playbin"
#define WESTEROS_SINK 			"westerossink"
#define BUFFER_SIZE_LONG		1024
#define BUFFER_SIZE_SHORT		264
#define NORMAL_PLAYBACK_RATE		1.0
#define FCS_MICROSECONDS		1000000
#define Sleep(RunSeconds)               start = std::chrono::steady_clock::now(); \
                                        Runforseconds = RunSeconds; \
                                        while(1) { \
                                        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(Runforseconds)) \
                                             break; \
                                        }
#define WaitForOperation                Sleep(5)

char m_play_url[BUFFER_SIZE_LONG] = {'\0'};
char tcname[BUFFER_SIZE_SHORT] = {'\0'};
char TDK_PATH[BUFFER_SIZE_SHORT] = {'\0'};
char channel_url[BUFFER_SIZE_LONG] = {'\0'};
char audio_change_test[BUFFER_SIZE_SHORT] = {'\0'};
vector<string> operationsList;

/*
 * Default values for avstatus check flag and play_timeout if not received as input arguments
 */

bool checkAVStatus = false;
bool checkPTS = true;
int play_timeout = 10;
int Runforseconds;
auto start = std::chrono::steady_clock::now();
bool latency_check_test = false;
bool firstFrameReceived = false;
bool use_fpsdisplaysink = true;
int SecondChannelTimeout =0;
bool ChannelChangeTest = false;
GstClockTime timestamp, latency, time_elapsed;
bool audioChanged = false;
bool videoChanged = false;
bool writeToFile= false;
FILE *filePointer;

/*
 * Playbin flags
 */

typedef enum {
  GST_PLAY_FLAG_VIDEO         = (1 << 0),
  GST_PLAY_FLAG_AUDIO         = (1 << 1),
  GST_PLAY_FLAG_TEXT          = (1 << 2)
} GstPlayFlags;

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
    gint n_text;                        /* Number of embedded text streams */
    gint n_audio;                       /* Number of embedded audio streams */
    gint n_video;                       /* Number of embedded video streams */
    gint current_video;                 /* Currently playing video streams */
    gint current_text;                  /* Currently playing text stream */
    gint current_audio;                 /* Currently playing audio stream */
} MessageHandlerData;

/*
 * Methods
 */
static void handleMessage (MessageHandlerData *data, GstMessage *message);

/*******************************************************************************************************************************************
Purpose:                To continue the state of the pipeline and check whether operation is being carried throughout the specified interval
Parameters:
playbin                   - The pipeline which is to be monitored
RunSeconds:               - The interval for which pipeline should be monitored
********************************************************************************************************************************************/
static void PlaySeconds(GstElement* playbin,int RunSeconds)
{
   gint64 startPosition;
   gint64 currentPosition;
   gint difference;
   GstMessage *message;
   GstBus *bus;
   MessageHandlerData data;
   gint64 pts;
   gint64 old_pts;
   gint pts_buffer=5;
   gint RanForTime=0;
   GstElement *videoSinkFromPlaybin;
   GstElement *videoSink;
   GstState cur_state;
   gint play_jump = 0;
   gint previous_position = 0;
   gint jump_buffer = 3;
   GstStateChangeReturn state_change;
   gint dropped_frames;
   gint rendered_frames;
   gint previous_rendered_frames;
   float drop_rate;

   /* Update data variables */
   data.playbin = playbin;
   data.setRateOperation = FALSE;
   data.terminate = FALSE;
   data.eosDetected = FALSE;

   do
   {
       /*
        * Polling for the state change to reflect with 1s timeout
        */
       state_change = gst_element_get_state (playbin, &cur_state, NULL, GST_SECOND);
   }while (state_change == GST_STATE_CHANGE_ASYNC);

   fail_unless (gst_element_query_position (playbin, GST_FORMAT_TIME, &startPosition), "Failed to query the current playback position");
   fail_unless (state_change != GST_STATE_CHANGE_FAILURE, "Failed to get current playbin state");
   g_object_get (playbin,"video-sink",&videoSinkFromPlaybin,NULL);

   if (use_fpsdisplaysink)
   {
	//Get westerossink from fpsdisplaysink
	g_object_get (videoSinkFromPlaybin, "video-sink",&videoSink,NULL);
   }
   else
   {
	videoSink = videoSinkFromPlaybin;
   }

   if (checkPTS)
   {
        g_object_get (videoSink,"video-pts",&pts,NULL);
        old_pts = pts;
   }

   printf("\nRunning for %d seconds, start Position is %lld\n",RunSeconds,startPosition/GST_SECOND);
   fail_unless (gst_element_query_position (playbin, GST_FORMAT_TIME, &currentPosition), "Failed to query the current playback position");
   previous_position = (currentPosition/GST_SECOND);

   if ((cur_state == GST_STATE_PAUSED)  && (state_change != GST_STATE_CHANGE_ASYNC))
   {
        do
        {
            Sleep(1);
            printf("\nCurrent State is PAUSED , waiting for %d\n", RunSeconds);
	    fail_unless (gst_element_query_position (playbin, GST_FORMAT_TIME, &currentPosition), "Failed to query the current playback position");
	    play_jump = int(currentPosition/GST_SECOND) - previous_position;
            previous_position = (currentPosition/GST_SECOND);

	    fail_unless(play_jump == 0,"Playback is not PAUSED");

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

   if (use_fpsdisplaysink)
   {
	g_object_get (videoSinkFromPlaybin,"frames-rendered", &previous_rendered_frames, NULL);
   }

   bus = gst_element_get_bus (playbin);
   do
   {
	Sleep(1);
        fail_unless (gst_element_query_position (playbin, GST_FORMAT_TIME, &currentPosition), "Failed to query the current playback position");
        difference = int(abs((currentPosition/GST_SECOND) - (startPosition/GST_SECOND)));
        printf("\nCurrent Position : %lld , Playing after operation for: %d",(currentPosition/GST_SECOND),difference);

	play_jump = int(currentPosition/GST_SECOND) - previous_position;
	printf("\nPlay jump = %d", play_jump);

	if (use_fpsdisplaysink)
	{
	    g_object_get (videoSinkFromPlaybin,"frames-dropped",&dropped_frames,NULL);
	    g_object_get (videoSinkFromPlaybin,"frames-rendered", &rendered_frames, NULL);
	    fail_unless (rendered_frames > previous_rendered_frames, "Frames not rendered properly");
	    drop_rate = ((float)dropped_frames/rendered_frames)*100;
	    printf("\n\nDropped Frames %d \nRendered frames %d \nDrop Rate %f", dropped_frames, rendered_frames, drop_rate);
	    previous_rendered_frames = rendered_frames;
	}

	if (play_jump != NORMAL_PLAYBACK_RATE)
            jump_buffer -=1;

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
        else
        {
            printf ("All messages are clear. No more message after seek\n");
            break;
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
	    old_pts = pts;
	}

	fail_unless(jump_buffer != 0 , "Playback is not happening at the expected rate");
	previous_position = (currentPosition/GST_SECOND);
   }while((difference <= RunSeconds) && !data.terminate && !data.eosDetected);

   printf("\nExiting from PlaySeconds, currentPosition is %lld\n",currentPosition/GST_SECOND);
   gst_object_unref (bus);
}

/*
 * Methods
 */

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

/******************************************************************************************************************
 * Purpose:                Callback function to get framerate info from fpsdisplaysink
 *****************************************************************************************************************/
static void FPSCallback (GstElement *fpsdisplaysink, gdouble fps, gdouble droprate, gdouble avgfps, gpointer udata)
{
   printf("\nDrop Rate:%lf \nCurrent FrameRate:%lf \nAvg Fps : %lf",droprate,fps,avgfps);
}

/******************************************************************************************************************
 * Purpose:                Write Frame Rate info obtained from fpsdisplaysink to video_info file
 *****************************************************************************************************************/
static void writeFPSdata (GstElement *fpsdisplaysink)
{
   FILE *filePointer;
   gchar *fps_msg;
   char video_info[BUFFER_SIZE_SHORT] = {'\0'};
   strcat (video_info, TDK_PATH);
   strcat (video_info, "/video_info");
   filePointer = fopen(video_info, "w");
   if (filePointer != NULL)
   {
       g_object_get (G_OBJECT (fpsdisplaysink), "last-message", &fps_msg, NULL);
       if (fps_msg != NULL)
       {
           g_print ("Frame info: %s\n", fps_msg);
	   fprintf(filePointer,"%s\n", fps_msg);
       }
   }
   fclose(filePointer);
}

static void parselatency()
{
    printf("\nLatency = %" GST_TIME_FORMAT"\n",GST_TIME_ARGS(latency));
    latency = GST_TIME_AS_MSECONDS(latency);
    printf("\nLatency = %lld milliseconds\n", latency);
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
        fprintf(filePointer,"Latency = %lld milliseconds\n", latency);
    }
    else
    {
	printf("\nLatency writing operation failed\n");
    }
    fclose(filePointer);
}

/* Print all propeties of the stream represented in the tag */
static void printTag (const GstTagList *tags, const gchar *tag, gpointer user_data)
{
    GValue value = { 0, };
    gchar *Data;
    gint depth = GPOINTER_TO_INT (user_data);
    
    gst_tag_list_copy_value (&value, tags, tag);

    if (G_VALUE_HOLDS_STRING (&value))
        Data = g_value_dup_string (&value);
    else
        Data = gst_value_serialize (&value);

    g_print ("%*s%s: %s\n", 2 * depth, " ", gst_tag_get_nick (tag), Data);
    if (writeToFile == true)
        fprintf(filePointer,"%*s%s: %s\n", 2 * depth, " ", gst_tag_get_nick (tag), Data);
    g_free (Data);
    g_value_unset (&value);
}

/*
 * Based on GStreamer sample code, which is:
 * Copyright (C) GStreamer developers
 * Here licensed under the MIT license
 */

static void getStreamProperties (MessageHandlerData *data)
{
    gint i;
    GstTagList *tags;

    /* Read some properties */
    g_object_get (data->playbin, "n-video", &data->n_video, NULL);
    g_object_get (data->playbin, "n-audio", &data->n_audio, NULL);

    g_print ("Stream contains %d audio stream(s)\n",data->n_audio);

    /* Retrieve the stream's video tags
    g_print ("\n");
    for (i = 0; i < data->n_video; i++) 
    {
        tags = NULL;
        g_signal_emit_by_name (data->playbin, "get-video-tags", i, &tags);
        if (tags) 
	{
           g_print ("Tags:\n");
           gst_tag_list_foreach (tags, printTag, GINT_TO_POINTER (1));
           gst_tag_list_free (tags);
        }
    }*/

    g_print ("\n");
    for (i = 0; i < data->n_audio; i++) 
    {
        tags = NULL;
        /* Retrieve the stream's audio tags */
        g_signal_emit_by_name (data->playbin, "get-audio-tags", i, &tags);
        if (tags) 
	{
            g_print ("Tags:\n");
            gst_tag_list_foreach (tags, printTag, GINT_TO_POINTER (1));
            gst_tag_list_free (tags);
        }
    }


    g_object_get (data->playbin, "current-audio", &data->current_audio, NULL);

    g_print ("\n");
    g_print ("Currently playing audio stream %d \n", data->current_audio);
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

/********************************************************************************************************************
Purpose:               Callback function to set a variable to true on receiving first frame
*********************************************************************************************************************/
static void firstFrameCallback(GstElement *sink, guint size, void *context, gpointer data)
{
   bool *gotFirstFrameSignal = (bool*)data;

   /* Get timestamp of firstFrameRecived to calculate latency */
   if ((latency_check_test) && (!firstFrameReceived))
   {
       timestamp = gst_clock_get_time ((sink)->clock);
   }

   printf ("Received first frame signal\n");
   /*
    * Set the Value to global variable once the first frame signal is received
    */
   *gotFirstFrameSignal = true;
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
    GstQuery *query;

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
            data->eosDetected = TRUE;
            data->terminate = TRUE;
            break;
            /*
            * In case of seek event, state change of various gst elements occur asynchronously
            * We can check if the seek event happened in between by querying the current position while ASYNC_DONE message is retrieved
            * If the current position is not updated, we will wait until bus is clear or error/eos occurs
            */
        case GST_MESSAGE_STATE_CHANGED:
            data->stateChanged = TRUE;
        case GST_MESSAGE_ASYNC_DONE:
	    if (data->setRateOperation == TRUE)
            {
               query = gst_query_new_segment (GST_FORMAT_DEFAULT);
               fail_unless (gst_element_query (data->playbin, query), "Failed to query the current playback rate");
               gst_query_parse_segment (query, &(data->currentRate), NULL, NULL, NULL);
               if (data->setRate == data->currentRate)
               {
                   time_elapsed = gst_clock_get_time ((data->playbin)->clock);
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
                   time_elapsed = gst_clock_get_time ((data->playbin)->clock);
               }
            }
            break;
        case GST_MESSAGE_STREAM_START:
            data->streamStart = TRUE;
            break;
        default:
            printf ("Unexpected message received.\n");
            data->terminate = TRUE;
            break;
    }
    gst_message_unref (message);
}

/********************************************************************************************************************
 * Purpose     	: Test to do basic initialisation and shutdown of gst pipeline with playbin element and westeros-sink
 * Parameters   : Playback url
 ********************************************************************************************************************/
GST_START_TEST (test_init_shutdown)
{
    GstElement *playbin;
    GstElement *westerosSink;
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

    /*
     * Set westros-sink
     */
    westerosSink = gst_element_factory_make(WESTEROS_SINK, NULL);
    fail_unless (westerosSink != NULL, "Failed to create 'westerossink' element");

    /*
     * Link the westeros-sink to playbin
     */
    g_object_set (playbin, "video-sink", westerosSink, NULL);

    if (playbin)
    {
       gst_element_set_state (playbin, GST_STATE_NULL);
    }
    /*
     * Cleanup after use
     */
    gst_object_unref (playbin);
}
GST_END_TEST;

/********************************************************************************************************************
 * Purpose     	: Test to do generic playback using playbin element and westeros-sink
 * Parameters   : Playback url
 ********************************************************************************************************************/
GST_START_TEST (test_generic_playback)
{
    bool is_av_playing = false;
    GstElement *playbin;
    GstElement *westerosSink;
    GstElement *fpssink;
    gint flags;
    GstMessage *message;
    GstBus *bus;
    MessageHandlerData data;

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

    /*
     * Set westros-sink
     */
    westerosSink = gst_element_factory_make(WESTEROS_SINK, NULL);
    fail_unless (westerosSink != NULL, "Failed to create 'westerossink' element");

    /*
     * Link video-sink with playbin
     */
    if (use_fpsdisplaysink)
    {
        /*
         * Create and link fpsdisplaysink if configuration is enabled
         */
        fpssink = gst_element_factory_make ("fpsdisplaysink", NULL);
        fail_unless (fpssink != NULL, "Failed to create 'fpsdisplaysink' element");
        g_object_set (playbin, "video-sink",fpssink, NULL);
        g_object_set (fpssink, "video-sink", westerosSink, NULL);
        g_object_set (fpssink, "signal-fps-measurements", true, NULL);
        g_object_set (fpssink, "fps-update-interval", 1000, NULL);
        g_signal_connect(fpssink, "fps-measurements", G_CALLBACK(FPSCallback), NULL);
    }
    else
    {
        g_object_set (playbin, "video-sink", westerosSink, NULL);
    }

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

    WaitForOperation;
    /*
     * Check if the first frame received flag is set
     */
    fail_unless (firstFrameReceived == true, "Failed to receive first video frame signal");

    /*
     * Wait for 'play_timeout' seconds(recieved as the input argument) before checking AV status
     */
    PlaySeconds(playbin,play_timeout);
    /*
     * Write FrameRate info to video_info file
     */
    if (use_fpsdisplaysink)
    {
        writeFPSdata(fpssink);
    }
    /*
     * Calculate latency by obtaining the sink base time
     */
    if (latency_check_test)
    {
         latency = timestamp - gst_element_get_base_time (GST_ELEMENT_CAST (westerosSink));
	 parselatency();
    }
    /*
     * Check for AV status if its enabled
     */
    if (true == checkAVStatus)
    {
        is_av_playing = check_for_AV_status();
        fail_unless (is_av_playing == true, "Video is not playing in TV");
    }
    GST_LOG("DETAILS: SUCCESS, Video playing successfully \n");

    if (playbin)
    {
       fail_unless (gst_element_set_state (playbin, GST_STATE_NULL) !=  GST_STATE_CHANGE_FAILURE);
    }
    if (ChannelChangeTest)
    {
       /*
        * Set the url received from argument as the 'channel_url' for playbin
        */
       fail_unless (channel_url != NULL, "Second Channel url should not be NULL");
       g_object_set (playbin, "uri", channel_url, NULL);

       /*
        * Set all the required variables before polling for the message
        */
       data.streamStart= FALSE;
       data.terminate = FALSE;
       data.playbin = playbin;

       printf("\nLoading Second Channel\n");

       /*
        * Set playbin to PLAYING
        */
       GST_LOG( "Setting Second Channel to Playing State\n");
       fail_unless (gst_element_set_state (playbin, GST_STATE_PLAYING) !=  GST_STATE_CHANGE_FAILURE);
       GST_LOG( "Second Channel Set to Playing State\n");
       
       bus = gst_element_get_bus (playbin);
       do
       {
           message = gst_bus_timed_pop_filtered(bus, 2 * GST_SECOND,(GstMessageType)((GstMessageType) GST_MESSAGE_STREAM_START|(GstMessageType) GST_MESSAGE_ERROR | (GstMessageType) GST_MESSAGE_EOS ));
           if (NULL != message)
           {
               handleMessage (&data, message);
           }
           else
           {
               printf ("All messages are clear. No more message after seek\n");
               break;
           }
       }while(!data.streamStart && !data.terminate);

       /*
        * Verify that ERROR/EOS messages are not recieved
        */
       fail_unless (FALSE == data.terminate, "Unexpected error or End of Stream recieved\n");
       /*
        * Verify that STREAM_START message is received
        */
       fail_unless (TRUE == data.streamStart, "Unable to obtain message indicating start of a new stream\n");
       /*
        * Check for AV status if its enabled
        */
       if (true == checkAVStatus)
       {
          is_av_playing = check_for_AV_status();
          fail_unless (is_av_playing == true, "Video is not playing in TV");
       }
       GST_LOG("DETAILS: SUCCESS, Video playing successfully \n");
       /*
        * Wait for 'SecondChannelTimeout' seconds(received as the input argument) for video to play
        */
       PlaySeconds(playbin,SecondChannelTimeout);

       if (playbin)
       {
          gst_element_set_state (playbin, GST_STATE_NULL);
       }
    }
    /*
     * Cleanup after use
     */
    gst_object_unref (playbin);
}
GST_END_TEST;

/********************************************************************************************************************
 * Purpose      : Test to change the state of pipeline from play to pause
 * Parameters   : Playback url
 ********************************************************************************************************************/
GST_START_TEST (test_play_pause_pipeline)
{
    bool is_av_playing = false;
    gint flags;
    GstState cur_state;
    GstElement *playbin;
    GstElement *westerosSink;
    GstElement *fpssink;
    
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

    /*
     * Create westerosSink instance
     */
    westerosSink = gst_element_factory_make(WESTEROS_SINK, NULL);
    fail_unless (westerosSink != NULL, "Failed to create 'westerossink' element");

    /*
     * Link video-sink with playbin
     */
    if (use_fpsdisplaysink)
    {
	/*
	 * Create and link fpsdisplaysink if configuration is enabled
	 */
        fpssink = gst_element_factory_make ("fpsdisplaysink", NULL);
	fail_unless (fpssink != NULL, "Failed to create 'fpsdisplaysink' element");
        g_object_set (playbin, "video-sink",fpssink, NULL);
        g_object_set (fpssink, "video-sink", westerosSink, NULL);
        g_object_set (fpssink, "signal-fps-measurements", true, NULL);
        g_object_set (fpssink, "fps-update-interval", 1000, NULL);
        g_signal_connect(fpssink, "fps-measurements", G_CALLBACK(FPSCallback), NULL);
    }
    else
    {
	g_object_set (playbin, "video-sink", westerosSink, NULL);
    }

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

    WaitForOperation;

    /*
     * Check if the first frame received flag is set
     */
    fail_unless (firstFrameReceived == true, "Failed to receive first video frame signal");

    /*
     * Wait for 'play_timeout' seconds(recieved as the input argument) before changing the pipeline state
     * We are waiting for 5 more seconds before checking pipeline status, so reducing the wait here
     */
    PlaySeconds(playbin,play_timeout - 5);
    int height;
    int width;
    g_object_get (westerosSink, "video-height", &height, NULL);
    g_object_get (westerosSink, "video-width", &width, NULL);
    printf("\nVideo height = %d\nVideo width = %d", height, width);

    /*
     * Ensure that playback is happening before pausing the pipeline
     */

    /*
     * Check for AV status if its enabled
     */
    if (true == checkAVStatus)
    {
        is_av_playing = check_for_AV_status();
        fail_unless (is_av_playing == true, "Video is not playing in TV");
    }
    printf ("\nDETAILS: SUCCESS, Video playing successfully\n");

    /*
     * Set pipeline to PAUSED
     */
    gst_element_set_state (playbin, GST_STATE_PAUSED);

    /*
     * Wait for 5 seconds before checking the pipeline status
     */
    PlaySeconds(playbin,5);
    fail_unless_equals_int (gst_element_get_state (playbin, &cur_state,
            NULL, 0), GST_STATE_CHANGE_SUCCESS);
    GST_LOG("\n********Current state: %s\n",gst_element_state_get_name(cur_state));

    fail_unless_equals_int (cur_state, GST_STATE_PAUSED);
    printf ("DETAILS: SUCCESS, Current state is: %s \n", gst_element_state_get_name(cur_state));

    /*
     * Write FrameRate info to video_info file
     */
    if (use_fpsdisplaysink)
    {
        writeFPSdata(fpssink);
    }

    if (playbin)
    {
       gst_element_set_state(playbin, GST_STATE_NULL);
    }

    /*
     * Cleanup after use
     */
    gst_object_unref (playbin);
}
GST_END_TEST;

/********************************************************************************************************************
 * Purpose      : Test to check that EOS message is recieved
 * Parameters   : Playback url
 ********************************************************************************************************************/
GST_START_TEST (test_EOS)
{
    bool is_av_playing = false;
    GstMessage *message;
    GstBus *bus;
    bool received_EOS = false;
    gint flags;
    GstElement *playbin;
    GstElement *westerosSink;
    GstElement *fpssink;

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

    /*
     * Set westros-sink
     */
    westerosSink = gst_element_factory_make(WESTEROS_SINK, NULL);
    fail_unless (westerosSink != NULL, "Failed to create 'westerossink' element");

    /*
     * Link video-sink with playbin
     */
    if (use_fpsdisplaysink)
    {
        /*
         * Create and link fpsdisplaysink if configuration is enabled
         */
        fpssink = gst_element_factory_make ("fpsdisplaysink", NULL);
        fail_unless (fpssink != NULL, "Failed to create 'fpsdisplaysink' element");
        g_object_set (playbin, "video-sink",fpssink, NULL);
        g_object_set (fpssink, "video-sink", westerosSink, NULL);
        g_object_set (fpssink, "signal-fps-measurements", true, NULL);
        g_object_set (fpssink, "fps-update-interval", 1000, NULL);
        g_signal_connect(fpssink, "fps-measurements", G_CALLBACK(FPSCallback), NULL);
    }
    else
    {
        g_object_set (playbin, "video-sink", westerosSink, NULL);
    }

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
    WaitForOperation;
    /*
     * Check if the first frame received flag is set
     */
    fail_unless (firstFrameReceived == true, "Failed to receive first video frame signal");
    /*
     * Wait for 5 seconds before checking AV status
     */
    PlaySeconds (playbin,5);
    /*
     * Check for AV status if its enabled
     */
    if (true == checkAVStatus)
    {
        is_av_playing = check_for_AV_status();
        fail_unless (is_av_playing == true, "Video is not playing in TV");
    }
    printf ("DETAILS: SUCCESS, Video playing successfully \n");

    /*
     * Polling the bus for messages
     */
    bus = gst_element_get_bus (playbin);
    fail_if (bus == NULL);
    /* 
     * Wait for receiving EOS event
     */
    start = std::chrono::steady_clock::now();
    do
    {
	message = gst_bus_pop (bus);
        if (message != NULL)
        {
            if (GST_MESSAGE_EOS == GST_MESSAGE_TYPE(message))
                received_EOS = true;
        }
	if (std::chrono::steady_clock::now() - start > std::chrono::seconds(EOS_TIMEOUT))
            break;
    }while(!received_EOS);

    fail_unless (received_EOS == true, "Failed to recieve EOS message");
    printf ("EOS Received\n");
    gst_message_unref (message);

    /*
     * Write FrameRate info to video_info file
     */
    if (use_fpsdisplaysink)
    {
        writeFPSdata(fpssink);
    }

    if (playbin)
    {
       gst_element_set_state(playbin, GST_STATE_NULL);
    }
    /*
     * Cleanup after use
     */
    gst_object_unref (playbin);
    gst_object_unref (bus);
}
GST_END_TEST;

GST_START_TEST (test_frameDrop)
{
    GstElement *pipeline, *source, *sink;
    gchar *fps_msg;
    GstBus *bus;
    gint jump_buffer = 3;
    gint play_jump = 0;
    int rendered,dropped;
    double current,average;
    int previous_rendered_frames, rendered_frames;
    gint64 currentPosition;
    gint previous_position = 0;
    gint difference;
    gint64 startPosition;
    float drop_rate;

    /* Create the elements */
    source = gst_element_factory_make ("videotestsrc", "source");
    fail_unless (source != NULL, "Failed to create 'videotestsrc' element");
    sink = gst_element_factory_make ("fpsdisplaysink", "sink");
    fail_unless (sink  != NULL, "Failed to create 'fpsdisplaysink'  element");

    /* Create the empty pipeline */
    pipeline = gst_pipeline_new ("test-pipeline");
    fail_unless (pipeline != NULL, "Failed to create 'pipeline'");

    /* Build the pipeline */
    gst_bin_add_many (GST_BIN (pipeline), source, sink, NULL);
    fail_unless (gst_element_link (source, sink) == true, "Elements could not be linked.");

    /* Modify the source's properties */
    g_object_set (source, "pattern", 0, NULL);

    /*
     * Set pipeline to PLAYING
     */
    GST_FIXME( "Setting to Playing State\n");
    fail_unless (gst_element_set_state (pipeline, GST_STATE_PLAYING) !=  GST_STATE_CHANGE_FAILURE);
    GST_FIXME( "Set to Playing State\n");

    WaitForOperation;

    fail_unless (gst_element_query_position (pipeline, GST_FORMAT_TIME, &currentPosition), "Failed to query the current playback position");
    g_object_get (sink,"frames-rendered", &rendered_frames, NULL);
    previous_position = (currentPosition/GST_SECOND);
    previous_rendered_frames = rendered_frames;

    bus = gst_element_get_bus (pipeline);
    do
    {
        Sleep(1);
        fail_unless (gst_element_query_position (pipeline, GST_FORMAT_TIME, &currentPosition), "Failed to query the current playback position");
	difference = int(abs((currentPosition/GST_SECOND) - (startPosition/GST_SECOND)));
	printf("\nCurrent Position : %lld",(currentPosition/GST_SECOND));
	play_jump = int(currentPosition/GST_SECOND) - previous_position;
	printf("\nPlay jump = %d", play_jump);

	if (play_jump != NORMAL_PLAYBACK_RATE)
            jump_buffer -=1;

	fail_unless(jump_buffer != 0 , "Playback is not happenning at the expected rate");
	previous_position = (currentPosition/GST_SECOND);

	g_object_get (sink,"frames-rendered", &rendered_frames, NULL);
	fail_unless (rendered_frames > previous_rendered_frames, "Frames not rendered properly");
	previous_rendered_frames = rendered_frames;

    }while(difference <= play_timeout);

    g_object_get (G_OBJECT (sink), "last-message", &fps_msg, NULL);
    fail_unless( fps_msg!= NULL,"Unable to obtain last-message from fpsdisplaysink");
    if (fps_msg != NULL)
    {
        g_print ("Frame info: %s\n", fps_msg);
	char buffer[250],*temp;
	sprintf(buffer, "%s", fps_msg);
	temp = strstr(buffer, ": ") +2;
        rendered = atoi(temp);

        temp = strstr(temp, ": ") + 2;
        dropped = atoi(temp);

        temp = strstr(temp, ": ") + 2;
        current = atof(temp);

        temp = strstr(temp, ": ") + 2;
        average = atof(temp);

        fail_unless (rendered > 0,"Frames rendering not happening as expected");
        drop_rate = ((float)(dropped/rendered))*100;
	fail_unless (drop_rate < 1.0,"Frame drop rate is high");
	fail_unless (average > 0, "Average frame rate was not as expected");
	fail_unless (current > 0, "Current framerate was not as expected");
    }

    /*
     * Write FrameRate info to video_info file
     */
    writeFPSdata(sink);

    if (pipeline)
    {
       gst_element_set_state(pipeline, GST_STATE_NULL);
    }

    /*
     * Cleanup after use
     */
    gst_object_unref (pipeline);
}
GST_END_TEST;


/********************************************************************************************************************
 * Purpose      : Test to do audio change during playback using playbin element and westeros-sink
 * Parameters   : Audio Change during playback
 ********************************************************************************************************************/
GST_START_TEST (test_audio_change)
{
    bool is_av_playing = false;
    GstElement *playbin;
    GstElement *westerosSink;
    GstElement *fpssink;
    gint flags;
    MessageHandlerData data;
    GstTagList *tags;
    strcat (audio_change_test, TDK_PATH);
    strcat (audio_change_test, "/audio_change_log");
    filePointer = fopen(audio_change_test, "w");
    fail_unless (filePointer != NULL,"Unable to open filePointer for writing");
     
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

    /*
     * Set westros-sink
     */
    westerosSink = gst_element_factory_make(WESTEROS_SINK, NULL);
    fail_unless (westerosSink != NULL, "Failed to create 'westerossink' element");

    /*
     * Link video-sink with playbin
     */
    if (use_fpsdisplaysink)
    {
        /*
         * Create and link fpsdisplaysink if configuration is enabled
         */
        fpssink = gst_element_factory_make ("fpsdisplaysink", NULL);
        fail_unless (fpssink != NULL, "Failed to create 'fpsdisplaysink' element");
        g_object_set (playbin, "video-sink",fpssink, NULL);
        g_object_set (fpssink, "video-sink", westerosSink, NULL);
        g_object_set (fpssink, "signal-fps-measurements", true, NULL);
        g_object_set (fpssink, "fps-update-interval", 1000, NULL);
        g_signal_connect(fpssink, "fps-measurements", G_CALLBACK(FPSCallback), NULL);
    }
    else
    {
        g_object_set (playbin, "video-sink", westerosSink, NULL);
    }

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

    WaitForOperation; 
    /*
     * Check if the first frame received flag is set
     */
    fail_unless (firstFrameReceived == true, "Failed to receive first video frame signal");
    PlaySeconds(playbin,5);
    data.playbin = playbin;
    getStreamProperties(&data);
    fail_unless (1 < data.n_audio,"Stream has only 1 audio stream. Audio Change requires minimum of two audio streams");
    printf("Current Audio is %d\n", data.current_audio);
    writeToFile = true;
    g_signal_emit_by_name (data.playbin, "get-audio-tags", data.current_audio, &tags);
    if (tags)
    {
        g_print ("Tags:\n");
        gst_tag_list_foreach (tags, printTag, GINT_TO_POINTER (1));
        gst_tag_list_free (tags);
    }

    /*
     * Wait for 'play_timeout' seconds(recieved as the input argument) before checking AV status
     */
    PlaySeconds(playbin,play_timeout);

    for( int index=0; index < data.n_audio; index++)
    {
         if( index != data.current_audio)
         {
             printf("\nSwitching to %d audio stream\n", index);
             printf("\nSetting current-audio to %d\n",index);
             g_object_set (data.playbin, "current-audio", index, NULL);
             // Waiting for audio switch
             g_object_get (data.playbin, "current-audio", &data.current_audio, NULL);
             fail_unless(data.current_audio == index, "FAILED : Unable to switch to %d audio stream",index);
	     g_signal_emit_by_name (data.playbin, "get-audio-tags", data.current_audio, &tags);
             if (tags)
             {
                 g_print ("Tags:\n");
                 gst_tag_list_foreach (tags, printTag, GINT_TO_POINTER (1));
                 gst_tag_list_free (tags);
             }
	     PlaySeconds(playbin,5);
             printf("\nSUCCESS : Switched to audio stream %d, Playing for %d seconds\n",index,play_timeout);
	     PlaySeconds(playbin,play_timeout);
         }
    }
    /*
     * Check for AV status if its enabled
     */
    if (true == checkAVStatus)
    {
        is_av_playing = check_for_AV_status();
        fail_unless (is_av_playing == true, "Video is not playing in TV");
    }
    GST_LOG("DETAILS: SUCCESS, Video playing successfully for all audio streams\n");

    /*
     * Write FrameRate info to video_info file
     */
    if (use_fpsdisplaysink)
    {
        writeFPSdata(fpssink);
    }

    if (playbin)
    {
       fail_unless (gst_element_set_state (playbin, GST_STATE_NULL) !=  GST_STATE_CHANGE_FAILURE);
    }
    /*
     * Cleanup after use
     */
    fclose(filePointer);
    gst_object_unref (playbin);
}
GST_END_TEST;


static Suite *
media_pipeline_suite (void)
{
    Suite *gstPluginsSuite = suite_create ("playbin_plugin_test");
    TCase *tc_chain = tcase_create ("general");
    /*
     * Set timeout to play_timeout if play_timeout > DEFAULT_TEST_SUITE_TIMEOUT(360) seconds
     */
    guint timeout = DEFAULT_TEST_SUITE_TIMEOUT;
    if (play_timeout > DEFAULT_TEST_SUITE_TIMEOUT)
    {
        timeout = play_timeout;
    }
    tcase_set_timeout (tc_chain, timeout);
    suite_add_tcase (gstPluginsSuite, tc_chain);

    GST_INFO ("Test Case name is %s\n", tcname);
    printf ("Test Case name is %s\n", tcname);
    
    if (strcmp ("test_generic_playback", tcname) == 0)
    {
       tcase_add_test (tc_chain, test_generic_playback);
       GST_INFO ("tc %s run successfull\n", tcname);
       GST_INFO ("SUCCESS\n");
    }
    else if (strcmp ("test_play_pause_pipeline", tcname) == 0)
    {
       tcase_add_test (tc_chain, test_play_pause_pipeline);
       GST_INFO ("tc %s run successfull\n", tcname);
       GST_INFO ("SUCCESS\n");
    }
    else if (strcmp ("test_EOS", tcname) == 0)
    {
       tcase_add_test (tc_chain, test_EOS);
       GST_INFO ("tc %s run successfull\n", tcname);
       GST_INFO ("SUCCESS\n");
    }
    else if (strcmp ("test_init_shutdown", tcname) == 0)
    {
       tcase_add_test (tc_chain, test_init_shutdown);
       GST_INFO ("tc %s run successfull\n", tcname);
       GST_INFO ("SUCCESS\n");
    }
    else if (strcmp ("test_channel_change_playback", tcname) == 0)
    {
       ChannelChangeTest = true;
       tcase_add_test (tc_chain, test_generic_playback);
       GST_INFO ("tc test_channel_change_playback run successfull\n");
       GST_INFO ("SUCCESS\n");
    }
    else if (strcmp ("test_playback_latency", tcname) == 0)
    {
       latency_check_test = true;
       tcase_add_test (tc_chain, test_generic_playback);
       GST_INFO ("tc %s run successfull\n", tcname);
       GST_INFO ("SUCCESS\n");
    }
    else if (strcmp ("test_audio_change", tcname) == 0)
    {
       tcase_add_test (tc_chain, test_audio_change);
       GST_INFO ("tc %s run successfull\n", tcname);
       GST_INFO ("SUCCESS\n");
    }
    else if (strcmp ("test_frameDrop", tcname) == 0)
    {
       tcase_add_test (tc_chain, test_frameDrop);
       GST_INFO ("tc %s run successfull\n", tcname);
       GST_INFO ("SUCCESS\n");
    }
    return gstPluginsSuite;
}



int main (int argc, char **argv)
{
    Suite *testSuite;
    int returnValue = 0;
    int arg = 0;
    char *timeoutparser;
    std::vector<int> TimeoutList;
    char* timeout = NULL;

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
    if (argc == 2)
    {
    	strcpy (tcname, argv[1]);

    	GST_INFO ("\nArg 2: TestCase Name: %s \n", tcname);
    	printf ("\nArg 2: TestCase Name: %s \n", tcname);
    }
    else if (argc >= 3)
    {
        strcpy (tcname, argv[1]);
	if ((strcmp ("test_init_shutdown", tcname) == 0) || 
	    (strcmp ("test_play_pause_pipeline", tcname) == 0) || 
            (strcmp ("test_generic_playback", tcname) == 0) ||
            (strcmp ("test_playback_latency", tcname) == 0) ||
            (strcmp ("test_audio_change", tcname) == 0) ||
	    (strcmp ("test_frameDrop", tcname) == 0) ||
            (strcmp ("test_EOS", tcname) == 0))
	{
	    if (strcmp ("test_frameDrop", tcname) == 0)
	    {
		arg = 2;
	    }
	    else
	    {
		strcpy(m_play_url,argv[2]);
		arg = 3;
	    }

            for (; arg < argc; arg++)
            {
                if (strcmp ("checkavstatus=yes", argv[arg]) == 0)
                {
                    checkAVStatus = true;
                }
                if (strstr (argv[arg], "timeout=") != NULL)
                {
                    strtok (argv[arg], "=");
      		    play_timeout = atoi (strtok (NULL, "="));
                }
		if (strcmp ("checkPTS=no", argv[arg]) == 0)
                {
                    checkPTS = false;
                }
		if (strcmp ("checkFPS=no", argv[arg]) == 0)
	        {
		    use_fpsdisplaysink = false;
		}
            }

            printf ("\nArg : TestCase Name: %s \n", tcname);
            printf ("\nArg : Playback url: %s \n", m_play_url);

        }
        else if (strcmp ("test_channel_change_playback", tcname) == 0)
        {
            strcpy(m_play_url,argv[2]);
            strcpy(channel_url,argv[3]);

            for (arg = 4; arg < argc; arg++)
            {
                if (strcmp ("checkavstatus=yes", argv[arg]) == 0)
                {
                    checkAVStatus = true;
                }
		if (strcmp ("checkPTS=no", argv[arg]) == 0)
		{
		    checkPTS = false;
		}
		if (strcmp ("checkFPS=no", argv[arg]) == 0)
                {
                    use_fpsdisplaysink = false;
                }
                if (strstr (argv[arg], "timeout=") != NULL)
                {
                    strtok (argv[arg], "=");
                    timeoutparser = strtok(NULL, "=");
                    timeout = strtok (timeoutparser, ",");
                    while (timeout != NULL)
                    {
                        TimeoutList.push_back(atoi(timeout));
                        timeout = strtok (NULL, ",");
                    }
                    SecondChannelTimeout  = TimeoutList[1];
                    play_timeout = TimeoutList[0];
                }
            }
            printf ("\nArg : TestCase Name: %s \n", tcname);
            printf ("\nArg : First Channel: %s \n", m_play_url);
            printf ("\nArg : FirstChannel timeout: %d \n", play_timeout);
            printf ("\nArg : Second Channel : %s \n",channel_url);
            printf ("\nArg : SecondChannel timeout: %d\n", SecondChannelTimeout);
        }
    }
    else
    {
        printf ("FALIURE : Insufficient arguments\n");
        returnValue = 0;
	goto exit;
    }
    gst_check_init (&argc, &argv);
    testSuite = media_pipeline_suite ();
    returnValue =  gst_check_run_suite (testSuite, "playbin_plugin_test", __FILE__);
exit:
    return returnValue;
}
