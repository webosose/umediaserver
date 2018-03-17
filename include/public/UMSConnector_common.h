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

#ifndef _UMS_CONNECTOR_COMMON_H
#define _UMS_CONNECTOR_COMMON_H

const int UMS_CONNECTOR_MAX_NAME_SIZE = 50;
const int UMS_CONNECTOR_REGISTER_RETRY_COUNT = 10;

// used by c wrapper interface
typedef struct UMSConnectorHandle UMSConnectorHandle;    // connector handle
typedef struct UMSConnectorMessage UMSConnectorMessage;  // actual message payload

typedef bool (*UMSConnectorEventFunction) (UMSConnectorHandle *hdl, UMSConnectorMessage *msg, void *context);
typedef struct {
	const char *event;
	UMSConnectorEventFunction function;
} UMSConnectorEventHandler;

typedef enum {
	UMS_CONNECTOR_PRIVATE_BUS,
	UMS_CONNECTOR_PUBLIC_BUS,
	UMS_CONNECTOR_DUAL_BUS,
	UMS_CONNECTOR_INVALID_BUS

} UMSConnectorBusType;

#endif  //_UMS_CONNECTOR_COMMON_H
