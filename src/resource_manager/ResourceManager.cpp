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

#include <set>
#include <functional>
#include <boost/lexical_cast.hpp>  // for lexical_cast<string>(number)
#include <boost/algorithm/string_regex.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <pbnjson.hpp>

#include <ResourceManager.h>
#include <Logger_macro.h>

//#define RESOURCE_MANAGER_DEBUG 1

using namespace std;
using namespace libconfig;
using namespace pbnjson;
using namespace boost;
using namespace std::chrono;
using namespace uMediaServer;

namespace {
Logger _log(UMS_LOG_CONTEXT_RESOURCE_MANAGER);
}

#define RETURN_IF(exp,rv,msgid,format,args...) \
		{ if(exp) { \
			LOG_ERROR(_log, msgid, format, ##args); \
			return rv; \
		} \
		}

static string intToString(int number)
{
	stringstream ss;   //create a stringstream
	ss << number;      //add number to the stream
	return ss.str();   //return a string with the contents of the stream
}

//
// ---- PUBLIC
//

ResourcePool::ResourcePool(const libconfig::Setting & config) {
	LOG_DEBUG(_log, "+ Global resources+");

	auto parse_resource_config = [] (const Setting & resource_cfg)->system_resource_cfg_t {
		string id, name;
		uint32_t qty;

		if ( !(resource_cfg.lookupValue("id", id)
				&& resource_cfg.lookupValue("name", name)
				&& resource_cfg.lookupValue("qty", qty)) ) {
			throw std::runtime_error("invalid resource");
		}

		if (qty > MAX_RESOURCES_QTY) {
			LOG_WARNING(_log, "MAX_RES_QTY_EXCEEDED",
						"\t! resource %s: quantity forcefully set to supported maximum %d",
						id.c_str(), MAX_RESOURCES_QTY);
			qty = MAX_RESOURCES_QTY;
		}

		LOG_DEBUG(_log, "\t+ resource +");
		LOG_DEBUG(_log, "\t\t id=%s", id.c_str());
		LOG_DEBUG(_log, "\t\t name=%s", name.c_str());
		LOG_DEBUG(_log, "\t\t qty=%d", qty);

		return {id, qty, name};
	};

	for(size_t i = 0; i < config.getLength(); ++i) {
		const Setting & resource_cfg = config[i];
		std::string type;
		if (resource_cfg.lookupValue("type", type) && type == "MUTUALLY_EXCLUSIVE") {
			const Setting & mutex_resources = resource_cfg["resources"];
			std::vector<system_resource_cfg_t> mutex_list;
			for(size_t j = 0; j < mutex_resources.getLength(); ++j) {
				const Setting & mutex_cfg = mutex_resources[j];
				try {
					mutex_list.push_back(parse_resource_config(mutex_cfg));
					// add mutex resource ids to the lists
					for (size_t k = 0; k < j; ++k) {
						mutex_list[k].mutex_resources.insert(mutex_list.back().id);
						mutex_list.back().mutex_resources.insert(mutex_list[k].id);
					}
				} catch (const std::exception & e) {
					LOG_ERROR(_log, "INVALID_RESOURCE", "\t! %s at index %zu:%zu", e.what(), i, j);
					continue;
				}
			}
			for (auto & res : mutex_list) {
				pool.insert({res.id, res});
			}
		}
		else {
			try {
				auto resource = parse_resource_config(resource_cfg);
				pool.insert({resource.id, resource});
			} catch (const std::exception & e) {
				LOG_ERROR(_log, "INVALID_RESOURCE", "\t! %s at index %zu", e.what(), i);
			}
		}
	}
}

resource_list_t ResourcePool::acquire(const resource_request_t & resources,
		resource_request_t & failures) {
	resource_list_t acquired;
	// first collect any mutex resources that might block current request
	failures = allocated_mutexes(resources);
	// struct to preserve order of allocated resources
	struct allocation_order_t {
		// anonymous request iterator (source)
		resource_request_t::const_iterator from;
		// destination to insert allocated resources
		resource_list_t::iterator to;
		// system resources pool iterator (to avoid double lookup)
		std::map<std::string, system_resource_cfg_t>::iterator pool_it;
	};
	std::list<allocation_order_t> allocation_order;
	// on the first pass we're trying to allocate indexed resources only
	// anonymous requests is to be stored in allocation_order
	for (auto descriptor_it = resources.begin(); descriptor_it != resources.end(); ++descriptor_it) {
		auto pool_it = pool.find(descriptor_it->id);
		// resource not found
		if (pool_it == pool.end()) {
			/* TODO: how should we properly handle request for non-existent resource?
			 * policy manager will certainly fail to "free" this resource */
			failures.push_back(*descriptor_it);
			std::stringstream sd; sd << *descriptor_it;
			LOG_ERROR(_log, MSGERR_RESOURCE_FIND, "attempt to acquire invalid resource : %s",
					sd.str().c_str());
		}
		// indexed resource
		else if (descriptor_it->index < pool_it->second.max_qty) {
			// succeeded
			if (pool_it->second.units.test(descriptor_it->index)) {
				pool_it->second.units.reset(descriptor_it->index);
				acquired.push_back({descriptor_it->id, descriptor_it->index});
				// reset destination for predcessors in allocation_order
				for (auto tail_it = allocation_order.rbegin(); tail_it != allocation_order.rend() &&
					 tail_it->to == acquired.end(); ++tail_it) {
					tail_it->to = --acquired.end();
				}
			}
			// failed
			else {
				failures.push_back(*descriptor_it);
				std::stringstream sd; sd << *descriptor_it;
				LOG_DEBUG(_log, "failed to acquire resource : %s", sd.str().c_str());
			}
		}
		// anonymous resource
		else {
			allocation_order.push_back({descriptor_it, acquired.end(), pool_it});
		}
	}

	// second pass; try anonymous requests; put results in order
	for (const auto & order : allocation_order) {

		LOG_DEBUG(_log, "available=%d, qty=%d, min=%d",
				  order.pool_it->second.qty(), order.from->qty, order.from->min);

		size_t need_more = order.from->qty;
		// extra = qty - min
		size_t extra = 0;
		if (order.from->qty > order.from->min) {
			need_more = order.from->min;
			extra = order.from->qty - order.from->min;
		}
		size_t index = order.pool_it->second.max_qty - 1;
		for (; need_more > 0 && index != size_t(-1); --index) {
			if (order.pool_it->second.units.test(index)) {
				order.pool_it->second.units.reset(index);
				acquired.insert(order.to, {order.from->id, index});
				--need_more;
				// acqure extra resources if they fit
				if (need_more == 0 && extra <= order.pool_it->second.qty()) {
					need_more += extra;
				}
			}
		}

		// not enough resources
		if (need_more) {
			failures.push_back({order.from->id, need_more});
			std::stringstream sd; sd << *order.from;
			LOG_DEBUG(_log, "failed to acquire resource : %s, shortage = %zu ",
					sd.str().c_str(), failures.back().qty);
		}
	}
	// rollback and compact on failure
	if (failures.size()) {
		release(acquired);
		acquired.clear();
		failures.compact();
	}
	return acquired;
}

resource_list_t ResourcePool::release(const resource_list_t & resources) {
	resource_list_t released;
	for (const auto & unit : resources) {
		auto pool_it = pool.find(unit.id);
		if (pool_it != pool.end() && unit.index < pool_it->second.max_qty &&
				(!pool_it->second.units.test(unit.index))) {
			pool_it->second.units.set(unit.index);
			released.push_back(unit);
		}
		else {
			std::stringstream s; s << unit;
			LOG_DEBUG(_log, "attempt to release invalid resource : %s", s.str().c_str());
		}
	}
	return released;
}

void ResourcePool::print() const {
	LOG_DEBUG(_log,"+---------------");
	LOG_DEBUG(_log,"RESOURCES(system):");

	for(const auto & res : pool) {
		std::stringstream s; s << res.second;
		LOG_DEBUG(_log,"+  %s", s.str().c_str());
	}
	LOG_DEBUG(_log,"+---------------");
}

void ResourcePool::update(string id, uint32_t qty, bool remove)
{
	LOG_DEBUG(_log, "id=%s, max_qty=%d", id.c_str(), qty);

	auto pool_it = pool.find(id);
	if (pool_it != pool.end()) {
		LOG_DEBUG(_log, "FOUND: add resource. id=%s, max_qty=%d", id.c_str(), qty);

		if ( remove ) {
			LOG_DEBUG(_log, "REMOVE : id=%s",id.c_str());
			pool.erase(id);
			return;
		}

		// update
		LOG_DEBUG(_log, "UPDATE : id=%s",id.c_str());
		pool_it->second.max_qty = qty;
		pool_it->second.units.reset();
		for (size_t i = 0; i < qty; ++i)
			pool_it->second.units.set(i);

		return;
	}

	// add new resource
	LOG_DEBUG(_log, "ADD : id=%s",id.c_str());
	pool.insert({id, {id, qty, "<factory add>"}});
	return;
}

int32_t ResourcePool::get_max_qty(std::string id) {
	auto pool_it = pool.find(id);
	if (pool_it != pool.end()) {
		return static_cast<int32_t>(pool_it->second.max_qty);
	}
	return -1;
}


resource_request_t ResourcePool::allocated_mutexes(const resource_request_t & resources) {
	resource_request_t blocking_mutex_resources;
	std::set<std::string> processed_mutex_ids;
	for (const auto & descriptor : resources) {
		auto requested_pool_it = pool.find(descriptor.id);
		// TODO: handle failed case - clear sign of inconsistency
		if (requested_pool_it != pool.end()) {
			for (const auto & mutex_id : requested_pool_it->second.mutex_resources) {
				if (processed_mutex_ids.find(mutex_id) == processed_mutex_ids.end()) {
					processed_mutex_ids.insert(mutex_id);
					auto pool_it = pool.find(mutex_id);
					// TODO: handle failed case - clear sign of inconsistency
					if (pool_it != pool.end() && pool_it->second.allocated()) {
						blocking_mutex_resources.push_back({ mutex_id, pool_it->second.allocated() });
					}
				}
			}
		}
	}
	return blocking_mutex_resources;
}

ResourceManager::ResourceManager(const Config &config)
{
	//
	// TODO : use libconfig++ ParseFile exceptions to debug malformed c
	//            onfiguration files
	//
	readResourceConfig(config);
}

ResourceManager::~ResourceManager()
{
	policy_priority.clear();
}

void ResourceManager::readResourceConfig(const Config &config)
{
	const Setting& root = config.getRoot();
	try {
		const Setting &policies_cfg = root["pipelines"];
		const Setting & resource_cfg = root["resources"];
		system_resources = ResourcePool::ptr_t(new ResourcePool(resource_cfg));
		readPipelinePolicies(policies_cfg);
	}
	catch (const SettingException &ex)
	{
		LOG_ERROR(_log, MSGERR_CONFIG, "Configuration does not contain settings: %s",
				ex.getPath());
	}
}

void ResourceManager::readPipelinePolicies(const Setting &policies_cfg)
{
	int cnt = policies_cfg.getLength();

	LOG_DEBUG(_log, "+ Policy+");
	for(int i = 0; i < cnt; ++i) {
		string type;
		uint32_t priority;

		const Setting &policy_cfg = policies_cfg[i];

		if ( ! (policy_cfg.lookupValue("type", type)) ) {
			LOG_ERROR(_log, MSGERR_NO_POLICY_TYPE,
					"type: failed reading pipeline configuration.");
		}
		if ( ! (policy_cfg.lookupValue("priority",priority)) ) {
			LOG_ERROR(_log, MSGERR_NO_POLICY_PRIO,
					"priority: failed reading pipeline configuration.");
		}

		LOG_DEBUG(_log, "\t+ Policy configuration #%d +",cnt);
		LOG_DEBUG(_log, "\t\t type=%s",type.c_str());
		LOG_DEBUG(_log, "\t\t priority=%d",priority);

		policy_priority[type]=priority;
	}

	_log.setLogLevel("DEBUG");
} // end constructor

// -------------------------------------
// ResourceManager API (library)
//

void ResourceManager::addResource(const string &id, uint32_t qty)
{
	LOG_DEBUG(_log, "id=%s,qty=%d",id.c_str(),qty);
	system_resources->update(id,qty);
}

void ResourceManager::removeResource(const string &id)
{
	LOG_DEBUG(_log, "id=%s",id.c_str());
	system_resources->update(id,0,true);
}

void ResourceManager::updateResource(const string &id, uint32_t qty)
{
	LOG_DEBUG(_log, "id=%s,qty=%d",id.c_str(),qty);
	system_resources->update(id,qty);
}

void ResourceManager::setLogLevel(const std::string & level) {
	_log.setLogLevel(level);
}

// @f registerPipeline
// @description : register with Resource Manager.  Session is persistent
//                across all start/end transaction and acquire/release cycles.
//                Registered connections and their current resource requirements
//                will be tracked by Resource Manager.
//
// @param type as specified in Resource Manager configuration file
//              pipeline settings
// @ return : connection_id
//
bool ResourceManager::registerPipeline(const string &connection_id, const string &type)
{
	if (type.empty()) {
		LOG_DEBUG(_log, "registerPipeline: type string is empty");
		return false;
	}

	lock_t l(mutex);

	resource_manager_connection_t connection;

	uint32_t priority;
	if(!findPriority(type, &priority)){
		LOG_ERROR_EX(_log, MSGERR_POLICY_FIND, __KV({{KVP_POLICY_PRI, type}}),
				"type=%s not found.", type.c_str());
		return false;
	}

	connection.connection_id = connection_id;
	connection.type = type;
	connection.timestamp = timeNow();
	connection.is_managed = false;
	connection.policy_state = resource_manager_connection_t::policy_state_t::READY;
	connection.is_foreground = false;
	connection.is_focus = false;
	connection.is_visible = true;
	connection.priority = priority;
	connections[connection_id] = connection;

	LOG_DEBUG(_log, "connection_id=%s, type=%s, priority: %d",
			  connection_id.c_str(), type.c_str(), connection.priority);

#if RESOURCE_MANAGER_DEBUG
	showActivePipelines();
	showConnections();
	showSystemResources();
#endif

	return true;
}

// @f resetPipeline
// @description : release all pipeline bound resources and locks
// @param connection_id
//
//
bool ResourceManager::resetPipeline(const std::string &connection_id)
{
	lock_t l(mutex);

	LOG_DEBUG(_log, "connection_id=%s", connection_id.c_str());

	auto connection = findConnection(connection_id);
	if (connection == nullptr)
		return false;

	connection->policy_state = resource_manager_connection_t::policy_state_t::READY;

	if (!connection->resources.empty()) {
		resource_list_t released = connection->resources;
		for (const auto & unit : released) {
			releaseDisplayResource(unit);
		}
		system_resources->release(connection->resources);
		connection->resources.erase(connection->resources.begin(),
				connection->resources.end());
		if (m_release_callback) m_release_callback(connection_id, released);
	}

#if RESOURCE_MANAGER_DEBUG
	showActivePipelines();
	showSystemResources();
#endif

	return true;
}

// @f unregisterPipeline
// @description : unregister with Resource Manager
// @param connection_id
//
//
bool ResourceManager::unregisterPipeline(const string &connection_id)
{
	lock_t l(mutex);

	resetPipeline(connection_id);
	connections.erase(connection_id);

#if RESOURCE_MANAGER_DEBUG
	showActivePipelines();
	showConnections();
	showSystemResources();
#endif

	return true;
}

void ResourceManager::setAcquireCallback(callback_t callback) {
	m_acquire_callback = callback;
	// invoke callback for all existing resources
	if (m_acquire_callback) {
		for (const auto & connection : connections) {
			if (!connection.second.resources.empty())
				m_acquire_callback(connection.first, connection.second.resources);
		}
	}
}


// @f acquire
// @brief acquire resources
//
// @param connection_id
//
// @param resource_request IN list of resources required (JSON)
//   example request.
//   [
//     {
//        "resource":"VDEC",
//        "qty":1,
//        "attribute":"display0",
//        "index":1
//     },
//     {
//        "resource":"ADEC",
//        "qty":1
//     }
//   ]
//
// @param failed_resources OUT list of requested resources which are
//        not available.  Used to determine if policy candidate
//        selection is required.
//
//
bool ResourceManager::acquire(const std::string &connection_id,
		const string &acquire_request,
		dnf_request_t &failed_resources,
		string &acquire_response)
{
	dnf_request_t resource_request;
	bool rv;

	lock_t l(mutex);

	failed_resources.clear();

	auto connection = connections.find(connection_id);
	if(connection == connections.end()) {
		LOG_CRITICAL(_log, MSGERR_CONN_FIND, "connection_id not found.");
		return false;
	}

	// prepare acquire response
	JGenerator serializer(NULL);
	pbnjson::JValue response_json = pbnjson::Object();
	pbnjson::JValue resources_array = pbnjson::Array();
	response_json.put("state",false);   // reset to true if acquire succeeds
	response_json.put("connectionId",connection_id);

	rv = decodeAcquireRequest(acquire_request, resource_request);
	if( rv == false ) {
		serializer.toString(response_json,  pbnjson::JSchema::AllSchema(), acquire_response);
		LOG_CRITICAL(_log, MSGERR_JSON_PARSE, "decodeResourceRequest error. "
				"acquire_response=%s", acquire_response.c_str());
		return false;
	}
	// sort request branches by number of requested resources
	resource_request.sort([] (const resource_request_t & a, const resource_request_t & b)
		{ return a.qty < b.qty; }
	);

	connection->second.timestamp = timeNow();

	for (const resource_request_t & branch : resource_request) {

		resource_request_t shortage;
		resource_list_t acquired_resources = system_resources->acquire(branch, shortage);

		if (acquired_resources.size()) {
			response_json.put("state",true);
			resources_array = pbnjson::Array();
			for (const auto & unit : acquired_resources) {
				LOG_DEBUG(_log, "acquired: %s[%zu]", unit.id.c_str(), unit.index);

				pbnjson::JValue resource_obj = pbnjson::Object();
				resource_obj.put("resource", unit.id);
				resource_obj.put("index", (int)unit.index);
				resource_obj.put("qty", 1);
				acquireDisplayResource(unit, resource_obj);
				resources_array.append(resource_obj);

				addActiveResource(connection->second, unit);
			}
			// acquire succeeded - wrap up, notify and break
			if (m_acquire_callback) m_acquire_callback(connection_id, acquired_resources);
			failed_resources.clear();
			break;
		}
		else {
			failed_resources.push_back(shortage);
			pbnjson::JValue failed_array = pbnjson::Array();
			// fill out resources array with failed items
			for (const auto & resource : shortage) {
				LOG_DEBUG(_log, "shortage: %s[%zu]=%zu",
						resource.id.c_str(), resource.index, resource.qty);

				pbnjson::JValue resource_obj = pbnjson::Object();
				resource_obj.put("resource", resource.id);
				resource_obj.put("index", (int)resource.index);
				resource_obj.put("qty", (int)resource.qty);
				failed_array.append(resource_obj);
			}
			// backward compatible encoding for single branch request
			if (resource_request.size() == 1)
				resources_array = failed_array;
			else
				resources_array.append(failed_array);
		}
	}

	response_json.put("resources", resources_array);

	if (!serializer.toString(response_json,  pbnjson::JSchema::AllSchema(), acquire_response)) {
		LOG_CRITICAL(_log, MSGERR_JSON_SERIALIZE, "JSON serialize failed.");
		return false;
	}

	LOG_DEBUG(_log, "acquire_response=%s", acquire_response.c_str());

#if RESOURCE_MANAGER_DEBUG
	showSystemResources();
	showActivePipelines();
#endif

	return true;
}

// @f release
// @brief release resources
//
bool ResourceManager::release(const std::string &connection_id,
		const string& release_request)
{
	resource_list_t to_release;

	lock_t l(mutex);

	auto connection = findConnection(connection_id);
	RETURN_IF(nullptr ==  connection, false, MSGERR_CONN_FIND, "connection not found");

	LOG_DEBUG(_log, " release_request=%s",release_request.c_str());

	if (connection->resources.empty()) {
		LOG_DEBUG(_log, "release: resources empty");
		return false;
	}

	if (!decodeReleaseRequest(release_request, to_release)) {
		LOG_DEBUG(_log, "release: decodeReleaseRequest fail");
		return false;
	}

	resource_list_t released = system_resources->release(to_release);

	for (const auto & unit : released) {
		remActiveResource(*connection, unit);
		releaseDisplayResource(unit);
	}
	if (m_release_callback) m_release_callback(connection_id, released);

#if RESOURCE_MANAGER_DEBUG
	showSystemResources();
	showActivePipelines();
#endif
	return true;
}

//
// ---- PRIVATE
//

// Helper parsers

resource_unit_t parseResourceUnit(const JValue & resource) {
	std::string id;
	int32_t index;

	if (!resource.isObject())
		throw std::runtime_error("Request parsig failure");

	if (! (resource.hasKey("resource") && CONV_OK == resource["resource"].asString(id)) )
		throw std::runtime_error("Request parsing failure");

	if (! (resource.hasKey("index") && CONV_OK == resource["index"].asNumber(index)) )
		throw std::runtime_error("Request parsing failure");

	return {id, (size_t)index};
}

resource_descriptor_t parseResource(const JValue & resource) {

	std::string id, attribute;
	int32_t index, qty, min;

	if (!resource.isObject())
		throw std::runtime_error("Request parsig failure");

	if (! (resource.hasKey("resource") && CONV_OK == resource["resource"].asString(id)) )
		throw std::runtime_error("Request parsing failure");

	if (! (resource.hasKey("qty") && CONV_OK == resource["qty"].asNumber(qty)))
		throw std::runtime_error("Request parsing failure");

	if (resource.hasKey("index") && CONV_OK == resource["index"].asNumber(index)) {
		qty = 1;
	} else {
		index = -1;
	}

	if (! (resource.hasKey("attribute") && CONV_OK == resource["attribute"].asString(attribute)) ) {
		attribute = "";
	}

	if (! (resource.hasKey("min") && CONV_OK == resource["min"].asNumber(min)) ) {
		min = qty;  // no minimum specified
	}

	return {id, (size_t)qty, (size_t)index, attribute, (size_t)min};
}

resource_request_t parseResourceRequest(const JValue & resources) {

	resource_request_t request;

	if (!resources.isArray())
		throw std::runtime_error("Request parsig failure");

	for (size_t i = 0; i < resources.arraySize(); ++i)
		request.push_back(parseResource(resources[i]));

	return request;
}

resource_list_t parseResourceList(const JValue & resources) {

	resource_list_t list;

	if (!resources.isArray())
		throw std::runtime_error("Request parsig failure");

	for (size_t i = 0; i < resources.arraySize(); ++i)
		list.push_back(parseResourceUnit(resources[i]));

	return list;
}


// @f decodeReleaseRequest
// @brief decode release resources request
//
// example release requests
//
// JSON format for request:  Array of requests.
//
// [
//   {"resource":"VDEC", "index":1},
//   {"resource":"ADEC", "index":0},
//   {"resource":"ADEC", "index":1}
// ]
//
// Parsed structure (struct resource_list_t):
//
//   +-> resource #0 [ id, index ]  (struct resource_unit_t)
//   |
//   +-> resource #1 [ id, index ]
//   |
//   +-> resource #N [ id, index ]
//

bool ResourceManager::decodeReleaseRequest(const string & release_request,
			resource_list_t & resources) {

	resources.clear();
	JDomParser parser;

	if (!parser.parse(release_request, JSchema::AllSchema())) {
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. msg=%s ",
				release_request.c_str());
		return false;
	}

	JValue request = parser.getDom();

	try {
		resources = parseResourceList(request);
	} catch (const std::runtime_error & e) {
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "%s. msg=%s ",
				e.what(), release_request.c_str());
		return false;
	}

	return true;
}

