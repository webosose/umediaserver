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

#include "AcquireQueue.h"
#include <Logger.h>
#include <Logger_id.h>
#include <Logger_macro.h>

namespace uMediaServer {

namespace {
	Logger __log(UMS_LOG_CONTEXT_RESOURCE_MANAGER);
}

AcquireQueue::AcquireQueue() : rm (nullptr) {}

void AcquireQueue::setResourceManager(ResourceManager *resource_manager) {
	rm = resource_manager;
	rm->setPolicyActionCallback([this](const std::string & id, const resource_list_t & resources) {
		if (policy_action_callback)
			policy_action_callback("com.webos.media", id, resource_request_t(resources));
	});
}

void AcquireQueue::enqueueRequest(const std::string & connection_id, const std::string & service_name,
							 const std::string & resource_request, bool trigger_policy) {
	acquire_requests.push_back({ connection_id, service_name, {}, resource_request, "", trigger_policy });
	// if this is the only request - try to process it immediately
	if (acquire_requests.size() == 1) {
		processRequest();
	}
}

void AcquireQueue::policyActionResult(const std::string & candidate_id, bool accepted) {
	rm->setPolicyState(candidate_id, (accepted ? resource_manager_connection_t::policy_state_t::READY
											   : resource_manager_connection_t::policy_state_t::DENIED));
	// we have pa  denial - reprocess request
	if (!accepted) {
		auto & candidates = acquire_requests.front().candidates;
		if (!acquire_requests.empty() &&
				std::find(candidates.begin(), candidates.end(), candidate_id) != candidates.end()) {
			processRequest();
		}
	}
}

void AcquireQueue::resourceReleased() {
	if (!acquire_requests.empty())
		retryRequest();
}

void AcquireQueue::setPolicyActionCallback(
		const std::function<bool(const std::string &, const std::string &,
								 const resource_request_t &)> & cb) {
	policy_action_callback = cb;
}

void AcquireQueue::setAcquireResultCallback(
		const std::function<bool(const std::string &, const std::string &)> &cb) {
	acquire_result_callback = cb;
}

void AcquireQueue::logAcquireWaiters() {
	LOG_TRACE(__log, "========= ACQUIRE WAITERS ==========");
	for (const auto & waiter : acquire_requests) {
		std::stringstream s;
		s << "waiter_id=" << waiter.connection_id << ", candidates_id=[";
		for (const auto & candidate : waiter.candidates) {
			s << candidate << ", ";
		}
		s << "], service_name=" << waiter.service_name
				<< ", resources=" << waiter.resources;

		LOG_TRACE(__log, "%s", s.str().c_str());
	}
	LOG_TRACE(__log, "====================================");
}

void AcquireQueue::removeWaiter(const std::string & connection_id) {
	for (auto it = acquire_requests.begin(); it != acquire_requests.end(); ++it) {
		if (it->connection_id == connection_id) {
			LOG_TRACE(__log, "Removing acquire_waiter: connection_id=%s,"
					"service_name = %s, resources=%s",
					it->connection_id.c_str(),
					it->service_name.c_str(),
					it->resources.c_str());
			acquire_requests.erase(it);
			break;
		}
	}
	logAcquireWaiters();
}

// first request processing attempt
void AcquireQueue::processRequest() {
	logAcquireWaiters();
	acquire_request_connection_t & request = acquire_requests.front();
	dnf_request_t failed_resources;

	// handling malformed request
	if (!rm->acquire(request.connection_id, request.resources,
			failed_resources, request.response)) {
		LOG_ERROR(__log, MSGERR_ACQUIRE_FAILED, "acquire call failed");
		return informWaiter();
	}

	// acquire succeded or we had tryAcquire - pop waiter
	if (failed_resources.empty() || !request.trigger_policy) {
		return informWaiter();
	}

	// select policy candidates
	request.candidates.clear();
	for (const resource_request_t & shortage : failed_resources) {
		std::list<std::string> candidates;
		if (rm->selectPolicyCandidates(request.connection_id, shortage, candidates)) {
			// prefer to bother as less as possible  policy candidates
			if (request.candidates.empty() || request.candidates.size() > candidates.size()) {
				request.candidates = std::move(candidates);
			}
		}
	}

	// unable to find suitable candidates
	if (request.candidates.empty()) {
		LOG_WARNING(__log, MSGERR_POLCAND_FIND, "no suitable candidate found");
		return informWaiter();
	}

	// trigger policy action
	if (!triggerPolicy())
		return informWaiter();
}

// retry request processing
void AcquireQueue::retryRequest(bool trigger_policy) {
	logAcquireWaiters();
	acquire_request_connection_t & request = acquire_requests.front();
	dnf_request_t failed_resources;

	// we already processed this request at least once - it has to be valid
	rm->acquire(request.connection_id, request.resources, failed_resources, request.response);

	// acquire succeded or no policy candidates left
	if (failed_resources.empty() || request.candidates.empty()) {
		return informWaiter();
	}

	// try next cadidate policy action
	if (trigger_policy && !triggerPolicy()) {
		return informWaiter();
	}
}

bool AcquireQueue::triggerPolicy() {
	auto & request = acquire_requests.front();
	if (policy_action_callback) {
		for (const auto & candidate : request.candidates) {
			auto candidate_conn = rm->findConnection(candidate);
			if (!candidate_conn) {
				LOG_ERROR(__log, MSGERR_POLICY_FAILED, "failed to send policy action");
				return false;
			}
			if (candidate_conn->policy_state != resource_manager_connection_t::policy_state_t::SENT &&
					!policy_action_callback(request.connection_id, candidate, candidate_conn->policy_resources)) {
				LOG_ERROR(__log, MSGERR_POLICY_FAILED, "failed to send policy action");
				return false;
			}
			candidate_conn->policy_state = resource_manager_connection_t::policy_state_t::SENT;
		}
	}
	return !!policy_action_callback;
}

void AcquireQueue::informWaiter() {
	const auto & request = acquire_requests.front();

	// reset policy action state for all pipelines that denied requests
	rm->resetPolicyState(resource_manager_connection_t::policy_state_t::DENIED,
						 resource_manager_connection_t::policy_state_t::READY);

	// call result callback
	if (!acquire_result_callback || !acquire_result_callback(request.service_name, request.response)) {
		LOG_ERROR(__log, MSGERR_SEND_ACQUIRE_RESULT, "failed to send acquire result");
	}

	// pop out request
	acquire_requests.pop_front();

	// and try to process the next one
	if (!acquire_requests.empty()) {
		processRequest();
	}
}

}
