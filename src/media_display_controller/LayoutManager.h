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

#ifndef __LAYOUT_MANAGER_H__
#define __LAYOUT_MANAGER_H__

#include <functional>
#include "interfaces.h"

namespace uMediaServer {

class LayoutManager : public mdc::ILayoutManager {
public:
	typedef std::function<void(const std::string &, const mdc::display_out_t &)> layout_change_callback_t;
	LayoutManager(const mdc::IConnectionPolicy & policy, int width, int height);

#if UMS_INTERNAL_API_VERSION == 2
	virtual mdc::display_out_t suggest_layout(const ums::video_info_t & vi, const std::string & id) const override;
#else
	virtual mdc::display_out_t suggest_layout(const mdc::video_info_t & vi, const std::string & id) const override;
#endif
	void set_layout_change_callback(layout_change_callback_t && callback);

private:
	const mdc::IConnectionPolicy & _connection_policy;
	int _width;
	int _height;
	layout_change_callback_t _layout_change_callback;
};

} // namespace uMediaServer

#endif // __LAYOUT_MANAGER_H__
