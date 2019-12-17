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

//
// Resource Manager API
//

#ifndef __RESOURCE_MANAGER_CLIENT_H
#define __RESOURCE_MANAGER_CLIENT_H

#include <iostream>
#include <stdexcept>
#include <stdio.h>
#include <string>
#include <pthread.h>
#include <errno.h>
#include <stdint.h>
#include <functional>
#include <map>
#include <mutex>
#include <condition_variable>
#include <memory>

#include <pbnjson.hpp>
#include <UMSConnector.h>
#include <uMediaTypes.h>

#define UMEDIASERVER_CONNECTION_ID "com.webos.media"

namespace uMediaServer {

typedef enum { CONNECTION_OPENED, CONNECTION_CLOSED } connection_state_t;

// create static dispatch method to allow object methods to be used as call backs for UMSConnector events
#define RMC_RESPONSE_HANDLER(_class_, _cb_, _member_) \
		static bool _cb_(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx) { \
	_class_ *self = static_cast<_class_ *>(ctx); \
	bool rv = self->_member_(handle, message,ctx);   \
	return rv;  } \
	bool _member_(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx);

class Mutex;

class ResourceManagerClient
{
public :

	// used by external clients
	ResourceManagerClient();

	ResourceManagerClient(const std::string& connection_id);

	virtual ~ ResourceManagerClient();

	bool registerPipeline(std::string type, const std::string& app_id = std::string());

	bool unregisterPipeline();

	bool acquire(const std::string &resources, std::string &resource_response_json);
	bool tryAcquire(const std::string &resources, std::string &resource_response_json);

	bool release(std::string resources);

	bool notifyForeground();
	bool notifyBackground();
	bool notifyActivity();
	bool notifyPipelineStatus(const std::string& pipeline_status);

	bool getDisplayId(const std::string &app_id);

	const char * getConnectionID() { return connection_id.data(); };
	int32_t getDisplayID();

	// @f registerPolicyActionHandler
	// @description : register policy action even handler with Resource Manager
	//
	// @param handler policy action event handler
	//
	template <typename FuncType>
	void registerPolicyActionHandler( FuncType handler ) { policyActionClientHandler = handler; }

private :
	Logger log;
	std::string connection_id;       // returned from ResourceManager open_connection command
	int32_t display_id;
	std::string connection_category;
	connection_state_t connection_state;

	std::string key_source;   // character source for unique id generation

	pthread_cond_t open_condition;
	pthread_mutex_t mutex;
	pthread_t message_process_thread;

	Mutex * api_mutex;

	// wait for remote client to release resources after
	//  selection by policy
	struct acquire_waiter_t {
		typedef std::shared_ptr<acquire_waiter_t> ptr_t;
		acquire_waiter_t() : state(false) {}
		bool state;
		std::string acquire_response;
		std::condition_variable cv;
	};

	std::map<std::string, acquire_waiter_t::ptr_t> acquire_waiters;
	std::mutex acquire_mutex;

	std::mutex display_id_mutex;
	std::condition_variable display_id_cv;
	bool is_valid_display_id = false;

	GMainLoop *main_loop;
	GMainContext * main_context;
	UMSConnector *connector;
	std::string resource_manager_connection_id;

	// arguemnts: action, resources, requestor_type, requestor_name, connection_id
	std::function <bool (const char * action, const char * resources,
			const char * requestor_type, const char * requestor_name,
			const char * connection_id)> policyActionClientHandler;

	void ResourceManagerClientInit();

	bool waitEvent(uint32_t *event,
			uint32_t desired_state,
			pthread_mutex_t * lock,
			pthread_cond_t * condition,
			unsigned int secs);

	bool signalEvent(uint32_t *event,
			uint32_t state,
			pthread_mutex_t * lock,
			pthread_cond_t * condition);

	std::string generateUniqueID();

	bool _acquire(const std::string &resources,
			std::string &resource_response_json,
			bool block=true);

	bool informWaiter(std::string waiter, bool state, std::string response);

	std::string createRetObject(bool returnValue, const std::string& mediaId);

	// open connection event
	RMC_RESPONSE_HANDLER(ResourceManagerClient,openConnectionCallback, openConnectionResponse);

	// policy action event
	RMC_RESPONSE_HANDLER(ResourceManagerClient,policyActionCallback, policyActionResponse);

	// acquire complete event
	RMC_RESPONSE_HANDLER(ResourceManagerClient,acquireCompleteCallback, acquireCompleteResponse);

	// generic command response handler
	RMC_RESPONSE_HANDLER(ResourceManagerClient,commandResponseCallback,commandResponse);

	RMC_RESPONSE_HANDLER(ResourceManagerClient,getDisplayIdResponseCallback,getDisplayIdResponse);

	// @f
	// @brief false subscription handler.
	//   uMS IPC mechanism luna-service2 only properly tracks subscribed clients.
	//
	RMC_RESPONSE_HANDLER(ResourceManagerClient,subscribeResponseCallback,subscribeResponse);

	ResourceManagerClient(ResourceManagerClient const& ) {};  // copy constructor
	void operator=(const ResourceManagerClient&) {};  // assignment constructor

	int startMessageThread() {
		return pthread_create(&message_process_thread, NULL, messageThread, this);
	}

	template <typename FuncType>
	void registerUpdateStatusHandler( FuncType handler ) { updateStatusHandler = handler; }

	template <typename FuncType>
	void registerUpdateResourcesStatusHandler( FuncType handler ) { updateResourcesStatusHandler = handler; }

	void run() { connector->wait(); }
	void stop() { connector->stop(); }

	// Thread to run event loop for subscription and command messages
	static void * messageThread(void *ctx) {
		ResourceManagerClient * self = static_cast<ResourceManagerClient *>(ctx);
		self->run();
		return NULL;
	}

	void subscribe();
	bool getStateData(const std::string& message, std::string& name, pbnjson::JValue &value);

	std::function <bool (const char* status, const char* appType)> updateStatusHandler;
	std::function <bool (const bool available, const char* resources)> updateResourcesStatusHandler;
};

} // namespace uMediaServer

#endif  // __RESOURCE_MANAGER_CLIENT_H
