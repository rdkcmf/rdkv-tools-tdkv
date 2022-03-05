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

#define EOS_TIMEOUT 			-1
#define DEFAULT_TEST_SUITE_TIMEOUT	360
#define VIDEO_STATUS 			"/CheckVideoStatus.sh"
#define AUDIO_STATUS 			"/CheckAudioStatus.sh"
#define PLAYBIN_ELEMENT 		"playbin"
#define WESTEROS_SINK 			"westerossink"
#define BUFFER_SIZE_LONG		1024
#define BUFFER_SIZE_SHORT		264
#define NORMAL_PLAYBACK_RATE		1.0
#define FCS_MICROSECONDS		1000000
#define PlaySeconds(RunSeconds)         start = std::chrono::steady_clock::now(); \
                                        Runforseconds = RunSeconds; \
                                        while(1) { \
                                        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(Runforseconds)) \
                                             break; \
                                        }


char tcname[BUFFER_SIZE_SHORT] = {'\0'};
char m_play_url[BUFFER_SIZE_LONG] = {'\0'};
char channel_url[BUFFER_SIZE_LONG] = {'\0'};
char TDK_PATH[BUFFER_SIZE_SHORT] = {'\0'};
vector<string> operationsList;
/*
 * Default values for avstatus check flag and play_timeout if not received as input arguments
 */
bool checkAVStatus = false;
int play_timeout = 10; 
int Runforseconds;
auto start = std::chrono::steady_clock::now();
bool latency_check_test = false;
int SecondChannelTimeout =0;
bool ChannelChangeTest = false;
GstClockTime timestamp, latency, time_elapsed;
bool firstFrameReceived = false;
bool audioChanged = false;
bool videoChanged = false;

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
  REWIND16x_RATE        = -16,
  REWIND4x_RATE         = -4,
  REWIND2x_RATE		= -2,
  FASTFORWARD2x_RATE 	= 2,
  FASTFORWARD4x_RATE    = 4,
  FASTFORWARD16x_RATE  	= 16
} PlaybackRates;


/* 
 * Structure to pass arguments to/from the message handling method
 */