// @f decodeAcquireRequest
// @brief decode acquire resources request
//
// example acquire requests
//
// JSON format for request:  Array of requests or composites.
// Composite is array of alternatives (branches) of resources.
// Any combination of alternatives will satisfy request
//
// [
//   {"resource":"VDEC", "qty": 2},
//   {"resource":"ADEC", "qty": 1, "index":0},
//	 [
//     [
//       {"resource":"HEVC", "qty": 1},
//       {"resource":"BW", "qty": 1}
//     ],
//     {"resource":"MSVC", "qty": 2}
//	 ],
//   {"resource":"VENC", "qty": 1}
// ]
//
// Parsed structure (struct dnf_request_t):
//
//   +-> branch #0 (struct resource_request_t)
//   |
//   +-> branch #1 (struct resource_request_t)
//   |
//   +-> branch #N (struct resource_request_t)
//

bool ResourceManager::decodeAcquireRequest(const string & acquire_request,
			dnf_request_t & resources) {

	resources.clear();
	JDomParser parser;

	if (!parser.parse(acquire_request, JSchema::AllSchema())) {
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. msg=%s ",
				acquire_request.c_str());
		return false;
	}

	JValue request = parser.getDom();

	if (!request.isArray()) {
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "Request parsing failure. msg=%s ",
				acquire_request.c_str());
		return false;
	}

	// add an empty branch
	resources.push_back(resource_request_t());

	// roll out 3-level request to dnf
	try {
		for (size_t i = 0; i < request.arraySize(); ++i) {
			const JValue & part = request[i];
			// resource descriptor
			if (part.isObject()) {
				auto descriptor = parseResource(part);
				// push request descriptor to each branch
				for (auto & branch : resources) {
					branch.push_back(descriptor);
					branch.qty += descriptor.qty;
				}
			}
			// fork
			else if (part.isArray() && part.arraySize() > 0) {
				// we need to copy existing branches for each new one
				dnf_request_t original = resources;
				for (size_t j = 1; j < part.arraySize(); ++j) {
					resources.insert(resources.end(), original.begin(), original.end());
				}
				auto push_to = resources.begin();
				for (size_t j = 0; j < part.arraySize(); ++j) {
					const JValue & branch = part[j];
					// each branch may contain one or more resources
					if (branch.isObject()) {
						auto descriptor = parseResource(branch);
						for (size_t cnt = 0; cnt < original.size(); ++cnt) {
							push_to->push_back(descriptor);
							(push_to++)->qty += descriptor.qty;
						}
					} else if (branch.isArray()) {
						resource_request_t list = parseResourceRequest(branch);
						for (size_t cnt = 0; cnt < original.size(); ++cnt) {
							for (const auto & descriptor : list) {
								push_to->push_back(descriptor);
								push_to->qty += descriptor.qty;
							}
							++push_to;
						}
					}
				}
			}
		}
	} catch (const std::runtime_error & e) {
		resources.clear();
		LOG_ERROR(_log, MSGERR_JSON_PARSE, "%s. msg=%s ",
				e.what(), acquire_request.c_str());
		return false;
	}

	return true;
}

