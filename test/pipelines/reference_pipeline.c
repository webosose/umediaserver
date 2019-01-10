// Copyright (c) 2019 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//

/********************************************************************************
 * @file playbin2_pipeline.c
 *
 * @brief This is a demo playbin2 component of the micromediaserver implementation
 ********************************************************************************/
#ifndef __USE_GNU
#define __USE_GNU
#endif

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <pthread.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <glib.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <pthread.h>
#include <luna-service2/lunaservice.h>
#include <pbnjson.h>
#include <Logger_macro.h>
#include "reference_pipeline.h"

/* Function forward declarations */
static bool LoadEvent(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx);
static bool UnloadEvent(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx);
static bool PlayEvent(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx);
static bool PauseEvent(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx);
static bool SeekEvent(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx);
static bool SetPlayRateEvent(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx);
static bool PropertyChangeSubscriptionEvent(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx);
static bool SendResponse(bool resp, UMSConnectorHandle* sender, UMSConnectorMessage* message);

static bool _UnloadPipeline(pipeline_context_t * pcontext);


/***********************************************************
  Pipeline process global state structure
***********************************************************/
static pipeline_context_t  pipeline_context = {0};

static LogContext log;

/*
  Event handlers :
    client state events
    load   = open HW resources and routes. uMediaServer has already confirmed
               no other pipline of this class is consuming the required resources.
    unload = release HW resources and routes.
    stop = stop pipeline
    exit = <optional> clean up and exit pipeline process. Set "killable" flag in
            uMediaServer configuration file to prevent event.
 */
UMSConnectorEventHandler eventMethods[] = {
    {"load",                LoadEvent},
    {"unload",              UnloadEvent},
    {"play",                PlayEvent},
    {"pause",               PauseEvent},
    {"seek",                SeekEvent},
    {"setPlayRate",         SetPlayRateEvent},
    {"stateChange",         PropertyChangeSubscriptionEvent},
    {NULL,                  NULL}
};

bool policyActionCallback(const char *action, const char * resources,
		const char *requestor_type, const char * requestor_name,
		const char *connectionId)
{
	// requestor_type and requestor_name as defined uMS pipeline config file
	LOG_DEBUG(log,"!!!!!!!!!!!!! Policy action:"
			" policy=%s"
			" resources=%s"
			" requestor_type=%s"
			" requestor_name=%s"
			" connectionId=%s",
			action, resources,
			requestor_type,requestor_name,
			connectionId);

	_UnloadPipeline(&pipeline_context);
	UMSConnectorDestroy((&pipeline_context)->UMSConn_handle);

	return true;
}

/***********************************************************
 *
 *  M A I N
 *
 **********************************************************/
/*! Main routine. */
int main (int argc, char *argv[])
{
	AcquireLogContext(UMS_LOG_CONTEXT_PIPELINE, &log);
    GError *err = NULL;
    int c = 0;
    int service_name_specified = 0;
    int controller_name_specified = 0;

    opterr = 0;

    pipeline_context.m_mainLoop = NULL;
    pipeline_context.m_pipe_ready = pipeline_context.m_video_loaded = FALSE;
    pipeline_context.m_current_time_notify_interval = MSG_NOTIFY_INTERVAL;

    gboolean gstr = gst_init_check(&argc, &argv, &err);
    LOG_TRACE(log, "GStreamer initialization has %s", gstr ? "SUCCEEDED" : "FAILED");

    /* startup parameters:
        s = UMSConnector service name
	*/
	while ((c = getopt (argc, argv, "s:")) != -1) {
        switch (c) {
			case 's': {
                    strcpy(pipeline_context.service_name,optarg);
                    service_name_specified = 1;
                    break;
            }
            case '?':
				printf("usage: pipeline process -s#: specifies UMSConnector service name.");
        }
    }

    g_strlcpy( pipeline_context.connection_id, "<default>", MAX_SERVICE_STRING);

    if (service_name_specified) {
		char * uid = g_strrstr_len(pipeline_context.service_name, MAX_SERVICE_STRING, ".") + 1;
		g_strlcpy( pipeline_context.connection_id, uid, MAX_SERVICE_STRING);
    }
    else {
        LOG_DEBUG(log, "No service name specified. %s", pipeline_context.service_name);
        exit(-1);
    }

    LOG_DEBUG(log, "Pipeline starting, PID = %d, connection_id = %s", (int)getpid(),
              pipeline_context.connection_id);

    /* Create connector object to handle uMediaServer events and send status/state messages. */
    LOG_TRACE(log, "Creating an instance of UMSConnector - registering as: \"%s\"", pipeline_context.service_name);
    pipeline_context.UMSConn_handle = UMSConnectorCreatePrivate(pipeline_context.service_name,
                                                         NULL, &pipeline_context);
    if (!pipeline_context.UMSConn_handle) {
        LOG_CRITICAL(log, MSGERR_PIPELINE_INIT, "Pipeline process startup logic failed. UMSConnectorCreate failed.");
        return -1;
    }

    pthread_mutex_init(&pipeline_context.lock,NULL);

    /* return GMainLoop from UMSConnector to use in later operations.  NOT REQUIRED. */
    pipeline_context.m_mainLoop = UMSConnectorGetMainLoop(pipeline_context.UMSConn_handle);

    /* add event function handlers */
    UMSConnectorAddEventHandlers(pipeline_context.UMSConn_handle, eventMethods);

    /* wait for uMedia Server events/commands */
	UMSConnectorWait(pipeline_context.UMSConn_handle);

    return 0;

}

/**
 * @f pipeline_init
 * create gstreamer pipeline
 */
void* pipeline_init(void *ctx)
{

    pipe_container *pipeline = (pipe_container*)ctx;
    const gchar* sret_name = NULL;
    GstStateChangeReturn sret;
    GstBus* bus = NULL;
    GSource* busSource;
    GMainContext* context;
    GstElementFactory *factory = NULL;

    LOG_TRACE(log, "+++ Initialize gstreamer +++");

    if (pipeline) {
        LOG_TRACE(log, " pipeline appears to be valid = %p video file = %s", pipeline, &pipeline->mediafile[7]);
    }
    else {
        LOG_TRACE(log, " Pipeline invalid - ERROR");
        goto _On_error;
    }

    LOG_TRACE(log, "\t ++++++++++++++++ Playbin2 pipeline version starting ++++++++++++++++");

    /* create playbin2 element with check */
    factory = gst_element_factory_find("playbin");
    if (!factory) {
        LOG_TRACE(log, "Failed to find factory of type 'playbin'");
        return NULL;
    }

    pipeline->m_pipeline = gst_element_factory_create (factory, "playbin");
    if (!pipeline->m_pipeline) {
        LOG_TRACE(log, "Failed to create playbin element, even though its factory exists!");
        return NULL;
    }
    LOG_TRACE(log, "pipeline->m_pipeline = %p", pipeline->m_pipeline);
    LOG_TRACE(log, "Setting flags");

    g_object_set (pipeline->m_pipeline, "flags", GST_PLAY_FLAG_AUDIO | GST_PLAY_FLAG_VIDEO |
                  GST_PLAY_FLAG_BUFFERING, NULL);
    bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline->m_pipeline));
    g_assert (bus != NULL);
    g_assert (pipeline_context.m_mainLoop != NULL);
    LOG_TRACE(log, "Creating bus watch");
    busSource = gst_bus_create_watch (bus);
    g_assert (busSource != NULL);
    context = g_main_loop_get_context (pipeline_context.m_mainLoop);
    g_source_set_callback (busSource, (GSourceFunc) bus_call, pipeline, NULL);
    g_source_attach (busSource, context);
    gst_object_unref (bus);
    LOG_TRACE(log, "Invoking signal connects");
    g_object_set (pipeline->m_pipeline, "buffer-size", BUFFER_SIZE, NULL);
    g_object_set (pipeline->m_pipeline, "buffer-duration", BUFFER_DURATION * GST_SECOND, NULL);
    g_signal_connect (pipeline->m_pipeline, "about-to-finish", G_CALLBACK (S_aboutToFinish), pipeline);
    g_signal_connect (pipeline->m_pipeline, "video-changed", G_CALLBACK (S_videoChanged), pipeline);
    g_signal_connect (pipeline->m_pipeline, "audio-changed", G_CALLBACK (S_audioChanged), pipeline);
    g_signal_connect (pipeline->m_pipeline, "video-tags-changed", G_CALLBACK (S_videoTagsChanged), pipeline);
    g_signal_connect (pipeline->m_pipeline, "audio-tags-changed", G_CALLBACK (S_audioTagsChanged), pipeline);
    g_signal_connect (pipeline->m_pipeline, "source-setup", G_CALLBACK (S_sourceSetup), pipeline);

    /* NOTE: Playbin2 must be in NULL or READY state when setting uri property
     * playbin2 wants an absolute URI filename format: "file:///media/internal/rat.mp4"
     * Example URIs are file:///home/joe/movie.avi or http://www.joedoe.com/foo.ogg */
    LOG_TRACE(log, "Now setting up the Playback uri=%s, file name = %s", pipeline->mediafile, &pipeline->mediafile[7]);

    g_object_set (pipeline->m_pipeline, "uri", pipeline->mediafile, NULL);
    LOG_TRACE(log, "Setting up sinks...");
    pthread_mutex_lock(&pipeline_context.lock);

    videoSinkSetup ();

    LOG_TRACE(log, "Setting PAUSED state...");
    sret = gst_element_set_state (pipeline->m_pipeline, GST_STATE_PAUSED);
    sret_name = gst_element_state_change_return_get_name (sret);

    if (sret == GST_STATE_CHANGE_FAILURE) {
        LOG_WARNING(log, MSGERR_GST_STATE_CHANGE,
            "GST_STATE_CHANGE_FAILURE : setupPlayback Set PAUSED -> %s", sret_name);
        dumpPipeline ("playbin2_preroll");
        pipeline_context.m_video_loaded = false;
    }
    else if (sret == GST_STATE_CHANGE_NO_PREROLL || sret == GST_STATE_CHANGE_SUCCESS) {
        LOG_TRACE(log,
            "GST_STATE_CHANGE_NO_PREROLL ||"
            "sret == GST_STATE_CHANGE_SUCCESS : setupPlayback Set PAUSED -> %s",
            sret_name);
        pipeline_context.m_video_loaded = true;
    }
    else if (sret == GST_STATE_CHANGE_ASYNC) {
        LOG_TRACE(log, "GST_STATE_CHANGE_ASYNC : setupPlayback Set PAUSED -> %s", sret_name);
        if (setElementStateAndWait(pipeline->m_pipeline, GST_STATE_PAUSED, PREROLL_TIMEOUT)) {
            LOG_TRACE(log, "setupPlayback Set PAUSED done");
            pipeline_context.m_video_loaded = true;
        }
        else {
            LOG_TRACE(log,
                "Dumping pipeline - setElementStateAndWait :"
                "setupPlayback: preroll failed");
            dumpPipeline ("playbin2_preroll");
            pipeline_context.m_video_loaded = false;
        }
    }

    pipeline->m_inState = GST_STATE_PAUSED;
    LOG_TRACE(log, "Thread exiting... sret = %d", sret);

    pipeline_context.m_pipe_ready = TRUE;

    pthread_mutex_unlock(&pipeline_context.lock);

    return NULL;

