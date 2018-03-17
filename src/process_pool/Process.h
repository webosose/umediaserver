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

#ifndef __PROCESS_H__
#define __PROCESS_H__

#include <functional>
#include <list>
#include <memory>
#include <GLibHelper.h>

namespace uMediaServer {

struct env_var_t {
	std::string name;
	std::string value;
	std::string op;
};

class Process {
public:
	typedef std::function<void(pid_t, int)> exit_callback_t;
	typedef std::list<env_var_t> environment_t;
	typedef std::shared_ptr<Process> ptr_t;
	Process(const std::string & cmd, exit_callback_t && exit_cb = nullptr,
			const environment_t & env = {});
	Process(std::function<void()> && proc, exit_callback_t && exit_cb = nullptr,
			const environment_t & env = {});
	~Process();

	pid_t pid() const;
	void stop(size_t timeout_ms = 500);

private:
	pid_t _pid;
	exit_callback_t _exit_cb;
	std::unique_ptr<GMainTimer> _kill_timer;
	size_t _watcher_tag;

	void setEnvironment(const environment_t & env) const;
	void setChildWatcher();

	// noncopyable
	Process(const Process &) = delete;
	Process & operator =(const Process &) = delete;
};

} // namespace uMediaServer::pp

#endif /* __PROCESS_H__ */