// @f endcodeResourceRequest
// @brief Encode resource data structure into json string format
//
// @param resources OUT string
//
// @param resource_request OUT : "DE0=2 | DE1=4 | DE2 = 2, BUFFER=4, HDMI0 = 1 | HDMI1 = 1"
//
bool ResourceManager::encodeResourceRequest(const resource_request_t & resources,
		string & resource_request)
{
	JValue root = Array();

	for (const auto & unit : resources) {
		JValue junit = Object();
		junit.put("resource", unit.id);
		// TODO: where are specializations for
		// pbnjson::JValue<uintXX_t>::JValue(const uintXX_t &) ?
		junit.put("qty", (int32_t)unit.qty);
		junit.put("index", (int32_t)unit.index);
		junit.put("attribute", unit.attribute);
		root.append(junit);
	}

	return JGenerator().toString(root, JSchema::AllSchema(), resource_request);
}

// -------------------------------------
// General Utilities
//

// @f showSystemResources
// @brief show system resource status
//
void ResourceManager::showSystemResources() const
{
	system_resources->print();
}

// @f showConnections
// @brief show connected connections
//
void ResourceManager::showConnections()
{
	if( connections.empty()) {
		LOG_DEBUG(_log, "connections map: EMPTY\n");
		return;
	}

	LOG_DEBUG(_log,"+ ----- RESOURCE MANAGER CONNECTIONS");

	map<string, resource_manager_connection_t>::const_iterator it;
	for( it = connections.begin(); it != connections.end(); ++it) {
		LOG_DEBUG(_log, "   ** %s",it->second.connection_id.c_str());
		LOG_DEBUG(_log, "   + %s", it->second.type.c_str());
		LOG_DEBUG(_log, "   + %s", it->second.service_name.c_str());
	}

	LOG_DEBUG(_log, "+------");
	return;
}

