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

#include <map>
#include <set>
#include <list>
#include <string>
#include <memory>
#include <mutex>
#include <boost/noncopyable.hpp>
#include <luna-service2/lunaservice.h>
#include "Logger_macro.h"
#include "UMSConnector_common.h"

#ifndef LOG_LUNA_RX_MSG
#define LOG_LUNA_RX_MSG(__log, __msg)\
	LOG_DEBUG((__log), "rx: sender: %s, token: 0x%08lX, method: %s, body: %s",\
		LSMessageIsHubErrorMessage(__msg) ? "luna" : LSMessageGetSender(__msg), /* sender */ \
		LSMessageIsHubErrorMessage(__msg) ? 0UL : LSMessageGetToken(__msg),     /* token  */ \
		LSMessageGetMethod(__msg) ? LSMessageGetMethod(__msg) : "notify",       /* method */ \
		LSMessageGetPayload(__msg)                                              /* body   */ )
#endif

#ifndef LOG_LUNA_COMMAND
#define LOG_LUNA_COMMAND(__log, __token, __method, __body)\
	LOG_DEBUG(__log, "tx: token: 0x%08lX, method: %s, body: %s",\
		__token, __method, __body)
#endif

#ifndef LOG_LUNA_RESPONSE
#define LOG_LUNA_RESPONSE(__log, __msg, __body)\
	LOG_DEBUG(__log, "tx: receiver: %s, token: 0x%08lX, method: %s, body: %s",\
		LSMessageGetSender(__msg),  /* receiver */ \
		LSMessageGetToken(__msg),   /* token    */ \
		LSMessageGetMethod(__msg),  /* method   */ \
		__body                      /* body     */ )
#endif

#ifndef LOG_LUNA_NOTIFICATION
#define LOG_LUNA_NOTIFICATION(__log, __body)\
	LOG_DEBUG(__log, "tx: method: notify, body: %s", __body)
#endif

/// Helper entity that support binding luna message callbacks to logging context
class CallbackManager {
public:
	typedef std::unique_ptr<CallbackManager> ptr_t;

	/**
	 * Proxy command handler callback
	 * @param luna bus connection handle
	 * @param luna message
	 * @param user defined context
	 * @return true if message successfully processed
	 */
	static bool CommandHandlerProxy(LSHandle * handle, LSMessage * message, void * data);
	/**
	 * Proxy one time replay handler callback
	 * @param luna bus connection handle
	 * @param luna message
	 * @param user defined context
	 * @return true if message successfully processed
	 */
	static bool ReplyHandlerProxy(LSHandle * handle, LSMessage * message, void * data);
	/**
	 * Proxy subscription message handler callback
	 * @param luna bus connection handle
	 * @param luna message
	 * @param user defined context
	 * @return true if message successfully processed
	 */
	static bool SubscriptionHandlerProxy(LSHandle * handle, LSMessage * message, void * data);

	/**
	 * Creates instance of CallbackManager
	 * @param user context to be reused in command handlers
	 * @param logger object
	 */
	CallbackManager(void * context, std::shared_ptr<const uMediaServer::Logger> log);
	~CallbackManager();

	/**
	 * Registers new command handler
	 * @param luna message category
	 * @param luna message method to handle
	 * @param actual command handler
	 * @return context to bind to corresponding luna message category
	 */
	void * registerCommandHandler(const std::string & category, const std::string & method,
				UMSConnectorEventFunction handler);
	/**
	 * Register one time reply handler
	 * @param actual reply handler
	 * @param user context
	 * @return context to bind to corresponding reply callback
	 */
	void * registerReplyHandler(UMSConnectorEventFunction handler, void * context);
	/**
	 * Register subscription messages handler
	 * @param subscription service uri
	 * @param actual message handler callback
	 * @param user context
	 * @return context to bind to corresponding reply callback
	 */
	void * registerSubscriptionHandler(const std::string & serviceUri,
				UMSConnectorEventFunction handler, void * context);
	/**
	 * Unregister subscription messages handler
	 * @param data returned from registerSubscriptionHandler
	 * @return true if subscription existed, and was removed
	 */
	 bool unregisterSubscriptionHandler(void* data);

private:
	CallbackManager();

	// internal classes
	class CallbackWrapper : boost::noncopyable {
	protected:
		CallbackWrapper(void * context, std::shared_ptr<const uMediaServer::Logger> log)
			: m_context(context), m_log(log) {}
		std::weak_ptr<const uMediaServer::Logger> m_log;
		void * m_context;
	};

	class CommandCategory : public CallbackWrapper {
		friend class CallbackManager;
	private:
		CommandCategory(void * context, std::shared_ptr<const uMediaServer::Logger> log)
			: CallbackWrapper(context, std::move(log)) {}
		void registerHandler(const std::string & method, UMSConnectorEventFunction handler) {
			m_handlers[method] = handler;
		}
		bool handleCommand(LSHandle * handle, LSMessage * message);

		std::map<std::string, UMSConnectorEventFunction> m_handlers;
	};

	class EventHandler : public CallbackWrapper {
		friend class CallbackManager;
	private:
		EventHandler(const std::string & ls_uri,
		             void * context,
		             std::shared_ptr<const uMediaServer::Logger> log,
				         UMSConnectorEventFunction handler)
			: CallbackWrapper(context, std::move(log)), m_ls_uri(ls_uri), m_handler(handler) {}
		bool handleEvent(LSHandle * handle, LSMessage * message);

		std::string m_ls_uri;
		UMSConnectorEventFunction m_handler;
	};

	class ReplyHandler : public CallbackWrapper {
		friend class CallbackManager;
	private:
		ReplyHandler(CallbackManager * manager, void * context,
				std::shared_ptr<const uMediaServer::Logger> log,
				UMSConnectorEventFunction handler)
			: CallbackWrapper(context, std::move(log)), m_handler(handler), m_manager(manager) {}
		bool handleEvent(LSHandle * handle, LSMessage * message);

		UMSConnectorEventFunction m_handler;
		CallbackManager * m_manager;
	};

	// logging context
	std::shared_ptr<const uMediaServer::Logger> m_log;

	// commands callback context
	void * m_context;

	// callbacks storage
	std::mutex m_mutex;
	std::map<std::string, CommandCategory*> m_categories;
	std::set<ReplyHandler*>                 m_replies;
	std::set<EventHandler*>                 m_subscriptions;
};
