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

#include <functional>
#include <thread>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>
#include <Logger_macro.h>
#include <GenerateUniqueID.h>
#include <ResourceManagerClient.h>

using namespace uMediaServer;
using namespace pbnjson;
using namespace std;

namespace uMediaServer {
class Mutex : public std::mutex {};
typedef std::lock_guard<std::mutex> Lock;
}

#define RESOURCE_MANAGER_CLIENT_CONNECTION_BASE_ID "com.webos.rm.client."
#define RESOURCE_MANAGER_CONNECTION_ID "com.webos.media"
#define RESOURCE_MANAGER_CLIENT_CONNECTION_ID_LENGTH 10

static const int ACQUIRE_WAIT_TIMEOUT = 8;

#define RETURN_IF(exp,rv,msgid,format,args...) \
		{ if(exp) { \
			LOG_ERROR(log,msgid,format, ##args); \
			return rv; \
		} \
		}

namespace {
Logger log(UMS_LOG_CONTEXT_CLIENT);
}

static string intToString(int number)
{
	stringstream ss;   //create a stringstream
	ss << number;      //add number to the stream
	return ss.str();   //return a string with the contents of the stream
}

//
// ---- PUBLIC
//

//->Start of API documentation comment block
/**
 * @ResourceManagerClient
 * @function
 * @name ResourceManagerClient
 * @description Create Resource Manager client for external clients.
 *    Only clients NOT spawned by uMediaServer should use this interface.

 * @param {string} connection_id_ connection ID of managed media pipelines
 * @returns {ResourceManagerClient} returns ResourceManager object
 */
//->End of API documentation comment block

ResourceManagerClient::ResourceManagerClient() :
				resource_manager_connection_id(RESOURCE_MANAGER_CONNECTION_ID),
				connection_state(CONNECTION_CLOSED),
				log(UMS_LOG_CONTEXT_RESOURCE_MANAGER_CLIENT),
				api_mutex(new Mutex)
{
	ResourceManagerClientInit();
}

//->Start of API documentation comment block
/**
 * @ResourceManagerClient
 * @function
 * @name ResourceManagerClient
 * @description Create Resource Manager client for managed media clients.
 *    Only clients spawned by uMediaServer should use this interface.

 * @param {string} connection_id_ connection ID of managed media pipelines
 * @returns {ResourceManagerClient} returns ResourceManager object
 */
//->End of API documentation comment block

ResourceManagerClient::ResourceManagerClient(const string& connection_id_) :
				resource_manager_connection_id(RESOURCE_MANAGER_CONNECTION_ID),
				connection_state(CONNECTION_CLOSED),
				log(UMS_LOG_CONTEXT_RESOURCE_MANAGER_CLIENT),
				api_mutex(new Mutex)
{

	ResourceManagerClientInit();

	connection_state = CONNECTION_OPENED;
	connection_id = connection_id_;
	LOG_DEBUG(log, "managed client. connection_id=%s",
			connection_id.c_str());
}

void ResourceManagerClient::ResourceManagerClientInit()
{
	string uid = GenerateUniqueID()();

	log.setUniqueId(uid);
	string service_name = RESOURCE_MANAGER_CLIENT_CONNECTION_BASE_ID + uid;

	main_context = g_main_context_new();
	main_loop = g_main_loop_new(main_context, FALSE);

	try {
		connector = new UMSConnector(service_name,main_loop,
				static_cast<void*>(this), UMS_CONNECTOR_PRIVATE_BUS);
	} catch (const std::runtime_error & e) {
		LOG_ERROR_EX(log, MSGERR_UMC_CREATE, __KV({{KVP_ERROR, e.what()}}),
				"Failed to instantiate a UMSConnector object");
		throw;
	}

	// handle policy event
	connector->addEventHandler("policyAction",policyActionCallback);

	// acquire complete
	connector->addEventHandler("acquireComplete",acquireCompleteCallback);

	pthread_cond_init(&open_condition,NULL);
	pthread_mutex_init(&mutex,NULL);

	connection_category = "";  // TODO break out to "rm" category

	startMessageThread();
}

ResourceManagerClient::~ResourceManagerClient()
{
	pthread_cond_destroy(&open_condition);
	pthread_mutex_destroy(&mutex);
	delete api_mutex;
	delete connector;
	g_main_context_unref(main_context);
	g_main_loop_unref(main_loop);

	pthread_join(message_process_thread, NULL);
}

bool ResourceManagerClient::subscribeResponse(UMSConnectorHandle* handle,
		UMSConnectorMessage* message, void* ctx)
{
	std::string msg = connector->getMessageText(message);

	pbnjson::JDomParser parser;
	pbnjson::JSchemaFragment input_schema("{}");

	RETURN_IF( !parser.parse(msg, input_schema), false,
			MSGERR_JSON_PARSE, "JDomParse. input=%s",msg.c_str());

	pbnjson::JValue parsed = parser.getDom();

	if (parsed.hasKey("subscribed") || parsed.hasKey("returnValue")) {
		if (!parsed["subscribed"].asBool() || !parsed["returnValue"].asBool()) {
			return false;
		}

		if (parsed.hasKey("updateStatus") && updateStatusHandler) {
			string status = parsed["updateStatus"]["status"].asString();
			string appType = parsed["updateStatus"]["appType"].asString();
			updateStatusHandler(status.c_str(), appType.c_str());
		} else if (parsed.hasKey("updateResourcesStatus") && updateResourcesStatusHandler) {
			updateResourcesStatusHandler(parsed["updateResourcesStatus"]["available"].asBool(),
										parsed["updateResourcesStatus"]["resources"].asString().c_str());
		}
		return true;
	}

	pbnjson::JValue value;
	std::string name;
	bool rv = getStateData(msg,name,value);

	if (name == "updateStatus") {
		if (updateStatusHandler) {
			string status = parsed["updateStatus"]["status"].asString();
			string appType = parsed["updateStatus"]["appType"].asString();
			updateStatusHandler(status.c_str(), appType.c_str());
		}
	} else if (name == "updateResourcesStatus") {
		if (updateResourcesStatusHandler) {
			updateResourcesStatusHandler(value["available"].asBool(), value["resources"].asString().c_str());
		}
	}

	return true;
}

// @f subscribe
// @brief subscribe to uMediaServer state change events
//
void ResourceManagerClient::subscribe()
{
	pbnjson::JValue args = pbnjson::Object();

	args.put("mediaId",connection_id);

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failure to serializer.toString()");
		return;
	}

	string cmd = resource_manager_connection_id + "/subscribe";
	connector->subscribe(cmd, payload_serialized, subscribeResponseCallback, (void*)this);
}

