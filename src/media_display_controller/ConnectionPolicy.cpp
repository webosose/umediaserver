// Copyright (c) 2016-2019 LG Electronics, Inc.
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
#include <Logger_macro.h>
#include <sstream>
#include <utility>
#include "ConnectionPolicy.h"


namespace uMediaServer {

namespace {
Logger _log(UMS_LOG_CONTEXT_MDC);
}

VideoChannelConnection::VideoChannelConnection(ConnectionPolicy & policy) : _policy(policy) {}

int32_t VideoChannelConnection::try_connect(mdc::IMediaObject::ptr_t media) {
	_policy.log_connections("Video connection request", media->id());
	// TODO: remove these checks once mdc has full control over all media
	// this will make connection policy more aggressive

	if (!media->foreground())
		return -1;
	if (media->hasVideo()) {
		auto it = std::find_if(_policy._video_requested.begin(), _policy._video_requested.end(),
							   ConnectionPolicy::free_or_background);
		if (it == _policy._video_requested.end()) {
			// fallback - now kickout media without focus
			it = std::find_if(_policy._video_requested.begin(), _policy._video_requested.end(),
							  ConnectionPolicy::free_or_no_focus);
			// bad luck
			if (it == _policy._video_requested.end())
				return -1;
		}
		it->second.wptr = media;
		LOG_DEBUG(_log, "Connecting video (%s) to %s, first:%d", media->id().c_str(),
				  it->second.name.c_str(), it->first);
		return it->first;
	}
	return -1;
}

void VideoChannelConnection::commit_connect(mdc::IMediaObject::ptr_t media) {
	_policy.log_connections("Video connection commit", media->id());
	for (auto it = _policy._video_requested.begin(); it != _policy._video_requested.end(); ++it) {
		auto requestor = it->second.wptr.lock();
		if (requestor && requestor->id() == media->id()) {
			_policy._video_connected[it->first].wptr = media;
			break;
		}
	}
}

int32_t VideoChannelConnection::connected(const std::string &id) const {
	return ConnectionPolicy::get(_policy._video_connected.begin(), _policy._video_connected.end(), id);
}

int32_t VideoChannelConnection::requested(const std::string &id) const {
	return ConnectionPolicy::get(_policy._video_requested.begin(), _policy._video_requested.end(), id);
}

std::string VideoChannelConnection::sink_name(const std::string & id, const int32_t type) const {
	ConnectionPolicy::output_map_t & output_map = (type == 0 ? _policy._video_connected : _policy._video_requested);
	auto cit = std::find_if(output_map.begin(), output_map.end(),
							[&id](ConnectionPolicy::output_map_t::const_reference e) {
			auto media = e.second.wptr.lock();
			return media && media->id() == id;
	});
	if (cit != output_map.end()) {
		return cit->second.name;
	}
	return std::string();
}

void VideoChannelConnection::disconnected(const std::string &id) {
	_policy.log_connections("Video disconnected", id);
	auto cit = std::find_if(_policy._video_connected.begin(), _policy._video_connected.end(),
							[&id](ConnectionPolicy::output_map_t::const_reference e) {
			auto media = e.second.wptr.lock();
			return media && media->id() == id;
	});
	if (cit != _policy._video_connected.end()) {
		// clear connected media
		cit->second.wptr = mdc::IMediaObject::ptr_t();
		auto rit = _policy._video_requested.find(cit->first);
		auto requested_media = rit->second.wptr.lock();
		// clear requested media if it is match connected one
		if (requested_media && requested_media->id() == id) {
			rit->second.wptr = mdc::IMediaObject::ptr_t();
		}
	}
}

AudioChannelConnection::AudioChannelConnection(ConnectionPolicy & policy) : _policy(policy) {}

int32_t AudioChannelConnection::try_connect(mdc::IMediaObject::ptr_t media) {
	_policy.log_connections("Audio connection request", media->id());
	// TODO: remove these checks
	if (!media->foreground())
		return -1;
	if (media->hasAudio()) {

		auto sit = std::find_if(_policy._audio_requested.begin(), _policy._audio_requested.end(), [](ConnectionPolicy::output_map_t::const_reference & e){
		                        return e.second.name == "SOUND";});
		auto current = sit->second.wptr.lock();
		if (ConnectionPolicy::sound_connection_test(media, current, _policy._audio_stack)) {
			sit->second.wptr = media;
			LOG_DEBUG(_log, "Connecting audio (%s) to SOUND", media->id().c_str());
			return sit->first;
		}
	}
	return -1;
}

void AudioChannelConnection::commit_connect(mdc::IMediaObject::ptr_t media) {
	_policy.log_connections("Audio connection commit", media->id());
	auto sit = std::find_if(_policy._audio_requested.begin(), _policy._audio_requested.end(), [](ConnectionPolicy::output_map_t::const_reference & e){
	                        return e.second.name == "SOUND";});
	if (sit != _policy._audio_requested.end())
		sit->second.wptr = media;
}

int32_t AudioChannelConnection::connected(const std::string &id) const {
	return ConnectionPolicy::get(_policy._audio_connected.begin(), _policy._audio_connected.end(), id);
}

int32_t AudioChannelConnection::requested(const std::string &id) const {
	return ConnectionPolicy::get(_policy._audio_requested.begin(), _policy._audio_requested.end(), id);
}

std::string AudioChannelConnection::sink_name(const std::string & id, const int32_t type) const {
	ConnectionPolicy::output_map_t & output_map = (type == 0 ? _policy._audio_connected : _policy._audio_requested);
	auto cit = std::find_if(output_map.begin(), output_map.end(),
							[&id](ConnectionPolicy::output_map_t::const_reference e) {
			auto media = e.second.wptr.lock();
			return media && media->id() == id;
	});
	if (cit != output_map.end()) {
		return cit->second.name;
	}
	return std::string();
}


void AudioChannelConnection::disconnected(const std::string &id) {
	_policy.log_connections("Audio disconnected", id);
	auto cit = _policy._audio_connected.end();
	auto connected_media = cit->second.wptr.lock();
	// filter out false positives
	if (connected_media && connected_media->id() == id) {
		auto rit = _policy._audio_requested.end();
		auto requested_media = rit->second.wptr.lock();
		// clear requested media if it matches connected
		if (requested_media && requested_media->id() == id) {
			rit->second.wptr = mdc::IMediaObject::ptr_t();
		}
		cit->second.wptr = mdc::IMediaObject::ptr_t();
	}
}

bool ConnectionPolicy::free_or_background(output_map_t::const_reference e) {
	auto media = e.second.wptr.lock();
	return (!media || !media->foreground());
}

bool ConnectionPolicy::free_or_no_focus (output_map_t::const_reference & e){
	auto media = e.second.wptr.lock();
	return (!media || !media->focus());
}

// true == connection accepted
bool ConnectionPolicy::sound_connection_test(mdc::IMediaObject::ptr_t in, mdc::IMediaObject::ptr_t out,
											 const std::pair<std::string, std::string> & audio_stack) {
	// empty media => reject
	if (!in) {
		LOG_DEBUG(_log, "Audio connection request REJECTED (in is null)");
		return false;
	}
	// sound sink is free => accept
	if (!out) {
		LOG_DEBUG(_log, "Audio connection request ACCEPTED (out is null)");
		return true;
	}
	LOG_DEBUG(_log, "Audio connection request: in = %s, out = %s", in->id().c_str(), out->id().c_str());
	// fix spontanious reconnects in dass
	if (in->id() == out->id()) {
		LOG_DEBUG(_log, "Audio connection request ACCEPTED (out and in are the same)");
		return true;
	}
	// background media => reject
	if (!in->foreground()) {
		LOG_DEBUG(_log, "Audio connection request REJECTED (in is in a background)");
		return false;
	}
	// background media => accept
	if (!out->foreground()) {
		LOG_DEBUG(_log, "Audio connection request ACCEPTED (out is in a background)");
		return true;
	}
	// sound sink is ocuppied bu focused pipeline from the same app => reject, otherwise => accept
	if (in->appId() == out->appId()) {
		LOG_DEBUG(_log, "Audio connection request %s (based on out focus)",
				 !out->focus() ? "ACCEPTED" : "REJECTED");
		return !out->focus();
	}
	// special case to address dass reconnects with music widget on top => reject
	if (!out->hasVideo() && in->focus() && in->id() == audio_stack.second) {
		LOG_DEBUG(_log, "Audio connection request REJECTED (focus reconnect attempt with music widget on)");
		return false;
	}
	// default => accept
	LOG_DEBUG(_log, "Audio connection request ACCEPTED (by default)");
	return true;
}

ConnectionPolicy::ConnectionPolicy(		   const std::pair<std::string, std::string> & audio_stack)
	: _audio_stack(audio_stack)
	, _audio_channel(*this), _video_channel(*this) {

  set_audio_object(0, "SOUND");
}

mdc::IChannelConnection & ConnectionPolicy::audio() {
	return _audio_channel;
}

const mdc::IChannelConnection & ConnectionPolicy::audio() const {
	return _audio_channel;
}

mdc::IChannelConnection & ConnectionPolicy::video() {
	return _video_channel;
}

const mdc::IChannelConnection & ConnectionPolicy::video() const {
	return _video_channel;
}

mdc::IMediaObject::ptr_t ConnectionPolicy::video_connected(const std::string & sink_name) const {
	return get(sink_name, _video_connected.begin(), _video_connected.end());
}

mdc::IMediaObject::ptr_t ConnectionPolicy::video_requested(const std::string & sink_name) const {
	return get(sink_name, _video_requested.begin(), _video_requested.end());
}

mdc::IMediaObject::ptr_t ConnectionPolicy::audio_connected(const std::string & sink_name) const {
	return get(sink_name, _audio_connected.begin(), _audio_connected.end());
}

mdc::IMediaObject::ptr_t ConnectionPolicy::audio_requested(const std::string & sink_name) const {
	return get(sink_name, _audio_requested.begin(), _audio_requested.end());
}


void ConnectionPolicy::set_video_object(int32_t sink_index, std::string sink_name) {
	sink_name_map_t sink_map = {mdc::IMediaObject::ptr_t(), sink_name};
	_video_requested.insert(std::make_pair(sink_index, sink_map));
	_video_connected.insert(std::make_pair(sink_index, sink_map));
}

void ConnectionPolicy::set_audio_object(int32_t sink_index, std::string sink_name) {
	sink_name_map_t sink_map = {mdc::IMediaObject::ptr_t(), sink_name};
	_audio_requested.insert(std::make_pair(sink_index, sink_map));
	_audio_connected.insert(std::make_pair(sink_index, sink_map));

}

void ConnectionPolicy::log_connections(const std::string &prefix, const std::string &id) const {
	std::ostringstream str;
	str << prefix.c_str() << " (" << id.c_str() << "): ";
	for (int i = 0;  i < _video_connected.size(); i++) {
		auto c = _video_connected.at(i).wptr.lock();
		auto r = _video_requested.at(i).wptr.lock();
		str << _video_connected.at(i).name << " => " << (c ? c->id().c_str() : "NULL") << "[" << (r ? r->id().c_str() : "NULL") << "], ";
	}

	for (int i = 0; i < _audio_connected.size(); i++) {
		auto c = _audio_connected.at(i).wptr.lock();
		auto r = _audio_requested.at(i).wptr.lock();
		str << _audio_connected.at(i).name << " => " << (c ? c->id().c_str() : "NULL") << "[" << (r ? r->id().c_str() : "NULL") << "]";
	}

	LOG_DEBUG(_log, "%s", str.str().c_str());
}


mdc::IMediaObject::ptr_t ConnectionPolicy::get(const_iter_t b, const_iter_t e, int32_t sink) {
	auto it = std::find_if(b, e, [&sink](output_map_t::const_reference m){
			return m.first == sink;
	});
	if (it != e)
		return it->second.wptr.lock();
	return nullptr;
}

mdc::IMediaObject::ptr_t ConnectionPolicy::get(const std::string & sink_name, const_iter_t b, const_iter_t e) {
	auto it = std::find_if(b, e, [&sink_name](output_map_t::const_reference m){
			return m.second.name == sink_name;
	});
	if (it != e)
		return it->second.wptr.lock();
	return nullptr;
}

int32_t ConnectionPolicy::get(const_iter_t b, const_iter_t e, const std::string & id) {
	auto it = std::find_if(b, e, [&id](output_map_t::const_reference m) {
			auto media = m.second.wptr.lock();
			return media && media->id() == id;
	});
	if (it != e)
		return it->first;
	return -1;
}

}
