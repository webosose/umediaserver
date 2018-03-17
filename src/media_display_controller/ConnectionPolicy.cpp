#include <algorithm>
#include <Logger_macro.h>
#include "ConnectionPolicy.h"


namespace uMediaServer {

namespace {
Logger _log(UMS_LOG_CONTEXT_MDC);
}

VideoChannelConnection::VideoChannelConnection(ConnectionPolicy & policy) : _policy(policy) {}

mdc::sink_t VideoChannelConnection::try_connect(mdc::IMediaObject::ptr_t media) {
	_policy.log_connections("Video connection request", media->id());
	// TODO: remove these checks once mdc has full control over all media
	// this will make connection policy more aggressive
	if (!media->foreground())
		return mdc::sink_t::VOID;
	if (_policy._acb_spy.check_blacklist(media->appId()))
		return mdc::sink_t::VOID;
	if (media->hasVideo()) {
		auto it = std::find_if(_policy._requested.begin(), --_policy._requested.end(),
							   ConnectionPolicy::free_or_background);
		if (it == --_policy._requested.end()) {
			// fallback - now kickout media without focus
			it = std::find_if(_policy._requested.begin(), --_policy._requested.end(),
							  ConnectionPolicy::free_or_no_focus);
			// bad luck
			if (it == --_policy._requested.end())
				return mdc::sink_t::VOID;
		}
		it->second = media;
		LOG_DEBUG(_log, "Connecting video (%s) to %s", media->id().c_str(),
				  (it->first == mdc::sink_t::MAIN ? "MAIN" : "SUB"));
		return it->first;
	}
	return mdc::sink_t::VOID;
}

void VideoChannelConnection::commit_connect(mdc::IMediaObject::ptr_t media) {
	_policy.log_connections("Video connection commit", media->id());
	for (auto it = _policy._requested.begin(); it != --_policy._requested.end(); ++it) {
		auto requestor = it->second.lock();
		if (requestor && requestor->id() == media->id()) {
			_policy._connected[it->first] = media;
			break;
		}
	}
}

mdc::sink_t VideoChannelConnection::connected(const std::string &id) const {
	return ConnectionPolicy::get(_policy._connected.begin(), --_policy._connected.end(), id);
}

mdc::sink_t VideoChannelConnection::requested(const std::string &id) const {
	return ConnectionPolicy::get(_policy._requested.begin(), --_policy._requested.end(), id);
}

void VideoChannelConnection::disconnected(const std::string &id) {
	_policy.log_connections("Video disconnected", id);
	auto cit = std::find_if(_policy._connected.begin(), --_policy._connected.end(),
							[&id](ConnectionPolicy::output_map_t::const_reference e) {
			auto media = e.second.lock();
			return media && media->id() == id;
	});
	if (cit != --_policy._connected.end()) {
		// clear connected media
		cit->second = mdc::IMediaObject::ptr_t();
		auto rit = _policy._requested.find(cit->first);
		auto requested_media = rit->second.lock();
		// clear requested media if it is match connected one
		if (requested_media && requested_media->id() == id) {
			rit->second = mdc::IMediaObject::ptr_t();
		}
	}
}

AudioChannelConnection::AudioChannelConnection(ConnectionPolicy & policy) : _policy(policy) {}

mdc::sink_t AudioChannelConnection::try_connect(mdc::IMediaObject::ptr_t media) {
	_policy.log_connections("Audio connection request", media->id());
	// TODO: remove these checks
	if (!media->foreground())
		return mdc::sink_t::VOID;
	if (_policy._acb_spy.check_blacklist(media->appId()))
		return mdc::sink_t::VOID;
	if (media->hasAudio()) {
		auto sit = _policy._requested.find(mdc::sink_t::SOUND);
		auto current = sit->second.lock();
		if (ConnectionPolicy::sound_connection_test(media, current, _policy._audio_stack)) {
			sit->second = media;
			LOG_DEBUG(_log, "Connecting audio (%s) to SOUND", media->id().c_str());
			return mdc::sink_t::SOUND;
		}
	}
	return mdc::sink_t::VOID;
}

void AudioChannelConnection::commit_connect(mdc::IMediaObject::ptr_t media) {
	_policy.log_connections("Audio connection commit", media->id());
	_policy._connected[mdc::sink_t::SOUND] = media;
}

mdc::sink_t AudioChannelConnection::connected(const std::string &id) const {
	return ConnectionPolicy::get(--_policy._connected.end(), _policy._connected.end(), id);
}

mdc::sink_t AudioChannelConnection::requested(const std::string &id) const {
	return ConnectionPolicy::get(--_policy._requested.end(), _policy._requested.end(), id);
}

void AudioChannelConnection::disconnected(const std::string &id) {
	_policy.log_connections("Audio disconnected", id);
	auto cit = --_policy._connected.end();
	auto connected_media = cit->second.lock();
	// filter out false positives
	if (connected_media && connected_media->id() == id) {
		auto rit = --_policy._requested.end();
		auto requested_media = rit->second.lock();
		// clear requested media if it matches connected
		if (requested_media && requested_media->id() == id) {
			rit->second = mdc::IMediaObject::ptr_t();
		}
		cit->second = mdc::IMediaObject::ptr_t();
	}
}

bool ConnectionPolicy::free_or_background(output_map_t::const_reference e) {
	auto media = e.second.lock();
	return (!media || !media->foreground());
}

bool ConnectionPolicy::free_or_no_focus (output_map_t::const_reference & e){
	auto media = e.second.lock();
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

ConnectionPolicy::ConnectionPolicy(const mdc::IAcbObserver & acb_spy,
								   const std::pair<std::string, std::string> & audio_stack)
	: _acb_spy(acb_spy), _audio_stack(audio_stack)
	, _audio_channel(*this), _video_channel(*this) {
	_requested[mdc::sink_t::MAIN]  = mdc::IMediaObject::ptr_t();
	_requested[mdc::sink_t::SUB]   = mdc::IMediaObject::ptr_t();
	_requested[mdc::sink_t::SOUND] = mdc::IMediaObject::ptr_t();
	_connected[mdc::sink_t::MAIN]  = mdc::IMediaObject::ptr_t();
	_connected[mdc::sink_t::SUB]   = mdc::IMediaObject::ptr_t();
	_connected[mdc::sink_t::SOUND] = mdc::IMediaObject::ptr_t();
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

mdc::IMediaObject::ptr_t ConnectionPolicy::connected(mdc::sink_t sink) const {
	return get(_connected.begin(), _connected.end(), sink);
}

mdc::IMediaObject::ptr_t ConnectionPolicy::requested(mdc::sink_t sink) const {
	return get(_requested.begin(), _requested.end(), sink);
}

void ConnectionPolicy::log_connections(const std::string &prefix, const std::string &id) const {
	auto c_main  = _connected.at(mdc::sink_t::MAIN).lock();  auto r_main  = _requested.at(mdc::sink_t::MAIN).lock();
	auto c_sub   = _connected.at(mdc::sink_t::SUB).lock();   auto r_sub   = _requested.at(mdc::sink_t::SUB).lock();
	auto c_sound = _connected.at(mdc::sink_t::SOUND).lock(); auto r_sound = _requested.at(mdc::sink_t::SOUND).lock();
	LOG_DEBUG(_log, "%s (%s): MAIN => %s[%s], SUB => %s[%s], SOUND => %s[%s]", prefix.c_str(), id.c_str(),
			  (c_main  ? c_main ->id().c_str() : "NULL"), (r_main  ? r_main ->id().c_str() : "NULL"),
			  (c_sub   ? c_sub  ->id().c_str() : "NULL"), (r_sub   ? r_sub  ->id().c_str() : "NULL"),
			  (c_sound ? c_sound->id().c_str() : "NULL"), (r_sound ? r_sound->id().c_str() : "NULL"));
}

mdc::IMediaObject::ptr_t ConnectionPolicy::get(const_iter_t b, const_iter_t e, mdc::sink_t sink) {
	auto it = std::find_if(b, e, [&sink](output_map_t::const_reference m){
			return m.first == sink;
	});
	if (it != e)
		return it->second.lock();
	return nullptr;
}

mdc::sink_t ConnectionPolicy::get(const_iter_t b, const_iter_t e, const std::string & id) {
	auto it = std::find_if(b, e, [&id](output_map_t::const_reference m) {
			auto media = m.second.lock();
			return media && media->id() == id;
	});
	if (it != e)
		return it->first;
	return mdc::sink_t::VOID;
}

}