_On_error:

    LOG_ERROR(log, MSGERR_PLAYBACK_THR, "Problem encountered - ERROR in playback thread");;

    /* If the pipeline still exists unref it.  All elements in pipeline will be implicitly cleaned up.
     * Any elements not in the pipeline will also be destroyed. */
    if (pipeline->m_pipeline) {
        pipeline->m_inState = GST_STATE_NULL;
        setState(pipeline->m_pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline->m_pipeline);
        gst_object_unref(pipeline->m_audiosink);
        gst_object_unref(pipeline->m_videosink);
        pipeline->m_decoderPipeline = pipeline->m_pipeline = pipeline->m_audiosink = pipeline->m_videosink = NULL;
    }

    return NULL;

}

/* Static wrappers */

void S_aboutToFinish (GstElement* element, gpointer data)
{
    LOG_TRACE(log, "S_aboutToFinish: invoked");
}

static int greatestCommonDivisor(int a, int b)
{
    while (b) {
        int temp = a;
        a = b;
        b = temp % b;
    }

    return ABS(a);
}

static gboolean videoChangeTimeoutCallback(GstElement* element)
{
    gint videoTracks = 0;
    GstElement* videoSink;
    g_object_get(element, "n-video", &videoTracks, "video-sink", &videoSink, NULL);

    GstPad* pad = gst_element_get_static_pad(videoSink, "sink");
    GstCaps* caps = gst_pad_get_current_caps (pad);

    guint64 width = 0, height = 0;
    gint originalWidth, originalHeight;
    GstVideoInfo info;
    int pixelAspectRatioNumerator, pixelAspectRatioDenominator;
    if (!GST_IS_CAPS(caps) || !gst_caps_is_fixed(caps)
        || ! gst_video_info_from_caps (&info, caps))
        goto clean;

    originalWidth = info.width;
    originalHeight = info.height;
    pixelAspectRatioNumerator = info.par_n;
    pixelAspectRatioDenominator = info.par_d;

    // Calculate DAR based on PAR and video size.
    int displayWidth = originalWidth * pixelAspectRatioNumerator;
    int displayHeight = originalHeight * pixelAspectRatioDenominator;

    // Divide display width and height by their GCD to avoid possible overflows.
    int displayAspectRatioGCD = greatestCommonDivisor(displayWidth, displayHeight);
    displayWidth /= displayAspectRatioGCD;
    displayHeight /= displayAspectRatioGCD;

    // Apply DAR to original video size. This is the same behavior as in xvimagesink's setcaps function.
    if (!(originalHeight % displayHeight)) {
        LOG_TRACE(log, "Keeping video original height");
        width = gst_util_uint64_scale_int(originalHeight, displayWidth, displayHeight);
        height = (guint64) originalHeight;
    } else if (!(originalWidth % displayWidth)) {
        LOG_TRACE(log, "Keeping video original width");
        height = gst_util_uint64_scale_int(originalWidth, displayHeight, displayWidth);
        width = (guint64) originalWidth;
    } else {
        LOG_TRACE(log, "Approximating while keeping original video height");
        width = gst_util_uint64_scale_int(originalHeight, displayWidth, displayHeight);
        height = (guint64) originalHeight;
    }

    LOG_TRACE(log, "Natural size: %" G_GUINT64_FORMAT "x%" G_GUINT64_FORMAT, width, height);

clean:
    gst_caps_unref(caps);
    gst_object_unref(pad);
    gst_object_unref(videoSink);

    //sendVideoTrackUpdate(videoTracks > 0, width, height);
    return FALSE;
}

void S_videoChanged (GstElement* element, gpointer data)
{
    LOG_TRACE(log, "S_videoChanged: invoked");
    // This function is called in the GStreamer streaming thread, so schedule
    // a function to be executed in the main thread to comply with luna-service
    // requirements.
    g_timeout_add(0, (GSourceFunc) videoChangeTimeoutCallback, element);
}

static gboolean audioChangeTimeoutCallback(GstElement* element)
{
    gint audioTracks = 0;
    g_object_get(element, "n-audio", &audioTracks, NULL);
    //sendAudioTrackUpdate(audioTracks > 0);
    return FALSE;
}

void S_audioChanged (GstElement* element, gpointer data)
{
    LOG_TRACE(log, "S_audioChanged: invoked");
    // This function is called in the GStreamer streaming thread, so schedule
    // a function to be executed in the main thread to comply with luna-service
    // requirements.
    g_timeout_add(0, (GSourceFunc) audioChangeTimeoutCallback, element);
}

void S_videoTagsChanged (GstElement* element, gint stream, gpointer data)
{
    LOG_TRACE(log, "S_videoTagsChanged: invoked");
}

void S_audioTagsChanged (GstElement* element, gint stream, gpointer data)
{
    LOG_TRACE(log, "S_audioTagsChanged: invoked");
}

void S_sourceSetup (GstElement* element, GstElement* source, gpointer data)
{
    LOG_TRACE(log, "S_sourceSetup: invoked");
    pipe_container *pipeline = (pipe_container*)data;

    if (!pipeline->m_pipeline)
        return;

    GstElement* sourceElement = 0;
    g_object_get(pipeline->m_pipeline, "source", &sourceElement, NULL);
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(sourceElement), "user-agent"))
        g_object_set(sourceElement, "user-agent", pipeline->userAgent, NULL);
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(sourceElement), "extra-headers")) {

        GstStructure* headers = gst_structure_new("extra-headers",
                                                  "Referer", G_TYPE_STRING, pipeline->referrer,
                                                  "Cookie", G_TYPE_STRING, pipeline->cookies,
                                                  NULL);
        g_object_set(sourceElement, "extra-headers", headers, NULL);
        gst_structure_free(headers);
    }
    gst_object_unref(sourceElement);


    // TODO send this event with correct value which can be obtained from streaming src
    streaming_info_t streamingInfo;
    streamingInfo.instantBitrate = 20000;
    streamingInfo.totalBitrate = 14000;
    sendStreamingInfo(streamingInfo);
}

void audioSinkSetup ()
{
    GstElement* sink;
    const gchar* factory;
    const gchar* device;
    gint64 buffer_time, latency_time;

    factory = "alsasink";
    device = "media";
    buffer_time = G_GINT64_CONSTANT (150000);
    latency_time = G_GINT64_CONSTANT (75000);

    sink = gst_element_factory_make (factory, NULL);
    if (sink == NULL) {
        LOG_ERROR(log, MSGERR_AUDIOSINK_SETUP, "audioSinkSetup: can't create %s", factory);
        return;
    }
    g_object_set (sink, "device", device, NULL);
    g_object_set (sink, "buffer-time", buffer_time, NULL);
    g_object_set (sink, "latency-time", latency_time, NULL);

    // playbin2 takes ownership so don't keep a reference
    g_object_set (pipeline_context.pipeline->m_pipeline, "audio-sink", sink, NULL);

}

void videoSinkSetup ()
{
    GstElement* sink;
    const gchar* factory;

    factory = "ximagesink";

    LOG_TRACE(log, "videoSinkSetup: factory %s", factory);
    sink = gst_element_factory_make (factory, NULL);
    if (sink == NULL) {
        LOG_ERROR(log, MSGERR_VIDEOSINK_SETUP, "videoSinkSetup: can't create %s \n", factory);
        return;
    }

    // playbin2 takes ownership so don't keep a reference
    g_object_set (pipeline_context.pipeline->m_pipeline, "video-sink", sink, NULL);

}

void dumpPipeline (const gchar* name)
{
    // .dot dumped in /media/internal. Usage on your workstation:
    // dot -Tsvg -o myfile.svg mediaserver.PAUSED_PLAYING.dot
    // Generated myfile.svg is viewable in most browsers
    GST_DEBUG_BIN_TO_DOT_FILE (GST_BIN (pipeline_context.pipeline->m_pipeline), GST_DEBUG_GRAPH_SHOW_ALL, name);
}

/**
 * @f LoadEvent
 * "load" state event message from uMediaServer.  Create or acquire all
 * HW resources/routing required to operate Pipeline(client) but do not play.
 * Note: uMediaServer ResourceManager will has already pre-authorized resources and pipeline
 * can acquire resources without conflict.
 */
