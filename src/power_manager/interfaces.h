// Copyright (c) 2017-2018 LG Electronics, Inc.
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

#ifndef __POWER_MANAGER_INTERFACES_H__
#define __POWER_MANAGER_INTERFACES_H__

#include <functional>

namespace uMediaServer { namespace pwr {

struct IPowerManager {
	typedef std::function<void()> callback_t;
	// shutdown signal handler
	virtual void set_shutdown_handler(callback_t &&) = 0;
};

}} // namespace uMediaServer::pwr

#endif // __POWER_MANAGER_INTERFACES_H__
