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

#ifndef _RESOURCE_MANAGER_H
#define _RESOURCE_MANAGER_H

#include <sstream>
#include <vector>
#include <deque>
#include <list>
#include <map>
#include <set>
#include <mutex>
#include <memory>
#include <bitset>
#include <functional>
#include <chrono>
#include <ctime>

#include <pbnjson.hpp>
#include <libconfig.h++>
#include <boost/lexical_cast.hpp>
#include <boost/noncopyable.hpp>
#include <dto_types.h>
#include <uMediaTypes.h>

// Convert struct timespec struct to ms resoltion
#define GET_MS(_time_ ) (_time_.tv_sec * 1000) + (_time_.tv_nsec / 1000000UL)

namespace uMediaServer {

// indicate incoming resource request did not specify an index
const uint16_t MAX_RESOURCES_QTY = 8*sizeof(size_t);

// unit of system resource
// note: number of instances limited by sizeof(size_t) == 32 on target
//
struct system_resource_cfg_t {
	system_resource_cfg_t(const std::string & id, uint32_t qty = 1, const std::string & name = "",
						  const std::set<std::string> & mutexes = std::set<std::string>())
	: id(id), name(name), max_qty(qty), mutex_resources(mutexes)
	{
		for (size_t i = 0; i < max_qty; ++i)
			units.set(i);
	}
	// available resource units
	size_t qty() const {
		return units.count();
	}
	// allocated resource units
	size_t allocated() const {
		return max_qty - units.count();
	}
	bool operator == (const system_resource_cfg_t & other) const {
		return max_qty == other.max_qty && units == other.units &&
			   id == other.id && name == other.name && mutex_resources == other.mutex_resources;
	}
	bool operator != (const system_resource_cfg_t & other) const {
		return !(*this == other);
	}
	friend std::ostream & operator << (std::ostream & os, const system_resource_cfg_t & res) {
		return os << res.id << "(" << res.name << ")" << "="
				<< res.qty() << "(" << res.max_qty << ")"
				<< " [" << res.units.to_string().substr(MAX_RESOURCES_QTY - res.max_qty) << "]";
	}

	// resource id
	std::string id;
	// resource description
	std::string name;
	// max quantity specified in configuration
	uint32_t max_qty;
	// resource units allocation info true == free, false == occupied
	std::bitset<MAX_RESOURCES_QTY> units;
	// resource mutually exclusive modes
	std::set<std::string> mutex_resources;
};

// resource unit = id + index
//
struct resource_unit_t {
	resource_unit_t(const std::string & id, size_t index = (size_t)-1)
		: id(id), index(index) {}
	resource_unit_t(const resource_unit_t & other)
		: id (other.id), index(other.index) {}
	bool operator == (const resource_unit_t & other) const {
		return  index == other.index && id == other.id;
	}
	bool operator != (const resource_unit_t & other) const {
		return !(*this == other);
	}
	friend std::ostream & operator << (std::ostream & os,
			const resource_unit_t & ru) {
		os << ru.id << "[";
		ru.index == (size_t)-1 ? os << '*' : os << ru.index;
		return os << "]";
	}

	std::string id;
	size_t index;
};

// resource list - list of resource units
typedef std::list<resource_unit_t> resource_list_t;

// requested resource unit descriptor
//
struct resource_descriptor_t : resource_unit_t {
	resource_descriptor_t(const std::string & id, size_t qty = 1, size_t index = (size_t)-1,
						  const std::string & attribute = "", size_t min = (size_t)-1)
		: resource_unit_t(id, index), qty(qty), attribute(attribute), min(min) {}
	resource_descriptor_t(const resource_descriptor_t & other)
		: resource_unit_t(other), qty(other.qty), attribute(other.attribute), min(other.min) {}
	resource_descriptor_t(const resource_unit_t & unit)
		: resource_descriptor_t(unit.id, 1, unit.index) {}

	bool operator == (const resource_descriptor_t & other) const {
		return resource_unit_t::operator==(other) &&
				qty == other.qty && attribute == other.attribute;
	}