static bool LoadEvent(UMSConnectorHandle* sender, UMSConnectorMessage* message, void* ctx)
{
    pipeline_context_t * pcontext = (pipeline_context_t *)ctx;
    bool ret = true;
    int result = 0;
    pthread_t serviceThread;
    pthread_attr_t threadAttr;
    char *text_message = NULL;

    text_message = (char*)UMSConnectorGetMessageText(sender, message);
    LOG_TRACE(log, "LOAD : %s ", text_message);

    JSchemaInfo schemaInfo;
    jschema_info_init(&schemaInfo, jschema_all(), NULL, NULL); // no external refs & no error handlers
    jvalue_ref parsed = jdom_parse(j_cstr_to_buffer(text_message), DOMOPT_NOOPT, &schemaInfo);

    if (jis_null(parsed)) {
            LOG_TRACE(log, "LOAD ERROR : %s", text_message);
            return SendResponse( false, sender, message);
	}

    jvalue_ref uri_ref = jobject_get(parsed, j_cstr_to_buffer("uri"));
    const char* uri = jstring_get(uri_ref).m_str;
    LOG_TRACE(log, "URI : %s ", uri);

	jvalue_ref id_ref = jobject_get(parsed, j_cstr_to_buffer("id"));
	const char * id = jstring_get(id_ref).m_str;
  LOG_TRACE(log, "ID : %s ", id);
    if (pcontext->pipeline) {
        LOG_TRACE(log, "Load creating a new pipeline container, previous was = %p", pcontext->pipeline);
        free(pcontext->pipeline);
    }
    LOG_TRACE(log, "Load creating new pipe container, m_pipeline = %p : %s", pcontext->pipeline, uri);

    pcontext->pipeline = (pipe_container*)malloc(sizeof(pipe_container));
    if (NULL == pcontext->pipeline) {
        LOG_ERROR(log, MSGERR_PLAY, "FATAL ERROR - play failed, m_pipeline = %p : video file = %s ",
            pcontext->pipeline, uri);
        return SendResponse( false, sender, message);
    }

    LOG_TRACE(log,
        "Successfully created a new pipeline container,"
        " m_pipeline = %p : ums_message = %p : video file = %s",
        pcontext->pipeline, message, uri);

    strncpy(pcontext->connection_id, id, MAX_SERVICE_STRING);
    memset((char*)pcontext->pipeline, 0, sizeof(pipe_container));
    strncpy(pcontext->pipeline->mediafile, uri, MAX_MEDIA_FILENAME);
    pcontext->pipeline->m_notified_end = false;
    pcontext->pipeline->m_prevPosition = 0;

    LOG_TRACE(log, "Initialize gstreamer = %s", uri);

    pcontext->rmc_handle = ResourceManagerClientCreate(pcontext->connection_id,
        policyActionCallback);
    if ( pcontext->rmc_handle == NULL ) {
        LOG_ERROR(log, MSGERR_RMC_CREATE, "ResourceManagerClientCreate failed.");
        return -1;
    }

    char * acquire_request = g_strdup_printf(
        " [ "
           "{\"resource\" : \"%s\","
            "\"qty\": %d"
           "},"
           "{\"resource\" : \"%s\","
            "\"qty\": %d"
           "}"
        "]",
        "VDEC", 1,
        "ADEC", 1);

    char *acquire_response; // note : must free allocated memory after parsing

    ret = ResourceManagerClientAcquire(pcontext->rmc_handle,
        acquire_request, &acquire_response);

    if ( ret == false ) {
        LOG_WARNING(log, MSGERR_PIPELINE_LOAD,
            "Unable to acquire required resources to play media.");
        sendError(10001,"Unable to acquire required resoruces to play media.");

        _UnloadPipeline(&pipeline_context);
        exit(-1);
    }
    g_free(acquire_request);

    LOG_INFO(log,"","+++++++++++++++++++++++++++"
        " RESPONSE : %s"
        "+++++++++++++++++++++++++++\n",
        acquire_response);

    free(acquire_response);

    pipeline_init((void*)(pcontext->pipeline));

    const char * connection_id;
    connection_id = ResourceManagerClientGetConnectionID(pcontext->rmc_handle);

    LOG_INFO(log,""," Resource Manager connection_id : %s",
        connection_id);

    if (pipeline_context.m_video_loaded) {
        LOG_DEBUG(log,
            "+++++++++++++++++++++++++++"
            " READY TO PLAY "
            "+++++++++++++++++++++++++++");
    }
    else {
        LOG_WARNING(log, MSGERR_PIPELINE_LOAD, "");
        pipeline_context.m_pipe_ready = FALSE;
    }

    LOG_DEBUG(log,"reference pipeline sending ... loadCompleted");

    sendLoadState("loadCompleted");

    /* Reply to the client first and then inform the subscribers */
    SendResponse( pipeline_context.m_video_loaded ? true : false, sender, message);
    return ret;
}

/**
 * @f UnloadEvent
 * "unload" state event message from uMediaServer.
 * Release HW resources/routing required to operate Pipeline(client).
 * After message uMediaserver ResourceManager will authorize resources to be claimed by
 * another pipeline.
 */
static bool UnloadEvent(UMSConnectorHandle* sender, UMSConnectorMessage* message, void* ctx)
{
    pipeline_context_t * pcontext = (pipeline_context_t *)ctx;
    const char *text_message = UMSConnectorGetMessageText(sender, message);

    LOG_TRACE(log, "UNLOAD: m_Pipecontainer = %p : message = %s",
        pcontext->pipeline, text_message);

    SendResponse( true, sender, message);
    sendLoadState("unloadCompleted");
    _UnloadPipeline(pcontext);
    UMSConnectorDestroy(pcontext->UMSConn_handle);
    return true;
}

static bool _UnloadPipeline(pipeline_context_t * pcontext)
{
    LOG_TRACE(log, "UNLOAD: m_Pipecontainer = %p", pcontext->pipeline);

    if (pcontext->pipeline && (pcontext->pipeline->m_inState != GST_STATE_NULL)) {
        pcontext->pipeline->m_inState = GST_STATE_NULL;
        pcontext->pipeline->mCurrentPositionInMs = 0;
        pcontext->pipeline->m_durationInMs = 0;
        LOG_TRACE(log, "Stop is now cleaning up the pipeline");
        setElementStateAndWait(pcontext->pipeline->m_pipeline, GST_STATE_NULL, 50);
        if (GST_STATE_CHANGE_SUCCESS == getState(pcontext->pipeline->m_pipeline)) {
            LOG_TRACE(log, "*** set pipeline state to NULL successful");
        }
        g_source_destroy(pcontext->m_posUpdateTimer);
        g_source_unref(pcontext->m_posUpdateTimer);
        pcontext->m_posUpdateTimer = NULL;
        gst_object_unref(pcontext->pipeline->m_pipeline);
    }
    else {
        LOG_TRACE(log, "Pipeline was already stopped or never started - ignoring command");
    }

    pthread_mutex_lock(&pipeline_context.lock);
    pipeline_context.m_video_loaded  = false;
    pipeline_context.m_pipe_ready = false;
    pipeline_context.m_current_time_notify_interval = MSG_NOTIFY_INTERVAL;

    ResourceManagerClientDestroy(pcontext->rmc_handle);

    return true;
}

/**
 * @f PlayEvent
 * "play" state event message from uMediaServer.  Set pipeline state to play.
 *
 */
static bool PlayEvent(UMSConnectorHandle* sender, UMSConnectorMessage* message, void* ctx)
{
    pipeline_context_t * pcontext = (pipeline_context_t *)ctx;
    bool ret = true;

    pthread_mutex_lock(&pipeline_context.lock);
    if (false == pipeline_context.m_video_loaded) {
        pthread_mutex_unlock(&pipeline_context.lock);
        SendResponse( false, sender, message);
        sendStateUpdate("playing",false);
        return false;
    }
    else {
        pthread_mutex_unlock(&pipeline_context.lock);
    }

    LOG_TRACE(log, "+++++++++++++++ SETTING PIPE TO PLAYING STATE ++++++++++++++");
    setElementStateAndWait(GST_ELEMENT(pcontext->pipeline->m_pipeline), GST_STATE_PLAYING, 50);

    setMsgNotifyInterval(pcontext->pipeline, pipeline_context.m_current_time_notify_interval);

    pcontext->pipeline->m_inState = GST_STATE_PLAYING;
    pcontext->pipeline->m_decoderPipeline = pcontext->pipeline->m_pipeline;

    sendStateUpdate("playing",true);
    SendResponse( ret, sender, message);

    return true;
}

/**
 * @f PauseEvent
 * "pause" state event message from uMediaServer.  Pause video pipeline.
 *
 */
static bool PauseEvent(UMSConnectorHandle* sender, UMSConnectorMessage* message, void* ctx)
{
    pipeline_context_t * pcontext = (pipeline_context_t *)ctx;
    char *text_message = NULL;

    text_message = (char*)UMSConnectorGetMessageText(sender, message);
    LOG_TRACE(log, "pause was invoked, m_pipeline = %p ", pcontext->pipeline);

    if ((!pcontext->pipeline) || (pcontext->pipeline && (pcontext->pipeline->m_inState != GST_STATE_PLAYING))) {
        LOG_TRACE(log, "pause failed, m_pipeline = %p : message: %s ", pcontext->pipeline, text_message);
        return SendResponse( false, sender, message);
    }
    else {
        pcontext->pipeline->m_inState = GST_STATE_PAUSED;
        setState(pcontext->pipeline->m_pipeline, GST_STATE_PAUSED);
    }

    sendStateUpdate("paused",true);
    SendResponse( true, sender, message);
    return true;
}

/**
 * @f SeekEvent
 * "seek" state event message from uMediaServer.  Seek pipeline to position.
 *
 */