// @f findConnection
// @brief find connection specified by connection_id
//
resource_manager_connection_t * ResourceManager::findConnection(const string &connection_id)
{
	lock_t l(mutex);

	auto it = connections.find(connection_id);
	if(it != connections.end()) {
		return &(it->second);
	}
	return nullptr;
}

// ------------------------
// Policy API

void ResourceManager::showActivePipelines()
{
	if ( connections.empty()) {
		LOG_DEBUG(_log, "active_pipelines: EMPTY\n");
		return;
	}

	LOG_DEBUG(_log,"=========== Active pipelines ");
	for ( auto it = connections.begin(); it != connections.end(); it++) {
		string resources_string;
		for (auto rit = it->second.resources.begin(); rit != it->second.resources.end(); ++rit) {
			resources_string += rit->id + "=" + intToString(rit->index) + " ";
		}

		LOG_DEBUG(_log, "\t+ id=%s, type=%s, "
				"policy_state = %d, resources=%s ",
				it->second.connection_id.c_str(),
				it->second.type.c_str(),
				it->second.policy_state,
				resources_string.empty() ? "NO RESOURCES" : resources_string.c_str());
	}
	LOG_DEBUG(_log, "===========================");
}

void ResourceManager::addActiveResource(resource_manager_connection_t & owner,
		const resource_unit_t & unit) {
	owner.resources.push_back(unit);
}