typedef struct CustomData {
    GstElement *playbin;  		/* Playbin element handle */
    GstState cur_state;         	/* Variable to store the current state of pipeline */
    gint64 seekPosition;		/* Variable to store the position to be seeked */
    gint64 currentPosition; 		/* Variable to store the current position of pipeline */
    gboolean terminate;    		/* Variable to indicate whether execution should be terminated in case of an error */
    gboolean seeked;    		/* Variable to indicate if seek to requested position is completed */
    gboolean eosDetected;		/* Variable to indicate if EOS is detected */
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

/********************************************************************************************************************
Purpose:               To get the current status of the AV running
Parameters:
scriptname [IN]       - The input scriptname
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
static void printTag (const GstTagList *tagList, const gchar *tag_char, gpointer custom_data)
{
    GValue value = { 0, };
    gint depth = GPOINTER_TO_INT (custom_data);
    gchar *Data;

    gst_tag_list_copy_value (&value, tagList, tag_char);

    if (G_VALUE_HOLDS_STRING (&value))
        Data = g_value_dup_string (&value);
    else
        Data = gst_value_serialize (&value);

    g_print ("%*s%s: %s\n", 2 * depth, " ", gst_tag_get_nick (tag_char), Data);
    g_free (Data);

    g_value_unset (&value);
}

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
     * Link the westeros-sink to playbin
     */
    g_object_set (playbin, "video-sink", westerosSink, NULL);

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
    
    /*
     * Wait for 'play_timeout' seconds(recieved as the input argument) before checking AV status
     */
    PlaySeconds(play_timeout);
    /*
     * Check if the first frame received flag is set
     */
    fail_unless (firstFrameReceived == true, "Failed to receive first video frame signal");
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
       PlaySeconds(SecondChannelTimeout);

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

    /*
     * Wait for 'play_timeout' seconds(recieved as the input argument) before chaging the pipeline state
     * We are waiting for 5 more seconds before checking pipeline status, so reducing the wait here
     */
    PlaySeconds(play_timeout - 5);
    /*
     * Ensure that playback is happening before pausing the pipeline
     */
    /*
     * Check if the first frame received flag is set
     */
    fail_unless (firstFrameReceived == true, "Failed to receive first video frame signal");
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
     * Set pipeline to PAUSED
     */
    gst_element_set_state (playbin, GST_STATE_PAUSED);
    /*
     * Wait for 5 seconds before checking the pipeline status
     */
    PlaySeconds(5);
    fail_unless_equals_int (gst_element_get_state (playbin, &cur_state,
            NULL, 0), GST_STATE_CHANGE_SUCCESS);
    GST_LOG("\n********Current state: %s\n",gst_element_state_get_name(cur_state));

    fail_unless_equals_int (cur_state, GST_STATE_PAUSED);
    printf ("DETAILS: SUCCESS, Current state is: %s \n", gst_element_state_get_name(cur_state));
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
    gint flags;
    GstElement *playbin;
    GstElement *westerosSink;

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

    /*
     * Wait for 5 seconds before checking AV status
     */
    PlaySeconds (5);
    /*
     * Check if the first frame received flag is set
     */
    fail_unless (firstFrameReceived == true, "Failed to receive first video frame signal");
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
    message = gst_bus_poll (bus, (GstMessageType)GST_MESSAGE_EOS, EOS_TIMEOUT);
    fail_unless (GST_MESSAGE_EOS == GST_MESSAGE_TYPE(message), "Failed to recieve EOS message");
    printf ("EOS Recieved\n");
    gst_message_unref (message);

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

/********************************************************************************************************************
 * Purpose      : Test to do trickplay in the pipeline
 * Parameters   : Playback url
 ********************************************************************************************************************/
GST_START_TEST (test_trickplay)
{
    bool is_av_playing = false;
    vector<string>::iterator operationItr;
    char* operationBuffer = NULL;
    string operation;
    double operationTimeout = 10.0;
    bool seekOperation = false;
    int seekSeconds = 0;
    gint flags;
    GstState cur_state;
    GstElement *playbin;
    GstElement *westerosSink;
    GstMessage *message;
    GstBus *bus;
    gdouble rate = 0;
    MessageHandlerData data;
    double timeout;
    GstQuery *query;
    gdouble currentRate = 0.0;
    gint64 currentPosition = GST_CLOCK_TIME_NONE;
    gint64 seekPosition;
    GstStateChangeReturn state_change;

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

     /*
     * Set the first frame recieved callback
     */
    g_signal_connect( westerosSink, "first-video-frame-callback", G_CALLBACK(firstFrameCallback), &firstFrameReceived);
    /*
     * Set the firstFrameReceived variable as false before starting play
     */
    firstFrameReceived= false;

    /*
     * Set playbin to PLAYING by setting boolean pasue to false
     */
    GST_FIXME( "Setting to Playing State\n");
    fail_unless (gst_element_set_state (playbin, GST_STATE_PLAYING) !=  GST_STATE_CHANGE_FAILURE);
    GST_FIXME( "Set to Playing State\n");

    /*
     * Wait for 5 seconds before verifying the playback status
     */
    PlaySeconds(5);
    /*
     * Ensure that playback is happening before starting trickplay
     */
    /*
     * Check if the first frame received flag is set
     */
    fail_unless (firstFrameReceived == true, "Failed to receive first video frame signal");
    /*
     * Check for AV status if its enabled
     */
    if (true == checkAVStatus)
    {
        is_av_playing = check_for_AV_status();
        fail_unless (is_av_playing == true, "Video is not playing in TV");
    }
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
        operation = strtok (operationBuffer, ":");
	operationTimeout = atof (strtok (NULL, ":"));

	if (!operation.empty())
	{  
		timeout = operationTimeout;
		if ("fastforward2x" == operation)
                {
		    /*
		     * Fastforward the pipeline to 2
		     */
                    rate = FASTFORWARD2x_RATE;
		}
		else if ("fastforward4x" == operation)
                {
	            /*
		     * Fastforward the pipeline to 4
		     */
	            rate = FASTFORWARD4x_RATE;
		}
	        else if ("fastforward16x" == operation)
		{
		    /*
		     * Fastforward the pipeline to 16
		     */
		    rate = FASTFORWARD16x_RATE;	
		}
		else if ("rewind2x" == operation)
	        {
		    /*
		     * Rewind the pipeline to -2
		     */
		    rate = REWIND2x_RATE;
		}
		else if ("rewind4x" == operation)
                {
		    /*
		     * Rewind the pipeline to -4
		     */
		    rate = REWIND4x_RATE;
		}
		else if ("rewind16x" == operation)
		{
		    /*
		     * Rewind the pipeline to -16
		     */
		    rate = REWIND16x_RATE;
		}
		else if ("seek" == operation)
                {
		    /*
		     * Acquire seek seconds and set playabck rate to 1
		     */
		    seekSeconds = atoi(strtok(NULL, ":"));
		    seekOperation = true;
		    rate = 1;
		}
		else if ( ("play" == operation) || ("pause" == operation) )
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
		/*
		 * Get Current Playback Rate
		 */
	        query = gst_query_new_segment (GST_FORMAT_DEFAULT);
	        fail_unless (gst_element_query (playbin, query), "Failed to query the current playback rate");
	        gst_query_parse_segment (query, &currentRate, NULL, NULL, NULL);
	       
	     	/*
		 * Get Current Playback Position
		 */
                fail_unless (gst_element_query_position (playbin, GST_FORMAT_TIME, &currentPosition), "Failed to query the current playback position");

		/*
                 * For rewind operations were playback rate is negative, ensure that there is enough duration 
                 * to rewind the pipeline correctly by querying the current position and checking if 
                 * current position >= required duration (abs (rate) * timeout)
                 */
                if ( rate != currentRate) 
		{
		    if (rate < 0)
                    {
			   /*
                            * If the current position is greater than required duration
                            */
                           if (currentPosition < ((int)abs (rate) * timeout * GST_SECOND))
                           { 
                                 printf ("There is not enough duration for rewinding the pipeline, so seeking to %f before rewind\n",  ((int)abs (rate) * timeout));
				 /*
                                  * Seek to the required position
                                  */
				 seekSeconds = ((int)abs (rate) * timeout);
				 seekOperation = true;
			   }
	             }
		}
		/*
		 * SEEK OPERATION
		 */
		if ( seekOperation )
		{
                    seekPosition = GST_SECOND * seekSeconds;
	            if ( currentPosition/GST_SECOND == seekPosition/GST_SECOND)
		    {
			PlaySeconds(5);
		    }	
		    /*
                     * Set the playback position to the seekSeconds position
                     */
		    timestamp = gst_clock_get_time ((playbin)->clock);
                    fail_unless (gst_element_seek (playbin, NORMAL_PLAYBACK_RATE, GST_FORMAT_TIME,
                                   GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, seekPosition,
                                   GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE), "Failed to seek");
		    /*
                     * During seek, the state of various gstreamer elements changes,
                     * so we are polling the bus to wait till the bus is clear of state change events
                     * before verifying the success of seek operation. We are exiting the loop if the position change happens in between
                     * or if an error/eos is recieved
                     */
                    bus = gst_element_get_bus (playbin);
                    data.terminate = FALSE;
                    data.seeked = FALSE;
                    data.playbin = playbin;
                    data.seekPosition = seekPosition;
                    data.currentPosition = GST_CLOCK_TIME_NONE;
                    data.stateChanged = FALSE;
                    do
                    {
                        message = gst_bus_timed_pop_filtered (bus, 2 * GST_SECOND,
                                             (GstMessageType) ((GstMessageType) GST_MESSAGE_STATE_CHANGED |
                                             (GstMessageType) GST_MESSAGE_ERROR | (GstMessageType) GST_MESSAGE_EOS |
                                             (GstMessageType) GST_MESSAGE_ASYNC_DONE ));
                        if (NULL != message)
                        {
                            handleMessage (&data, message);
                        }
                        else
                        {
                            printf ("All messages are clear. No more message after seek\n");
                            break;
                        }
                    } while (!data.terminate && !data.seeked);

                    /* Verify that ERROR/EOS messages are not recieved */
                    fail_unless (FALSE == data.terminate, "Unexpected error or End of Stream received\n");

		    /* Verify that SEEK message is received */
                    fail_unless (TRUE == data.seeked, "Seek Unsuccessfull\n");

		    if (latency_check_test)
                    {
                        latency = time_elapsed - timestamp;
			parselatency();
                    }

		    /* Verify that stateChanged message is received */
                    fail_unless (TRUE == data.stateChanged, "State change message was not received\n");

                    //Convert time to seconds
                    data.currentPosition /= GST_SECOND;
                    data.seekPosition /= GST_SECOND;

                    printf("SEEK SUCCESSFULL :  CurrentPosition %lld seconds, SeekPosition %lld seconds\n", data.currentPosition, data.seekPosition);
                    GST_LOG ("SEEK SUCCESSFULL :  CurrentPosition %lld seconds, SeekPosition %lld seconds\n", data.currentPosition, data.seekPosition);
                    gst_object_unref (bus);
		}

		/*
		 * SETRATE OPERATION
		 */
		if (rate != currentRate)
		{
                    GST_LOG ("Setting the playback rate to %f\n", rate);
		    PlaySeconds(5);
		    currentPosition = GST_CLOCK_TIME_NONE;
		    fail_unless (gst_element_query_position (playbin, GST_FORMAT_TIME, &currentPosition), "Failed to query the current playback position");

		    /*
                     * Playback rates can be positive or negative depending on whether we are fast forwarding or rewinding
                     * Below are the Playback rates used in our test scenarios
                     * Fastforward Rates:
                     * 	1) 2.0
                     * 	2) 4.0
                     * 	3) 16.0
                     * Rewind Rates:
                     * 	1) -2.0
                     * 	2) -4.0
                     * 	3) -16.0
                     */
                    /*
                     * Rewind the pipeline if rate is a negative number
                     */
		    timestamp = gst_clock_get_time ((playbin)->clock);
                    if (rate < 0)
                    {
                       fail_unless (gst_element_seek (playbin, rate, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
                     GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE, GST_SEEK_TYPE_SET, currentPosition), "Failed to set playback rate");
                    }
		    /*
                     * Fast forward the pipeline if rate is a positive number
                     */
                    else
                    {
                       fail_unless (gst_element_seek (playbin, rate, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, currentPosition,
                         GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE), "Failed to set playback rate");
                    }
		    bus = gst_element_get_bus (playbin);
                    data.terminate = FALSE;
                    data.seeked = FALSE;
                    data.setRateOperation = TRUE;
                    data.setRate = rate;
                    data.currentRate = 0.0;
                    data.playbin = playbin;
                    data.currentPosition = GST_CLOCK_TIME_NONE;
                    data.stateChanged = FALSE;
                    do
                    {
                        message = gst_bus_timed_pop_filtered (bus, 2 * GST_SECOND,
                                             (GstMessageType) ((GstMessageType) GST_MESSAGE_STATE_CHANGED |
                                             (GstMessageType) GST_MESSAGE_ERROR | (GstMessageType) GST_MESSAGE_EOS |
                                             (GstMessageType) GST_MESSAGE_ASYNC_DONE ));
                        if (NULL != message)
                        {
                            handleMessage (&data, message);
                        }
                        else
                        {
                            printf ("All messages are clear. No more message after seek\n");
                            break;
                        }
                    } while (!data.terminate && !data.seeked);

		    fail_unless (data.setRate == data.currentRate, "Failed to do set rate to %f correctly\nCurrent playback rate is: %f\n", data.setRate, data.currentRate);
		    printf ("Rate is set to %s %dx speed\nPlaying for %f seconds",(rate > 0)?"fastforward":"rewind", (int)abs (rate),timeout);
		    if (latency_check_test)
		    {
			latency = time_elapsed - timestamp;
			parselatency();
		    }
		    PlaySeconds(timeout);
		    printf ("Successfully executed %s %dx speed for %f seconds\n", (rate > 0)?"fastforward":"rewind", (int)abs (rate), timeout);
		    gst_object_unref (bus);
		}
	        if ("play" == operation)
                {
		    /*
		     * If pipeline is already in playing state with normal playback rate (1.0),
		     * just wait for operationTimeout seconds, instead os setting the pipeline to playing state again
		     */	
		    fail_unless_equals_int (gst_element_get_state (playbin, &cur_state,
                                                                   NULL, 0), GST_STATE_CHANGE_SUCCESS);
		    if ((cur_state != GST_STATE_PLAYING))
		    {
                     	 /* Set the playbin state to GST_STATE_PLAYING
                         */
			fail_unless (gst_element_set_state (playbin, GST_STATE_PLAYING) !=  GST_STATE_CHANGE_FAILURE);
			//Waiting for state change
			do
                        {
                            /*
                             * Polling for the state change to reflect with 10 ms timeout
                             */
                            state_change = gst_element_get_state (playbin, &cur_state, NULL, 10000000);
                        } while (state_change == GST_STATE_CHANGE_ASYNC);
			printf ("********Current state is: %s \n", gst_element_state_get_name(cur_state));
	    		/*
	    	   	 * Sleep for the requested time
	    		 */
	    		PlaySeconds (operationTimeout );
		    }
		    /*
		     * If playback is already happening wait for the requested time
		     */
		    else
	 	    {
                        printf ("Playbin is already playing, so waiting for %f seconds\n", operationTimeout);
			PlaySeconds (operationTimeout );
		    }
		    /*
                     * Ensure that playback is happening properly
                     */
                    /*
                     * Check for AV status if its enabled
                     */
                    if (true == checkAVStatus)
                    {
                        is_av_playing = check_for_AV_status();
                        fail_unless (is_av_playing == true, "Video is not playing in TV");
                        printf ("DETAILS: SUCCESS, Video playing successfully \n");
                    }
		}
	        /*
	         * If the operation is to pause the pipeline 
	         */
	        else if ("pause" == operation)
                {
		    /*
		     * Set the playbin state to GST_STATE_PAUSED
		     */	
		     gst_element_set_state (playbin, GST_STATE_PAUSED);
		     PlaySeconds(5);
                     fail_unless_equals_int (gst_element_get_state (playbin, &cur_state, NULL, 0), GST_STATE_CHANGE_SUCCESS);
		     fail_unless_equals_int (cur_state, GST_STATE_PAUSED);
		     printf("\n********Current state: %s\n",gst_element_state_get_name(cur_state));
                     GST_LOG("\n********Current state: %s\n",gst_element_state_get_name(cur_state));
	    	    /*
	    	     * Sleep for the requested time
	    	     */
	    	    PlaySeconds (operationTimeout );
		}
                if (true == checkAVStatus)
                {
                        is_av_playing = check_for_AV_status();
                        fail_unless (is_av_playing == true, "Video is not playing in TV");
                        printf ("DETAILS: SUCCESS, Video playing successfully \n");
 		}
         }
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
 * Purpose      : Test to do audio change during playback using playbin element and westeros-sink
 * Parameters   : Audio Change during playback
 ********************************************************************************************************************/
GST_START_TEST (test_audio_change)
{
    bool is_av_playing = false;
    GstElement *playbin;
    GstElement *westerosSink;
    gint flags;
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
     * Link the westeros-sink to playbin
     */
    g_object_set (playbin, "video-sink", westerosSink, NULL);

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

    PlaySeconds(5);
    /*
     * Check if the first frame received flag is set
     */
    fail_unless (firstFrameReceived == true, "Failed to receive first video frame signal");
    data.playbin = playbin;
    getStreamProperties(&data);
    fail_unless (1 != data.n_audio,"Stream has only 1 audio stream. Audio Change requires minimum of two audio streams");
    printf("Current Audio is %d", data.current_audio);

    /*
     * Wait for 'play_timeout' seconds(recieved as the input argument) before checking AV status
     */
    PlaySeconds(play_timeout);

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
	     PlaySeconds(5);
             printf("\nSUCCESS : Switched to audio stream %d, Playing for %d seconds\n",index,play_timeout);
	     PlaySeconds(play_timeout);
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
    if (playbin)
    {
       fail_unless (gst_element_set_state (playbin, GST_STATE_NULL) !=  GST_STATE_CHANGE_FAILURE);
    }
    /*
     * Cleanup after use
     */
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
    else if (strcmp ("test_trickplay", tcname) == 0)
    {
       tcase_add_test (tc_chain, test_trickplay);
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
    else if (strcmp ("test_trickplay_latency", tcname) == 0)
    {
       latency_check_test = true;
       tcase_add_test (tc_chain, test_trickplay);
       GST_INFO ("tc %s run successfull\n", tcname);
       GST_INFO ("SUCCESS\n");
    }
    else if (strcmp ("test_audio_change", tcname) == 0)
    {
       tcase_add_test (tc_chain, test_audio_change);
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
    char *operationString = NULL;
    char* operation = NULL;
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
            (strcmp ("test_trickplay_latency", tcname) == 0) ||
            (strcmp ("test_EOS", tcname) == 0) ||
            (strcmp("test_trickplay", tcname) == 0))
	{
            strcpy(m_play_url,argv[2]);
            for (arg = 3; arg < argc; arg++)
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
                if (strstr (argv[arg], "operations=") != NULL)
		{
		   /*
		    * The trickplay operations can be given in
		    * operations="" argument as coma separated string
		    * eg: operations=play:play_timeout,fastforward2x:timeout,seek:timeout:seekvalue
		    */
                   strtok (argv[arg], "=");
		   operationString = strtok(NULL, "=");
		   operation = strtok (operationString, ",");
                   while (operation != NULL)
		   {
		       operationsList.push_back(operation);
   		       operation = strtok (NULL, ",");
		   }
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
