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

#ifndef DPC_H_
#define DPC_H_
#include <pthread.h>
#include <unistd.h>
#include <Logger_macro.h>

namespace uMediaServer {

template <class FuncType> // FuncType has to be a functor
class DPC {
public:
	DPC(unsigned int delay, FuncType f) : m_delay(delay), m_f(f),
		m_log(UMS_LOG_CONTEXT_PROCESS_CONTROLLER), m_cancelled(false) {}

	bool execute() {
		int error;
		void *arg = reinterpret_cast<void*>(this);
		error = pthread_create(&m_thread, NULL, delay_thread, arg);
		if (error) {
			LOG_ERROR_EX(m_log, MSGERR_THREAD_CREATE,
					__KV({{KVP_ERROR_CODE, error}}),
					"pthread_create failed with error %d", error);
			return false;
		}
		return true;
	}

	bool cancel() {
		m_cancelled = true;
		int error;
		int retval;
		error = pthread_join(m_thread, (void**)&retval);
		if (error) {
			LOG_ERROR_EX(m_log, MSGERR_THREAD_JOIN,
					__KV({{KVP_ERROR_CODE, error}}),
					"pthread_join failed with error %d", error);
			return false;
		}
		LOG_TRACE(m_log, "pthread_join finished ok with retval %d", retval);
		return true;
	}

private:
	Logger m_log;
	unsigned int m_delay;
	FuncType m_f;
	pthread_t m_thread;
	bool m_cancelled;
	static void *delay_thread(void * arg) {
		// TODO do we need to set pthread_mask for this thread
		const unsigned int tic = 500; /* check cancellation every 500 usec */
		unsigned int elapsed = 0;
		DPC *self = reinterpret_cast<DPC*>(arg);
		LOG_TRACE(self->m_log, "Sleeping %d us", self->m_delay);
		while(!self->m_cancelled && elapsed < self->m_delay) {
			usleep(tic);
			elapsed += tic;
		}
		if (self->m_cancelled) {
			LOG_TRACE(self->m_log, "Functor call cancelled.");
		}
		else {
			LOG_TRACE(self->m_log, "Calling functor %d", 0);
			self->m_f();
		}
		return NULL;
	}
	DPC(DPC const&);             // copy constructor is private
	DPC& operator=(DPC const&);  // assignment operator is private
};

} // namespace uMediaServer

#endif /* DPC_H_ */
