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

#include "CallbackManager.h"

// Callback manager
bool CallbackManager::CommandHandlerProxy(LSHandle * handle, LSMessage * message, void * data) {
	CommandCategory * category = reinterpret_cast<CommandCategory*>(data);
	if (category != NULL)
		return category->handleCommand(handle, message);
	static uMediaServer::Logger log(UMS_LOG_CONTEXT_CONNECTOR);
	LOG_ERROR(log, MSGERR_RESOLVE_HCMD, "Unable to resolve command handler");
	return false;
}

bool CallbackManager::ReplyHandlerProxy(LSHandle * handle, LSMessage * message, void * data) {
	ReplyHandler * handler = reinterpret_cast<ReplyHandler*>(data);
	if (handler != NULL) {
		bool result = handler->handleEvent(handle, message);
		CallbackManager * manager = handler->m_manager;
		if (manager != NULL) {
			std::lock_guard<std::mutex> lk(manager->m_mutex);
			manager->m_replies.erase(handler);
		}
		delete handler;
		return result;
	}
	static uMediaServer::Logger log(UMS_LOG_CONTEXT_CONNECTOR);
	LOG_ERROR(log, MSGERR_RESOLVE_HREP, "Unable to resolve replay handler");
	return false;
}

bool CallbackManager::SubscriptionHandlerProxy(LSHandle * handle, LSMessage * message, void * data) {
	EventHandler * handler = reinterpret_cast<EventHandler*>(data);
	if (handler != NULL) {
		return handler->handleEvent(handle, message);
	}
	static uMediaServer::Logger log(UMS_LOG_CONTEXT_CONNECTOR);
	LOG_ERROR(log, MSGERR_RESOLVE_HSUB, "Unable to resolve subscription handler");
	return false;
}

CallbackManager::CallbackManager(void * context, std::shared_ptr<const uMediaServer::Logger> log)
		: m_context(context), m_log(std::move(log)) {
}

CallbackManager::~CallbackManager() {
	std::lock_guard<std::mutex> lk(m_mutex);

	auto cit = m_categories.begin();
	auto cend = m_categories.end();
	for (; cit != cend; ++cit) {
		delete cit->second;
	}
	auto rit = m_replies.begin();
	auto rend = m_replies.end();
	for (; rit != rend; ++rit) {
		delete (*rit);
	}
	auto sit = m_subscriptions.begin();
	auto send = m_subscriptions.end();
	for (; sit != send; ++sit) {
		delete (*sit);
	}
}

void * CallbackManager::registerCommandHandler(const std::string & category,
			const std::string & method, UMSConnectorEventFunction handler) {
	std::lock_guard<std::mutex> lk(m_mutex);
	auto cit = m_categories.find(category);
	CommandCategory * cmdCategory = NULL;
	if (cit == m_categories.end()) {
		cmdCategory = new CommandCategory(m_context, m_log);
		m_categories[category] = cmdCategory;
	} else {
		cmdCategory = cit->second;
	}
	cmdCategory->registerHandler(method, handler);
	return cmdCategory;
}

void * CallbackManager::registerReplyHandler(UMSConnectorEventFunction handler,
			void * context) {
	std::lock_guard<std::mutex> lk(m_mutex);
	ReplyHandler * replyHandler = new ReplyHandler(this, context, m_log, handler);
	m_replies.insert(replyHandler);
	return replyHandler;
}

void * CallbackManager::registerSubscriptionHandler(const std::string & serviceUri,
			UMSConnectorEventFunction handler, void * context) {
	std::lock_guard<std::mutex> lk(m_mutex);

	EventHandler * evtHandler = new EventHandler(serviceUri, context, m_log, handler);
	m_subscriptions.insert(evtHandler);
	return evtHandler;
}

bool CallbackManager::unregisterSubscriptionHandler(void* data) {
	std::lock_guard<std::mutex> lk(m_mutex);
	bool ret_val = true;
	EventHandler* handler = reinterpret_cast<EventHandler*>(data);
	auto it = m_subscriptions.find(handler);
	if (it != m_subscriptions.end()) {
	  EventHandler* evtHandler = *it;
	  m_subscriptions.erase(it);
	  delete evtHandler;
	} else  {
	  ret_val = false;
	}
	return ret_val;
}

// CommandCategory
bool CallbackManager::CommandCategory::handleCommand(LSHandle * handle, LSMessage * message) {
	auto log_ptr = m_log.lock();
	if (log_ptr)
		LOG_LUNA_RX_MSG(*log_ptr, message);
	std::string method = LSMessageGetMethod(message);
	auto it = m_handlers.find(method);
	if (it != m_handlers.end()) {
		return it->second ? it->second(reinterpret_cast<UMSConnectorHandle*>(handle),
					reinterpret_cast<UMSConnectorMessage*>(message), m_context) : true;
	}
	if (log_ptr)
		LOG_ERROR((*log_ptr), MSGERR_RESOLVE_HCMD, "Unable to resolve command handler");
	return false;
}

// EventHandler
bool CallbackManager::EventHandler::handleEvent(LSHandle * handle, LSMessage * message) {
	auto log_ptr = m_log.lock();
	if (log_ptr)
		LOG_LUNA_RX_MSG(*log_ptr, message);
	return m_handler ? m_handler(reinterpret_cast<UMSConnectorHandle*>(handle),
			reinterpret_cast<UMSConnectorMessage*>(message), m_context) : true;
}

// ReplyHandler
bool CallbackManager::ReplyHandler::handleEvent(LSHandle * handle, LSMessage * message) {
	auto log_ptr = m_log.lock();
	if (log_ptr)
		LOG_LUNA_RX_MSG(*log_ptr, message);
	return m_handler ? m_handler(reinterpret_cast<UMSConnectorHandle*>(handle),
			reinterpret_cast<UMSConnectorMessage*>(message), m_context) : true;
}

