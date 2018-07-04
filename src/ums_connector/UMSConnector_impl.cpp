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

#include <boost/lexical_cast.hpp>  // for lexical_cast<string>(number)

#include <Logger.h>
#include <Logger_macro.h>
#include "UMSConnector.h"
#include <UMSConnector_impl.h>
#include "GenerateUniqueID.h"
#include <thread>
#include <algorithm>

using namespace std;
using namespace pbnjson;
using namespace uMediaServer;

typedef struct LSErrorWrapper : LSError {
	LSErrorWrapper() { LSErrorInit(this); }
	~LSErrorWrapper() { LSErrorFree(this); }
	LSErrorWrapper * operator &() { LSErrorFree(this); return this; }
} ls_error_t;

#define LOG_LS_ERROR(__msgid, __error, __format, ...)\
			LOG_ERROR_EX((*log), __msgid, __KV({ {KVP_SERVICE, service_name.c_str()},\
				{KVP_ERROR, __error.message} }), __format, ##__VA_ARGS__)

UMSConnector::UMSConnector_impl::UMSConnector_impl(const string& name,
		GMainLoop *mainLoop,   // nullptr, will create new main loop unless use_default_context = true
		void * user_data,
		bool use_default_context)

   : log(new Logger(UMS_LOG_CONTEXT_CONNECTOR)),
     service_name(name),
     subscription_key(name),
     user_data(user_data),
     m_callbackManager(new CallbackManager(user_data, log)),
     stopped_(false),
	 run_state_altered(false),
	 mainLoop_(mainLoop),
	 m_token(0),
	 idle_task_(0)
{
	// setting session ids
	if (name.empty()) { // client
		std::string sid = GenerateUniqueID()();
		// TODO: take a look on service_name usage do we really need it?
		service_name = sid;
		log->setUniqueId(sid);
		LOG_DEBUG((*log), "Starting client session");
	}
	else {
		size_t offset = service_name.size() > UMEDIASERVER_UNIQUE_ID_LENGTH ?
				service_name.size() - UMEDIASERVER_UNIQUE_ID_LENGTH : 0;
		log->setUniqueId(service_name.c_str() + offset);
	}

	// TODO constructor cannot return error.  Move LSResiger to and "init" function.

	LOG_DEBUG((*log), "UMSConnector luna service 2 implementation initialized.");

	bool ret;
	ls_error_t lserror;

	GMainContext * main_context = nullptr;

	if( use_default_context ) {
		LOG_DEBUG((*log), "Using DEFAULT GMainContext");
		main_context = g_main_context_ref_thread_default(); // reference default context
	}
	else if( mainLoop != NULL ) {
		LOG_DEBUG((*log), "GMainLoop provided. Attaching to existing GMainLoop.");
		mainLoop_ = g_main_loop_ref(mainLoop);
		main_context = g_main_loop_get_context(mainLoop_);
		g_main_context_ref(main_context);
	}
	else {
		LOG_DEBUG((*log), "No GMainLoop provided. Creating GMainLoop.");
		mainLoop_ = g_main_loop_new(NULL, FALSE);
		main_context = g_main_loop_get_context(mainLoop_);
		g_main_context_ref(main_context);
	}

	auto f_set_sub_cancel = [this] (LSHandle* h) {
		ls_error_t lserr;
		if (!LSSubscriptionSetCancelFunction(h, UMSConnector::UMSConnector_impl::_CancelCallback, this, &lserr)) {
			LOG_ERROR((*log), MSGERR_COMM_SUBSCRIBE, "LSSubscriptionSetCancelFunction FAILED: %s", lserr.message);
			throw std::runtime_error("LSSubscriptionSetCancelFunction FAILED");
		}
	};

	ret = LSRegister(name.c_str(), &m_service.lshandle, &lserror);
	if (!ret) {
		LOG_LS_ERROR(MSGERR_SERVICE_REGISTER, lserror,
				"LSRegister FAILED for name=%s !!", name.c_str());
		throw std::runtime_error("LSRegister FAILED");
	}

	LOG_DEBUG((*log), "LSRegister was successful - returned m_service=%p name=%s",
			m_service.lshandle, name.c_str());

	ret = LSRegisterCategory(m_service.lshandle,"/",NULL,NULL,NULL,&lserror);
	if (!ret) {
		LOG_LS_ERROR(MSGERR_CATEGORY_REGISTER, lserror,
				"LSRegister FAILED for name=%s !!", name.c_str());
		throw std::runtime_error("LSRegister FAILED");
	}

	if( user_data ) {
		ret = LSCategorySetData(m_service.lshandle, "/", user_data, &lserror);
		if (!ret) {
			LOG_LS_ERROR(MSGERR_CATEGORY_DATA, lserror,
					"LSCategorySetData() FAILED for name=%s !!", name.c_str());
			throw std::runtime_error("LSCategorySetData FAILED");
		}
	}

	ret = LSGmainContextAttach(m_service.lshandle, main_context, &lserror);
	if( !ret ) {
		LOG_LS_ERROR(MSGERR_GMAIN_ATTACH, lserror,
				"LSGmainAttach FAILED for name=%s !!", service_name.c_str());
		throw std::runtime_error("LSGmainAttach FAILED");
	}
	f_set_sub_cancel(m_service.lshandle);

	g_main_context_unref(main_context);
	log->setLogLevel(kPmLogLevel_Debug);
}

UMSConnector::UMSConnector_impl::~UMSConnector_impl()
{
	ls_error_t lsError;

	LOG_DEBUG((*log), "Disconnecting from luna service bus.");
	subscriptionsT *subsc_ptr = NULL;

	if(!m_service.lshandle) {
		LOG_WARNING_EX((*log), MSGERR_SERVICE_NOTREG,
					__KV({{KVP_SERVICE, service_name.c_str()}}),
					"UMSConnector_impl::~UMSConnector_impl");
		return;
	}

	// Clean up any outstanding subscription calls
	while (m_subscriptions.empty() == false) {
		subsc_ptr = m_subscriptions.front();
		LOG_TRACE((*log), "Freeing connector subscription: %p, handle=%p", subsc_ptr, subsc_ptr->shandle);
		if (subsc_ptr) {
                        // We can ignore the return value of LSCallCancel
                        // because the event loop is ending in this function.
                        // Normally if LSCallCancel fails the callback might still
                        // arrive later, and therefore we cannot delete the object
                        // created as client data for the callback. But because
                        // the event loop is ending, there is no risk of the
                        // callback arriving later, and so we ignore the return value
                        // of LSCallCancel and also delete the client data for the callbacks
                        // which happens when we call m_callbackmanager.reset() below
			LSCallCancel(subsc_ptr->shandle,
						 subsc_ptr->stoken,
						 NULL);
		}
		m_subscriptions.pop_front();
		delete subsc_ptr;
		subsc_ptr = NULL;
	}

	// Cancel service ready signals subscriptions
	for (const auto & sub : service_status_subscriptions) {
		if (! LSCancelServerStatus(getBusHandle(""), sub.second.cookie, &lsError))
		{
			LOG_LS_ERROR(MSGERR_UNREGISTER, lsError, "failed LSCancelServerStatus")
		}
	}

	// suspending main loop before unregistering from ls hub if we run it
	if (run_state_altered) {
		g_main_loop_quit(mainLoop_);
		GMainContext * main_context = g_main_loop_get_context(mainLoop_);
		while(!g_main_context_acquire(main_context)) {
			sched_yield();
		}
		g_main_context_release(main_context);
	}

	// no more deferred subscription cancellations
	if (idle_task_ != 0)
		g_source_remove(idle_task_);

	if (!LSUnregister(m_service.lshandle, &lsError)) {
		LOG_LS_ERROR(MSGERR_UNREGISTER, lsError, "failed to unregister from hub")
	}
	
	if( mainLoop_ )
		g_main_loop_unref(mainLoop_);

	// free all method structures created.
	for(unsigned int i = 0; i < eventHandlers.size(); ++i) {
		LOG_TRACE((*log), "freeing: %s, function=%p", eventHandlers[i]->name, eventHandlers[i]->function);
		g_free((gpointer*)eventHandlers[i]->name);
		delete[] eventHandlers[i];
	}
	eventHandlers.erase(eventHandlers.begin(),eventHandlers.end());
	m_callbackManager.reset();
}

bool UMSConnector::UMSConnector_impl::wait()
{
	LOG_DEBUG((*log), "LSGmainAttach waiting for events on name=%s !!", service_name.c_str());

	if (!stopped_) {
		run_state_altered = !g_main_loop_is_running(mainLoop_);
		g_main_loop_run(mainLoop_);
	}

	return true;
}

bool UMSConnector::UMSConnector_impl::stop()
{
	if (stopped_)
		return true;

	stopped_ = true;

	GSource * g_src = g_timeout_source_new(0);
	g_source_set_callback(
				g_src,
				[] (void * l) { g_main_loop_quit((GMainLoop*)l); return 0; },
				mainLoop_,
				NULL
	);
	g_source_attach(g_src, g_main_loop_get_context(mainLoop_));
	g_source_unref(g_src);

	return true;
}

bool UMSConnector::UMSConnector_impl::addEventHandler(string event, UMSConnectorEventFunction func, const std::string &category)
{
	return addEventHandler(event, func, UMS_CONNECTOR_DUAL_BUS, category);
}

bool UMSConnector::UMSConnector_impl::addEventHandler(string event, UMSConnectorEventFunction func, UMSConnectorBusType bus, const std::string &category)
{
	LOG_TRACE((*log), "category=%s, event=%s, func=%p", category.c_str(), event.c_str(), func);

	LSMethod *lsmethod = new LSMethod[2];

	string strCategory = "/" + category;
	// TODO : Luna Service is awkward. type cast function pointer. not ideal.
	ls_error_t lserror;
	bool rv;

	lsmethod[0] = {
		.name = g_strdup((gchar*)event.c_str()),
		.function = CallbackManager::CommandHandlerProxy,
	};
	lsmethod[1] = {}; // terminate list of methods. LS2 needs to know when methods array ends

	// setup callback proxy
	void * context = m_callbackManager->registerCommandHandler(strCategory, event, func);

  LSHandle *lshandle = m_service.lshandle;
  if (lshandle == nullptr) {
    LOG_LS_ERROR(MSGERR_CATEGORY_APPEND, lserror, "Cannot register %s on bus.", event.c_str());
    goto fail;
  }
  rv = LSRegisterCategoryAppend(lshandle, strCategory.c_str(), lsmethod, NULL, &lserror);
  if( !rv) {
    LOG_LS_ERROR(MSGERR_CATEGORY_APPEND, lserror, "LSRegisterCategoryAppend() failed.");
    goto fail;
  }
  rv = LSCategorySetData(lshandle, strCategory.c_str(), context, &lserror);
  if (!rv) {
    LOG_LS_ERROR(MSGERR_CATEGORY_DATA, lserror, "LSCategorySetData() FAILED !!");
    goto fail;
  }
	eventHandlers.push_back(&lsmethod[0]);   // store for later clean up

	return true;

fail:
	g_free((gpointer*)lsmethod[0].name);
	delete[] lsmethod;

	return false;
}

bool UMSConnector::UMSConnector_impl::addEventHandlers(UMSConnectorEventHandler *handlers)
{
	for(; handlers->event != NULL && handlers->function != NULL; ++handlers ) {
		if (!addEventHandler(handlers->event, handlers->function, ""))
			return false;
	}
	return true;
}

const char * UMSConnector::UMSConnector_impl::getMessageSender(UMSConnectorMessage *message)
{
	static const char * luna = "luna";
	LSMessage * msg = reinterpret_cast<LSMessage*>(message);
	// LSMessageGetSender segfaults on luna hub error messages
	return LSMessageIsHubErrorMessage(msg) ? luna : LSMessageGetSender(msg);
}

const char * UMSConnector::UMSConnector_impl::getMessageText(UMSConnectorMessage *message)
{
	return LSMessageGetPayload(reinterpret_cast<LSMessage*>(message));
}

const char * UMSConnector::UMSConnector_impl::getSenderServiceName(UMSConnectorMessage *message)
{
	return LSMessageGetSenderServiceName(reinterpret_cast<LSMessage*>(message));
}

bool UMSConnector::UMSConnector_impl::sendSimpleResponse(UMSConnectorHandle *sender,UMSConnectorMessage* message, bool resp)
{
	std::string response = (resp) ? "{\"returnValue\":true}" : "{\"returnValue\":false}";
	return sendResponseObject(sender, message, response);
}

bool UMSConnector::UMSConnector_impl::sendResponse(UMSConnectorHandle *sender,UMSConnectorMessage* message,
													const std::string &key, const std::string &value)
{
	// create key:value json string
	// eg: "{"key":"value"}
	string response = "{\"" + key + "\":\"" + value + "\"}";
	return sendResponseObject(sender, message, response);
}


bool UMSConnector::UMSConnector_impl::sendResponseObject(UMSConnectorHandle *sender,UMSConnectorMessage* message, const std::string &object)
{
	LSHandle *ls_sender = reinterpret_cast<LSHandle*>(sender);
	LSMessage * ls_message = reinterpret_cast<LSMessage*>(message);
	ls_error_t ls_error;

	if (LSMessageReply(ls_sender, ls_message, object.c_str(), &ls_error)) {
		LOG_LUNA_RESPONSE((*log), ls_message, object.c_str());
		return true;
	}

	LOG_LS_ERROR(MSGERR_COMM_REPLAY, ls_error, "LSMessageReplay failed: %s", ls_error.message);
	return false;
}

LSHandle * UMSConnector::UMSConnector_impl::getBusHandle(const std::string &key)
{
	return m_service.lshandle;
}

bool UMSConnector::UMSConnector_impl::sendMessage(const string &uri,const string &payload,
		UMSConnectorEventFunction cb, void *ctx)
{
	ls_error_t lserror;
	LSHandle * service_hdl;
	bool rv;
	string true_uri;
	size_t cmd_delimiter;

	// Have to strip the uri of its appended command string
	cmd_delimiter = uri.find('/');
	if (cmd_delimiter == string::npos)
		true_uri = uri;
	else
		true_uri = uri.substr(0, cmd_delimiter);

	service_hdl = getBusHandle(true_uri);

	bool identifier_missing = true;
	string ls_uri = uri;
	vector<string> identifiers = { "palm://", "luna://"};
	for( auto& i : identifiers) {
		size_t pos = ls_uri.find(i);
		if( pos != string::npos ) {
			identifier_missing = false;
			break;
		}
	}

	if ( identifier_missing ) {
		// missing identifier. Guess "palm://"
		ls_uri.insert(0,identifiers[0]);
	}

	// setup callback proxy if user provided callback
	void * context = NULL;
	bool (*replyProxy)(LSHandle *, LSMessage *, void *) = CallbackManager::ReplyHandlerProxy;
	if ( cb != NULL ) {
		context = m_callbackManager->registerReplyHandler(cb, ctx);
	}

	size_t slash = ls_uri.rfind('/');
	std::string method = (slash == std::string::npos) ? "unknown" : ls_uri.substr(slash + 1);
	LOG_LUNA_COMMAND((*log), m_token, method.c_str(), payload.c_str());

	rv = LSCallOneReply(service_hdl,ls_uri.c_str(), payload.c_str(),
						replyProxy, context, &m_token, &lserror);

	if( !rv ) {
		LOG_LS_ERROR(MSGERR_COMM_SEND, lserror, "LSCallOneReply failed: %s", lserror.message);
		return false;
	}

	return true;
}

std::string UMSConnector::UMSConnector_impl::getLSUri(const std::string & uri)
{
	// TODO: remove uri manipulations. this call fails on something like luna://...
	string ls_uri = uri, identifier = "palm://";

	size_t pos = ls_uri.find(identifier);
	if( pos == string::npos ) {
		ls_uri.insert(0,identifier);  // insert required identifier
	}

	return ls_uri;
}

size_t UMSConnector::UMSConnector_impl::subscribe(const string &uri, const std::string &payload,
												  UMSConnectorEventFunction cb, void *ctx)
{
	ls_error_t lserror;

	// TODO: remove uri manipulations. this call fails on something like luna://...
	string ls_uri = getLSUri(uri);

	LOG_TRACE((*log), "subscribe to events from ls_uri = %s", ls_uri.c_str());

	// setup callback proxy
	void * context = m_callbackManager->registerSubscriptionHandler(ls_uri, cb, ctx);

	LSHandle * handle = m_service.lshandle;

	bool rv = LSCall(handle, ls_uri.c_str(), payload.c_str(),
				CallbackManager::SubscriptionHandlerProxy,
				context, &m_token, &lserror);

	if( !rv ) {
		LOG_LS_ERROR(MSGERR_COMM_SEND, lserror, "LSCall failed: %s", lserror.message);
		return 0;
	}

	subscriptionsT *new_subscr = new subscriptionsT;
	new_subscr->shandle = handle;
	new_subscr->stoken = m_token;
	new_subscr->scall = uri;
	new_subscr->deferred_cancel = false;
	new_subscr->evtHandler = context;
	m_subscriptions.push_back(new_subscr);

	LOG_DEBUG((*log), "subscribe success: %s,%d", new_subscr->scall.c_str(), new_subscr->stoken);

	JDomParser parser;
	if (parser.parse(payload,  pbnjson::JSchema::AllSchema())) {
		JValue mediaId = parser.getDom()["mediaId"];
		if (mediaId.isString())
			LOG_DEBUG((*log), "Subscribed to notifications from: %s", mediaId.asString().c_str());
	}

	return m_token;
}

gboolean UMSConnector::UMSConnector_impl::idle_func(gpointer user_data) {
  UMSConnector::UMSConnector_impl* obj = reinterpret_cast<UMSConnector::UMSConnector_impl*>(user_data);
  return obj->processDeferredSubscriptionCancellations() ? G_SOURCE_CONTINUE : G_SOURCE_REMOVE;
}

bool UMSConnector::UMSConnector_impl::processDeferredSubscriptionCancellations() {
  bool ret_val = false;

  for (auto iter = m_subscriptions.begin(); iter != m_subscriptions.end(); iter++ ) {
    if ((*iter)->deferred_cancel) {
      ls_error_t error;
      LOG_TRACE((*log), "Deferred cancel entry: %p", *iter);
      ret_val = true;
      if (!LSCallCancel((*iter)->shandle, (*iter)->stoken, &error)) {
        // if LSCallCancel fails, log a warning here
        LOG_WARNING((*log), MSGERR_UNREGISTER, "LSCallCancel failed: %s,%d", (*iter)->scall.c_str(), (*iter)->stoken);
      }
      // whether call succeeds or fails, it should be safe to unregister
      // because we are in the idle loop
      LOG_DEBUG((*log), "unregisterSubscriptionHandler: %s,%d", (*iter)->scall.c_str(), (*iter)->stoken);
      m_callbackManager->unregisterSubscriptionHandler((*iter)->evtHandler);
      delete *iter;
      m_subscriptions.erase(iter);
      break;
    }
  }

  if (ret_val == false) {
    // no more deferred subscription cancellations
    if (idle_task_ != 0)
      g_source_remove(idle_task_);
    idle_task_ = 0;
  }

  return ret_val;
}

void UMSConnector::UMSConnector_impl::unsubscribe(const string &uri, size_t token) {

	auto iter = std::find_if(m_subscriptions.begin(), m_subscriptions.end(), [&](const subscriptionsT* s){
		return s->scall == uri && s->stoken == token;
	});
	if (iter == m_subscriptions.end()) {
		LOG_WARNING((*log), MSGERR_SUBS_FIND, "Subscription: %s,%d not found", uri.c_str(), token);
	} else {
		ls_error_t error;
		if (LSCallCancel((*iter)->shandle, (*iter)->stoken, &error)) {
	          LOG_DEBUG((*log), "unsubscribe: unreg right away : %s,%d", (*iter)->scall.c_str(), (*iter)->stoken);
		  m_callbackManager->unregisterSubscriptionHandler((*iter)->evtHandler);
		  delete *iter;
		  m_subscriptions.erase(iter);
		} else {
		  // if LSCallCancel is not successful, we cannot delete the object
		  // set a flag here and delete the object in the idle function
	          LOG_DEBUG((*log), "unsubscribe: defer unreg: %s,%d", (*iter)->scall.c_str(), (*iter)->stoken);
		  (*iter)->deferred_cancel = true;
		  if (idle_task_ == 0)
		    idle_task_ = g_idle_add(idle_func, reinterpret_cast<gpointer>(this));
		}
	}
}

bool UMSConnector::UMSConnector_impl::serverStatusUpdate(LSHandle *sh, LSMessage *message, void *ctx)
{
	UMSConnector::UMSConnector_impl *connector = reinterpret_cast<UMSConnector::UMSConnector_impl*>(ctx);
	connector->cancelSendMessage(sh, message, ctx);
	return true;
}

bool UMSConnector::UMSConnector_impl::cancelSendMessage(LSHandle *sh, LSMessage *message, void *ctx)
{
	const char *status = LSMessageGetPayload(message);

	JDomParser parser;
	if (!parser.parse(status, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR((*log), MSGERR_JSON_PARSE, "JDomParse error.");
		return false;
	}

	// {"serviceName":"palm://com.palm.umediapipeline_141764800","connected":"false"}
	JValue root = parser.getDom();
	if (!root.hasKey("serviceName")) {
		LOG_ERROR_EX((*log), MSGERR_JSON_SCHEMA, __KV({{KVP_MSG, status}}),
				"service status message malformed. no 'serviceName' specified in '%s'.", status);
		return false;
	}

	JValue name = root["serviceName"];
	string serviceName = name.asString();

	JValue connected = root["connected"];
	bool connectedState = connected.asBool();

	LSMessageToken token = LSMessageGetToken(message);

	LOG_TRACE((*log), ">>>>>>>>> Server status update, connected=%s, service=%s, token=%ld",
			connectedState?"UP":"DOWN", serviceName.c_str(),(long)token);

	ls_error_t lserror;
	if (!connectedState) {
		bool rv = false;
		LOG_TRACE((*log), ">>>>>>>>> cancel last sendMessage for %s, service is DOWN.", serviceName.c_str());
		if (true == (rv = LSCallCancel(sh, m_token, &lserror))) {
			// Clean up any outstanding subscription calls
			if (m_subscriptions.empty() == false) {
				for ( auto subscriptions_Iter = m_subscriptions.begin( ); subscriptions_Iter != m_subscriptions.end( );
					  subscriptions_Iter++ ) {
					if ((*subscriptions_Iter)->shandle == sh) {
						LOG_TRACE((*log), "Freeing connection entry: %p", *subscriptions_Iter);
						m_callbackManager->unregisterSubscriptionHandler((*subscriptions_Iter)->evtHandler);
						delete *subscriptions_Iter;
						m_subscriptions.erase(subscriptions_Iter);
						break;
					}
				}
			}
		}
		return(rv);
	}
	return true;
}

bool UMSConnector::UMSConnector_impl::addSubscriber (UMSConnectorHandle *subscriber,UMSConnectorMessage *message, const string &key)
{
	ls_error_t lserror;
	LSHandle *lshandle_subscriber = reinterpret_cast<LSHandle*>(subscriber);
	LSMessage *lsmessage = reinterpret_cast<LSMessage*>(message);
	bool success = true;

	// use the specified key if one was passed
	string subkey = (key.size() ? key : subscription_key);
	LOG_TRACE((*log), "UMSConnector_impl::addSubscriber *** m_service=%p, lshandle_subscriber=%p, lsmessage=%p,"
			  " subkey=%s, subkey_len=%d", m_service.lshandle, lshandle_subscriber,
			  lsmessage, subkey.c_str(), subkey.size());

	// add the senders connection handle to the list of subscribers.
	if (!LSSubscriptionAdd(lshandle_subscriber, subkey.c_str(), lsmessage, &lserror)) {
		LOG_LS_ERROR(MSGERR_COMM_SUBSCRIBE, lserror, "LSSubscriptionAdd failed: %s", lserror.message);
		success = false;
	}

	std::stringstream result; result << "{\"subscription\":" << (success ? "true" : "false") << "}";
	sendResponseObject(subscriber, message, result.str());

	return success;
}

bool UMSConnector::UMSConnector_impl::removeSubscriber(UMSConnectorHandle *subscriber,UMSConnectorMessage *message, const string &key)
{
	ls_error_t lserror;
	LSHandle *lshandle_subscriber = reinterpret_cast<LSHandle*>(subscriber);
	LSMessage *lsmessage = reinterpret_cast<LSMessage*>(message);

	if (!key.size()) {
		LOG_ERROR((*log), "LS2_ERR", "UMSConnector_impl::removeSubscriber no key given");
		return false;
	}

	LOG_TRACE((*log), "UMSConnector_impl::removeSubscriber *** m_service=%p, lshandle_subscriber=%p, lsmessage=%p,"
			  " subkey=%s, subkey_len=%d", m_service.lshandle, lshandle_subscriber,
			  lsmessage, key.c_str(), key.size());

	LSError err;
	LSSubscriptionIter *iter = NULL;
	bool found_subscriber = false;
	bool success = false;
	const char* unique_client_id = LSMessageGetSender(lsmessage);
	if (LSSubscriptionAcquire(lshandle_subscriber, key.c_str(), &iter, &err)) {
		LSMessage *subscriber_message;
		while (LSSubscriptionHasNext(iter)) {
			subscriber_message = LSSubscriptionNext(iter);
			if (unique_client_id == LSMessageGetSender(subscriber_message)) {
				found_subscriber = true;
				success = true;
				LSSubscriptionRemove(iter);
				break;
			}
		}
	}

	if (!found_subscriber) {
		LOG_ERROR((*log), "LS2_ERR", "UMSConnector_impl::removeSubscriber subscriber not found. m_service=%p, lshandle_subscriber=%p, lsmessage=%p,"
			  " subkey=%s", m_service.lshandle, lshandle_subscriber, lsmessage, key.c_str());
	}
	return success;
}

bool UMSConnector::UMSConnector_impl::sendChangeNotificationLongLong(const string &name,
		unsigned long long value)
{
	string svalue = boost::lexical_cast<string>(value);  // convert to string
	return sendChangeNotification(name,svalue);
}

bool UMSConnector::UMSConnector_impl::sendChangeNotificationString(const string &name,
		const string &value, const string &key)
{
	return sendChangeNotification(name,value,key);
}

bool UMSConnector::UMSConnector_impl::sendChangeNotification(const string &name,
		const string &value, const string &key)
{
	string json_string = "{\"name\":\"" + name + "\",\"value\":" + value + "}";
	string subkey = (key.size() ? key : subscription_key);

	return sendChangeNotificationJsonString(json_string, subkey);
}

bool UMSConnector::UMSConnector_impl::sendChangeNotificationJsonString(const string &json_string,
		const string &key)
{
	ls_error_t lserror;
	LSHandle * service_hdl;
	string subkey = (key.size() ? key : subscription_key);
	bool rv;

	service_hdl = getBusHandle(subkey);
	if (!LSSubscriptionReply(service_hdl, subkey.c_str(), json_string.c_str(), &lserror)) {
		LOG_LS_ERROR(MSGERR_COMM_NOTIFY, lserror,
				"LSSubscriptionReply failed: %s", lserror.message);
		return false;
	}
	LOG_LUNA_NOTIFICATION((*log), json_string.c_str());

	return true;
}

bool UMSConnector::UMSConnector_impl::_CancelCallback(LSHandle* sh, LSMessage* reply, void* ctx)
{
	UMSConnector::UMSConnector_impl& obj = *(static_cast<UMSConnector::UMSConnector_impl*>(ctx));

	const char* unique_client_id = LSMessageGetSender(reply);
	auto e = obj.watchers.find(unique_client_id);
	if (e != obj.watchers.cend()) {
		(*e->second)();
		obj.watchers.erase(e);
	}
}

bool UMSConnector::UMSConnector_impl::addClientWatcher (UMSConnectorHandle* cl,
		UMSConnectorMessage* message, track_cb_t cb)
{
	ls_error_t lserr;
	LSHandle* cl_ = reinterpret_cast<LSHandle*>(cl);
	const char* unique_client_id = LSMessageGetSender(reinterpret_cast<LSMessage*>(message));

	LOG_DEBUG((*log), "adding watcher for client %s", unique_client_id);
	auto e = watchers.find(unique_client_id);
	auto cb_ = unique_ptr<track_cb_t>(new track_cb_t(forward<track_cb_t>(cb)));
	if (e != watchers.cend()) {
		(*e->second)();
		e->second = std::forward< decltype(cb_) >(cb_);
	} else {
		watchers.insert(make_pair(unique_client_id, std::forward< decltype(cb_) >(cb_)));
	}

	return true;
}

bool UMSConnector::UMSConnector_impl::delClientWatcher (UMSConnectorHandle* cl, UMSConnectorMessage* message)
{
	ls_error_t lserr;
	LSHandle* cl_ = reinterpret_cast<LSHandle*>(cl);
	const char* unique_client_id = LSMessageGetSender(reinterpret_cast<LSMessage*>(message));

	LOG_DEBUG((*log), "removing watcher for client %s", unique_client_id);
	auto e = watchers.find(unique_client_id);
	if (e != watchers.cend()) {
		watchers.erase(e);
	}

	return true;
}

void UMSConnector::UMSConnector_impl::subscribeServiceReady(const std::string & service_name, track_cb_t && cb) {
	auto handle = getBusHandle("");
	auto it = service_status_subscriptions.emplace
			(std::make_pair(service_name, cb_info_t { std::move(cb), nullptr }));
	if (! LSRegisterServerStatusEx(handle,
		service_name.c_str(),
		[](LSHandle * handle,
		const char * svc_name,
		bool connected,
		void * ctx)->bool {
			if (connected) {
				UMSConnector::UMSConnector_impl * self = static_cast<UMSConnector::UMSConnector_impl *>(ctx);
				auto it = self->service_status_subscriptions.find(svc_name);
				if (it != self->service_status_subscriptions.end() && it->second.cb)
					it->second.cb();
			}
			return true;
		},
		this,
		&it.first->second.cookie, nullptr))
	{
		LOG_ERROR((*log), MSGERR_SERVICE_REGISTER, "LSRegisterServerStatusEx FAILED");
	}
}

void UMSConnector::UMSConnector_impl::unsubscribeServiceReady(const std::string &service_name) {
	ls_error_t lsError;
	auto it = service_status_subscriptions.find(service_name);
	if (it != service_status_subscriptions.cend()) {
		if (!LSCancelServerStatus(getBusHandle(""), it->second.cookie, &lsError)) {
			LOG_LS_ERROR(MSGERR_UNREGISTER, lsError, "failed LSCancelServerStatus for service %s", service_name.c_str());
		} else {
			service_status_subscriptions.erase(it);
		}
	}
}

// message ref counting operations
void UMSConnector::UMSConnector_impl::refMessage(UMSConnectorMessage * message) {
	LSMessageRef(reinterpret_cast<LSMessage*>(message));
}

void UMSConnector::UMSConnector_impl::unrefMessage(UMSConnectorMessage * message) {
	LSMessageUnref(reinterpret_cast<LSMessage*>(message));
}