static bool SeekEvent(UMSConnectorHandle* sender, UMSConnectorMessage* message, void* ctx)
{
    pipeline_context_t * pcontext = (pipeline_context_t *)ctx;

    bool ret = true;
    const char *text_message = UMSConnectorGetMessageText(sender, message);
    bool was_paused = false;
    long long position = (long long)(atoi(text_message));
    GstStateChangeReturn state;

    LOG_TRACE(log, "SEEK: pcontext->pipeline = %p : %s ", pcontext->pipeline, text_message);

    if (pcontext->pipeline) {
        LOG_TRACE(log, "Seek Position is = %lld", position);
        pcontext->pipeline->m_notified_end = false;
    }
    else {
        LOG_WARNING(log, MSGERR_SEEK, "Seek Position failed ");
        return SendResponse( false, sender, message);
    }

    if ((position >= 0) && (pcontext->pipeline->m_pipeline)) {
        LOG_TRACE(log, "Calling seek on pipeline");
        GstClockTime seekTimeInNs = position * GST_MSECOND;
        /* we should pause pipeline prior to seeking and then resume if necessary... */
        if (pcontext->pipeline->m_inState == GST_STATE_PLAYING) {
            setState(pcontext->pipeline->m_pipeline, GST_STATE_PAUSED);
            if (GST_STATE_CHANGE_SUCCESS == (state = getState(pcontext->pipeline->m_pipeline))) {
                LOG_TRACE(log, "*** set state to paused successful ***");
                pcontext->pipeline->m_inState = GST_STATE_PAUSED;
                was_paused = true;
            }
            else {
                LOG_TRACE(log, "*** set state to paused unsuccessful ***");
            }
        }
        if (!gst_element_seek ( pcontext->pipeline->m_pipeline,
                                1.0,
                                GST_FORMAT_TIME,
                                (GstSeekFlags)((GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE)),
                                GST_SEEK_TYPE_SET,
                                seekTimeInNs,
                                GST_SEEK_TYPE_NONE,
                                GST_CLOCK_TIME_NONE)) {
                LOG_TRACE(log, "gst_element_seek to %" GST_TIME_FORMAT " failed", GST_TIME_ARGS(seekTimeInNs));
        }
        else {
                LOG_TRACE(log, "gst_element_seek to %" GST_TIME_FORMAT " was successfully initiated", GST_TIME_ARGS(seekTimeInNs));
                sendStateUpdate("seekDone", true);
        }
        if (was_paused) {
            ret = setState(pcontext->pipeline->m_pipeline, GST_STATE_PLAYING);
            if (GST_STATE_CHANGE_SUCCESS == (state = getState(pcontext->pipeline->m_pipeline))) {
                LOG_TRACE(log, "*** set state play successful");
                pcontext->pipeline->m_inState = GST_STATE_PLAYING;
            }
            else {
                LOG_TRACE(log, "*** set state to play unsuccessful *** ");
            }
        }
    }
    else {
        LOG_TRACE(log, "File position convert failed : %s : %lld", text_message, position);
        ret = false;
    }

    sendStateUpdate("seekDone",true);
    SendResponse( ret, sender, message);
    return ret;
}

/**
 * @f SetPlayRateEvent
 * "setPlayRate" state event message from uMediaServer.  set playbackrate to pipeline
 *
 */
static bool SetPlayRateEvent(UMSConnectorHandle* sender, UMSConnectorMessage* message, void* ctx)
{
    char *text_message = NULL;

    text_message = (char*)UMSConnectorGetMessageText(sender, message);

    if (text_message == NULL) {
        return SendResponse( false, sender, message);
    }

    LOG_TRACE(log, "SetPlayRateEvent : %s ", text_message);

    JSchemaInfo schemaInfo;
    jschema_info_init(&schemaInfo, jschema_all(), NULL, NULL); // no external refs & no error handlers
    jvalue_ref parsed = jdom_parse(j_cstr_to_buffer(text_message), DOMOPT_NOOPT, &schemaInfo);

    if (jis_null(parsed)) {
        LOG_WARNING(log, MSGERR_PLAYRATE, "PlaybackRate ERROR : %s ", text_message);
        return SendResponse( false, sender, message);
    }

    jvalue_ref rateref = jobject_get(parsed, j_cstr_to_buffer("playRate"));
    double rate;
    jnumber_get_f64(rateref, &rate);

    jvalue_ref audio_ref = jobject_get(parsed, j_cstr_to_buffer("audioOutput"));
    bool audioOutput;
    jboolean_get(audio_ref, &audioOutput);

    LOG_TRACE(log, "SetPlayRateEvent - rate : %f, audioOutput : %d ", rate, audioOutput);

    //TODO set playrate

    SendResponse( true, sender, message); /* acknowledge command */
    return true;
}

/**
 * @f PropertyChangeSubscriptionEvent
 * "stateChange" state message from uMediaServer. uMediaserver is subscribing to
 * pipeline property state changes in an Observer Pattern style.
 * Initiates property change notifications. All change notifications will be forwarded to the caller.
 */
static bool PropertyChangeSubscriptionEvent(UMSConnectorHandle* subscriber, UMSConnectorMessage* message, void* ctx)
{
    pipeline_context_t * pcontext = (pipeline_context_t *)ctx;

    LOG_TRACE(log, "############ PropertyChangeSubscriptionEvent adding new subscription.");
    LOG_TRACE(log, "UMSConn_handle = %p, subscriber = %p, message = %p ",
          pcontext->UMSConn_handle,subscriber, message);

    UMSConnectorAddSubscriber(pcontext->UMSConn_handle, subscriber, message);

    return true;
}

/**
 * @f SendResponse
 *
 */
bool SendResponse(bool resp, UMSConnectorHandle* sender, UMSConnectorMessage* message)
{
    return UMSConnectorSendSimpleResponse(pipeline_context.UMSConn_handle, sender, message, resp);
}

/***********************************************************
 *  Message handler support functions
 **********************************************************/
/**
 * @f getDuration
 * getDuration: gets the duration from the pipeline
 * returns true if query succeeded, durationInMs will contain the value
 * returns false if the query failed, value of durationInMs is not modified
 *
 */
static gboolean getDuration (pipe_container *pipeline, gint64* durationInMs)
{
    gint64 durationInNs = 0;

    GstFormat formatTime = GST_FORMAT_TIME;
    if (gst_element_query_duration (pipeline->m_pipeline, formatTime,  &durationInNs)) {
        pipeline->m_durationInMs = *durationInMs = durationInNs / GST_MSECOND;
        return true;
    }
    else {
        return false;
    }
}

/**
 * @f notifyPosition
 * notifyPosition update
 */
static void notifyPosition(pipe_container *pipeline, gint64 positionInMs)
{
    gint64 durationInMs = 0;
    getDuration(pipeline, &durationInMs);
    gfloat value = (gfloat)positionInMs;
    gfloat seconds = value / 1000;

    LOG_TRACE(log, "Video current offset: %f secs", seconds);

    /* durationInMs<0 may indicate live streaming,
     * i.e., radio station live broadcast */
    if (positionInMs <= durationInMs || durationInMs < 0) {
        pipeline->mCurrentPositionInMs = positionInMs;
    }
}

/**
 * @f notifyDuration
 * notifyDuration update
 */
static void notifyDuration(pipe_container *pipeline, gint64 durationInMs)
{
    /* TODO: Add duration logic for non-streaming case only */
    /* durationInMs<0 may indicate live streaming,
       i.e., radio station live broadcast */
    /* debug(TRACE, "Video Current Position: %d  ", pipeline->mCurrentPositionInMs); */
}

/****************************************************************************
 *
 *  Subscription update handlers
 *
 ****************************************************************************/

/**
 * @f sendStateUpdate
 * update uMediaServer with pipeline state
 */
static bool sendStateUpdate(char * state, bool ready)
{
	char * notification = g_strdup_printf(
				"{ \"%s\" : {"
				"\"state\":%s,"
				"\"mediaId\":\"%s\""
				"}}",
				state,
				ready ? "true" : "false",
				pipeline_context.connection_id);

	UMSConnectorSendChangeNotificationJsonString(pipeline_context.UMSConn_handle,
												 (const char *)notification);

	LOG_TRACE(log, "Sending state update. %s", notification);

	g_free(notification);

	return true;
}

/**
 * @f sendLoadState
 * update uMediaServer with pipeline state
 */
static bool sendLoadState(char *load_state)
{
	char * notification = g_strdup_printf(
				"{ \"%s\" : {"
				"\"mediaId\":\"%s\""
				"}}",
				load_state,
				pipeline_context.connection_id);

	UMSConnectorSendChangeNotificationJsonString(pipeline_context.UMSConn_handle,
												 (const char *)notification);

	LOG_TRACE(log, "Sending state update. %s", notification);

	g_free(notification);

	return true;
}

/**
 * @f sendMediaContentReady
 * send media content ready state
 */
static bool sendMediaContentReady(bool state)
{
	char * notification = g_strdup_printf(
				"{ \"mediaContentReady\" : {"
				"\"state\":%d,"
				"\"mediaId\":\"%s\""
				"}}",
				state,
				pipeline_context.connection_id);

	UMSConnectorSendChangeNotificationJsonString(pipeline_context.UMSConn_handle,
												 (const char *)notification);

	LOG_TRACE(log, "Sending state update. %s", notification);

	g_free(notification);

	return true;
}

/**
 * @f sendPositionUpdate
 * update uMediaServer with pipeline position state
 */