void ResourceManager::remActiveResource(resource_manager_connection_t & owner,
		const resource_unit_t & unit) {
	auto it = std::find(owner.resources.begin(), owner.resources.end(), unit);
	if (it != owner.resources.end())
		owner.resources.erase(it);
}

void ResourceManager::acquireDisplayResource(const resource_unit_t & unit, pbnjson::JValue & resource_obj) {
	int32_t max_qty_idx = -1;
	if (unit.id.find("DISP") != std::string::npos) {
		ums::disp_res_t dispRes = {-1,-1,-1};
		max_qty_idx = system_resources->get_max_qty(unit.id) - 1;
		if (max_qty_idx < 0) {
			LOG_ERROR(_log, "INVALID_RESOURCE_QUANTITY", "max qty of %s should be more than 0");
		}
		if (m_acquire_disp_resource_callback) {
			m_acquire_disp_resource_callback(unit.id, max_qty_idx - static_cast<int>(unit.index), dispRes);
		}
		resource_obj.put("plane-id", dispRes.plane_id);
		resource_obj.put("crtc-id", dispRes.crtc_id);
		resource_obj.put("conn-id", dispRes.conn_id);
	}
}

void ResourceManager::releaseDisplayResource(const resource_unit_t & unit) {
	int32_t max_qty_idx = -1;
	if (unit.id.find("DISP") != std::string::npos) {
		max_qty_idx = system_resources->get_max_qty(unit.id) - 1;
		if (max_qty_idx < 0) {
			LOG_ERROR(_log, "INVALID_RESOURCE_QUANTITY", "max qty of %s should be more than 0");
			return;
		}
		if (m_release_disp_resource_callback)
			m_release_disp_resource_callback(unit.id, max_qty_idx - static_cast<int>(unit.index));
		}
}

