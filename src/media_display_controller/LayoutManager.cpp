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

#include <algorithm>

#include <MediaDisplayController.h>
#include "LayoutManager.h"

namespace uMediaServer {

LayoutManager::LayoutManager(const mdc::IConnectionPolicy & policy, int width, int height)
	: _connection_policy(policy), _width(width), _height(height) {}

#if UMS_INTERNAL_API_VERSION == 2
mdc::display_out_t LayoutManager::suggest_layout(const ums::video_info_t & vi, const std::string & id) const {
	static int number_of_autolayouted_videos;
        number_of_autolayouted_videos = MediaDisplayController::instance()->numberOfAutoLayoutedVideos();
        if (!number_of_autolayouted_videos)
                number_of_autolayouted_videos = 1;

	static auto fit_frame = [this](int width, int height)->rect_t {
		if (width <= 0 || height <= 0) return {};
		double factor = std::min((1.0/number_of_autolayouted_videos) * _width / width, double(_height) / height);
		return {0, 0, int(factor * width), int(factor * height)};
	};
	rect_t fit_rc = fit_frame(vi.width, vi.height);
	auto sink = _connection_policy.video().sink_name(id);
	mdc::display_out_t result;
	if (sink.find("MAIN") != std::string::npos) {
		result = mdc::display_out_t(fit_rc);
	} else if (sink.find("SUB") != std::string::npos) {
		fit_rc.x += _width/number_of_autolayouted_videos;
		result = mdc::display_out_t(fit_rc);
	}
	if (_layout_change_callback) _layout_change_callback(id, result);
	return result;
}
#else
mdc::display_out_t LayoutManager::suggest_layout(const mdc::video_info_t & vi, const std::string & id) const {
	static int number_of_autolayouted_videos;
        number_of_autolayouted_videos = MediaDisplayController::instance()->numberOfAutoLayoutedVideos();
        if (!number_of_autolayouted_videos)
                number_of_autolayouted_videos = 1;

	static auto fit_frame = [this](int width, int height)->rect_t {
		if (width <= 0 || height <= 0) return {};
		double factor = std::min((1.0/number_of_autolayouted_videos) * _width / width, double(_height) / height);
		return {0, 0, int(factor * width), int(factor * height)};
	};
	rect_t fit_rc = fit_frame(vi.width, vi.height);
	auto sink = _connection_policy.video().sink_name(id);
	mdc::display_out_t result;
	if (sink.find("MAIN") != std::string::npos) {
		result = mdc::display_out_t(fit_rc);
	} else if (sink.find("SUB") != std::string::npos) {
		fit_rc.x += _width/number_of_autolayouted_videos;
		result = mdc::display_out_t(fit_rc);
	}

	if (_layout_change_callback) _layout_change_callback(id, result);
	return result;
}
#endif

void LayoutManager::set_layout_change_callback(layout_change_callback_t && callback) {
	_layout_change_callback = callback;
}

} // namespace uMediaServer
