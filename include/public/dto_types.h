// Copyright (c) 2015-2019 LG Electronics, Inc.
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

#ifndef __MDC_HFSM_DTO_TYPES_H__
#define __MDC_HFSM_DTO_TYPES_H__

#include <iostream>
#include <set>
#include <vector>
#include <cmath>

#define EPSILON 0.000001

typedef enum PLAYBACK_STATE {
	PLAYBACK_STOPPED,
	PLAYBACK_PAUSED,
	PLAYBACK_PLAYING
} playback_state_t;

namespace ums {

struct error_t {
	uint32_t code;
	std::string message;
};

typedef ::playback_state_t playback_state_t;

typedef int64_t time_t; // time in milliseconds

struct rational_t {
	int32_t num;
	int32_t den;
};

struct stream_info_t {
	std::string codec;
	uint64_t bit_rate;
};

struct video_info_t : stream_info_t {
	uint32_t width;
	uint32_t height;
	rational_t frame_rate;
};

struct audio_info_t : stream_info_t {
	uint32_t sample_rate;
};

struct program_info_t {
	uint32_t video_stream;
	uint32_t audio_stream;
};

struct source_info_t {
	std::string container;
	time_t duration;
	bool seekable;
	std::vector<program_info_t> programs;
	std::vector<video_info_t> video_streams;
	std::vector<audio_info_t> audio_streams;
};

struct disp_res_t {
	int32_t plane_id;
	int32_t crtc_id;
	int32_t conn_id;
};

} // namespace ums

namespace uMediaServer {

struct rect_t {
	rect_t(int _x = 0, int _y = 0, int _w = 0, int _h = 0) : x(_x), y(_y), w(_w), h(_h) {}
	operator bool () const { return w > 0 && h > 0; }
	bool operator == (const rect_t & other) const {
		return x == other.x && y == other.y && w == other.w && h == other.h;
	}
	friend std::ostream & operator << (std::ostream & os, const rect_t & r) {
		return os << "(" << r.x << "," << r.y << "; " << r.w << "x" << r.h << ")";
	}
	int x, y, w, h;
};

namespace mdc {

struct video3d_t {
	video3d_t() : original("2d"), current("2d"), type_lr("LR") {}
	std::string original;
	std::string current;
	std::string type_lr;
	bool operator == (const video3d_t & other) const {
		return original == other.original &&
				current == other.current &&
				type_lr == other.type_lr;
	}
	friend std::ostream & operator << (std::ostream & os, const video3d_t &v) {
		return os << "original=" << v.original
				  << ",current=" << v.current
				  << ",type_lr=" << v.type_lr;}
};

struct ratio_t {
	ratio_t() : width(1), height(1) {}
	int32_t width;
	int32_t height;
	bool operator == (const ratio_t & other) const {
		return width == other.width && height == other.height;
	}
	friend std::ostream & operator << (std::ostream & os, const ratio_t &r) {
		return os << r.width << ":" << r.height; }
};

inline bool areSame(double a, double b) {
   return fabs(a - b) < EPSILON;
}

struct video_info_t {
	video_info_t() : width(0), height(0), frame_rate(0),
		scan_type("progressive"), bit_rate(0),
		adaptive(false), path("file"), content("movie") {}

	int width;
	int height;
	double frame_rate;
	std::string scan_type;       // "VIDEO_PROGRESSIVE"
	ratio_t pixel_aspect_ratio;  // "1:1"
	int32_t bit_rate;
	video3d_t video3d;
	bool adaptive;
	std::string path;
	std::string content;         // "movie"
	bool operator == (const video_info_t & other) const {
		return width == other.width && height == other.height &&
				areSame(frame_rate, other.frame_rate) &&
				pixel_aspect_ratio == other.pixel_aspect_ratio &&
				bit_rate == other.bit_rate &&
				video3d == other.video3d &&
				adaptive == other.adaptive &&
				path == other.path &&
				content == other.content;
	}
	friend std::ostream & operator << (std::ostream & os, const video_info_t &v) {
		return os << "width=" << v.width
				  << ",height=" << v.height
				  << ",frame_rate=" << v.frame_rate
				  << ",scan_type=" << v.scan_type
				  << ",pixel_aspect_ratio={" << v.pixel_aspect_ratio << "}"
				  << ",bit_rate=" << v.bit_rate
				  << ",video_3d={ " << v.video3d << "}"
				  << ",adaptive=" << (v.adaptive ? "true":"false")
				  << ",path=" << v.path
				  << ",content=" << v.content; }
};

struct res_t {
	res_t() : type(INVALID), index(-1) {}
	res_t(const std::string & u, int32_t i) : unit(u), index(i) {
		if(u == "VDEC" || u == "IMG_DEC" || u == "VHDMIRX" || u == "VADC" || u == "AVD" || u == "HDMI_INPUT")
			type = VIDEO;
		else if(u == "ADEC" || u == "PCMMC")
			type = AUDIO;
		else
			type = INVALID;
	}
	operator bool () const {
		return type != INVALID;
	}
	bool operator < (const res_t & other) const {
		return (type < other.type || unit < other.unit || index < other.index);
	}
	bool operator == (const res_t & other) const {
		return (type == other.type && unit == other.unit && index == other.index);
	}