static bool sendPositionUpdate(gint64 currPosition)
{
	if (pipeline_context.pipeline) {
		LOG_TRACE(log, "Sending position update -> currPosition: %lld.",
				(unsigned long long)currPosition);

		char * notification = g_strdup_printf(
			"{ \"currentTime\" : {"
			"\"currentTime\":%lld,"
			"\"mediaId\":\"%s\""
			"}}",
			(unsigned long long)currPosition,
			pipeline_context.connection_id);

		UMSConnectorSendChangeNotificationJsonString(pipeline_context.UMSConn_handle,
				(const char *)notification);

		LOG_TRACE(log, "Sending state update. %s", notification);

		g_free(notification);

	}
	return true;
}

/**
 * @f sendBufferRange
 * update uMediaServer with pipeline buffering information
 */
static bool sendBufferRange(gint64 percent)
{
	if (pipeline_context.pipeline) {
		LOG_TRACE(log, "Sending bufferingInfo-> %lld.", percent);

		char * notification = g_strdup_printf(
			"{ \"bufferRange\" : {"
			"\"percent\":%lld,"
			"\"mediaId\":\"%s\""
			"}}",
			(unsigned long long)percent,
			pipeline_context.connection_id);

		UMSConnectorSendChangeNotificationJsonString(pipeline_context.UMSConn_handle,
				(const char *)notification);

		g_free(notification);

	}
	return true;
}

char checkMeta (char m)
{
	switch (m)
	{
	case '\"' :
		return '\"';
	case '\\' :
		return '\\';
	case '\n' :
		return 'n';
	case '\b' :
		return 'b';
	case '\f' :
		return 'f';
	case '\r' :
		return 'r';
	case '\t' :
		return 't';
	default	  :
		return 0;
	}
}

char * parseErrorText(const char * mess)
{
	static char buffer[MAX_BUF_LEN];
	int i = 0, j = 0;
	while (mess[i] != '\0' && j < MAX_BUF_LEN - 2)
	{
		char meta = checkMeta(mess[i]);
		if (meta)
		  buffer[j++] = '\\';
		buffer[j++] = meta ? meta : mess[i];
		++i;
	}
	buffer[j] = '\0';
	return buffer;
}

/**
 * @f sendError
 * update uMediaServer with pipeline error
 */
static bool sendError(gint64 errCode, char * message)
{
	if (pipeline_context.pipeline) {
		LOG_TRACE(log, "Sending error -> errCode: %lld.", (unsigned long long)errCode);

		message = parseErrorText(message);

		char * notification = g_strdup_printf(
			"{ \"error\" : {"
			"\"errorCode\":%lld,"
			"\"errorText\":\"%s\","
			"\"mediaId\":\"%s\""
			"}}",
			(unsigned long long)errCode,
			message,
			pipeline_context.connection_id);

		UMSConnectorSendChangeNotificationJsonString(pipeline_context.UMSConn_handle,
				(const char *)notification);

		LOG_TRACE(log, "Sending state update. %s", notification);

		g_free(notification);

	}
	return true;
}

/**
 * @f sendSourceInfo
 * send current media's source information to uMediaServer
 */
static bool sendSourceInfo(source_info_t *sourceInfo)
{
	if (pipeline_context.pipeline) {
		LOG_TRACE(log, "Sending source infomation");

		char programInfo[MAX_SOURCE_INFO_LENGTH];
		int d,i,j;
		if(sourceInfo->numPrograms)
			d = sprintf(programInfo, "\"programInfo\" : [");

		program_info_t *program = sourceInfo->programInfo;
		for (i = 0; i < sourceInfo->numPrograms; i++) {
			d += sprintf(programInfo + d, "{");
			d += sprintf(programInfo + d,
					"\"duration\" : %ld,"
					"\"numAudioTracks\": %d,",
					program->duration,
					program->numAudioTracks);

			//audio track info
			if(program->numAudioTracks)
				d += sprintf(programInfo + d, "\"audioTrackInfo\" : [");
			audio_track_t *audioTrackInfo = program->audioTrackInfo;
			for (j = 0; j < program->numAudioTracks; j++) {
				d += sprintf(programInfo + d,
						"{\"language\" : \"%s\","
						"\"codec\" : \"%s\","
						"\"profile\" : \"%s\","
						"\"level\" : \"%s\","
						"\"bitRate\" : %d,"
						"\"sampleRate\" : %f,"
						"\"channels\" : %d,"
						"\"audioType\" : %d}",
						audioTrackInfo->language,
						audioTrackInfo->codec,
						audioTrackInfo->profile,
						audioTrackInfo->level,
						audioTrackInfo->bitRate,
						audioTrackInfo->sampleRate,
						audioTrackInfo->channels,
						audioTrackInfo->audioType);

				if(audioTrackInfo->nextAudioTrackInfo) {
					d += sprintf(programInfo + d,",");
					audioTrackInfo = audioTrackInfo->nextAudioTrackInfo;
				}
			}

			if(program->numAudioTracks)
				d += sprintf(programInfo + d, "],");

			//video track info
			d += sprintf(programInfo + d,
					"\"numVideoTracks\": %d,",
					program->numVideoTracks);

			if(program->numVideoTracks)
				d += sprintf(programInfo + d, "\"videoTrackInfo\" : [");
			video_track_t *videoTrackInfo = program->videoTrackInfo;
			for (j = 0; j < program->numVideoTracks; j++) {
				d += sprintf(programInfo + d,
						"{\"angleNumber\" : %d,"
						"\"codec\" : \"%s\","
						"\"profile\" : \"%s\","
						"\"level\" : \"%s\","
						"\"width\" : %d,"
						"\"height\" : %d,"
						"\"aspectRatio\" : \"%s\","
						"\"frameRate\" : %f,"
						"\"bitRate\" : %d,"
						"\"progressive\" : %s}",
						videoTrackInfo->angleNumber,
						videoTrackInfo->codec,
						videoTrackInfo->profile,
						videoTrackInfo->level,
						videoTrackInfo->width,
						videoTrackInfo->height,
						videoTrackInfo->aspectRatio,
						videoTrackInfo->frameRate,
						videoTrackInfo->bitRate,
						videoTrackInfo->progressive?"true":"false");

				if(videoTrackInfo->nextVideoTrackInfo) {
					d += sprintf(programInfo + d,",");
					videoTrackInfo = videoTrackInfo->nextVideoTrackInfo;
				}
			}
			if(program->numVideoTracks)
				d += sprintf(programInfo + d, "],");

			//subtitle track info
			d += sprintf(programInfo + d,
					"\"numSubtitleTracks\": %d%s",
					program->numSubtitleTracks,
					(program->numSubtitleTracks) ? "," : "");

			if(program->numSubtitleTracks)
				d += sprintf(programInfo + d, "\"subtitleTrackInfo\" : [");

			subtitle_track_t *subtitleTrackInfo = program->subtitleTrackInfo;
			for (j = 0; j < program->numSubtitleTracks; j++) {
				d += sprintf(programInfo + d,
						"{\"language\" : \"%s\"}",
						subtitleTrackInfo->language);

				if(subtitleTrackInfo->nextSubtitleTrackInfo) {
					d += sprintf(programInfo + d,",");
					subtitleTrackInfo = subtitleTrackInfo->nextSubtitleTrackInfo;
				}
			}
			if(program->numSubtitleTracks)
				d += sprintf(programInfo + d, "]");

			d += sprintf(programInfo + d, "}");

			if(program->nextProgramInfo) {
				d += sprintf(programInfo + d,",");
				program = program->nextProgramInfo;
			}
		}

		if(sourceInfo->numPrograms)
			sprintf(programInfo + d, "]");

		char * notification = g_strdup_printf(
			"{ \"sourceInfo\" : {"
			"\"container\": \"%s\","
			"\"numPrograms\": %d,"
			"%s,"
			"\"seekable\": %s,"
			"\"trickable\": %s,"
			"\"mediaId\":\"%s\""
			"}}",
			sourceInfo->container,
			sourceInfo->numPrograms,
			programInfo,
			sourceInfo->seekable ? "true" : "false",
			sourceInfo->trickable ? "true" : "false",
			pipeline_context.connection_id);

		LOG_TRACE(log, "sourceInfo : %s", notification);

		UMSConnectorSendChangeNotificationJsonString(pipeline_context.UMSConn_handle,
				(const char *)notification);

		g_free(notification);
	}
	return true;
}

/**
 * @f sendRecordInfo
 * send current video track information to uMediaServer
 */
static bool sendRecordInfo(record_info_t recordInfo)
{
	if (pipeline_context.pipeline) {
		LOG_TRACE(log, "Sending record info");

		char * notification = g_strdup_printf(
			"{ \"recordInfo\" : {"
			"\"recordState\": %s,"
			"\"elapsedMiliSecond\": %d,"
			"\"bitRate\": %d,"
			"\"fileSize\": %d,"
			"\"fps\": %d,"
			"\"mediaId\":\"%s\""
			"}}",
			recordInfo.recordState ? "true" : "false",
			recordInfo.elapsedMiliSecond,
			recordInfo.bitRate,
			recordInfo.fileSize,
			recordInfo.fps,
			pipeline_context.connection_id);

		UMSConnectorSendChangeNotificationJsonString(pipeline_context.UMSConn_handle,
				(const char *)notification);

		g_free(notification);
	}
	return true;
}

/**
 * @f sendVideoInfo
 * send current video track information to uMediaServer
 */
static bool sendVideoInfo(video_info_t videoInfo)
{
	if (pipeline_context.pipeline) {
		LOG_TRACE(log, "Sending video info");

		char * notification = g_strdup_printf(
			"{ \"videoInfo\" : {"
			"\"width\": %d,"
			"\"height\": %d,"
			"\"aspectRatio\":\"%s\","
			"\"frameRate\": %f,"
			"\"mode3D\":\"%s\","
			"\"mediaId\":\"%s\""
			"}}",
			videoInfo.width,
			videoInfo.height,
			videoInfo.aspectRatio,
			videoInfo.frameRate,
			videoInfo.mode3D,
			pipeline_context.connection_id);

		UMSConnectorSendChangeNotificationJsonString(pipeline_context.UMSConn_handle,
				(const char *)notification);

		g_free(notification);
	}
	return true;
}

