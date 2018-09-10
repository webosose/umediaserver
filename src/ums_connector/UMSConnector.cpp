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

#include <iostream>
#include <stdexcept>
#include <vector>
#include <sstream>
#include <stdio.h>
#include <cassert>
#include <string>
#include <algorithm>
#include <iterator>

#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <deque>
#include <list>
#include <pbnjson.hpp>

#include <Logger_macro.h>
#include "UMSConnector.h"
#include <UMSConnector_impl.h>
#include <luna-service2/lunaservice.h>

using namespace std;
using namespace pbnjson;
using namespace uMediaServer;

namespace {
struct ReadyInfo {
	std::string cmd;
	std::string payload;
	bool isReady(std::string& e) { return e == "stateChange"; }
};

std::map<UMSConnector*,ReadyInfo> readyinfo;
}

/**
 * @f UMSConnector
 *   constructor
 *   @param name service name.  eg: com.webos.media.
 *   @param mainLoop_ optional. External/existing GMainLoop can be passed to be used by
 *                     UMSConnector.  If not provided a GMainLoop will be created automatically
 *   @param user_data user data will be passed to registered call back functions and can be
 *                         used to redirect static class members to actual objects
 *
 */
UMSConnector::UMSConnector(const string& name,
		GMainLoop *mainLoop_,
		void * user_data,
		UMSConnectorBusType bus_type,
		bool use_default_context,
		bool app_permission)

	: log(UMS_LOG_CONTEXT_CONNECTOR), name(name) {
	LOG_TRACE(log, "UMSConnector interface initialized");

	pImpl = new UMSConnector_impl(name, mainLoop_, user_data, use_default_context, app_permission);
}

UMSConnector::~UMSConnector() {
	LOG_TRACE(log, "called");

	delete pImpl;
	pImpl = NULL;
}

/**
 * @f wait
 *    run message processing event loop and wait for messages
 */
bool UMSConnector::wait()
{
	if( pImpl ) {
		return pImpl->wait();
	}
	return false;
}

/**
 * @f stop
 *   stop message processing event loop and exit wait method
 */
bool UMSConnector::stop()
{
	if( pImpl ) {
		return pImpl->stop();
	}
	return false;
}

/**
 * @f addEventHandler
 *   add method to handle incoming events/messages
 */
bool UMSConnector::addEventHandler(string event,UMSConnectorEventFunction func, const string &category)
{
	if (pImpl && pImpl->addEventHandler(event, func, category)) {
		auto e = readyinfo.find(this);
		if (e != readyinfo.end() && e->second.isReady(event)) {
			pImpl->sendMessage(e->second.cmd, e->second.payload, handleCmdEvent, NULL);
			readyinfo.erase(e);
		}
		return true;
	}
	return false;
}

/**
 * @f addEventHandler
 *   add method to handle incoming events/messages
 */
bool UMSConnector::addEventHandler(string event,UMSConnectorEventFunction func, UMSConnectorBusType bus, const string &category)
{
	if( pImpl ) {
		return pImpl->addEventHandler(event, func, bus, category);
	}
	return false;
}

/**
 * @f addEventHandlers
 *   add multiple methods to handle incoming events/messages
 */
bool UMSConnector::addEventHandlers(UMSConnectorEventHandler *methods)
{
	if (pImpl && pImpl->addEventHandlers(methods)) {
		auto e = readyinfo.find(this);
		if (e != readyinfo.end()) {
			pImpl->sendMessage(e->second.cmd, e->second.payload, handleCmdEvent, NULL);
			readyinfo.erase(e);
		}
		return true;
	}
	return false;
}

/**
 * @f getMessageText
 *   get message text from incoming event/message
 */
const char * UMSConnector::getMessageText(UMSConnectorMessage *message)
{
	if( pImpl ) {
		return pImpl->getMessageText(message);
	}
	return NULL;
}

/**
 * @f getMessageSender
 *   return UID of sender
 */
