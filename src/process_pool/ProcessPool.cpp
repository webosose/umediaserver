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

#include <algorithm>
// TODO: remove this dependency
#include <Logger_macro.h>
#include <UMSConnector.h>
#include <GenerateUniqueID.h>
#include <Registry.h>
#include "ProcessPool.h"

namespace uMediaServer {

namespace {
	Logger _log(UMS_LOG_CONTEXT_PROCESS_CONTROLLER);
}

ProcessPool::ProcessPool(IServiceReadyWatcher & watcher, exited_cb_t && exited_cb)
	: _watcher(watcher), _exited_cb(std::move(exited_cb)) {
	// Initialize Pipeline Pool
	auto & sql = *Reg::Registry::instance()->dbi();
	std::list<pipeline_cfg_t> pipeline_configs;
	try {
		sql << "select * from pipelines;", DBI::into(pipeline_configs);
	} catch (const DBI::Exception & e) {
		LOG_ERROR(_log, MSGERR_POOL_INIT,
				"Pipeline pooling failed. Configuration DB read error: %s", e.what());
		return;
	}
	for (const pipeline_cfg_t & config : pipeline_configs) {
		if (config.pool_size) {
			// save pool config
			_pool_config.pool_size[config.type] = config.pool_size;
			_pool_config.refill_delay += config.pool_refill_delay;
			// create initial pool entry
			_ready_pool[config.type] = {};
			// delay start first process
			if (!_refill_timer)
				_refill_timer.reset(new GMainTimer(1000 * config.pool_fill_delay,
												   std::bind(&ProcessPool::refill, this)));
		}
	}
	if (_pool_config.pool_size.size()) _pool_config.refill_delay /= _pool_config.pool_size.size();
}

ProcessPool::~ProcessPool() {
	for (auto & sp : _started_pool)
		sp.second.process->stop();
	for (auto & cp : _ready_pool)
		for (auto & p : cp.second)
			p.second.process->stop();
}

void ProcessPool::hire(const std::string & type, const std::string & id, dispatch_cb_t && cb) {
	// do we have pooled process?
	auto cit = _ready_pool.find(type);
	if (cit != _ready_pool.end()) {
		auto & pool = cit->second;
		if (!pool.empty()) {
			auto pit = pool.begin();
			_active_pool[pit->second.process->pid()] = id;
			cb(pit->second.service_name, pit->second.process);
			pool.erase(pit);
			// schedule refill
			if (need_refill())
				_refill_timer.reset(new GMainTimer(1000 * _pool_config.refill_delay,
												   std::bind(&ProcessPool::refill, this)));
			return;
		}
	}
	// start new process and enqueue request
	_wait_list.push_back({type, id, std::move(cb)});
	start(type);
}

void ProcessPool::retire(Process::ptr_t p, const std::string &service_name) {
	std::string id;
	auto ait = _active_pool.find(p->pid());
	if (ait != _active_pool.end()) {
		id = ait->second;
		_active_pool.erase(ait);
	}
	// is process still running?
	if (0 == kill(p->pid(), 0)) {
		_retired_pool[p->pid()] = {std::move(id), service_name, p};
		p->stop();
	}
}

size_t ProcessPool::pooled(const std::string & type) const {
	auto rit = _ready_pool.find(type);
	if (rit == _ready_pool.end())
		return 0;
	return rit->second.size();
}

void ProcessPool::start(const std::string & type) {
	// get process configuration
	auto registry = Reg::Registry::instance();
	pipeline_cfg_t config;
	registry->get("pipelines", type, config);
	registry->get("environment", type, config.env_vars);

	std::string service_name = PIPELINE_CONNECTION_BASE_ID + GenerateUniqueID()();

	// start process
	std::string cmd = config.bin + " -s" + service_name;
	using namespace std::placeholders;
	Process::ptr_t proc = std::make_shared<Process>
			(cmd, std::bind(&ProcessPool::exited, this, _1 ,_2), config.env_vars);
	_started_pool.emplace(std::make_pair(service_name, proc_info_t{type, service_name, proc}));

	LOG_INFO(_log, MSGNFO_PROC_STARTED, "New process %d of type %s started.", proc->pid(), type.c_str());

	// subscribe to service ready event
	_watcher.watch(service_name, std::bind(&ProcessPool::ready, this, proc->pid(), service_name));
}

void ProcessPool::exited(pid_t pid, int status) {
	// try to free retired process.
	auto rit = _retired_pool.find(pid);
	if (rit != _retired_pool.end()) {
		_exited_cb(rit->second.id);
		_watcher.unwatch(rit->second.ls2_server);
		_retired_pool.erase(rit);
		LOG_DEBUG(_log, "Retired process %d exited with status %d.", pid, status);
		return;
	}

	// premature exit (policy action, crash, etc) of working process.
	auto wit = _active_pool.find(pid);
	if (wit != _active_pool.end()) {
		_exited_cb(wit->second);
		_watcher.unwatch(wit->second);
		LOG_WARNING(_log, MSGERR_PROC_EXIT, "Active process %d exited with status %d.", pid, status);
		return;
	}

	// premature exit of pooled pipeline (highly unlikely).
	for (auto sit = _started_pool.begin(); sit != _started_pool.end(); ++sit) {
		if (sit->second.process->pid() == pid) {
			_watcher.unwatch(sit->second.service_name);
			_started_pool.erase(sit);
			LOG_ERROR(_log, MSGERR_PROC_EXIT, "Pooled process %d exited with status %d.", pid, status);
			return;
		}
	}
	for (auto cit = _ready_pool.begin(); cit != _ready_pool.end(); ++cit) {
		auto & pool = cit->second;
		auto pit = pool.find(pid);
		if (pit != pool.end()) {
			_watcher.unwatch(pit->second.service_name);
			pool.erase(pit);
			LOG_ERROR(_log, MSGERR_PROC_EXIT, "Pooled process %d exited with status %d.", pid, status);
			if (need_refill())
				_refill_timer.reset(new GMainTimer(1000 * _pool_config.refill_delay,
												   std::bind(&ProcessPool::refill, this)));
			return;
		}
	}
}

void ProcessPool::ready(pid_t pid, const std::string & service_name) {
	LOG_DEBUG(_log, "New process %d registered on the bus as %s.", pid, service_name.c_str());
	auto pit = _started_pool.find(service_name);
	if (pit != _started_pool.end()) {
		const auto & pi = pit->second;
		// do we have active waiter of the same type
		auto wit = std::find_if(_wait_list.begin(), _wait_list.end(),
								[&pi](dispatch_queue_t::const_reference dr){
			return dr.type == pi.type;
		});
		if (wit != _wait_list.end()) {
			// dispatch process
			_active_pool[pid] = wit->id;
			wit->cb(service_name, pi.process);
			_wait_list.erase(wit);
		} else {
			// move process to college pool
			_ready_pool[pi.type].emplace(std::make_pair(pi.process->pid(), pi));
		}
		_started_pool.erase(pit);
	}
	if (need_refill())
		_refill_timer.reset(new GMainTimer(1000 * _pool_config.refill_delay,
										   std::bind(&ProcessPool::refill, this)));
}

bool ProcessPool::need_refill() const {
	if (_refill_timer || _started_pool.size())
		return false;
	for (const auto & rp : _ready_pool) {
		if (_pool_config.pool_size.at(rp.first) > rp.second.size())
			return true;
	}
	return false;
}

void ProcessPool::refill() {
	_refill_timer.reset();
	for (const auto & rp : _ready_pool) {
		if (_pool_config.pool_size.at(rp.first) > rp.second.size())
			return start(rp.first);
	}
}

} // namespace uMediaServer