// ---------------------------------
// ---  ResourceManagerClient API

//->Start of API documentation comment block
/**
 * @ResourceManagerClient
 * @function
 * @name registerPipeline
 * @description Register pipeline with Resource Manager

 * @param {string} type pipeline type as defined in uMediaServer
 *                     configuration file
 * @returns {bool} true/false succeed or failure.
 */
//->End of API documentation comment block

bool ResourceManagerClient::registerPipeline(std::string type, const std::string& app_id)
{
	Lock l(*api_mutex);

	RETURN_IF(connection_state == CONNECTION_OPENED,
			false, MSGERR_CONN_OPEN, "Connection already open.")

	pbnjson::JValue args = pbnjson::Object();

	args.put("type", type);
	args.put("appId", app_id);

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failure to serializer.toString()");
		return false;
	}

	string cmd = resource_manager_connection_id + connection_category + "/registerPipeline";
	connector->sendMessage(cmd, payload_serialized, openConnectionCallback, (void*)this);

	// wait for connection_id response from Resource Manager
	LOG_DEBUG(log, "Opening connection.  Wait for connection_id response");

	bool rv = waitEvent((uint32_t*)&connection_state, CONNECTION_OPENED,
			&mutex,	&open_condition, 30);

	if( rv == false ) {
		LOG_ERROR(log, MSGERR_CONN_TIMEOUT, "open connection timed out.");
		return false;
	}

	subscribe();  // subscribe to uMS/RM but only to be tracked as a client

	LOG_DEBUG(log, "Connection_opened.");

	return true;
}