/**
 * @f sendAudioInfo
 * send current audio track information to uMediaServer
 */
static bool sendAudioInfo(audio_info_t audioInfo)
{
	if (pipeline_context.pipeline) {
		LOG_TRACE(log, "Sending audio info");

		char * notification = g_strdup_printf(
			"{ \"audioInfo\" : {"
			"\"sampleRate\": %f,"
			"\"channels\": %d,"
			"\"mediaId\":\"%s\""
			"}}",
			audioInfo.sampleRate,
			audioInfo.channels,
			pipeline_context.connection_id);

		UMSConnectorSendChangeNotificationJsonString(pipeline_context.UMSConn_handle,
				(const char *)notification);

		g_free(notification);
	}
	return true;
}


/**
 * @f sendStreamingInfo
 * send current streaming information to uMediaServer
 */
static bool sendStreamingInfo(streaming_info_t streamingInfo)
{
	if (pipeline_context.pipeline) {
		LOG_TRACE(log, "Sending streaming info");

		char * notification = g_strdup_printf(
			"{ \"streamingInfo\" : {"
			"\"instantBitrate\": %d,"
			"\"totalBitrate\": %d,"
			"\"mediaId\":\"%s\""
			"}}",
			streamingInfo.instantBitrate,
			streamingInfo.totalBitrate,
			pipeline_context.connection_id);

		UMSConnectorSendChangeNotificationJsonString(pipeline_context.UMSConn_handle,
				(const char *)notification);

		g_free(notification);
	}
	return true;
}

/**
 * @f sendTrackUpdate
 * update uMediaServer with pipeline track information
 */
static bool sendTrackUpdate(char *type, gint index)
{
	if (pipeline_context.pipeline) {
		LOG_TRACE(log, "Sending track update -> type : %s, index : %d", type, index);

		char * notification = g_strdup_printf(
			"{ \"trackSelected\" : {"
			"\"type\": \"%s\","
			"\"index\": %d,"
			"\"mediaId\":\"%s\""
			"}}",
			type,
			index,
			pipeline_context.connection_id);

		UMSConnectorSendChangeNotificationJsonString(pipeline_context.UMSConn_handle,
				(const char *)notification);

		g_free(notification);
	}
	return true;
}

/**
 * @f positionUpdateEvent
 * update uMediaserver with position information from gstreamer pipeline
 */
static gboolean positionUpdateEvent (void* data)
{
    gint64 currPosition = 0;
    gint64 durationInMs = 0;
    gint64 duration = 0;
    pipe_container *pipeline = (pipe_container*)data;

    if (pipeline && (pipeline->m_inState == GST_STATE_PLAYING) && (!pipeline->m_notified_end)) {
        if (NULL == pipeline->m_positionQuery) {
            pipeline->m_positionQuery = gst_query_new_position (GST_FORMAT_TIME);
        }
        if (!gst_element_query (pipeline->m_pipeline, pipeline->m_positionQuery)) {
            return true;
        }
        gst_query_parse_position (pipeline->m_positionQuery, NULL, &currPosition);

        if (currPosition > pipeline_context.pipeline->m_prevPosition) {
            pipeline_context.pipeline->m_prevPosition = currPosition;
        }

        LOG_TRACE(log, "*** currPosition = %" G_GINT64_FORMAT " , duration = %" G_GINT64_FORMAT " ",
            currPosition, duration);
        gint64 currentPositionInMs = (gint64)(currPosition / GST_MSECOND);
        sendPositionUpdate(currentPositionInMs);

        if (currPosition == duration) {
            LOG_TRACE(log, "\t**** END OF VIDEO **** ");
        }
        notifyPosition(pipeline, currentPositionInMs);

        /* If we don't have duration yet, query that as well. This is the case
         * with some demuxers that must play a few frames to estimate duration. */
        if (!getDuration(pipeline, &durationInMs)) {
            GstFormat time_format = GST_FORMAT_TIME;
            if (gst_element_query_duration (pipeline->m_pipeline, time_format, &duration) &&
                time_format == GST_FORMAT_TIME && duration > 0) {
                durationInMs = (gint32)(duration/GST_MSECOND);
                notifyDuration(pipeline, durationInMs);
            }
        }
    }
    return true;
}

/**
 * @f setElementState
 * Set the state of a gstreamer element using gst_element_set_state().
 */
static GstStateChangeReturn setElementState(GstElement *element, GstState state)
{
    return gst_element_set_state(element, state);
}

/**
 * @f getElementState
 * Fetch the state of the element using gst_element_get_state().
 */
static GstStateChangeReturn getElementState(GstElement *element, GstState* state, GstState* pending, GstClockTime timeout)
{
    return gst_element_get_state(element, state, pending, timeout);
}

/**
 * @f setElementStateAndWait
 * set element state and wait
 */
static bool setElementStateAndWait(GstElement *element, GstState state, int seconds)
{
    GstStateChangeReturn r = setElementState(element, state);
    switch (r) {

        case GST_STATE_CHANGE_FAILURE:
        case GST_STATE_CHANGE_SUCCESS:
        case GST_STATE_CHANGE_NO_PREROLL:
            break;

        case GST_STATE_CHANGE_ASYNC:
            r = getElementState(element, NULL, NULL, seconds * GST_SECOND);
            break;

        default:
            LOG_WARNING(log, MSGERR_STATE_WAIT,
                "setStateAndWait: received unexpected return from setState ");
            break;

    }
    if (GST_STATE_CHANGE_SUCCESS != r) {
        LOG_WARNING(log, MSGERR_STATE_WAIT, "setStateAndWait returning non- SUCCESS (%d) ", r);
    }
    LOG_TRACE(log, " * setElementStateAndWait changed state - change successful ");
    return(r != GST_STATE_CHANGE_FAILURE);
}

/**
 * @f setElementStateAndWait
 * destroyPosUpdateTimer handler function
 */
static void destroyPosUpdateTimer()
{
    if (pipeline_context.m_posUpdateTimer) {
        g_source_destroy (pipeline_context.m_posUpdateTimer);
        g_source_unref (pipeline_context.m_posUpdateTimer);
        pipeline_context.m_posUpdateTimer = NULL;
        LOG_TRACE(log, "******** m_posUpdateTimer was destroyed ******** ");
    }
}

/**
 * @f setMsgNotifyInterval
 * Set the interval for sending updates about position in the media stream.
 */
static void setMsgNotifyInterval (pipe_container *pipeline, int msgInterval)
{

    LOG_TRACE(log, "** setMsgNotifyInterval was invoked ** ");
    if ((!pipeline_context.m_posUpdateTimer) && (msgInterval > 0)) {
        LOG_TRACE(log, "setMsgNotifyInterval, setting msgInterval = %d ", msgInterval);
        pipeline_context.m_posUpdateTimer = g_timeout_source_new (msgInterval);
        LOG_TRACE(log, "setMsgNotifyInterval, setting Event Fn. ");
        g_source_set_callback (pipeline_context.m_posUpdateTimer, positionUpdateEvent, pipeline, NULL);

        /* In process per pipeline case we might not have a default main loop
         * or default main loop might be a wrong one, so we obtain the correct one
         * from the Environment. */
        if (pipeline_context.m_mainLoop) {
            LOG_TRACE(log, "setMsgNotifyInterval, attaching to mainloop ");
            g_source_attach (pipeline_context.m_posUpdateTimer, g_main_loop_get_context (pipeline_context.m_mainLoop) );
        }
    }
}

/**
 * @f getState
 * get element state
 */
static GstStateChangeReturn getState(GstElement *inPipeline)
{
    return(gst_element_get_state (inPipeline, NULL, NULL, -1));
}

/**
 * @f setState
 * set element state
 */
static gboolean setState(GstElement *inPipeline, GstState inState)
{
    gboolean ret = TRUE;
    char stateStr[MAX_STATE_STRING];

    LOG_TRACE(log, "setState invoked on the pipeline = %p ", inPipeline);

    switch (inState) {

        case GST_STATE_NULL:
            strcpy(stateStr, "GST_STATE_NULL");
            break;

        case GST_STATE_READY:
            strcpy(stateStr, "GST_STATE_READY");
            break;

        case GST_STATE_PAUSED:
            strcpy(stateStr, "GST_STATE_PAUSED");
            break;

        case GST_STATE_PLAYING:
            strcpy(stateStr, "GST_STATE_PLAYING");
            break;

        default:
            LOG_WARNING(log, MSGERR_STATE, "Unrecognized state: %d", inState);
            return FALSE;

    }

    GstState currentState;
    GstStateChangeReturn result;
    result = gst_element_set_state (inPipeline, inState);

    switch (result) {

        case GST_STATE_CHANGE_FAILURE: {
                LOG_WARNING(log, MSGERR_STATE, "failed to set the state to %s ", stateStr);
                ret = FALSE;
                break;
            }

        case GST_STATE_CHANGE_SUCCESS: {
                LOG_TRACE(log, "setting the state to %s succeeded ", stateStr);
                break;
            }

        case GST_STATE_CHANGE_ASYNC: {
                unsigned int waitSeconds = 2;
                LOG_TRACE(log, "waiting up to %d sec for the state change to %s to finish ",
                      waitSeconds, stateStr);
                result = gst_element_get_state (inPipeline, &currentState, NULL, GST_SECOND * waitSeconds);
                if (result == GST_STATE_CHANGE_SUCCESS) {
                    LOG_TRACE(log, "setting the state to %s succeeded ", stateStr);
                }
                else {
                    LOG_WARNING(log, MSGERR_STATE, "failed to set the state to %s ", stateStr);
                    ret = FALSE;
                }
                break;
            }

        default: {
                LOG_WARNING(log, MSGERR_STATE, "unexpected result from gst_element_set_state");
                ret = FALSE;
                break;
            }

    }
    return ret;
}