	friend std::ostream & operator << (std::ostream & os, const res_t & res) {
		return os << res.unit << ":" << res.index;
	}
	enum { INVALID, VIDEO, AUDIO } type;
	std::string unit;
	int32_t index;
};

struct res_info_t {
	bool operator == (const res_info_t & other) const {
		return vdecs == other.vdecs && adecs == other.adecs;
	}
	bool operator != (const res_info_t & other) const {
		return !(*this == other);
	}
	operator bool () const {
		return !(vdecs.empty() && adecs.empty());
	}
	friend std::ostream & operator << (std::ostream & os, const res_info_t & res) {
		std::string prepend;
		os << "adecs = [";
		for (const auto & adec : res.adecs) {
			os << prepend << adec;
			prepend = ",";
		}
		prepend.clear();
		os << "], vdecs = [";
		for (const auto & vdec : res.vdecs) {
			os << prepend << vdec;
			prepend = ",";
		}
		os << "]";
		return os;
	}
	void add_resource(const std::string & unit, size_t index) {
		res_t res(unit, index);
		switch (res.type) {
			case res_t::VIDEO: vdecs.insert(res); break;
			case res_t::AUDIO: adecs.insert(res); break;
		}
	}

	std::set<res_t> adecs;
	std::set<res_t> vdecs;
};

struct display_out_t {
	display_out_t(const rect_t & _src_rect, const rect_t & _out_rect, bool _fs)
		: src_rect(_src_rect), out_rect(_out_rect), fullscreen(_fs) {}
	explicit display_out_t(const rect_t & _out_rect) : display_out_t(rect_t(), _out_rect, false) {}
	explicit display_out_t(bool _fs = false) : display_out_t(rect_t(), rect_t(), _fs) {}
	bool operator == (const display_out_t & other) const {
		return src_rect == other.src_rect && out_rect == other.out_rect &&
				fullscreen == other.fullscreen;
	}
	friend std::ostream & operator << (std::ostream & os, const display_out_t & d) {
		if (d.fullscreen)
			return os << "fullscreen";
		if (d.src_rect)
			os << d.src_rect << " => ";
		return os << d.out_rect;
	}
	rect_t src_rect;
	rect_t out_rect;
	bool fullscreen;
};

struct media_element_state_t {
	media_element_state_t(const std::string & _id,
						  const std::vector<std::string> & _states,
						  const std::pair<int32_t, int32_t> & _connections)
		: id(_id), states(_states), connections(_connections) {}
	media_element_state_t(const std::string & _id, const std::vector<std::string> & _states)
		: media_element_state_t(_id, _states, {-1, -1}) {}
	media_element_state_t(const std::string & _id, const std::pair<int32_t, int32_t> & _connections)
		: media_element_state_t(_id, std::vector<std::string>(), _connections) {}
	explicit media_element_state_t(const std::string & _id = std::string())
		: media_element_state_t(_id, std::vector<std::string>()) {}
	media_element_state_t(const media_element_state_t & mes)
		: media_element_state_t(mes.id, mes.states, mes.connections) {}
	operator bool() const {
		return !id.empty();
	}
	std::string id;
	std::vector<std::string> states;
	std::pair<int32_t, int32_t> connections;
};

}} // uMediaServer::mdc

#endif // __MDC_HFSM_DTO_TYPES_H__