//->Start of API documentation comment block
/**
 * @ResourceManagerClient
 * @function
 * @name unregisterPipeline
 * @description un Register pipeline with Resource Manager
 * @returns {bool} true/false succeed or failure.
 */
//->End of API documentation comment block

bool ResourceManagerClient::unregisterPipeline()
{
	Lock l(*api_mutex);

	RETURN_IF(connection_state == CONNECTION_CLOSED, false, MSGERR_CONN_CLOSED,
			"Connection closed. Call registerPipeline().")

	pbnjson::JValue args = pbnjson::Object();

	args.put("connectionId",connection_id);	 // {"connectionId":<connection_id>}

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failure to serializer.toString()");
		return false;
	}

	LOG_DEBUG(log, "close_connection(%s)", connection_id.c_str());

	string cmd = resource_manager_connection_id + connection_category + "/unregisterPipeline";
	connector->sendMessage(cmd, payload_serialized, commandResponseCallback, (void*)this);

	connection_state = CONNECTION_CLOSED;

	return true;
}

//->Start of API documentation comment block
/**
 * @ResourceManagerClient
 * @function
 * @name acquire
 * @description : acquire media HW resources
 * @param {string} resource_request IN list of resources required (JSON)
 *   example request.
 *   [
 *     {
 *        "resource":"VDEC",
 *        "qty":1,
 *        "attribute":"display0",
 *        "index":1
 *     },
 *     {
 *        "resource":"ADEC",
 *        "qty":1
 *     }
 *   ]
 *
 *
 * @param {string} resource request response (JSON)
 *   example response.
 *   [
 *     {
 *        "resource":"VDEC",
 *        "qty":1,
 *        "attribute":"display0",
 *        "index":1
 *     },
 *     {
 *        "resource":"ADEC",
 *        "qty":1,
 *        "index" : 0
 *     }
 *   ]
 *
 * @returns {bool} true/false succeed or failure.
 */
//->End of API documentation comment block

bool ResourceManagerClient::acquire(const std::string &resource_request_json,
		std::string &resource_response_json)
{
	return _acquire(resource_request_json, resource_response_json);
}

bool ResourceManagerClient::tryAcquire(const std::string &resource_request_json,
		std::string &resource_response_json)
{
	return _acquire(resource_request_json, resource_response_json, false);
}

