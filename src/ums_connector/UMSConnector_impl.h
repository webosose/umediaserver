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

#ifndef _UMS_CONNECTOR_IMPL_H
#define _UMS_CONNECTOR_IMPL_H

#include <iostream>
#include <stdexcept>
#include <vector>
#include <glib.h>
#include <sstream>
#include <stdio.h>
#include <cassert>
#include <Logger.h>
#include <luna-service2/lunaservice.h>
#include <string>
#include <pbnjson.hpp>

#include <UMSConnector_common.h>
#include "CallbackManager.h"

#include <functional>
#include <map>
#include <deque>
#include <memory>
#include <list>
#include <deque>
#include <unordered_map>

const int MAX_EVENT_NAME_SIZE = 20;

typedef struct subscriptionsT {
	LSHandle *shandle;
	LSMessageToken stoken;
	std::string scall;
	bool deferred_cancel;
	void* evtHandler;
} subscription_str;

typedef std::function<void()> track_cb_t;

class UMSConnector::UMSConnector_impl {
public:

	explicit UMSConnector_impl(const std::string& name,
			GMainLoop *mainLoop_ = nullptr,
			void * user_data = nullptr,
			UMSConnectorBusType bus_type = UMS_CONNECTOR_PUBLIC_BUS,
			bool use_default_context = false);

	~UMSConnector_impl();
	GMainLoop * getMainLoop() {	return mainLoop_; }

	// implementation of interface
	bool wait();  /* wait for messages/events */
	bool stop();  /* stop main event/message loop */
	bool addEventHandler(std::string event, UMSConnectorEventFunction func,
			const std::string &category = "");  /* add single event handler */
	bool addEventHandler(std::string event, UMSConnectorEventFunction func,
			UMSConnectorBusType bus,
			const std::string &category = "");  /* add single event handler */
	bool addEventHandlers(UMSConnectorEventHandler *mehods);
	const char * getMessageText(UMSConnectorMessage *message);
	const char * getMessageSender(UMSConnectorMessage *message);
	const char * getSenderServiceName(UMSConnectorMessage *message);
	bool sendMessage(const std::string &uri,const std::string &payload,
			UMSConnectorEventFunction cb,void *ctx);
	bool sendSimpleResponse(UMSConnectorHandle * sender, UMSConnectorMessage* message, bool resp);
	bool sendResponse(UMSConnectorHandle *sender,UMSConnectorMessage* message,
						const std::string &key, const std::string &value);
	bool sendResponseObject(UMSConnectorHandle *sender,
			UMSConnectorMessage* message, const std::string &object);

	// route for incoming messages and subscription requests
	void addRoute(const std::string &key, UMSConnectorMessage *message);
	void delRoute(const std::string &key);

	// Subscription methods
	// client side usage
	size_t subscribe(const std::string &uri, const std::string &payload,
					 UMSConnectorEventFunction cb, void *ctx);
	void unsubscribe(const std::string & uri, size_t token);

	// service side usage
	bool addSubscriber(UMSConnectorHandle * subscriber, UMSConnectorMessage *message, const std::string &key);
	bool removeSubscriber(UMSConnectorHandle * subscriber, UMSConnectorMessage *message, const std::string &key);
	bool sendChangeNotificationLongLong(const std::string &name, unsigned long long value);
	bool sendChangeNotificationString(const std::string &name, const std::string &value, const std::string &key);
	bool sendChangeNotificationJsonString(const std::string &json_string, const std::string &key);

	bool addClientWatcher(UMSConnectorHandle* cl, UMSConnectorMessage* message, track_cb_t cb);
	bool delClientWatcher(UMSConnectorHandle* cl, UMSConnectorMessage* message);

	void subscribeServiceReady(const std::string & service_name, track_cb_t && cb);
	void unsubscribeServiceReady(const std::string &service_name);

	// register with ls2 server for connection status updates
	static bool serverStatusUpdate(LSHandle *sh, LSMessage *message, void *ctx);
	bool cancelSendMessage(LSHandle *sh, LSMessage *message, void *ctx);

	// message ref counting operations
	static void refMessage(UMSConnectorMessage * message);
	static void unrefMessage(UMSConnectorMessage * message);

	// idle function for GLib used to deferred-cancel subscriptions
	static gboolean idle_func(gpointer user_data);

	// process deferred cancel subscriptons
	bool processDeferredSubscriptionCancellations();

private:
	std::shared_ptr<uMediaServer::Logger> log;

	std::string service_name;
	std::string subscription_key;
	GMainLoop *mainLoop_;
	union {
		LSHandle *lshandle;
		LSPalmService *palmservice;
	} m_service;
	// false if we have loop shared between several connectors
	bool run_state_altered;

	UMSConnectorBusType m_bus_type;
	LSMessageToken m_token;
	void * user_data;
	static pbnjson::JSchema emptySchema;

	// map of URI's associated with public or private bus routes
	std::unordered_map<std::string, UMSConnectorBusType> bus_routes;

	// need to keep track of luna service methods because
	// LSUnregister doesn't free the method structures
	std::deque<LSMethod *> eventHandlers;

	// Also need to track any subscriptions that the destructor must cleanup
	std::list<subscriptionsT*> m_subscriptions;

	// callback manager
	CallbackManager::ptr_t m_callbackManager;

	UMSConnectorBusType getMessageBusType(UMSConnectorMessage *message);
	LSHandle * getBusHandle(const std::string &key);

	// look for route based on URI(messages) or key(subscriptions)
	UMSConnectorBusType getRoute(const std::string &uri_key);

	// get LS uri from uri
	std::string getLSUri(const std::string & uri);

	// private methods
	bool sendChangeNotification(const std::string &name,
			const std::string &value, const std::string &key = "");

	UMSConnector_impl(UMSConnector_impl const& ) {};  // copy constructor
	void operator=(const UMSConnector_impl&) {};  // assignment constructor

	std::map<std::string, std::unique_ptr<track_cb_t>> watchers;
	static bool _CancelCallback(LSHandle* sh, LSMessage* reply, void* ctx);

	struct cb_info_t {
		track_cb_t cb;
		void * cookie;
	};
	std::map<std::string, cb_info_t> service_status_subscriptions;

	// run/wait protection
	bool stopped_;

	guint idle_task_;
};

#endif  //_UMS_CONNECTOR_IMPL_H