	bool operator != (const resource_descriptor_t & other) const {
		return !(*this == other);
	}

	friend std::ostream & operator << (std::ostream & os,
			const resource_descriptor_t & re) {
		os << (resource_unit_t)re << "=" << re.qty;
		if (!re.attribute.empty()) {
			os << "(" << re.attribute << ")";
		}
		return os;
	}

	size_t qty;    // total quantity requested
	size_t min;    // min quantity allowed, if specified
	std::string attribute;
};

// resource request
// constructed from "acquire" / "release" resource requests.
//
// Parse structure:
//   +-> resource descriptor #0  (struct resource_descriptor_t)
//   |   |
//   |   +->id
//   |   |
//   |   +->index
//   |   |
//   |   +->qty
//   |   |
//   |   +->attribute
//   |
//   +-> resource descriptor #N
//       |
//       +->id
//       |
//       +->index
//       |
//       +->qty
//       |
//       +->attribute
struct resource_request_t : std::list<resource_descriptor_t> {
	resource_request_t() : qty(0) {}
	resource_request_t( const std::list<resource_descriptor_t> & other)
		: std::list<resource_descriptor_t>(other) { count(); }
	resource_request_t(const resource_list_t & list) {
		for (const auto & ru : list) push_back(resource_descriptor_t(ru));
		count();
	}

	resource_request_t & operator = (const resource_request_t & other) {
		std::list<resource_descriptor_t>::operator = (other);
		qty = other.qty;
		return *this;
	}

	resource_request_t & operator = (const std::list<resource_descriptor_t> & other) {
		std::list<resource_descriptor_t>::operator = (other);
		count();
		return *this;
	}

	// compact request by merging anonymous requests of the same kind in one
	void compact() {
		for (auto resource_it = begin(); resource_it != end();) {
			// check only anonymous requests
			if (resource_it->index > MAX_RESOURCES_QTY) {
				auto prev_it = begin();
				for (; prev_it != resource_it; ++prev_it) {
					// anonymous requests of the same kind - merge them
					if (prev_it->index > MAX_RESOURCES_QTY && prev_it->id == resource_it->id) {
						prev_it->qty += resource_it->qty;
						resource_it = erase(resource_it);
						break;
					}
				}
				// resource requests merged - avoid iterator increment
				if (prev_it != resource_it)
					continue;
			}
			++resource_it;
		}
	}

	size_t count() {
		qty = 0;
		for (const auto & descriptor : static_cast<std::list<resource_descriptor_t>>(*this)) {
			qty += descriptor.qty;
		}
		return qty;
	}
	size_t qty;
};

inline resource_request_t operator - (const resource_request_t & a, const resource_request_t & b) {
	resource_request_t c(a);
	auto cit = c.begin();
	while (cit != c.end()) {
		auto bit = b.begin();
		for (;bit != b.end(); ++bit) {
			if (cit->id == bit->id) {
				// indexed or anonymous with the same qty
				if (*cit == *bit) {
					cit = c.erase(cit);
					break;
				// anonymous resource
				} else if (cit->index == -1 && bit->index == -1) {
					// less qty
					if (cit->qty > bit->qty) {
						cit->qty -= bit->qty;
						++cit;
					// more or the same qty
					} else {
						cit = c.erase(cit);
					}
					break;
				}
			}
		}
		if (bit == b.end())
			++cit;
	}
	c.count();
	return c;
}

typedef std::list<resource_request_t> dnf_request_t;

struct resource_manager_connection_t {
	typedef enum {
		READY = 0,                          // no active policy handling - ready to recv policy action
		SENT,                               // policy action sent but no reply received yet
		DENIED                              // policy processed and denied
	} policy_state_t;
	std::string connection_id;              // rm registration id
	std::string type;                       // pipeline type (drives priority)
	std::string service_name;               // rmc service name
	uint64_t timestamp;                     // last pipeline usage timestamp (drives LRU)
	bool is_managed;                        // whether pipeline foreground state is managed by MDC
	policy_state_t policy_state;            // state of policy action handling
	bool is_foreground;                     // app foreground flagg making pipeline imune to policy
	bool is_focus;                          // prevent policy action on focused video element
	bool is_visible;                        // pipelines that are not visible are subject to policy action before others
	resource_request_t policy_resources;    // resources to free upon policy
	resource_list_t resources;              // allocated resources
	uint32_t priority;                      // priority
	bool subscribed;
	std::string sub_type;                   // app window type
	std::string playing_state;
	int32_t pid;
	bool isChangeResolution;
};

typedef std::map<std::string, resource_manager_connection_t> resource_manager_connection_map_t;

/**
 * Pool of system resources
 */
class ResourcePool : boost::noncopyable {
public:
	typedef std::unique_ptr<ResourcePool> ptr_t;
	ResourcePool(const libconfig::Setting & config);
	/**
	 * Acquire specific resource with auto roll-back on failure
	 * @param IN resources to acquire
	 * @param OUT failed resources
	 * @returns list of actually allocated resources
	 */
	resource_list_t acquire(const resource_request_t & resources, resource_request_t & failures);
	/**
	 * Release specific resource and put it back to pool. Performs filtering on non-existent
	 * and clamping of extra releases
	 * @param resource to release
	 * @returns actually released resources
	 */
	resource_list_t release(const resource_list_t & resources);
	/**
	 *	Log pool state
	 */
	void print() const;

