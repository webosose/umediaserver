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

#ifndef _RESOURCE_MANAGER_CLIENT_WRAPPER_H
#define _RESOURCE_MANAGER_CLIENT_WRAPPER_H

#include <ResourceManagerClient_common.h>

#ifndef __cplusplus
	#ifndef false
		#define false 0
	#endif
	#ifndef true
		#define true !false
	#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

ResourceManagerClientHandle * ResourceManagerClientCreate(const char * connection_id,
	bool (*policyActionHandler)(const char *, const char *, const char *,
	const char *, const char *));

bool ResourceManagerClientDestroy(ResourceManagerClientHandle *);

bool ResourceManagerClientRegisterPipeline(ResourceManagerClientHandle *, const char * type);

bool ResourceManagerClientUnregisterPipeline(ResourceManagerClientHandle *);

bool ResourceManagerClientAcquire(ResourceManagerClientHandle *, const char *resources,
		char ** resource_response);

bool ResourceManagerClientTryAcquire(ResourceManagerClientHandle *, const char *resources,
		char ** resource_response);

const char * ResourceManagerClientGetConnectionID(ResourceManagerClientHandle *);

bool ResourceManagerClientRelease(ResourceManagerClientHandle *, const char *resources);

bool ResourceManagerClientNotifyActivity(ResourceManagerClientHandle *);
bool ResourceManagerClientNotifyForeground(ResourceManagerClientHandle *);
bool ResourceManagerClientNotifyBackground(ResourceManagerClientHandle *);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __RESOURCE_MANAGER_CLIENT_WRAPPER_H */