bool ResourceManager::notifyActivity(const std::string & id)
{
	lock_t l(mutex);

	auto connection = connections.find(id);
	if(connection == connections.end()) {
		LOG_CRITICAL(_log, MSGERR_CONN_FIND, "id=%s not found.", id.c_str());
		return false;
	}

	connection->second.timestamp = timeNow();
	return true;
}

// @f notifyForeground
// @b indicate connection is visible in the foreground and
//      must not be selected for policy action. Only one
//      connection will be permitted to be marked as foreground.
//
// @note In the resource manager design it is permissible
//      to select a foreground application
//      for policy action if the application is not actively utilizing
//      H/W media resources. Applications showing a screen capture on
//      pause will properly prevent the policy action from being visible
//      to users.  But ... due to the inability of the current H/W to
//      properly do a screen capture, foreground applications must be
//      excluded from being selected for policy action.
//
bool ResourceManager::notifyForeground(const std::string & id)
{
	lock_t l(mutex);

	auto connection = connections.find(id);
	if(connection == connections.end()) {
		LOG_CRITICAL(_log, MSGERR_CONN_FIND, "id=%s not found.", id.c_str());
		return false;
	}

	LOG_INFO(_log, MSGNFO_FOREGROUND_REQUEST, "+ FOREGROUND: connection_id=%s",id.c_str());

	// mark current connection as foreground.
	connection->second.is_foreground = true;

	return true;
}

