// Copyright (c) 2008-2019 LG Electronics, Inc.
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

#ifndef UMEDIA_SERVER_H_
#define UMEDIA_SERVER_H_

#include <iostream>
#include <stdexcept>
#include <vector>
#include <sstream>
#include <stdio.h>
#include <cassert>
#include <string>
#include <exception>
#include <signal.h>
#include <execinfo.h>
#include <ucontext.h>
#include <boost/lexical_cast.hpp>  // for lexical_cast<string>(number)
#include <boost/filesystem/path.hpp>
#include <boost/noncopyable.hpp>
#include <functional>
#include <deque>
#include <mutex>
#include <condition_variable>

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cxxabi.h>
#include <sys/time.h>
#include <time.h>
#include <cstdlib> //std::system
#include <mcheck.h>
#include <stdint.h>
#include <glib.h>
#include <memory>

#include <pbnjson.hpp>

#include <Logger.h>
#include <PipelineManager.h>
#include <ResourceManager.h>
#include <UMSConnector.h>
#include <DirectoryWatcher.h>
#include <AcquireQueue.h>
#include <Registry.h>
#include <AppObserver.h>
#include <AppLifeManager.h>

#define UMEDIASERVER_CONNECTION_ID "com.webos.media"

#ifdef UMSCONNECTOR_EVENT_HANDLER
#undef UMSCONNECTOR_EVENT_HANDLER
#endif

#define UMSCONNECTOR_EVENT_HANDLER(_class_, _cb_, _member_) \
		static bool _cb_(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx) { \
	_class_ *self = static_cast<_class_ *>(ctx); \
	bool rv = self->_member_(handle, message,ctx);   \
	return rv;  } \
	bool _member_(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx);

namespace uMediaServer {

struct lsm_window_event_t {
	std::string app_id;
	std::string process_id;
	std::string window_id;
	std::string window_type;

	friend std::ostream & operator << (std::ostream & os, const lsm_window_event_t & e) {
		return os << "app_id      = " << e.app_id << std::endl
				  << "process_id  = " << e.process_id << std::endl
				  << "window_id   = " << e.window_id << std::endl
				  << "window_type = " << e.window_type << std::endl; }
};
typedef std::map<std::string, lsm_window_event_t> lsm_event_map_t;


class ServiceReadyWatcher : public IServiceReadyWatcher {
public:
	ServiceReadyWatcher(UMSConnector * connector) : _connector(connector) {}
	void watch(const std::string & service_name, std::function<void()> && ready_callback) {
		_connector->subscribeServiceReady(service_name, std::move(ready_callback));
	}
	void unwatch(const std::string &service_name) {
		_connector->unsubscribeServiceReady(service_name);
	}
private:
	UMSConnector * _connector;
};

// singleton
class uMediaserver : boost::noncopyable {
public:
	static uMediaserver * instance(const std::string& conf_file = "") {
		static uMediaserver *pInstance;
		if( ! pInstance ) {
			pInstance = new uMediaserver(conf_file);
		}
		return pInstance;
	}

	~uMediaserver();

	void wait() {
		if( connector ) {
			connector->wait();
		}
	}

	// UMSConnector message handlers for Client(application) <-> uMediaserver event messages

	// load/unload pipeline
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,loadCallback,loadCommand);
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,attachCallback,attachCommand);
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,unloadCallback,unloadCommand);

	// pipeline media operation commands
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,playCallback,playCommand);
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,pauseCallback,pauseCommand);
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,seekCallback,seekCommand);
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,stateChangeCallback,stateChangeCommand);
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,unsubscribeCallback,unsubscribeCommand);
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,setDisplayWindowCallback,setDisplayWindowCommand);
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,setPlayRateCallback,setPlayRateCommand);
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,setVolumeCallback,setVolumeCommand);
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,getDisplayIdCallback,getDisplayIdCommand);

	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,startCameraRecordCallback,startCameraRecordCommand);
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,stopCameraRecordCallback,stopCameraRecordCommand);
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,takeCameraSnapshotCallback,takeCameraSnapshotCommand);

	// get foreground app info
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver, getForegroundAppInfoCallback, getForegroundAppInfoCommand);
	// pipeline state query API
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver, getPipelineStateCallback, getPipelineStateCommand);
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver, getActivePipelinesCallback, getActivePipelinesCommand);

	// Resource Manager API
	// TODO add API when dynamic resource manager task is complete GF-1507
	// set debug levels of various sub modules of uMS
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,registerPipelineCallback,registerPipelineCommand);
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,unregisterPipelineCallback,unregisterPipelineCommand);

	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,acquireCallback,acquireCommand);
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,reacquireCallback,reacquireCommand);
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,releaseCallback,releaseCommand);

	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,tryAcquireCallback,tryAcquireCommand);
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,notifyForegroundCallback,notifyForegroundCommand);
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,notifyBackgroundCallback,notifyBackgroundCommand);
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,notifyActivityCallback,notifyActivityCommand);
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,notifyPipelineStatusCallback,notifyPipelineStatusCommand);
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,trackAppProcessesCallback,trackAppProcessesCommand);

	// MDC API
	UMSCONNECTOR_EVENT_HANDLER(uMediaserver,registerMediaCallback,registerMediaCommand);

private:
	Logger log;
	UMSConnector * connector;
	ServiceReadyWatcher * sr_watcher;
	ResourceManager * rm;
	PipelineManager * pm;
	AcquireQueue acquire_queue;
	std::string bus_route_key_;
	AppObserver* app_observer_;
	AppLifeManager* app_life_manager_;

	static bool policyResponseCallback(UMSConnectorHandle * handle,
			UMSConnectorMessage * message, void * ctx);

	UMSConnectorHandle* senderForSetMaster;
	UMSConnectorMessage* messageForSetMaster;

	void initAcquireQueue();

	uMediaserver(const std::string& conf_file);

	std::string createRetObject(bool returnValue,
			const std::string& mediaId,
			const int& errorCode = 0,
			const std::string& errorText = "No Error");

	std::string createRetObject(bool returnValue,
			const std::string& mediaId,
			const std::string& returnString);

	DirectoryWatcher<std::function<void()>>::Ptr dynamic_config_dir_watcher_;
	std::list<std::string> dynamic_pipeline_types_;
	bool readConfigFile(const std::string& conf_file, libconfig::Config& cfg);
	void readDynamicPipelineConfigs();
	void removeDynamicPipelines();
	std::map<std::string, UMSConnectorMessage *> connection_message_map_;
	std::function<void(std::string)> unload_functor_;
	std::function<void(std::string)> unregister_functor_;

	ResourceManager::callback_t acquire_callback_;
};
} // namespace uMediaServer

#endif  // UMEDIA_SERVER_H_