/**
 * @f print_tab
 * pipeline print_tag function
 */
static void print_tag (const GstTagList* list, const gchar* tag, gpointer user_data)
{
    guint num = gst_tag_list_get_tag_size (list, tag);
    guint i = 0;
    const GValue *val = NULL;

    for (i = 0; i < num; ++i) {
        val = gst_tag_list_get_value_index (list, tag, i);
        if (G_VALUE_HOLDS_STRING (val)) {
            LOG_TRACE(log, "TAG  %20s (%15s) : %s",tag, G_VALUE_TYPE_NAME (val), g_value_get_string (val));
        }
        else if (G_VALUE_HOLDS_UINT (val)) {
            LOG_TRACE(log, "TAG  %20s (%15s) : %u", tag, G_VALUE_TYPE_NAME (val), g_value_get_uint (val));
        }
        else if (G_VALUE_HOLDS_DOUBLE (val)) {
            LOG_TRACE(log, "TAG  %20s (%15s) : %g", tag, G_VALUE_TYPE_NAME (val), g_value_get_double (val));
        }
        else if (G_VALUE_HOLDS_BOOLEAN (val)) {
            LOG_TRACE(log, "TAG  %20s (%15s) : %s", tag, G_VALUE_TYPE_NAME (val), (g_value_get_boolean (val)) ? "true" : "false");
        }
        else if (GST_VALUE_HOLDS_BUFFER (val)) {
            LOG_TRACE(log, "TAG  %20s (%15s) : buffer of size %u", tag, G_VALUE_TYPE_NAME (val), gst_buffer_get_size (gst_value_get_buffer (val)));
        }
        else if (G_VALUE_HOLDS (val,G_TYPE_DATE)) {
            LOG_TRACE(log, "TAG  %20s (%15s) : date (year=%u,...)", tag, G_VALUE_TYPE_NAME (val), g_date_get_year (g_value_get_boxed (val)));
        }
        else {
            LOG_TRACE(log, "TAG  %20s (%15s) : UNHANDLED", tag, G_VALUE_TYPE_NAME (val));
        }
    }
}

/**
 * @f analyze_streams
 * Extract some metadata from the streams using pads
 */

static void analyze_streams (pipe_container *pipeline) {
    gint i,j;
    GstTagList *tags;
    gchar *str;
    guint rate;
    GstPad *sinkpad = NULL;

    source_info_t *sourceInfo = (source_info_t*)malloc(sizeof(source_info_t));
    memset(sourceInfo,0,sizeof(source_info_t));

    //For common case. If multiple programs exist in a media, you should set the number of programs to this value.
    sourceInfo->numPrograms = 1;
    program_info_t* pProgramInfo = NULL;
    for (j = 0; j < sourceInfo->numPrograms; j++) {
        pProgramInfo = (program_info_t*) malloc(sizeof(program_info_t));
        memset(pProgramInfo,0,sizeof(program_info_t));

        if(j == 0)
            sourceInfo->programInfo = pProgramInfo;

        /* Read some properties */
        g_object_get (pipeline->m_pipeline, "n-video", &pProgramInfo->numVideoTracks, NULL);
        g_object_get (pipeline->m_pipeline, "n-audio", &pProgramInfo->numAudioTracks, NULL);
        g_object_get (pipeline->m_pipeline, "n-text", &pProgramInfo->numSubtitleTracks, NULL);

        if(pipeline->m_durationInMs)
            pProgramInfo->duration = pipeline->m_durationInMs;

        if(pProgramInfo->numVideoTracks) {
            g_object_get (pipeline->m_pipeline, "current-video", &pipeline->m_current_video, NULL);
            sendTrackUpdate("video",pipeline->m_current_video);
        }
        if(pProgramInfo->numAudioTracks) {
            g_object_get (pipeline->m_pipeline, "current-audio", &pipeline->m_current_audio, NULL);
            sendTrackUpdate("audio",pipeline->m_current_audio);
        }
        if(pProgramInfo->numSubtitleTracks) {
            g_object_get (pipeline->m_pipeline, "current-text", &pipeline->m_current_text, NULL);
            sendTrackUpdate("text",pipeline->m_current_text);
        }

        LOG_TRACE(log, "%d video stream(s), %d audio stream(s), %d text stream(s)\n",
                pProgramInfo->numVideoTracks, pProgramInfo->numAudioTracks, pProgramInfo->numSubtitleTracks);

        video_track_t *videoTrackInfo = NULL;
        for (i = 0; i < pProgramInfo->numVideoTracks; i++) {
            videoTrackInfo = (video_track_t*)malloc(sizeof(video_track_t));
            memset(videoTrackInfo,0,sizeof(video_track_t));

            if(i == 0)
                pProgramInfo->videoTrackInfo = videoTrackInfo;

            g_signal_emit_by_name (pipeline->m_pipeline, "get-video-pad", i, &sinkpad);
            if (sinkpad)
            {
                GstCaps *caps;
                if (((caps = gst_pad_get_current_caps (sinkpad))) != NULL)
                {
                    GstStructure *s = gst_caps_get_structure (caps, 0);
                    gint numerator,denominator;

                    LOG_TRACE(log, "video caps : %s", gst_caps_to_string(caps));

                    if(i == pipeline->m_current_video && gst_structure_has_field(s, "container"))
                        strcpy(sourceInfo->container, gst_structure_get_string (s, "container"));
                    gst_structure_get_int (s, "angle-number", &(videoTrackInfo->angleNumber));
                    if(gst_structure_has_field(s, "codec"))
                        strcpy(videoTrackInfo->codec, gst_structure_get_string (s, "codec"));
                    if(gst_structure_has_field(s, "profile"))
                        strcpy(videoTrackInfo->profile, gst_structure_get_string (s, "profile"));
                    if(gst_structure_has_field(s, "level"))
                        strcpy(videoTrackInfo->level, gst_structure_get_string (s, "level"));
                    gst_structure_get_int (s, "width", &(videoTrackInfo->width));
                    gst_structure_get_int (s, "height", &(videoTrackInfo->height));
                    if(gst_structure_has_field(s, "aspect-ratio"))
                        strcpy(videoTrackInfo->aspectRatio, gst_structure_get_string (s, "aspect-ratio"));
                    if(gst_structure_get_fraction (s, "framerate", &numerator, &denominator))
                        videoTrackInfo->frameRate = (float)numerator/(float)denominator;
                    gst_structure_get_int (s, "bitrate", &(videoTrackInfo->bitRate));
                    gst_caps_unref (caps);
                }
                gst_object_unref (sinkpad);
            }
            videoTrackInfo = videoTrackInfo->nextVideoTrackInfo;
        }

        sinkpad = NULL;
        audio_track_t *audioTrackInfo = NULL;
        for (i = 0; i < pProgramInfo->numAudioTracks; i++) {
            audioTrackInfo = (audio_track_t*)malloc(sizeof(audio_track_t));
            memset(audioTrackInfo,0,sizeof(audio_track_t));

            if(i == 0)
                pProgramInfo->audioTrackInfo = audioTrackInfo;

            g_signal_emit_by_name (pipeline->m_pipeline, "get-audio-pad", i, &sinkpad);
            if (sinkpad)
            {
                GstCaps *caps;
                 if (((caps = gst_pad_get_current_caps (sinkpad))) != NULL)
                {
                    GstStructure *s = gst_caps_get_structure (caps, 0);
                    gint numerator,denominator;

                    LOG_TRACE(log, "audio caps : %s", gst_caps_to_string(caps));

                    if(gst_structure_has_field(s, "language"))
                        strcpy(audioTrackInfo->language, gst_structure_get_string (s, "language"));
                    if(gst_structure_has_field(s, "codec"))
                        strcpy(audioTrackInfo->codec, gst_structure_get_string (s, "codec"));
                    if(gst_structure_has_field(s, "profile"))
                        strcpy(audioTrackInfo->profile, gst_structure_get_string (s, "profile"));
                    if(gst_structure_has_field(s, "level"))
                        strcpy(audioTrackInfo->level, gst_structure_get_string (s, "level"));
                    gst_structure_get_int (s, "rate", &(audioTrackInfo->bitRate));
                    if(gst_structure_get_fraction (s, "samplerate", &numerator, &denominator))
                       audioTrackInfo->sampleRate = (double)numerator/(double)denominator;
                    gst_structure_get_int (s, "channels", &(audioTrackInfo->channels));
                    gst_caps_unref (caps);
                }
                gst_object_unref (sinkpad);
            }
            audioTrackInfo = audioTrackInfo->nextAudioTrackInfo;
        }

        sinkpad = NULL;
        subtitle_track_t *subtitleTrackInfo = NULL;
        for (i = 0; i < pProgramInfo->numSubtitleTracks; i++) {
            subtitleTrackInfo = (subtitle_track_t*)malloc(sizeof(subtitle_track_t));
            memset(subtitleTrackInfo,0,sizeof(subtitle_track_t));

            if(i == 0)
                pProgramInfo->subtitleTrackInfo = subtitleTrackInfo;

            g_signal_emit_by_name (pipeline->m_pipeline, "get-text-pad", i, &sinkpad);
            if (sinkpad)
            {
                GstCaps *caps;
                if (((caps = gst_pad_get_current_caps (sinkpad))) != NULL)
                {
                    GstStructure *s = gst_caps_get_structure (caps, 0);
                    gint numerator,denominator;

                    LOG_TRACE(log, "subtitle caps : %s", gst_caps_to_string(caps));

                    if(gst_structure_has_field(s, "language"))
                        strcpy(subtitleTrackInfo->language, gst_structure_get_string (s, "language"));
                    gst_caps_unref (caps);
                }
                gst_object_unref (sinkpad);
            }
            subtitleTrackInfo = subtitleTrackInfo->nextSubtitleTrackInfo;
        }

        pProgramInfo = pProgramInfo->nextProgramInfo;
    }

    LOG_TRACE(log, "completed to parse tags for video stream : %p",sourceInfo->programInfo);

    sendSourceInfo(sourceInfo);

    // TODO send this event from proper place.
    video_info_t videoInfo = {0};
    if(sourceInfo->numPrograms && sourceInfo->programInfo->numVideoTracks) {
        videoInfo.width = sourceInfo->programInfo->videoTrackInfo->width;
        videoInfo.height = sourceInfo->programInfo->videoTrackInfo->height;
        if(sourceInfo->programInfo->videoTrackInfo->aspectRatio)
            strcpy(videoInfo.aspectRatio,sourceInfo->programInfo->videoTrackInfo->aspectRatio);
        videoInfo.frameRate = sourceInfo->programInfo->videoTrackInfo->frameRate;
        strcpy(videoInfo.mode3D,"test3dmode");
    }
    sendVideoInfo(videoInfo);

    // TODO send this event from proper place.
    audio_info_t audioInfo = {0};
    if(sourceInfo->numPrograms && sourceInfo->programInfo->numAudioTracks) {
        audioInfo.sampleRate = sourceInfo->programInfo->audioTrackInfo->sampleRate;
        audioInfo.channels = sourceInfo->programInfo->audioTrackInfo->channels;
    }
    sendAudioInfo(audioInfo);

    //TODO release structures in sourceInfo.
    if(sourceInfo->numPrograms) {
        program_info_t *programInfo = sourceInfo->programInfo;
        for (i=0; i < sourceInfo->numPrograms; i++) {
            program_info_t *nextProgramInfo = programInfo->nextProgramInfo;
            audio_track_t *audioTrackInfo = programInfo->audioTrackInfo;
            for(j=0 ; j <programInfo->numAudioTracks ; j++) {
                audio_track_t *NextAudioTrackInfo = audioTrackInfo->nextAudioTrackInfo;
                free(audioTrackInfo);
                audioTrackInfo = NextAudioTrackInfo;
            }
            video_track_t *videoTrackInfo = programInfo->videoTrackInfo;
            for(j=0 ; j <programInfo->numVideoTracks ; j++) {
                video_track_t *NextVideoTrackInfo = videoTrackInfo->nextVideoTrackInfo;
                free(videoTrackInfo);
                videoTrackInfo = NextVideoTrackInfo;
            }
            subtitle_track_t *subtitleTrackInfo = programInfo->subtitleTrackInfo;
            for(j=0 ; j <programInfo->numSubtitleTracks ; j++) {
                subtitle_track_t *NextSubtitleTrackInfo = subtitleTrackInfo->nextSubtitleTrackInfo;
                free(subtitleTrackInfo);
                subtitleTrackInfo = NextSubtitleTrackInfo;
            }
            free(programInfo);
            programInfo = nextProgramInfo;
        }
    }

    free(sourceInfo);

    LOG_TRACE(log, "Currently playing video stream %d, audio stream %d and text stream %d\n",
            pipeline->m_current_video, pipeline->m_current_audio, pipeline->m_current_text);
}

