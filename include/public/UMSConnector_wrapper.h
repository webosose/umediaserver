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

#ifndef _UMS_CONNECTOR_WRAPPER_H
#define _UMS_CONNECTOR_WRAPPER_H

#include <UMSConnector_common.h>

#ifndef __cplusplus
	#define true 1
	#define false 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

	UMSConnectorHandle * UMSConnectorCreate(char *name_, GMainLoop *mainLoop_, void *ctx);
	UMSConnectorHandle * UMSConnectorCreatePrivate(char *name_, GMainLoop *mainLoop_, void *ctx);
	bool UMSConnectorDestroy(UMSConnectorHandle *handle);
	bool UMSConnectorWait(UMSConnectorHandle *handle);
	bool UMSConnectorStop(UMSConnectorHandle *handle);
	bool UMSConnectorAddEventHandler(UMSConnectorHandle *handle, char *event,
									UMSConnectorEventFunction func);
	bool UMSConnectorAddEventHandlers(UMSConnectorHandle *handle, UMSConnectorEventHandler *methods);
	const char* UMSConnectorGetMessageText(UMSConnectorHandle *handle, UMSConnectorMessage *message);
	const char* UMSConnectorGetMessageSender(UMSConnectorHandle *handle, UMSConnectorMessage *message);
	const char* UMSConnectorGetSenderServiceName(UMSConnectorHandle *handle, UMSConnectorMessage *message);

	// send message to URI service.  Not for subscription messages.
	bool UMSConnectorSendMessage(UMSConnectorHandle *handle,
			const char *uri, const char * payload,
			UMSConnectorEventFunction func, void * ctx);

	// send simple true/false response to incomming messages
	bool UMSConnectorSendSimpleResponse(UMSConnectorHandle *handle, UMSConnectorHandle *sender,
			UMSConnectorMessage *message, bool resp);

	// send key=value JSON formatted response. 	e.g. "{"key":"value"}
	bool UMSConnectorSendResponse(UMSConnectorHandle *handle, UMSConnectorHandle *sender,
			UMSConnectorMessage *message, const char *key, const char *value);

	GMainLoop * UMSConnectorGetMainLoop(UMSConnectorHandle *handle);

	// Subscription functions require the Message Handle in addition Connector Handle

	// client side: subscribe for subscription messages
	bool UMSConnectorSubscribe(UMSConnectorHandle *handle,
			const char *uri, const char *payload,
			UMSConnectorEventFunction func,	void * ctx);

	// server side: add a subscriber
	bool UMSConnectorAddSubscriber(UMSConnectorHandle *handle,  // UMSConnector object handle
			UMSConnectorHandle *subscriber,  // sender reference
			UMSConnectorMessage *message); // payload

	bool UMSConnectorSendChangeNotificationLongLong(UMSConnectorHandle *handle,
			const char *name, unsigned long long value);

	bool UMSConnectorSendChangeNotificationString(UMSConnectorHandle *handle,
			const char *name, char *value);

	bool UMSConnectorSendChangeNotificationJsonString(UMSConnectorHandle *handle,
			const char *json);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _UMS_CONNECTOR_WRAPPER_H */