bool ResourceManagerClient::_acquire(const std::string &resource_request_json,
		std::string &resource_response_json, bool block)
{
	Lock l(*api_mutex);

	RETURN_IF(connection_state == CONNECTION_CLOSED, false, MSGERR_CONN_CLOSED, "Connection closed.")

	pbnjson::JValue args = pbnjson::Object();

	args.put("connectionId",connection_id);
	args.put("resources",resource_request_json);

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failure to serializer.toString()");
		return false;
	}

	string acquire_method;
	if ( block ) {
		acquire_method = "/acquire";
	}
	else {
		acquire_method = "/tryAcquire";
	}

	acquire_waiter_t::ptr_t waiter(new acquire_waiter_t);
	acquire_waiters[connection_id] = waiter;

	string cmd = resource_manager_connection_id + connection_category + acquire_method;

	std::unique_lock<std::mutex> outer_lock(acquire_mutex);
	connector->sendMessage(cmd, payload_serialized, commandResponseCallback, (void*)this);

	// create condition variable.  add to list of waiters. wait.
	LOG_INFO(log, MSGNFO_ACQUIRE_REQUEST, "WAITING: acquire resources. %s (%s)",
			connection_id.c_str(),resource_request_json.c_str());

	if (waiter->cv.wait_for(outer_lock, std::chrono::seconds(ACQUIRE_WAIT_TIMEOUT)) == std::cv_status::timeout) {
		LOG_ERROR(log, MSGERR_UNLOCK_TIMEOUT,
				"TIMEOUT: acquire_waiter waited notification for [%d]sec",
				ACQUIRE_WAIT_TIMEOUT);
		return false;
	}

	// TODO: looks like we can avoid lookup by using waiter
	// but in this case we should guarantee that we not going
	// to remove it from map within response or completion handler
	auto it = acquire_waiters.find(connection_id);
	if(it == acquire_waiters.end()) {
		LOG_CRITICAL(log,MSGERR_NO_CONN_ID, "%s. cannot find connection_id in acquire_waiters map.",
				static_cast<const char*>(connection_id.c_str()));
		return false;
	}

	const auto & waiter_response = *(it->second);
	LOG_INFO(log, MSGNFO_ACQUIRE_COMPLETED, "acquireComplete response: state=%s, resources = %s",
			waiter_response.state ? "true" : "false",
					waiter_response.acquire_response.c_str());

	resource_response_json = waiter_response.acquire_response;
	acquire_waiters.erase(connection_id);  // remove waiting request

	return waiter_response.state;
}

//->Start of API documentation comment block
/**
 * @ResourceManagerClient
 * @function
 * @name release
 * @description : release media HW resources
 * @param {string} resource_request IN list of resources required (JSON)
 *   example request.
 *   [
 *     {
 *        "resource":"VDEC",
 *        "qty":1,
 *        "attribute":"display0",
 *        "index":1
 *     },
 *     {
 *        "resource":"ADEC",
 *        "qty":1
 *     }
 *   ]
 *
 *
 * @returns {bool} true/false succeed or failure.
 */
//->End of API documentation comment block

bool ResourceManagerClient::release(std::string resources)
{
	Lock l(*api_mutex);

	RETURN_IF(connection_state == CONNECTION_CLOSED, false, MSGERR_CONN_CLOSED, "Connection closed.")

	pbnjson::JValue args = pbnjson::Object();

	args.put("connectionId",connection_id);
	args.put("resources",resources);

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failure to serializer.toString()");
		return false;
	}

	string cmd = resource_manager_connection_id + connection_category + "/release";
	connector->sendMessage(cmd, payload_serialized, commandResponseCallback, (void*)this);

	return true;
}

//->Start of API documentation comment block
/**
 * @ResourceManagerClient
 * @function
 * @name notifyForeground
 * @description : notify resource manager to avoid selecting due to client
 *   while in foreground
 */
//->End of API documentation comment block
bool ResourceManagerClient::notifyForeground()
{
	Lock l(*api_mutex);

	RETURN_IF(connection_state == CONNECTION_CLOSED, false, MSGERR_CONN_CLOSED, "Connection closed.")

	pbnjson::JValue args = pbnjson::Object();

	args.put("connectionId",connection_id);

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "json object serialization failed");
		return false;
	}

	string cmd = resource_manager_connection_id + connection_category + "/notifyForeground";
	connector->sendMessage(cmd, payload_serialized, commandResponseCallback, (void*)this);

	return true;
}

//->Start of API documentation comment block
/**
 * @ResourceManagerClient
 * @function
 * @name notifyBackground
 * @description : notify resource manager to allow selection
 */
//->End of API documentation comment block
bool ResourceManagerClient::notifyBackground()
{
	Lock l(*api_mutex);

	RETURN_IF(connection_state == CONNECTION_CLOSED, false, MSGERR_CONN_CLOSED, "Connection closed.")

	pbnjson::JValue args = pbnjson::Object();

	args.put("connectionId",connection_id);

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "json object serialization failed");
		return false;
	}

	string cmd = resource_manager_connection_id + connection_category + "/notifyBackground";
	connector->sendMessage(cmd, payload_serialized, commandResponseCallback, (void*)this);

	return true;
}