const char * UMSConnector::getMessageSender(UMSConnectorMessage *message)
{
	if( pImpl ) {
		return pImpl->getMessageSender(message);
	}
	return NULL;
}

/**
 * @f getSenderServiceName
 *   return service name of sender
 */
const char * UMSConnector::getSenderServiceName(UMSConnectorMessage *message)
{
	if( pImpl ) {
		return pImpl->getSenderServiceName(message);
	}
	return NULL;
}

/**
 * @f sendSimpleResponse
 *   return true/false response to incoming event/message
 */
bool UMSConnector::sendSimpleResponse(UMSConnectorHandle *sender, UMSConnectorMessage* message, bool resp)
{
	if( pImpl ) {
		return pImpl->sendSimpleResponse(sender,message,resp);
	}
	return false;
}

/**
 * @f sendResponse
 * send simple key=value json formatted response string. e.g.: "{"key":"value"}
 *
 */
bool UMSConnector::sendResponse(UMSConnectorHandle *sender, UMSConnectorMessage* message,
								const std::string &key, const std::string &value)
{
	if( pImpl ) {
		return pImpl->sendResponse(sender,message,key,value);
	}
	return false;
}


/**
 * @f sendResponseObject
 * send json formatted response object
 *
 */
bool UMSConnector::sendResponseObject(UMSConnectorHandle *sender, UMSConnectorMessage* message,
								const std::string &object)
{
	if( pImpl ) {
		return pImpl->sendResponseObject(sender, message, object);
	}
	return false;
}

/**
 * @f sendMessage
 *   send message to service
 *  @param uri service name + command.  eg: com.webos.media/play
 *  @param payload command payload in JSON format
 *
 *  example code:
 *  @code
 *
 * void send_my_message(const char * message) {
 *	JSchemaFragment inputSchema("{}");
 *	JValue args = pbnjson::Array();   // append all values into JSON array
 *	JValue payload = Object();        // push array of values into "arg" element
 *
 *	payload.put("args", args);
 *
 *	JGenerator serializer(NULL);   // serialize into string
 *	string payload_serialized;
 *
 *	if (!serializer.toString(message, inputSchema, payload_serialized)) {
 *		// handle error (e.g. generated json would fail to validate & or some other error occured)
 *		printf("(%s:%d) : ERROR: failed.serializer.toString()\n",__PRETTY_FUNCTION__,__LINE__);
 *		return;
 *	}
 *
 *	string cmd = m_umediaserver_connection_id + "/stateChange";
 *	connection->sendMessage(cmd, payload_serialized, stateChangeCallback, (void*)this, true);
 * }
 * @endcode
 *
 */
bool UMSConnector::sendMessage(const string &uri,const string &payload,
								UMSConnectorEventFunction cb,void *ctx)
{
	if( pImpl ) {
		return pImpl->sendMessage(uri,payload,cb,ctx);
	}
	return false;
}

/**
 * @f subscribe
 *  client: Subscribe to subscription message with a service
 *  @param uri service
 *  @param cb call back function for subsequent subscription messages from service
 *  @param ctx context passed back to caller in call back function
 *
 *  @returns subscription token or 0 in case of error
 *
 *  example code:
 *  @code
 *
 *  // Note: "stateChange" is the subscription command provided by uMediaServer.
 *  //   This value will change for each service's specific subscription command.
 *
 *  string cmd = "com.webos.media/stateChange";
 *  size_t token = connector->subscribe(cmd,subcriptionMessageHandler,(void*)classObjectPtr);
 *  @endcode
 */
size_t UMSConnector::subscribe(const string &uri, const std::string &payload,
							   UMSConnectorEventFunction cb,void *ctx)
{
	if( pImpl ) {
		return pImpl->subscribe(uri,payload,cb,ctx);
	}
	return 0;
}

/**
 * @f unsubscribe
 *  client: Cancel previous subscription
 *  @param uri call uri
 *  @param size_t subscription token
 *
 *  example code:
 *  @code
 *
 *  // Note: "stateChange" is the subscription command provided by uMediaServer.
 *  //   This value will change for each service's specific subscription command.
 *
 *  string call = "com.webos.media/stateChange";
 *  connector->unsubscribe(call, token);
 *  @endcode
 */