// @f notifyBackground
// @b indicate connection can be selected for policy action.
//
bool ResourceManager::notifyBackground(const std::string & id)
{
	lock_t l(mutex);

	auto connection = connections.find(id);
	if(connection == connections.end()) {
		LOG_CRITICAL(_log, MSGERR_CONN_FIND, "id=%s not found.", id.c_str());
		return false;
	}

	LOG_INFO(_log, MSGNFO_BACKGROUND_REQUEST, "+ BACKGROUND: connection_id=%s",id.c_str());
	connection->second.is_foreground = false;

	return true;
}

// @f notifyFocus
// @b indicate media object has been selected by user
//
bool ResourceManager::notifyFocus(const std::string & id)
{
	lock_t l(mutex);

	auto connection = connections.find(id);
	if(connection == connections.end()) {
		LOG_CRITICAL(_log, MSGERR_CONN_FIND, "id=%s not found.", id.c_str());
		return false;
	}

	LOG_DEBUG(_log,"+ FOCUS: connection_id=%s",id.c_str());

	connection->second.is_focus = true;

	// only one media object may be in focus
	for (auto & it : connections) {
		if (it.second.connection_id == id) continue;
		it.second.is_focus = false;
	}

	return true;
}

// @f notifyDisplay
// @b indicate whether media object is displaying
//
bool ResourceManager::notifyVisibility(const std::string & id, bool is_visible)
{
	lock_t l(mutex);

	auto connection = connections.find(id);
	if(connection == connections.end()) {
		LOG_WARNING(_log, MSGERR_CONN_FIND, "id=%s not found.", id.c_str());
		return false;
	}

	LOG_DEBUG(_log,"+ VISIBILITY: connection_id=%s state=%s ",
				id.c_str(), is_visible?"TRUE":"FALSE");

	connection->second.is_visible = is_visible;

	return true;
}

