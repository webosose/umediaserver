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

#include <unistd.h>
#include <sstream>
#include <iterator>
#include <cstring>
#include <signal.h>
#include <glib.h>
#include <vector>
#include "Process.h"

namespace uMediaServer {

Process::Process(const std::string & cmd, exit_callback_t && exit_cb,
				 const environment_t & env)	: _exit_cb(exit_cb), _watcher_tag(0) {
	_pid = fork();
	if (0 == _pid) {
		setEnvironment(env);
		std::istringstream iss(cmd);
		std::vector<std::string> tokens {std::istream_iterator<std::string>{iss},
										 std::istream_iterator<std::string>{}};
		char **argv = new char *[tokens.size() + 1];
		size_t v = 0;
		for (const auto & token : tokens) {
			argv[v] = new char[token.size() + 1];
			strncpy(argv[v++], token.c_str(),token.size()+1);
		}
		argv[v] = nullptr;
		execv(argv[0], argv);
		_exit(0);
	} else {
		setChildWatcher();
	}
}

Process::Process(std::function<void()> && proc, exit_callback_t && exit_cb,
				 const environment_t & env) : _exit_cb(exit_cb) {
	_pid = fork();
	if (0 == _pid) {
		setEnvironment(env);
		proc();
		_exit(0);
	} else {
		setChildWatcher();
	}
}

Process::~Process() {
	if (_watcher_tag)
		g_source_remove(_watcher_tag);
}

pid_t Process::pid() const {
	return _pid;
}

void Process::stop(size_t timeout_ms) {
	_kill_timer.reset(new GMainTimer(timeout_ms, [this](){
		auto pid = _pid;
		kill(pid, SIGTERM);
		_kill_timer.reset(new GMainTimer(5000, [pid](){ kill(pid, SIGKILL); }));
	}));
}

void Process::setEnvironment(const environment_t &env) const {
	for (auto const & e : env) {
		char * cur_val = getenv(e.name.c_str());
		std::string new_val;
		if (cur_val && (e.op == "PREPEND" || e.op == "APPEND")) {
			if (e.op == "PREPEND") {
				new_val = e.value + ":" + cur_val;
			}
			else {
				new_val = std::string(cur_val) + ":" + e.value;
			}
		}
		else {
			new_val = e.value;
		}
		setenv(e.name.c_str(), new_val.c_str(), 1);
	}
}

void Process::setChildWatcher() {
	_watcher_tag = g_child_watch_add(_pid, [](GPid pid, gint status, gpointer context){
			Process * self = static_cast<Process*>(context);
			// glib autoremoves child watch source after this call
			self->_watcher_tag = 0;
			if (pid == self->_pid && self->_exit_cb) self->_exit_cb(pid, status);
	}, this);
}

} // namespace uMediaServer
