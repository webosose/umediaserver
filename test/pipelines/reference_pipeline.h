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
 * @file playbin2_pipeline.h
 *
 * @brief This is a component of the uMediaServer implementation
 ********************************************************************************/
#ifndef _PLAYBIN2_PIPELINE_H_
#define _PLAYBIN2_PIPELINE_H_

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
#include <pthread.h>
#include <luna-service2/lunaservice.h>
#include <pbnjson.h>
#include <UMSConnector_wrapper.h>
#include <ResourceManagerClient_c.h>

#define MAX_MEDIA_FILENAME          2048
#define MAX_RESPONSE_LENGTH         640
#define MAX_STATE_STRING            80
#define MAX_SERVICE_STRING          120
#define MAX_USER_AGENT_LENGTH       1024
#define MAX_COOKIES_LENGTH          2048
#define MAX_REFERRER_LENGTH         2048
#define MAX_SOURCE_INFO_STRING      1024
#define MAX_SOURCE_INFO_LENGTH      2048
#define MAX_BUF_LEN                 2048

#define GST_PLAY_FLAG_VIDEO         (1 << 0)
#define GST_PLAY_FLAG_AUDIO         (1 << 1)
#define GST_PLAY_FLAG_TEXT          (1 << 2)
#define GST_PLAY_FLAG_VIS           (1 << 3)
#define GST_PLAY_FLAG_SOFT_VOLUME   (1 << 4)
#define GST_PLAY_FLAG_NATIVE_AUDIO  (1 << 5)
#define GST_PLAY_FLAG_NATIVE_VIDEO  (1 << 6)
#define GST_PLAY_FLAG_DOWNLOAD      (1 << 7)
#define GST_PLAY_FLAG_BUFFERING     (1 << 8)
#define GST_PLAY_FLAG_DEINTERLACE   (1 << 9)

#define PREROLL_TIMEOUT             50
#define PAUSE_TIMEOUT               50
#define PLAY_TIMEOUT                50
#define SEEK_TIMEOUT                2
#define HTTP_TIMEOUT                15
#define TEARDOWN_TIMEOUT            5
#define DECODER_INIT_TIMEOUT        2
#define BUFFER_SIZE                 (8 * 1024 * 1024)
#define BUFFER_DURATION             10
#define RTSP_BUFFER_DURATION        3
#define MSG_NOTIFY_INTERVAL         200
#define BYTES_LOADED_INTERVAL       1000

// Decodebin needed function forward declarations...
static void S_aboutToFinish (GstElement* element, gpointer data);
static void S_videoChanged (GstElement* element, gpointer data);
static void S_audioChanged (GstElement* element, gpointer data);
static void S_videoTagsChanged (GstElement* element, gint stream, gpointer data);
static void S_audioTagsChanged (GstElement* element, gint stream, gpointer data);
static void S_sourceSetup (GstElement* element, GstElement* source, gpointer data);
void audioSinkSetup();
void videoSinkSetup();
void dumpPipeline(const gchar* name);

// Data Class
//bufferRange structure
typedef struct buffer_range{
    long int beginTime;
    long int endTime;
    long int remaningTime;
    long int percent;
}buffer_range_t;

//sourceInfo structure
typedef struct audio_track {
    char language[MAX_SOURCE_INFO_STRING];
    char codec[MAX_SOURCE_INFO_STRING];
    char profile[MAX_SOURCE_INFO_STRING];
    char level[MAX_SOURCE_INFO_STRING];
    int bitRate;
    float sampleRate;
    int channels;
    int audioType;
    struct audio_track *nextAudioTrackInfo;
}audio_track_t;

typedef struct video_track {
    int angleNumber;
    char codec[MAX_SOURCE_INFO_STRING];
    char profile[MAX_SOURCE_INFO_STRING];
    char level[MAX_SOURCE_INFO_STRING];
    int width;
    int height;
    char aspectRatio[MAX_SOURCE_INFO_STRING];
    float frameRate;
    int bitRate;
    bool progressive;
    struct video_track *nextVideoTrackInfo;
}video_track_t;