/**
 * @f bus_call
 * gstreamer bus message callback function
 */
static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data)
{

    bool ret = true;
    pipe_container *pipeline = (pipe_container*)data;
    LOG_TRACE(log, "  -->bus msg ==> %s ", GST_MESSAGE_SRC_NAME(msg));

    if (msg) {

        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_TAG: {
                    GstTagList *tags = NULL;
                    gst_message_parse_tag(msg, &tags);
                    gst_tag_list_foreach(tags, print_tag, NULL);
                    gst_tag_list_free(tags);
                    break;
                }

            case GST_MESSAGE_STATE_CHANGED: {
                    // Ignore state changes from internal elements. They are
                    // forwarded to playbin2 anyway.
                    if (GST_MESSAGE_SRC(msg) != GST_OBJECT(pipeline->m_pipeline))
                        break;

                    GstState old_state;
                    GstState new_state;
                    gst_message_parse_state_changed(msg, &old_state, &new_state, NULL);
                    LOG_TRACE(log, "     from %s to %s ",
                          gst_element_state_get_name(old_state),
                          gst_element_state_get_name (new_state));

                    if (old_state == GST_STATE_PAUSED && new_state == GST_STATE_PLAYING) {
                        sendStateUpdate("playing", true);
                    } else if (old_state == GST_STATE_PLAYING && new_state == GST_STATE_PAUSED) {
                        sendStateUpdate("paused", true);
                    } else if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED) {
                        analyze_streams(data);
                        sendMediaContentReady(true);
                    } else if (old_state == GST_STATE_PAUSED && new_state == GST_STATE_READY) {
                        sendMediaContentReady(false);
                    }
                    break;
                }

            case GST_MESSAGE_STREAM_STATUS: {
                    GstStreamStatusType type;
                    gst_message_parse_stream_status(msg, &type, NULL);
                    switch (type) {
                    case GST_STREAM_STATUS_TYPE_CREATE:
                        LOG_TRACE(log, "     status GST_STREAM_STATUS_TYPE_CREATE ");
                        break;
                    case GST_STREAM_STATUS_TYPE_ENTER:
                        LOG_TRACE(log, "     status GST_STREAM_STATUS_TYPE_ENTER ");
                        break;
                    case GST_STREAM_STATUS_TYPE_LEAVE:
                        LOG_TRACE(log, "     status GST_STREAM_STATUS_TYPE_LEAVE ");
                        break;
                    case GST_STREAM_STATUS_TYPE_DESTROY:
                        LOG_TRACE(log, "     status GST_STREAM_STATUS_TYPE_DESTROY ");
                        break;
                    case GST_STREAM_STATUS_TYPE_START:
                        LOG_TRACE(log, "     status GST_STREAM_STATUS_TYPE_START ");
                        break;
                    case GST_STREAM_STATUS_TYPE_PAUSE:
                        LOG_TRACE(log, "     status GST_STREAM_STATUS_TYPE_PAUSE ");
                        break;
                    case GST_STREAM_STATUS_TYPE_STOP:
                        LOG_TRACE(log, "     status GST_STREAM_STATUS_TYPE_STOP ");
                        break;
                    default:
                        LOG_TRACE(log, "     status UNKNOWN ");
                        break;
                    }
                    break;
                }

            case GST_MESSAGE_ERROR: {
                    GError *err = NULL;
                    gchar *dbg_info = NULL;
                    gst_message_parse_error(msg, &err, &dbg_info);
                    LOG_ERROR(log, MSGERR_GST_MSG, "ERROR from element %s ", GST_OBJECT_NAME(msg->src));
                    LOG_DEBUG(log, "Debugging info: %s ", (dbg_info) ? dbg_info : "none");
                    sendError(err->code, dbg_info);

                    if ( err->code == GST_RESOURCE_ERROR_NOT_FOUND ) {
                        LOG_DEBUG(log, " GST_RESOURCE_ERROR_NOT_FOUND exiting pipeline.");
                        _UnloadPipeline(&pipeline_context);
                        UMSConnectorDestroy((&pipeline_context)->UMSConn_handle);
                    }

                    g_error_free(err);
                    g_free(dbg_info);
                    break;
                }

            case GST_MESSAGE_WARNING: {
                    GError *err = NULL;
                    gchar *dbg_info = NULL;
                    gst_message_parse_warning(msg, &err, &dbg_info);
                    LOG_WARNING(log, MSGERR_GST_MSG_WARN, "WARN from element %s ", GST_OBJECT_NAME(msg->src));
                    LOG_DEBUG(log, "Debugging info: %s ", (dbg_info) ? dbg_info : "none");
                    g_error_free(err);
                    g_free(dbg_info);
                    break;
                }

            case GST_MESSAGE_EOS: {
                    if (!pipeline->m_notified_end) {
                        pipeline->m_notified_end = true;
                        sendStateUpdate("endOfStream",true);
                    }
                    break;
                }

            case GST_MESSAGE_DURATION: {
                    gint64 durationInMs = 0;
                    getDuration(pipeline, &durationInMs);
                    break;
                }

            case GST_MESSAGE_ASYNC_DONE: {
                    break;
                }
            case GST_MESSAGE_BUFFERING:{
                  gint percent;
                  gst_message_parse_buffering (msg, &percent);

                  if (percent == 100) {
                      // TODO : fix this. should decide what do to automatically after buffering
                      // /* a 100% message means buffering is done */
                      // buffering = FALSE;
                      // /* if the desired state is playing, go back */
                      // if (target_state == GST_STATE_PLAYING) {
                      //	gst_element_set_state (pipeline, GST_STATE_PLAYING);
                      // }
                      // bufferring = FALSE;
                      if(pipeline->m_buffering == TRUE) {
                         pipeline->m_buffering = FALSE;
                         sendStateUpdate("bufferingEnd",true);
                      }
                  } else {
                      // TODO : fix this. should decide what do to automatically after buffering
                      /* buffering busy */
                      // if (buffering == FALSE && target_state == GST_STATE_PLAYING) {
                      //   /* we were not buffering but PLAYING, PAUSE  the pipeline. */
                      //   gst_element_set_state (pipeline, GST_STATE_PAUSED);
                      // }
                      // buffering = TRUE;
                      if(pipeline->m_buffering == FALSE) {
                         pipeline->m_buffering = TRUE;
                         sendStateUpdate("bufferingStart",true);
                      }
                  }
                  sendBufferRange(percent);
                  break;
            }

            default:
                break;

        }
    }

    return ret;
}
