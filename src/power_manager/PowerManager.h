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

#ifndef __POWER_MANAGER_H__
#define __POWER_MANAGER_H__

#include <Logger.h>
#include <UMSConnector.h>

#include "interfaces.h"

namespace uMediaServer { namespace pwr {

class PowerManager : public IPowerManager {
public:
	PowerManager(UMSConnector *);
	virtual void set_shutdown_handler(callback_t &&) override;

private:
	UMSConnector * connector_;
	callback_t shutdown_callback_;
};

}} // namespace uMediaServer::pwr

#endif // __POWER_MANAGER_H__