typedef struct subtitle_track {
    char language[MAX_SOURCE_INFO_STRING];
    struct subtitle_track *nextSubtitleTrackInfo;
}subtitle_track_t;

typedef struct program_info {
    long int duration;
    int numAudioTracks;
    audio_track_t *audioTrackInfo;
    int numVideoTracks;
    video_track_t *videoTrackInfo;
    int numSubtitleTracks;
    subtitle_track_t *subtitleTrackInfo;
    struct program_info *nextProgramInfo;
}program_info_t;

typedef struct source_info {
    char container[MAX_SOURCE_INFO_STRING];
    int numPrograms;
    bool seekable;
    bool trickable;
    program_info_t *programInfo;
}source_info_t;

typedef struct streaming_info {
    int instantBitrate;
    int totalBitrate;
}streaming_info_t;

typedef struct video_info {
    int width;
    int height;
    char aspectRatio[MAX_SOURCE_INFO_STRING];
    float frameRate;
    char mode3D[MAX_SOURCE_INFO_STRING];
}video_info_t;

typedef struct audio_info {
    float sampleRate;
    int channels;
}audio_info_t;

typedef struct record_info {
    bool recordState;
    int elapsedMiliSecond;
    int bitRate;
    int fileSize;
    int fps;
}record_info_t;

/** Pipeline instance data container */
typedef struct tag_pipe_container {

    GstState                                m_inState;
    GstElement                              *m_pipeline;
    GstElement                              *m_decodebin;
    GstElement                              *m_videosink;
    GstElement                              *m_audiosink;
    GstElement                              *m_src;
    GstElement                              *m_decoderPipeline;
    GstQuery*                               m_positionQuery;
    guint                                   m_watch;
    guint64                                 m_durationInMs;
    gint32                                  mCurrentPositionInMs;
    gint64                                  m_prevPosition;
    gboolean                                m_notified_end;
    gboolean                                m_buffering;
    gint                                    m_current_video;
    gint                                    m_current_audio;
    gint                                    m_current_text;
    source_info_t                           m_sourceInfo;
    char                                    mediafile[MAX_MEDIA_FILENAME];
    char userAgent[MAX_USER_AGENT_LENGTH];
    char cookies[MAX_COOKIES_LENGTH];
    char referrer[MAX_REFERRER_LENGTH];
} pipe_container;


/** Process instance context container */
typedef struct pipeline_context {

    UMSConnectorHandle                      *UMSConn_handle;
    ResourceManagerClientHandle             *rmc_handle;
    GMainLoop                               *m_mainLoop;
    GSource*                                m_posUpdateTimer;
    pipe_container                          *pipeline;
    gboolean                                m_pipe_ready;
    gboolean                                m_video_loaded;
    char                                  	m_key[MAX_SERVICE_STRING];
    gint32                                  m_current_time_notify_interval;
    gint32                                  m_buffer_range_interval;
    char                                    service_name[MAX_SERVICE_STRING];
    char                                    connection_id[MAX_SERVICE_STRING];
    pthread_mutex_t                         lock;

} pipeline_context_t;


static void* pipeline_play_thread(void *ptr);

/** pipeline state update functions */
static bool sendStateUpdate(char *state, bool ready);
static bool sendLoadState(char *load_state);
static bool sendBufferingRange(gint64 percent);
static bool sendError(gint64 errCode, char *message);
static bool sendSourceInfo(source_info_t *sourceInfo);
static bool sendVideoInfo(video_info_t videoInfo);
static bool sendAudioInfo(audio_info_t audioInfo);
static bool sendStreamingInfo(streaming_info_t streamingInfo);
static bool sendTrackUpdate(char *type, gint index);
static bool sendRecordInfo(record_info_t recordInfo);
static bool sendMediaContentReady(bool state);

static gboolean setState(GstElement *inPipeline, GstState inState);
static GstStateChangeReturn getState(GstElement *inPipeline);
static bool setElementStateAndWait(GstElement *element, GstState state, int seconds);
static void destroyPosUpdateTimer();
static void setMsgNotifyInterval (pipe_container *pipeline, int msgInterval);
static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data);
static void analyze_streams (pipe_container *pipeline);
#endif  /* _PLAYBIN2_PIPELINE_H_ */
