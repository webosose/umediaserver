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
//

#ifndef _UMS_CONNECTOR_H
#define _UMS_CONNECTOR_H

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <sstream>
#include <stdio.h>
#include <cassert>
#include <glib.h>

#include <UMSConnector_common.h>
#include <Logger.h>

#include <functional>

// must add "<PID>" to base name to allow multiple pipelines
#define PIPELINE_CONNECTION_BASE_ID "com.webos.pipeline."
#define PIPELINE_CONTROLLER_CONNECTION_BASE_ID "com.webos.pipelinectrl."

/*   (as defined in c++11) || (workaround for gcc < 4.7) */
#if (__cplusplus >= 201103 || (__cplusplus == 1 && __GXX_EXPERIMENTAL_CXX0X__))
#define __UMC_USE_CPP_11
typedef std::function<void()> track_cb_t;
#else
struct track_cb_t;
#endif

class UMSConnector {
public:

	// example name = com.gram.module_name
	explicit UMSConnector(const std::string& name,
			GMainLoop *mainLoop_ = NULL,void * user_data = NULL,
			UMSConnectorBusType bus_type = UMS_CONNECTOR_PUBLIC_BUS,
			bool use_default_context = false);

	// interface API to be implemented by UMSConnector_impl
	//
	bool wait();  // wait for messages/events
	bool stop();  // stop main event/message loop
	bool addEventHandler(std::string event, UMSConnectorEventFunction func, const std::string &category ="");  // add single event handler
	bool addEventHandler(std::string event, UMSConnectorEventFunction func, UMSConnectorBusType bus, const std::string &category ="");  // add single event handler
	bool addEventHandlers(UMSConnectorEventHandler *mehods);
	const char * getMessageText(UMSConnectorMessage *message);
	const char * getMessageSender(UMSConnectorMessage *message);
	const char * getSenderServiceName(UMSConnectorMessage *message);
	bool sendMessage(const std::string &uri, const std::string &payload,
			UMSConnectorEventFunction cb, void *ctx);
	bool sendSimpleResponse(UMSConnectorHandle * sender, UMSConnectorMessage* message, bool resp);
	bool sendResponse(UMSConnectorHandle *sender,UMSConnectorMessage* message,
						const std::string &key, const std::string &value);
	bool sendResponseObject(UMSConnectorHandle *sender, UMSConnectorMessage* message,
			const std::string &object);

	// route for incoming messages
	bool addRoute(UMSConnectorMessage* message);

	GMainLoop * getMainLoop();

	// Subscription methods

	// client side: subscribe for subscription messages
	size_t subscribe(const std::string &uri, const std::string &payload,
					 UMSConnectorEventFunction cb, void *ctx);
	void unsubscribe(const std::string & uri, size_t token);

	// server side: add a subscriber
	bool addSubscriber (UMSConnectorHandle * subscriber, UMSConnectorMessage *message, const std::string &key = "");
	bool removeSubscriber (UMSConnectorHandle * subscriber, UMSConnectorMessage *message, const std::string &key);
	bool sendChangeNotificationLongLong(const std::string &name, unsigned long long value);
	bool sendChangeNotificationString(const std::string &name, const std::string &value, const std::string &key = "");
	bool sendChangeNotificationJsonString(const std::string &json_string, const std::string &key = "");

	bool addClientWatcher(UMSConnectorHandle* cl, UMSConnectorMessage* message, track_cb_t cb);
	bool delClientWatcher(UMSConnectorHandle* cl, UMSConnectorMessage* message);

#ifdef __UMC_USE_CPP_11
	void subscribeServiceReady(const std::string & service_name, track_cb_t && cb);
	void unsubscribeServiceReady(const std::string &service_name);
#endif

	// message ref counting operations
	static void refMessage(UMSConnectorMessage * message);
	static void unrefMessage(UMSConnectorMessage * message);

	~UMSConnector();

private:
	class UMSConnector_impl;
	UMSConnector_impl *pImpl;

	uMediaServer::Logger log;
	std::string name;

	static bool handleCmdEvent(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx);

	UMSConnector(UMSConnector const& ) {};  // copy constructor
	void operator=(const UMSConnector&) {};  // assignment constructor
};

#endif  //_UMS_CONNECTOR_H
