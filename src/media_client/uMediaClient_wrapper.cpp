// Copyright (c) 2008-2018 LG Electronics, Inc.
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


#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>

#include "uMediaClient.h"
#include "uMediaClient_wrapper.h"


using namespace uMediaServer;
using namespace std;

// UMEDIASERVER_CONNECTION_ID

//
// create media player client
//
class MediaPlayer : public uMediaClient {
public:
	MediaPlayer(UMSConnectorBusType bus = UMS_CONNECTOR_PUBLIC_BUS) : uMediaClient(false, bus) {
		startInputMessageThread();
	};

	~MediaPlayer() {
		stop();   // exit uMS API event loop
		pthread_join(input_process_thread,NULL);
	};

	int startInputMessageThread() {
		return pthread_create(&input_process_thread,NULL,inputThread,this);
	}

	// Thread to run event loop for subscription and command messages
	static void * inputThread(void *ctx) {
		MediaPlayer * self = static_cast<MediaPlayer *>(ctx);
		self->run();
		return NULL;
	}

	// TODO determine how C api will overload subscription messages
	//        function registertion etc ...
	// override currentTimeEvent virtual method
	//bool currentTimeChanged(long long currentTime)	{
	//	printf("currentTime=%lld\n",currentTime);
	//	return true;
	//}

private:
	pthread_t input_process_thread;

};

uMediaClientHandle * uMediaClientCreate()
{
	MediaPlayer *mp = new MediaPlayer(UMS_CONNECTOR_PUBLIC_BUS);
	if ( mp == NULL ) {
		return NULL;
	}
	return reinterpret_cast<uMediaClientHandle *>(mp);
}

uMediaClientHandle * uMediaClientCreatePrivate()
{
	MediaPlayer *mp = new MediaPlayer(UMS_CONNECTOR_PRIVATE_BUS);
	if ( mp == NULL ) {
		return NULL;
	}
	return reinterpret_cast<uMediaClientHandle *>(mp);
}

// @f uMediaClientLoad
// @brief load requested media
//
int uMediaClientLoad(uMediaClientHandle hdl,
						char * uri,
						AudioStreamClass audioClass,
						char * payload)
{
	MediaPlayer * mp = reinterpret_cast<MediaPlayer *>(hdl);
	return mp->load(uri,audioClass,payload);
}

int uMediaClientAttach(uMediaClientHandle hdl,
						char * mediaId)
{
	MediaPlayer * mp = reinterpret_cast<MediaPlayer *>(hdl);
	return mp->attach(mediaId);
}

int uMediaClientUnload(uMediaClientHandle hdl)
{
	MediaPlayer * mp = reinterpret_cast<MediaPlayer *>(hdl);
	return mp->unload();
}

int uMediaClientPlay(uMediaClientHandle hdl)
{
	MediaPlayer * mp = reinterpret_cast<MediaPlayer *>(hdl);
	return mp->play();
}

int uMediaClientPause(uMediaClientHandle hdl)
{
	MediaPlayer * mp = reinterpret_cast<MediaPlayer *>(hdl);
	return mp->pause();
}

int uMediaClientSeek(uMediaClientHandle hdl, long position)
{
	MediaPlayer * mp = reinterpret_cast<MediaPlayer *>(hdl);
	return mp->seek(position);
}

int uMediaClientDestroy(uMediaClientHandle hdl)
{
	MediaPlayer * mp = reinterpret_cast<MediaPlayer *>(hdl);
	delete mp;
	return 1;
}