	/**
	 *	factory API
	 */
	void update(std::string id, uint32_t qty, bool remove=false);

	int32_t get_max_qty(std::string id);

private:
	/**
	 * Get list of allocated mutex resources
	 * @param IN resource request
	 * @returns list of blocking mutex resources to free
	 */
	resource_request_t allocated_mutexes(const resource_request_t & resources);

	std::map<std::string, system_resource_cfg_t> pool;
};

//--- singleton
class ResourceManager : boost::noncopyable {
public:
	typedef std::function<void(const std::string &, const resource_list_t &)> callback_t;
	typedef std::function<void(const std::string &, const int &, ums::disp_res_t & res)> acquire_plane_callback_t;
	typedef std::function<void(const std::string &, const int &)> release_plane_callback_t;
	ResourceManager(const libconfig::Config &config);

	void setLogLevel(const std::string & level);

	~ResourceManager();

	resource_manager_connection_t * findConnection(const std::string &connection_id);
	const resource_manager_connection_map_t& getConnections() const {
		return connections;
	}

	bool registerPipeline(const std::string &connection_id, const std::string &type, bool is_managed = false, bool is_foreground = false);

	bool unregisterPipeline(const std::string &connection_id);

	bool resetPipeline(const std::string &connection_id);

	bool acquire(const std::string &connection_id,
			const std::string &acquire_request,
			dnf_request_t &failed_resources,
			std::string &acquire_response);
	bool reacquire(const std::string &connection_id,
		const std::string &acquire_request,
		dnf_request_t &failed_resources,
		std::string &acquire_response);

	bool release(const std::string &connection_id,const std::string &resource_request);

	bool setServiceName(const std::string &connection_id,
			const std::string &service_name) {
		lock_t l(mutex);
		auto it = connections.find(connection_id);
		if (it != connections.end()) {
			it->second.service_name = service_name;
			return true;
		}
		return false;
	}

	bool encodeResourceRequest(const resource_request_t & resources,
			std::string & resource_request);

	// notify resource manager about some pipeline activity
	bool notifyActivity(const std::string & id);

	// indicate application is in foreground should not be selected by policy action.
	bool notifyForeground(const std::string & id);

	// indicate applicaion is in background and can be selected by policy action.
	bool notifyBackground(const std::string & id);

	// indicate connection is focused and should not be selected by policy action.
	bool notifyFocus(const std::string & id);

	// indicate connection is visible or not
	// if connection is not visible it should be selected by policy action
	// over another connection that is visible
	bool notifyVisibility(const std::string & id, bool is_visible);

	bool notifyPipelineStatus(const std::string & id, const std::string& playing_state, const int32_t pid);

