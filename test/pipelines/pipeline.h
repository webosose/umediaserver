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

#ifndef _PIPELINE_H
#define _PIPELINE_H

#include <cstdio>
#include <glib.h>
#include <gst/gst.h>
#include <gst/gstelement.h>
#include <Logger.h>
#include "micromediaserver.h"

const int VIDEO_FILE_NAME_MAX_SIZE = 500;

//
class LSHandle;
class LSMessage;

class Pipeline
{
public:
	Pipeline(GMainLoop *m_mainLoop);
	~Pipeline();

	static 	Pipeline* 		        GetInstance() { return s_instance; }
	static 	gboolean  		        setState(GstElement *inPipeline, GstState inState);
	static 	GstStateChangeReturn 	getState(GstElement *inPipeline);
	static 	void 	 		        playbin_callback(GstElement *decodebin, GstPad *pad, gboolean last, gpointer data);
	static 	void* 	 		        decodePipeline_thread(void *ptr);
	static gboolean 		        bus_call(GstBus *bus, GstMessage *msg, gpointer data);
	void 		 		            set_video_file(const char *file) { strncpy(m_video_file, file, VIDEO_FILE_NAME_MAX_SIZE); }
	GstElement*	 		            get_pipeline() { return m_pipeline; };
	char*		 		            get_video_file() { return m_video_file; };

	/**
	 * Set the interval to send updates about the current position.
	 * @param msgInterval the interval in ms
	 */
	virtual void setMsgNotifyInterval (int msgInterval);


protected:

	Pipeline(const Pipeline &rhs);
	Pipeline & operator=(const Pipeline &rhs);

	static 	gboolean 		        CallbackPositionUpdate (void* data);
	virtual void			        notifyPosition(gint32 positionInMs);
	virtual void			        notifyDuration(gint64 durationInMs);
	virtual gboolean 		        GetDuration (gint64* durationInMs);

	// Frequency of position updates in milliseconds
	gint64		                    m_playbackPosUpdateFreqInMs;

	// Delay between position updates
	GSource*                        m_posUpdateTimer;

	// Notification interval
	gboolean                        m_isNotifyInterval;

public:

	// Set the state of a gstreamer element using gst_element_set_state().
	static GstStateChangeReturn setElementState(GstElement *element, GstState state) {
	    return gst_element_set_state(element, state);
	}

	// Setstate and wait until complete
	static bool setElementStateAndWait(GstElement *element, GstState state, int seconds);

	// Fetch the state of the element using gst_element_get_state().
	static GstStateChangeReturn getElementState(GstElement *element, GstState* state, GstState* pending, GstClockTime timeout) {
	    return gst_element_get_state(element, state, pending, timeout);
	}

	MicroMediaServer 	            	*parent_mediaserver;
	GMainLoop 		                *m_mainLoop;
	GstElement 		                *m_src;
	GstElement 		                *m_decodebin;
	GstElement 		                *m_decoderPipeline;
	GstElement 		                *m_videosink;
	GstElement 		                *m_audiosink;
	GstState    		            	m_inState;
	GstElement 		                *m_pipeline;
	char 			                m_video_file[VIDEO_FILE_NAME_MAX_SIZE];
	guint 			                m_watch;
	LSHandle 		                *m_service;
	guint64 		                m_durationInMs;
	gint32 			                mCurrentPositionInMs;

private:

	GstElement*         	        	audioFile;
	GstElement*         	        	videoFile;
	GstQuery*           	        	m_positionQuery;
	guint64 		                lastBusActivity;
	gint32 			                currentPosition;
	gint32 			                duration;
	GSource* 		                thumbnailTimer;
	int 			                mCount;
	int 			                mWidth;
	int 			                mHeight;
	gint32 			                mStartTime;
	gint32 			                mEndTime;
	gint32 			                mSeekPeriod;
	std::string 		            	mFilenameBase;

	// Logging capability
	uMediaServer::Logger log;

protected:

	static Pipeline 	            	*s_instance;

};

#endif   // _PIPELINE_H