void UMSConnector::unsubscribe(const string &uri, size_t token)
{
	if( pImpl ) {
		pImpl->unsubscribe(uri, token);
	}
}

/**
 * @f addSubscriber
 * service : Add subscriber for subscription messages
 *
 */
bool UMSConnector::addSubscriber (UMSConnectorHandle *subscriber, UMSConnectorMessage* message, const string &key)
{
	if( pImpl ) {
		return pImpl->addSubscriber(subscriber,message,key);
	}
	return false;
}

/**
 * @f removeSubscriber
 * service : Add subscriber for subscription messages
 *
 */
bool UMSConnector::removeSubscriber (UMSConnectorHandle *subscriber, UMSConnectorMessage* message, const string &key)
{
	if( pImpl ) {
		return pImpl->removeSubscriber(subscriber,message,key);
	}
	return false;
}

/**
 * @f sendChangeNotificationString
 * service : Send string subscription message to subscribed clients
 *
 */
bool UMSConnector::sendChangeNotificationString(const string &name, const string &value, const string &key)
{
	if( pImpl ) {
		return pImpl->sendChangeNotificationString(name,value,key);
	}
	return false;
}

/**
 * @f sendChangeNotificationLongLong
 * service : Send long long subscription message to subscribed clients
 *
 */
bool UMSConnector::sendChangeNotificationLongLong(const string &name, unsigned long long value)
{
	if( pImpl ) {
		return pImpl->sendChangeNotificationLongLong(name,value);
	}
	return false;
}

/**
 * @f sendChangeNotificationJsonString
 * service : Send formatted JSON string to subscribed clients
 *
 */
bool UMSConnector::sendChangeNotificationJsonString(const string &json_string,
		const string &key)
{
	if( pImpl ) {
		return pImpl->sendChangeNotificationJsonString(json_string, key);
	}
	return false;
}

/**
 * @f addClientWatcher
 * Add a callback which will be called on client disconnect, unsubscribe or on
 * attempt to register another watcher for same client
 */
bool UMSConnector::addClientWatcher (UMSConnectorHandle* cl,
		UMSConnectorMessage* message, track_cb_t cb)
{
	if (pImpl) {
		return pImpl->addClientWatcher(cl, message, std::forward<track_cb_t>(cb));
	}
	return false;
}

/**
 * @f delClientWatcher
 * Remove watcher and associated callback
 */
bool UMSConnector::delClientWatcher (UMSConnectorHandle* cl,
		UMSConnectorMessage* message)
{
	if (pImpl) {
		return pImpl->delClientWatcher(cl, message);
	}
	return false;
}

/**
 * @f subscribeServiceReady
 * Subscribe on notification when some service appears on the bus
 */
void UMSConnector::subscribeServiceReady(const std::string & service_name, track_cb_t && cb) {
	if( pImpl ) {
		pImpl->subscribeServiceReady(service_name, std::move(cb));
	}
}

/**
 * @f unsubscribeServiceReady
 * Unsubscribe to the notification when some service appears on the bus
 */
void UMSConnector::unsubscribeServiceReady(const std::string &service_name) {
	if (pImpl) {
		pImpl->unsubscribeServiceReady(service_name);
	}
}

/**
 * @f getMainLoop
 * return GMainLoop event loop to allow clients to add additional services or events
 *
 */
GMainLoop * UMSConnector::getMainLoop()
{
	if( pImpl ) {
		return pImpl->getMainLoop();
	}
	return NULL;
}

/**
 * @f handleCmdEvent if sending command
 */
bool UMSConnector::handleCmdEvent(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx)
{
	return true;
}


// message ref counting operations
void UMSConnector::refMessage(UMSConnectorMessage * message) {
	UMSConnector_impl::refMessage(message);
}

void UMSConnector::unrefMessage(UMSConnectorMessage * message) {
	UMSConnector_impl::unrefMessage(message);
}