	// Policy API
	bool selectPolicyCandidates(const std::string &connection_id,
			const resource_request_t &failed_resources,
			std::list<std::string> &candidates);

	bool isValidType(const std::string &type);
	void setReleaseCallback(callback_t callback) {
		m_release_callback = callback;
	}
	void setAcquireCallback(callback_t callback);
	void setPolicyActionCallback(callback_t callback) {
		m_policy_action_callback = callback;
	}

	void setForegroundInfoCallback(std::function<void()> callback) {
		m_foreground_callback = callback;
	}

	void setUpdateStatusCallback(std::function<bool(const std::string &, const std::string &, const std::string &)> callback) {
		m_update_status_callback = callback;
	}

	void setUpdateResourcesStatusCallback(std::function<bool(const std::string &, const std::string &)> callback) {
		m_update_resources_status_callback = callback;
	}

	void addResource(const std::string &id, uint32_t qty);
	void removeResource(const std::string &id);
	void updateResource(const std::string &id, uint32_t qty);

	// TODO: remove when rm switched to registry iface
	void setPriority(const std::string & type, uint32_t priority);
	void removePriority(const std::string & type);

	bool setPolicyState(std::string id, resource_manager_connection_t::policy_state_t state) {
		lock_t l(mutex);
		auto it = connections.find(id);
		if(it != connections.end()) {
			it->second.policy_state = state;
			return true;
		}
		return false;
	}

	void setPolicyState(resource_manager_connection_t *c,
						resource_manager_connection_t::policy_state_t state) {
		lock_t l(mutex);
		c->policy_state = state;
	}

	void resetPolicyState(resource_manager_connection_t::policy_state_t initial,
						  resource_manager_connection_t::policy_state_t final) {
		lock_t l(mutex);
		for (auto & conn : this->connections) {
			if (conn.second.policy_state == initial)
				conn.second.policy_state = final;
		}
	}

	bool getForegroundInfo(pbnjson::JValue& forground_app_info_out);
	bool getActivePipeline(const std::string &id, pbnjson::JValue& connection_info_out);

	void setManaged(const std::string & id);
	bool getManaged(const std::string & id);
	void reclaimResources();

private:
	std::string key_source;   // character source for unique id generation
	std::recursive_mutex mutex;
	typedef std::lock_guard<std::recursive_mutex> lock_t;

	// connections to resource manager
	// Note: not active pipelines which will
	//       which will be stored in policy manager
	resource_manager_connection_map_t connections;

	// pool of HW resources. populated from configuration file
	ResourcePool::ptr_t system_resources;

	void showConnections();

	void showSystemResources() const;

	// TODO: remove when rm switched to registry iface
	void readResourceConfig(const libconfig::Config &config);
	void readPipelinePolicies(const libconfig::Setting &root);

	// Active resources
	//  Note: deprecated API until full policy engine is implemented
	// TODO: update to new data structures
	void addActiveResource(resource_manager_connection_t & owner,
						   const resource_unit_t & unit);
	void remActiveResource(resource_manager_connection_t & owner,
						   const resource_unit_t & unit);

	bool findPriority(const std::string &type, uint32_t *priority);
	bool findType(const std::string &type);
	uint64_t timeNow(void) {
		auto now = std::chrono::steady_clock::now();
		return std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
	}

	void showActivePipelines();

	bool decodeReleaseRequest(const std::string & release_request,
			resource_list_t & resources);

	bool decodeAcquireRequest(const std::string & acquire_request,
			dnf_request_t & resources);
	void replaceResourceInString(std::string &st1, const std::string &st2, uint32_t number);

	std::map<std::string,uint32_t > policy_priority;
	callback_t m_release_callback;
	callback_t m_acquire_callback;
	callback_t m_policy_action_callback;
	std::function<bool(const std::string&, const std::string&, const std::string&)> m_update_status_callback;
	std::function<bool(const std::string&, const std::string&)> m_update_resources_status_callback;
	std::function<void()> m_foreground_callback;
};

} // namespace uMediaServer

#endif  // _RESOURCE_MANAGER_H
