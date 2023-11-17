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

#include <glib.h>
#include "UMSConnector.h"
#include "UMSConnector_wrapper.h"
#include <Logger_macro.h>

using namespace std;
using namespace uMediaServer;

UMSConnectorHandle * UMSConnectorCreate(char *name, GMainLoop *mainLoop,void *ctx)
{
	try {
		UMSConnector *connector = new UMSConnector(name, mainLoop,ctx);
		return reinterpret_cast<UMSConnectorHandle *>(connector);
	}
	catch (...) {
		return NULL;
	}
}

UMSConnectorHandle * UMSConnectorCreatePrivate(char *name, GMainLoop *mainLoop,void *ctx)
{
	try {
		UMSConnector *connector = new UMSConnector(name, mainLoop, ctx, UMS_CONNECTOR_PRIVATE_BUS);
		return reinterpret_cast<UMSConnectorHandle *>(connector);
	}
	catch (...) {
		return NULL;
	}
}

bool UMSConnectorDestroy(UMSConnectorHandle *handle)
{
	UMSConnector * connector = reinterpret_cast<UMSConnector *>(handle);
	if (connector) {
		delete connector;
	}
	return true;
}

bool UMSConnectorWait(UMSConnectorHandle *handle)
{
	UMSConnector * connector = reinterpret_cast<UMSConnector *>(handle);
	if (connector) {
		return connector->wait();
	}
	return false;
}

bool UMSConnectorAddEventHandler(UMSConnectorHandle *handle, string event, UMSConnectorEventFunction func)
{
	UMSConnector * connector = reinterpret_cast<UMSConnector *>(handle);
	if (connector) {
		return connector->addEventHandler( std::move(event), func );
	}
	return false;
}

bool UMSConnectorAddEventHandlers(UMSConnectorHandle *handle, UMSConnectorEventHandler *eventMethods)
{
	UMSConnector * connector = reinterpret_cast<UMSConnector *>(handle);
	if (connector) {
		return connector->addEventHandlers( eventMethods );
	}
	return false;

}

const char* UMSConnectorGetMessageText(UMSConnectorHandle *handle, UMSConnectorMessage *message)
{
	UMSConnector * c = reinterpret_cast<UMSConnector *>(handle);
	if (c) {
		return c->getMessageText(message);
	}
	return NULL;
}

const char* UMSConnectorGetMessageSender(UMSConnectorHandle * handle, UMSConnectorMessage *message)
{
	UMSConnector * connector = reinterpret_cast<UMSConnector *>(handle);
	if (connector) {
		return connector->getMessageSender(message);
	}
	return NULL;
}

const char* UMSConnectorGetSenderServiceName(UMSConnectorHandle * handle, UMSConnectorMessage *message)
{
	UMSConnector * connector = reinterpret_cast<UMSConnector *>(handle);
	if (connector) {
		return connector->getSenderServiceName(message);
	}
	return NULL;
}

bool UMSConnectorSendMessage(UMSConnectorHandle *handle,
		const char *uri, const char * payload,
		UMSConnectorEventFunction func, void * ctx)
{
	UMSConnector * connector = reinterpret_cast<UMSConnector *>(handle);
	if (connector) {
		return connector->sendMessage(uri,payload,func,ctx);
	}
	return false;
}

bool UMSConnectorSendSimpleResponse(UMSConnectorHandle * handle, UMSConnectorHandle * sender,
									UMSConnectorMessage* message, bool resp)
{
	UMSConnector * connector = reinterpret_cast<UMSConnector *>(handle);
	if (connector) {
		return connector->sendSimpleResponse(sender, message, resp);
	}
	return false;
}

bool UMSConnectorSendResponse(UMSConnectorHandle * handle, UMSConnectorHandle * sender,
									UMSConnectorMessage* message, const char *key, const char *value)
{
	UMSConnector * connector = reinterpret_cast<UMSConnector *>(handle);
	if (connector) {
		return connector->sendResponse(sender, message, key, value);
	}
	return false;
}

bool UMSConnectorSubscribe(UMSConnectorHandle *handle,
							const char *uri, const char *payload,
							UMSConnectorEventFunction func,	void * ctx)
{
	UMSConnector * connector = reinterpret_cast<UMSConnector *>(handle);

	if (connector) {
		return connector->subscribe(uri,payload,func,ctx);
	}
	return false;
}

bool UMSConnectorAddSubscriber (UMSConnectorHandle * handle,UMSConnectorHandle *sender,
										UMSConnectorMessage *message)
{
	UMSConnector * connector = reinterpret_cast<UMSConnector *>(handle);

	if (connector) {
		return connector->addSubscriber(sender, message);
	}
	return false;
}

bool UMSConnectorSendChangeNotificationLongLong(UMSConnectorHandle * handle,const char * name,unsigned long long value)
{
	UMSConnector * connector = reinterpret_cast<UMSConnector *>(handle);

	if (connector) {
		return connector->sendChangeNotificationLongLong(name, value);
	}
	return false;
}

bool UMSConnectorSendChangeNotificationString(UMSConnectorHandle * handle,const char * name, char* value)
{
	UMSConnector * connector = reinterpret_cast<UMSConnector *>(handle);

	if (connector) {
		return connector->sendChangeNotificationString(name, value);
	}
	return false;
}

bool UMSConnectorSendChangeNotificationJsonString(UMSConnectorHandle * handle,const char * json)
{
	UMSConnector * connector = reinterpret_cast<UMSConnector *>(handle);

	if (connector) {
		return connector->sendChangeNotificationJsonString(json);
	}
	return false;
}

GMainLoop * UMSConnectorGetMainLoop(UMSConnectorHandle* handle)
{
	UMSConnector * connector = reinterpret_cast<UMSConnector *>(handle);
	if (connector) {
		return connector->getMainLoop();
	}
	return NULL;
}
