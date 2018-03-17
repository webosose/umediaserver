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

#ifndef __PROCESS_POOL_H__
#define __PROCESS_POOL_H__

#include <functional>
#include <map>
#include <list>
#include <boost/fusion/adapted.hpp>
#include "Process.h"

namespace uMediaServer {

struct pipeline_cfg_t {
	std::string type;
	std::string name;
	std::string bin;
	size_t pool_size;
	size_t pool_fill_delay;
	size_t pool_refill_delay;
	std::string schema_file;
	size_t priority;
	size_t max_restarts;

	std::list<env_var_t> env_vars;
};

struct IServiceReadyWatcher {
	virtual void watch(const std::string & service_name, std::function<void()> && ready_callback) = 0;
	virtual void unwatch(const std::string & service_name) = 0;
};

class ProcessPool {
public:
	typedef std::function<void(const std::string & service, Process::ptr_t p)> dispatch_cb_t;
	typedef std::function<void(const std::string & id)> exited_cb_t;

	ProcessPool(IServiceReadyWatcher & watcher, exited_cb_t && exited_cb);
	~ProcessPool();

	void hire(const std::string & type, const std::string & id, dispatch_cb_t && cb);
	void retire(Process::ptr_t p, const std::string &service_name = "");
	size_t pooled(const std::string & type) const;

private:
	struct proc_info_t {
		std::string type;
		std::string service_name;
		Process::ptr_t process;
	};
	typedef std::map<pid_t, proc_info_t> proc_map_t;
	struct dispatch_info_t {
		std::string type;
		std::string id;
		dispatch_cb_t cb;
	};
	typedef std::list<dispatch_info_t> dispatch_queue_t;
	struct proc_id_t {
		std::string id;
		std::string ls2_server;
		Process::ptr_t process;
	};

	struct pool_config_t {
		std::map<std::string, size_t> pool_size;
		size_t refill_delay = 0;
	};

	void start(const std::string & type);
	void exited(pid_t pid, int status);
	void ready(pid_t pid, const std::string & service_name);
	void refill();
	bool need_refill() const;

	std::map<std::string, proc_info_t>  _started_pool;  // just started processes
	std::map<std::string, proc_map_t>   _ready_pool;    // ready to use processes
	std::map<pid_t, std::string>        _active_pool;   // active processes to their media id map
	std::map<pid_t, proc_id_t>          _retired_pool;  // retired processes

	dispatch_queue_t                    _wait_list;     // process dispatch wait list

	IServiceReadyWatcher &              _watcher;       // process ready state watcher
	exited_cb_t                         _exited_cb;     // active / retired process exit callback
	std::unique_ptr<GMainTimer>         _refill_timer;  // process pool refill timer
	pool_config_t                       _pool_config;   // pooling configuration
};

} // namespace uMediaServer


// Note: boost::fusion adapt struct must be in global name space
//      reason = unknown black magic template stuff.
BOOST_FUSION_ADAPT_STRUCT (
		::uMediaServer::env_var_t,
		(std::string, name)
		(std::string, value)
		(std::string, op))

BOOST_FUSION_ADAPT_STRUCT (
		::uMediaServer::pipeline_cfg_t,
		(std::string, type)
		(std::string, name)
		(std::string, bin)
		(size_t, pool_size)
		(size_t, pool_fill_delay)
		(size_t, pool_refill_delay)
		(std::string, schema_file)
		(size_t, priority)
		(size_t, max_restarts))

#endif /* __PROCESS_POOL_H__ */
