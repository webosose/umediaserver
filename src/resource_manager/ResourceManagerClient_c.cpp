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
//

#include <glib.h>
#include <functional>
#include "ResourceManagerClient.h"
#include "ResourceManagerClient_c.h"
#include <Logger_macro.h>

using namespace std;
using namespace uMediaServer;
namespace {
	Logger _log(UMS_LOG_CONTEXT_RESOURCE_MANAGER_CLIENT);
}

static ResourceManagerClientHandle * _ResourceManagerClientCreate(const char * connection_id,
		bool (*policyActionHandler)(const char *, const char *, const char*,
		const char *, const char *));

// @f ResourceManagerClientCreate
// @brief create Resource Manager connection
// @param connection_id NULL = external client, value = internal managed client
//
ResourceManagerClientHandle * ResourceManagerClientCreate(const char * connection_id,
		bool (*policyActionHandler)(const char *, const char *, const char*,
		const char *, const char *))
{
	return _ResourceManagerClientCreate(connection_id, policyActionHandler);
}

static ResourceManagerClientHandle * _ResourceManagerClientCreate(const char * connection_id,
		bool (*policyActionHandler)(const char *, const char *, const char*,
		const char *, const char *))
{
	try {
		ResourceManagerClient *rc;

		if ( NULL == connection_id )
			rc = new ResourceManagerClient();
		else
			rc = new ResourceManagerClient(connection_id);

		if (rc == NULL ) {
			LOG_ERROR(_log, MSGERR_RMC_CREATE, "ResourceManagerClient failed.");
			return NULL;
		}

		if (policyActionHandler == NULL ) {
			LOG_ERROR(_log, MSGERR_NO_POLICY_ACT, "policyActionHandler cannot be NULL");
			return NULL;
		}

		rc->registerPolicyActionHandler(policyActionHandler);
		return reinterpret_cast<ResourceManagerClientHandle *>(rc);
	}
	catch (...) {
		return NULL;
	}
}

bool ResourceManagerClientDestroy(ResourceManagerClientHandle  *handle)
{
	ResourceManagerClient * rc = reinterpret_cast<ResourceManagerClient  *>(handle);
	if (rc) {
		delete rc;
	}
	return true;
}

bool ResourceManagerClientRegisterPipeline(ResourceManagerClientHandle  *handle, const char * type)
{
	ResourceManagerClient * rc = reinterpret_cast<ResourceManagerClient *>(handle);
	if (rc) {
		return rc->registerPipeline(type);
	}
	return false;
}

bool ResourceManagerClientUnregisterPipeline(ResourceManagerClientHandle  *handle)
{
	ResourceManagerClient * rc = reinterpret_cast<ResourceManagerClient *>(handle);
	if (rc) {
		return rc->unregisterPipeline();
	}
	return false;
}

bool ResourceManagerClientAcquire(ResourceManagerClientHandle  *handle,
		const char * resources, char ** resource_response)
{
	ResourceManagerClient * rc = reinterpret_cast<ResourceManagerClient *>(handle);
	if (rc) {
		string response;
		bool rv = rc->acquire(resources, response);

		*resource_response = (char*)malloc(response.length() + 1);
		if ( ! *resource_response ) {
			LOG_ERROR(_log, MSGERR_OUTOFMEMORY, "malloc failed.");
			return false;
		}

		strcpy(*resource_response,response.c_str());
		return rv;

	}
	return false;
}

bool ResourceManagerClientTryAcquire(ResourceManagerClientHandle  *handle,
		const char * resources, char ** resource_response)
{
	ResourceManagerClient * rc = reinterpret_cast<ResourceManagerClient *>(handle);
	if (rc) {
		string response;
		bool rv = rc->tryAcquire(resources, response);

		*resource_response = (char*)malloc(response.length() + 1);
		if ( ! *resource_response ) {
			LOG_ERROR(_log, MSGERR_OUTOFMEMORY, "malloc failed.");
			return false;
		}

		strcpy(*resource_response,response.c_str());
		return rv;

	}
	return false;
}

const char * ResourceManagerClientGetConnectionID(ResourceManagerClientHandle  *handle)
{
	ResourceManagerClient * rc = reinterpret_cast<ResourceManagerClient *>(handle);
	if (rc) {
		return rc->getConnectionID();
	}
	return NULL;
}

bool ResourceManagerClientRelease(ResourceManagerClientHandle  *handle, const char * resources)
{
	ResourceManagerClient * rc = reinterpret_cast<ResourceManagerClient *>(handle);
	if (rc) {
		return rc->release(resources);
	}
	return false;
}

bool ResourceManagerClientNotifyActivity(ResourceManagerClientHandle  *handle)
{
	ResourceManagerClient * rc = reinterpret_cast<ResourceManagerClient *>(handle);
	if (rc) {
		return rc->notifyActivity();
	}
	return false;
}

bool ResourceManagerClientNotifyForeground(ResourceManagerClientHandle  *handle)
{
	ResourceManagerClient * rc = reinterpret_cast<ResourceManagerClient *>(handle);
	if (rc) {
		return rc->notifyForeground();
	}
	return false;
}

bool ResourceManagerClientNotifyBackground(ResourceManagerClientHandle  *handle)
{
	ResourceManagerClient * rc = reinterpret_cast<ResourceManagerClient *>(handle);
	if (rc) {
		return rc->notifyBackground();
	}
	return false;
}