bool ResourceManager::selectPolicyCandidates(const std::string &connection_id,
		const resource_request_t &needed_resources,
		std::list<std::string> &candidates)
{
	lock_t l(mutex);

	resource_request_t needed = needed_resources;
	candidates.clear();
	LOG_DEBUG(_log,"+ connection_id=%s",connection_id.c_str());

	auto requestor = findConnection(connection_id);
	RETURN_IF(nullptr ==  requestor, false, MSGERR_CONN_FIND, "invalid connection");

	// select candidate for particular resource and push it into set
	// return true on success and false in case of failure
	auto check_candidate = [&](const resource_manager_connection_t * candidate)->resource_request_t {

		resource_request_t to_release;
		// look for needed resources. if any resources are lacking return false.
		for (const auto & resource : needed) {
			std::stringstream s;
			LOG_DEBUG(_log, "Searching pipeline %s.", candidate->connection_id.c_str());
			s << "+Need: | " << resource;
			LOG_DEBUG(_log, "%s", s.str().c_str());

			// searching for resources of the kind - anonymous case
			if (resource.index >= MAX_RESOURCES_QTY) {
				size_t have = 0;

				for (auto & unit : candidate->resources) {
					// have enough resources - break
					if (have == resource.qty)
						break;
					if (unit.id == resource.id) {
						have++;
					}
				}

				if ( have ) {
					to_release.push_back({resource.id, have});
				}
			}
			// indexed resource
			else {
				auto it = std::find_if(candidate->resources.begin(), candidate->resources.end(),
						[&resource] (const resource_unit_t & unit)->bool { return unit == resource; });

				if (it != candidate->resources.end()) {
					to_release.push_back({resource.id, 1, resource.index});
				}
			}
		}

		return to_release;
	};

	auto policy_less = [](const resource_manager_connection_t * a, const resource_manager_connection_t * b) {
		// sort by priotiry then foreground then focus then lru
		if (a->priority == b->priority) {
			if (a->is_foreground == b->is_foreground) {
				if (a->is_focus != b->is_focus) {
					return b->is_focus;
				} else if (a->is_visible != b->is_visible) {
					return b->is_visible;
				} else {
					return a->timestamp < b->timestamp;
				}
			} else {
				return b->is_foreground;
			}
		}
		return a->priority < b->priority;
	};

	std::set<resource_manager_connection_t *, decltype(policy_less)> active_pipelines(policy_less);

	for (auto & it : connections) {
		auto pass_filter = [&requestor](const resource_manager_connection_t & candidate) {
			// pass self or candidate that is ready for policy action
			return  (requestor->connection_id == candidate.connection_id) ||
					(candidate.policy_state == resource_manager_connection_t::policy_state_t::READY);
		};
		// filer + sorted insert
		if (pass_filter(it.second))
			active_pipelines.insert(&(it.second));
	}

	// walk through connections
	for (auto & candidate : active_pipelines) {
		LOG_DEBUG(_log, "@@@@     id=%s", candidate->connection_id.c_str());
		LOG_DEBUG(_log, "         priority=%d", candidate->priority);
		LOG_DEBUG(_log, "         foreground=%d", candidate->is_foreground);
		LOG_DEBUG(_log, "         focus=%d", candidate->is_focus);
		LOG_DEBUG(_log, "         timestamp=%llu, ", candidate->timestamp);
		LOG_DEBUG(_log, "====     policy_state=%d", candidate->policy_state);
		// exit when found all needed resources
		if (needed.empty()) break;
		// exit when bump into self
		if (candidate->connection_id == requestor->connection_id) break;

		candidate->policy_resources = check_candidate(candidate);
		if (!candidate->policy_resources.empty()) {
			needed = needed - candidate->policy_resources;
			LOG_DEBUG(_log, "candidate found: %s", candidate->connection_id.c_str());
			candidates.push_back(candidate->connection_id);
		}
	}

	// failure - some resources still needed
	if (!needed.empty()) {
		candidates.clear();
	}

	return !candidates.empty();
}

bool ResourceManager::isValidType(const std::string &type) {
	return findType(type);
}

bool ResourceManager::findPriority(const std::string &type,
		uint32_t *priority)
{
	auto it = policy_priority.find(type);
	if(it != policy_priority.end()) {
		*priority = (it->second);
		return true;
	}
	return false;
}

bool ResourceManager::findType(const std::string &type) {
	auto it = policy_priority.find(type);
	if(it != policy_priority.end()) {
		return true;
	}
	return false;
}

void ResourceManager::setPriority(const std::string & type, uint32_t priority) {
	policy_priority[type] = priority;
}

void ResourceManager::removePriority(const std::string & type) {
	policy_priority.erase(type);
}

void ResourceManager::setManaged(const std::string & id) {
	auto connection = connections.find(id);
	if(connection == connections.end())
		LOG_ERROR(_log, MSGERR_CONN_FIND, "id=%s not found.", id.c_str());
	else
		connection->second.is_managed = true;
}

void ResourceManager::reclaimResources() {
	if (m_policy_action_callback) {
		for (auto & c : connections) {
			resource_manager_connection_t & connection = c.second;
			m_policy_action_callback(connection.connection_id, connection.resources);
		}
	}
}
