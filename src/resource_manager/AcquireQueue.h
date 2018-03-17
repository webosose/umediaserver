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

#ifndef __ACQUIREQUEUE_H__
#define __ACQUIREQUEUE_H__

#include <deque>
#include <functional>
#include "ResourceManager.h"

namespace uMediaServer {

struct acquire_request_connection_t {
	std::string connection_id;          // acquire originator connection id
	std::string service_name;           // acquire originator service name
	std::list<std::string> candidates;  // list of selected policy action candidates ids
	std::string resources;              // encoded resource request
	std::string response;               // cached request response
	bool trigger_policy;                // are we allowed to trigger policy action
};

class AcquireQueue {

public:
	AcquireQueue();

	void setResourceManager(ResourceManager * resource_manager);

	void enqueueRequest(const std::string & connection_id, const std::string & service_name,
						const std::string & resource_request, bool trigger_policy = true);

	void policyActionResult(const std::string & candidate_id, bool accepted);
	void resourceReleased();

	void setPolicyActionCallback(const std::function<bool(const std::string &,
														  const std::string &,
														  const resource_request_t &)> & cb);
	void setAcquireResultCallback(const std::function<bool(	const std::string &,
															const std::string &)> & cb);

	void logAcquireWaiters();
	void removeWaiter(const std::string & connection_id);

private:

	void processRequest();
	void retryRequest(bool trigger_policy = false);

	bool triggerPolicy();

	void informWaiter();

	ResourceManager * rm;

	std::deque<acquire_request_connection_t> acquire_requests;
	std::function<bool(const std::string & connection_id, const std::string & candidate_id,
					   const resource_request_t & failed_resources)> policy_action_callback;
	std::function<bool(const std::string & service_name,
					   const std::string & result)>	acquire_result_callback;
};

} // namespace ums


#endif