//->Start of API documentation comment block
/**
 * @ResourceManagerClient
 * @function
 * @name notifyActivity
 * @description : notify resource manager activity time stamp
 */
//->End of API documentation comment block
bool ResourceManagerClient::notifyActivity()
{
	Lock l(*api_mutex);

	RETURN_IF(connection_state == CONNECTION_CLOSED, false, MSGERR_CONN_CLOSED, "Connection closed.")

	pbnjson::JValue args = pbnjson::Object();

	args.put("connectionId",connection_id);

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "json object serialization failed");
		return false;
	}

	string cmd = resource_manager_connection_id + connection_category + "/notifyActivity";
	connector->sendMessage(cmd, payload_serialized, commandResponseCallback, (void*)this);

	return true;
}

bool ResourceManagerClient::notifyPipelineStatus(const std::string& pipeline_status)
{
	Lock l(*api_mutex);

	RETURN_IF(connection_state == CONNECTION_CLOSED, false, MSGERR_CONN_CLOSED, "Connection closed.")
	pbnjson::JValue args = pbnjson::Object();

	args.put("connectionId", connection_id);
	args.put("pipelineStatus", pipeline_status);
	args.put("pid", static_cast<int32_t>(getpid()));

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, pbnjson::JSchema::AllSchema(), payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "json object serialization failed");
		return false;
	}

	string cmd = resource_manager_connection_id + connection_category + "/notifyPipelineStatus";
	connector->sendMessage(cmd, payload_serialized, commandResponseCallback, (void*)this);

	return true;
}

bool ResourceManagerClient::getDisplayId(const std::string &app_id)
{
	Lock l(*api_mutex);

	RETURN_IF(connection_state == CONNECTION_CLOSED, false, MSGERR_CONN_CLOSED, "Connection closed.")

	JSchemaFragment inputSchema("{}");
	pbnjson::JValue args = pbnjson::Object();

	args.put("appId", app_id);

	JGenerator serializer(NULL);   // serialize into string
	string payload_serialized;

	if (!serializer.toString(args, inputSchema, payload_serialized)) {
		LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "json object serialization failed");
		return false;
	}

	string cmd = resource_manager_connection_id + connection_category + "/getDisplayId";
	connector->sendMessage(cmd, payload_serialized, getDisplayIdResponseCallback, (void*)this);

	return true;
}



//
// ---- PRIVATE
//

