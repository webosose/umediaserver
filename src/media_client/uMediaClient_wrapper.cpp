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
class MediaPlayerClient : public uMediaClient {
public:
	MediaPlayerClient(UMSConnectorBusType bus = UMS_CONNECTOR_PUBLIC_BUS) : uMediaClient(false, bus) {
		startInputMessageThread();
	};

	~MediaPlayerClient() {
		stop();   // exit uMS API event loop
		pthread_join(input_process_thread,NULL);
	};

	int startInputMessageThread() {
		return pthread_create(&input_process_thread,NULL,inputThread,this);
	}

	// Thread to run event loop for subscription and command messages
	static void * inputThread(void *ctx) {
		MediaPlayerClient * self = static_cast<MediaPlayerClient *>(ctx);
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
    try {
        MediaPlayerClient *mp = new MediaPlayerClient(UMS_CONNECTOR_PUBLIC_BUS);
        return reinterpret_cast<uMediaClientHandle *>(mp);
    } catch (const std::runtime_error& e) {
        std::cerr << "Exception caught in uMediaClientCreate: " << e.what() << std::endl;
        return NULL;
    }
}

uMediaClientHandle * uMediaClientCreatePrivate()
{
    try {
        MediaPlayerClient *mp = new MediaPlayerClient(UMS_CONNECTOR_PRIVATE_BUS);
        return reinterpret_cast<uMediaClientHandle *>(mp);
    } catch (const std::runtime_error& e) {
        std::cerr << "Exception caught in uMediaClientCreatePrivate: " << e.what() << std::endl;
        return NULL;
    }
}

// @f uMediaClientLoad
// @brief load requested media
//
int uMediaClientLoad(uMediaClientHandle hdl,
						char * uri,
						AudioStreamClass audioClass,
						char * payload)
{
	MediaPlayerClient * mp = reinterpret_cast<MediaPlayerClient *>(hdl);
	return mp->load(uri,audioClass,payload);
}

int uMediaClientAttach(uMediaClientHandle hdl,
						char * mediaId)
{
	MediaPlayerClient * mp = reinterpret_cast<MediaPlayerClient *>(hdl);
	return mp->attach(mediaId);
}

int uMediaClientUnload(uMediaClientHandle hdl)
{
	MediaPlayerClient * mp = reinterpret_cast<MediaPlayerClient *>(hdl);
	return mp->unload();
}

int uMediaClientPlay(uMediaClientHandle hdl)
{
	MediaPlayerClient * mp = reinterpret_cast<MediaPlayerClient *>(hdl);
	return mp->play();
}

int uMediaClientPause(uMediaClientHandle hdl)
{
	MediaPlayerClient * mp = reinterpret_cast<MediaPlayerClient *>(hdl);
	return mp->pause();
}

int uMediaClientSeek(uMediaClientHandle hdl, long position)
{
	MediaPlayerClient * mp = reinterpret_cast<MediaPlayerClient *>(hdl);
	return mp->seek(position);
}

int uMediaClientDestroy(uMediaClientHandle hdl)
{
	MediaPlayerClient * mp = reinterpret_cast<MediaPlayerClient *>(hdl);
	delete mp;
	return 1;
}