bool ResourceManagerClient::openConnectionResponse(UMSConnectorHandle* handle,
		UMSConnectorMessage* message, void* ctx)
{
	JDomParser parser;

	string cmd = connector->getMessageText(message);

	if (!parser.parse(cmd, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(log, MSGERR_JSON_PARSE, "JDomParser.parse. raw=%s ",cmd.c_str());
		return false;
	}

	LOG_DEBUG(log, "open connection event received. response=%s",cmd.c_str());

	JValue parsed = parser.getDom();
	RETURN_IF(!parsed.hasKey("connectionId"), false, MSGERR_NO_CONN_ID,
			"connection to resource manager failed.");
	connection_id = parsed["connectionId"].asString();

	LOG_DEBUG(log, "openConnection complete. connection_id=%s.", connection_id.c_str());

	// signal waiter connection obtained
	signalEvent((uint32_t*)&connection_state,
			CONNECTION_OPENED, &mutex,
			&open_condition);

	return true;
}

bool ResourceManagerClient::policyActionResponse(UMSConnectorHandle* handle,
		UMSConnectorMessage* message,
		void* ctx)
{
	JDomParser parser;

	string cmd = connector->getMessageText(message);

	if (!parser.parse(cmd, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(log, MSGERR_JSON_PARSE, "JDomParse. input=%s",cmd.c_str());
		return false;
	}

	JValue state = parser.getDom();

	if ( !(*state.begin()).first.isString() ) {
		LOG_ERROR(log, MSGERR_JSON_SCHEMA, "error. policyAction != string");
		return false;
	}

	string name = (*state.begin()).first.asString();
	JValue value = state[name];

	RETURN_IF(name!="policyAction", false, MSGERR_NO_POL_ACT,
			"policy action must be specified");

	string action = value["action"].asString();
	string pa_connection_id = value["connectionId"].asString();
	string requestor_type = value["requestor_type"].asString();
	string requestor_name = value["requestor_name"].asString();
	string resources = value["resources"].asString();  // JSON array

	LOG_DEBUG(log, "policy action event received. "
			"action=%s, resources=%s, connection_id=%s",
			action.c_str(), resources.c_str(), pa_connection_id.c_str());

	// call client event handler
	bool accepted = policyActionClientHandler(action.c_str(),
			resources.c_str(), requestor_type.c_str(),
			requestor_name.c_str(), pa_connection_id.c_str());

	string response = createRetObject(accepted, pa_connection_id);
	connector->sendResponseObject(handle, message, response);

	return accepted;
}

// @f createRetObject
// @brief create return object
//
//
string ResourceManagerClient::createRetObject(bool returnValue, const string& mediaId)
{
	JValue retObject = Object();
	JGenerator serializer(NULL);
	string retJsonString;

	retObject.put("returnValue", returnValue);
	retObject.put("mediaId", mediaId);
	serializer.toString(retObject,  pbnjson::JSchema::AllSchema(), retJsonString);

	LOG_TRACE(log, "createRetObject retObjectString =  %s", retJsonString.c_str());
	return retJsonString;
}

// @f acquireCompleteResponse
// @brief handle acquire responses
//
bool ResourceManagerClient::acquireCompleteResponse(UMSConnectorHandle* handle,
		UMSConnectorMessage* message,
		void* ctx)
{
	JDomParser parser;

	string cmd = connector->getMessageText(message);

	RETURN_IF( !parser.parse(cmd, pbnjson::JSchema::AllSchema()), false,
			MSGERR_JSON_PARSE, "JDomParse. input=%s",cmd.c_str());

	JValue acquire_response = parser.getDom();

	RETURN_IF( ! acquire_response.hasKey("state"), false,
			MSGERR_JSON_SCHEMA,"no state key in acquireComplete response.");

	bool state = acquire_response["state"].asBool();
	string connection_id = acquire_response["connectionId"].asString();

	LOG_DEBUG(log, "acquireCompleteResponse state = %d",state);

	informWaiter(connection_id,state,cmd);
	return true;
}

// @f commandResponse
// @brief handle API errors for commands
//
bool ResourceManagerClient::commandResponse(UMSConnectorHandle* handle,
		UMSConnectorMessage* message, void* ctx)
{
	JDomParser parser;

	string cmd = connector->getMessageText(message);

	RETURN_IF( !parser.parse(cmd, pbnjson::JSchema::AllSchema()), false,
			MSGERR_JSON_PARSE, "JDomParse. input=%s",cmd.c_str());

	JValue command_response = parser.getDom();

	RETURN_IF( ! command_response.hasKey("returnValue"), false,
			MSGERR_JSON_SCHEMA,"no state key in commandResponse");

	bool state = command_response["state"].asBool();

	LOG_DEBUG(log, "commandResponse state = %d",state);

	// command failed. inform waiters
	if ( state == false ) {
		string connection_id = command_response["connectionId"].asString();
		informWaiter(connection_id,state,cmd);
	}

	return true;
}

//getDisplayIdResponse
bool ResourceManagerClient::getDisplayIdResponse(UMSConnectorHandle* handle,
		UMSConnectorMessage* message, void* ctx)
{
	std::lock_guard<std::mutex> lk(display_id_mutex);

	JDomParser parser;

	string cmd = connector->getMessageText(message);

	RETURN_IF( !parser.parse(cmd, pbnjson::JSchema::AllSchema()), false,
			MSGERR_JSON_PARSE, "JDomParse. input=%s",cmd.c_str());

	JValue command_response = parser.getDom();

	RETURN_IF( ! command_response.hasKey("returnValue"), false,
			MSGERR_JSON_SCHEMA,"no state key in commandResponse");

	int32_t display_id = command_response["display_id"].asNumber<int32_t>();

	LOG_DEBUG(log, "display_id = %d", display_id);

	this->display_id = display_id;
	is_valid_display_id = true;

	display_id_cv.notify_one();

	return true;
}


// @f informWaiters
// @brief utility function for informing waiters on resources
//
bool ResourceManagerClient::informWaiter(string waiter, bool state, string response)
{
	std::lock_guard<std::mutex> lk(acquire_mutex);

	auto it = acquire_waiters.find(waiter);
	if(it != acquire_waiters.end()) {
		LOG_DEBUG(log, "Notifying acquire_waiter: connection_id = %s",
				waiter.c_str());

		it->second->state = state;
		it->second->acquire_response = response;
		it->second->cv.notify_one();
	}

	return true;
}

bool ResourceManagerClient::waitEvent(uint32_t *event,
		uint32_t desired_state,
		pthread_mutex_t * lock,
		pthread_cond_t * condition,
		unsigned int secs)
{
	int cond_rv;
	bool rv = true;
	struct timespec ts;

	pthread_condattr_t attr;
	pthread_condattr_init(&attr);
	pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
	pthread_cond_init(condition, &attr);

	int ret = clock_gettime(CLOCK_MONOTONIC, &ts);
	if ( ret ) {
		LOG_ERROR(log, MSGERR_GETTIMEOFDAY, "clock_gettime() failed.");
		return false;
	}
	ts.tv_sec += secs;

	pthread_mutex_lock(lock);
	while ( *event != desired_state ) {
		cond_rv = pthread_cond_timedwait(condition, lock, &ts);
		if (cond_rv == ETIMEDOUT) {
			LOG_ERROR(log, MSGERR_EVENT_TIMEOUT,
					"Event failed to responed after '%d' seconds.",secs);
			rv = false;
			break;
		}
	}
	pthread_mutex_unlock(lock);

	return rv;
}

bool ResourceManagerClient::signalEvent(uint32_t *event,
		uint32_t state,
		pthread_mutex_t * lock,
		pthread_cond_t * condition)
{
	pthread_mutex_lock(lock);
	*event = state;
	pthread_cond_signal(condition);
	pthread_mutex_unlock(lock);
	return true;
}

// @f getStateName
// @brief return state change name
//
bool ResourceManagerClient::getStateData(const std::string& message, std::string& name, pbnjson::JValue &value)
{
	JDomParser parser;
	JSchemaFragment input_schema("{}");
	if (!parser.parse(message, input_schema)) {
		LOG_ERROR(log, MSGERR_JSON_PARSE, "JDomParse. input=%s", message.c_str());
		return false;
	}

	JValue state = parser.getDom();

	if ( !(*state.begin()).first.isString() ) {
		LOG_ERROR(log, MSGERR_JSON_SCHEMA, "error. stateChange name != string");
		return false;
	}

	name = (*state.begin()).first.asString();
	value = state[name];

	return value.isObject() ? true : false;
}

int32_t ResourceManagerClient::getDisplayID() {
	std::unique_lock<std::mutex> lk(display_id_mutex);
	if (!display_id_cv.wait_for(lk, std::chrono::seconds(ACQUIRE_WAIT_TIMEOUT),
		[this](){ return is_valid_display_id; })) {
		LOG_ERROR(log, MSGERR_UNLOCK_TIMEOUT,
			"TIMEOUT: getDisplayID() waited notification for [%d]sec", ACQUIRE_WAIT_TIMEOUT);
		return -1;
	}
	LOG_DEBUG(log, "display_id = %d", display_id);
	return display_id;
}
