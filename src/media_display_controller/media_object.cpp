// Copyright (c) 2015-2018 LG Electronics, Inc.
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

#include <Logger_macro.h>
#include <boost/algorithm/string.hpp>
#include <cxxabi.h>
#include "media_object.h"

namespace uMediaServer { namespace mdc {

std::string demangle(const char* name)
{
	int status = 0;
	std::unique_ptr<char, void(*)(void*)> res {
		abi::__cxa_demangle(name, nullptr, nullptr, &status),std::free
	};
	return (status==0) ? res.get() : name ;
}

std::string strip(const std::string & symbol) {
	try {
		std::vector<std::string> tokens;
		boost::split(tokens, symbol, boost::is_any_of("::"));
		return tokens.back();
	} catch (...) {
		// boost split failure - return unstripped
		return symbol;
	}
}

namespace {
Logger _log(UMS_LOG_CONTEXT_MDC);
}

bool uMediaServer::mdc::MediaObject::debug_state = false;

// MediaObject state machine
MediaObject::MediaObject(const std::string &id, const std::string &app_id, ITVDisplay &c,
						 IConnectionPolicy &p, ILayoutManager &lm)
	: _id(id), _app_id(app_id), _connector(c), _policy(p), _layout_manager(lm), _config{nullptr, nullptr} {
	initiate();
}

const std::string & MediaObject::id() const {
	return _id;
}

const std::string & MediaObject::appId() const {
	return _app_id;
}

bool MediaObject::foreground() const {
	return state_downcast<const states::Foreground *>();
}

bool MediaObject::focus() const {
	return state_downcast<const states::Focused *>();
}

bool MediaObject::hasAudio() const {
	auto event_ptr = _config[RESOURCES_CONFIG];
	if (event_ptr) {
		return !static_cast<const event::Acquire&>(*event_ptr).resources.adecs.empty();
	}
	return false;
}

bool MediaObject::hasVideo() const {
	auto event_ptr = _config[RESOURCES_CONFIG];
	if (event_ptr) {
		return !static_cast<const event::Acquire&>(*event_ptr).resources.vdecs.empty();
	}
	return false;
}

bool MediaObject::autoLayout() const {
	bool return_value = false;

	const event::SwitchToAutoLayout *event_ptr = dynamic_cast<const event::SwitchToAutoLayout *> (_config[DISPLAY_WIN_CONFIG].get());
	if (event_ptr) {
		return_value = true;
	}
	return return_value;
}

ITVDisplay &MediaObject::connector() {
	return _connector;
}

IConnectionPolicy &MediaObject::connection_policy() {
	return _policy;
}

ILayoutManager &MediaObject::layout_manager() {
	return _layout_manager;
}

template <>
void MediaObject::store_event(const event::MediaVideoData & event) {
	_config[VIDEO_INFO_CONFIG] = event.intrusive_from_this();
}
template <>
void MediaObject::store_event(const event::SetDisplayWindow & event) {
	_config[DISPLAY_WIN_CONFIG] = event.intrusive_from_this();
}
template <>
void MediaObject::store_event(const event::SwitchToFullscreen & event) {
	_config[DISPLAY_WIN_CONFIG] = event.intrusive_from_this();
}
template <>
void MediaObject::store_event(const event::SwitchToAutoLayout & event) {
	_config[DISPLAY_WIN_CONFIG] = event.intrusive_from_this();
}
template <>
void MediaObject::store_event(const event::Acquire & event) {
	res_info_t ri;
	if (_config[RESOURCES_CONFIG])
		ri = static_cast<const event::Acquire&>(*_config[RESOURCES_CONFIG]).resources;
	if (ri.vdecs.empty() && !event.resources.vdecs.empty())
		ri.vdecs.insert(*event.resources.vdecs.begin());
	if (ri.adecs.empty() && !event.resources.adecs.empty())
		ri.adecs.insert(*event.resources.adecs.begin());
	_config[RESOURCES_CONFIG] = event::Acquire(ri).intrusive_from_this();
}
template <>
void MediaObject::store_event(const event::Release & event) {
	if (_config[RESOURCES_CONFIG]) {
		res_info_t ri = static_cast<const event::Acquire&>
							(*_config[RESOURCES_CONFIG]).resources;
		if (!ri.vdecs.empty() && event.resources.vdecs.count(*ri.vdecs.begin()))
			ri.vdecs.clear();
		if (!ri.adecs.empty() && event.resources.adecs.count(*ri.adecs.begin()))
			ri.adecs.clear();
		_config[RESOURCES_CONFIG] = event::Acquire(ri).intrusive_from_this();
	}
}
template <>
void MediaObject::store_event(const event::SetOpacity & event) {
	_config[OPACITY_CONFIG] = event.intrusive_from_this();
}

void MediaObject::emit_stored_event(EventType event_type ) {
	auto event = _config[event_type];
	if (event) post_event(*event);
}

const MediaObject::event_base_type & MediaObject::retreive_event(EventType event_type) {
	event_base_ptr_type event = nullptr;
	switch (event_type) {
	case VIDEO_INFO_CONFIG:
		event = _config[VIDEO_INFO_CONFIG]; break;
	case DISPLAY_WIN_CONFIG:
		event = _config[DISPLAY_WIN_CONFIG]; break;
	case RESOURCES_CONFIG:
		event = _config[RESOURCES_CONFIG]; break;
	case OPACITY_CONFIG:
		event = _config[OPACITY_CONFIG]; break;
	default:
		throw std::runtime_error("Unsopported event type");
	}
	if (nullptr == event)
		throw std::runtime_error("Event not set");
	return *event;
}

void MediaObject::unconsumed_event(const event_base_type & event) {
	auto event_type = event.dynamic_type();
	if (event_type == event::SetDisplayWindow::static_type()) {
		store_event(static_cast<const event::SetDisplayWindow&>(event));
	} else if (event_type == event::SwitchToFullscreen::static_type()) {
		store_event(static_cast<const event::SwitchToFullscreen&>(event));
	} else if (event_type == event::SwitchToAutoLayout::static_type()) {
		store_event(static_cast<const event::SwitchToAutoLayout&>(event));
	} else if (event_type == event::MediaVideoData::static_type()) {
		store_event(static_cast<const event::MediaVideoData&>(event));
	} else if (event_type == event::SetOpacity::static_type()) {
		store_event(static_cast<const event::SetOpacity&>(event));
	} else {
		LOG_WARNING(_log, MSGERR_SKIPPED_EVENT, "%s : unprocessed %s mdc event",
					id().c_str(), demangle(typeid(event).name()).c_str());
	}
}

void MediaObject::enableDebug( bool state )
{
	debug_state = state;
}

// override state_machine.hpp process_event to insert debug information
void MediaObject::process_event( const event_base_type & evt )
{
	LOG_DEBUG(_log, "processing event of type %s ...", demangle(typeid(evt).name()).c_str());
	if( debug_state ) DisplayStateConfiguration();
	state_machine::process_event(evt);  // call base class event to finalize event
}

std::vector<std::string> MediaObject::getStates() const {
	std::vector<std::string> states;
	for (auto state_it = state_begin(); state_it != state_end(); ++state_it) {
		states.push_back(strip(demangle(typeid(*state_it).name())));
	}
	return states;
}

void MediaObject::DisplayStateConfiguration() const
{
	LOG_DEBUG(_log, "+--------- MDC STATE ---------+");
	auto states = getStates();
	for (const auto & state : states) {
		LOG_DEBUG(_log, "+ %s", state.c_str());
	}
	LOG_DEBUG(_log, "+-----------------------------+");
}

void MediaObject::commit_unmute(size_t channel) {
	if (hasVideo() && hasAudio()) {
		if (channel & ITVDisplay::VIDEO_CHANNEL) {
			LOG_DEBUG(_log, "%s: Video unmute request: unmuting...", _id.c_str());
			_connector.avblock_unmute(_id, ITVDisplay::VIDEO_CHANNEL | ITVDisplay::AUDIO_CHANNEL);
		} else if (channel & ITVDisplay::AUDIO_CHANNEL) {
			if (state_downcast<const states::OnScreen *>()) {
				LOG_DEBUG(_log, "%s: Audio unmute request: unmuting...", _id.c_str());
				_connector.avblock_unmute(_id, ITVDisplay::VIDEO_CHANNEL | ITVDisplay::AUDIO_CHANNEL);
			} else {
				LOG_DEBUG(_log, "%s: Audio unmute request: waiting on video...", _id.c_str());
			}
		}
	} else
		_connector.avblock_unmute(_id, channel);
}

// Init state
states::Init::Init(my_context context) : my_base(context) {
	MediaObject & media_object = outermost_context();
	LOG_DEBUG(_log, "%s : <<Init>>", media_object.id().c_str());
}

// Background state
states::Background::Background(my_context context) : my_base(context) {
	MediaObject & media_object = outermost_context();
	LOG_DEBUG(_log, "%s : <<Background>>", media_object.id().c_str());
}

// Foreground state
states::Foreground::Foreground(my_context context) : my_base(context) {
	MediaObject & media_object = outermost_context();
	LOG_DEBUG(_log, "%s : <<Foreground>>", media_object.id().c_str());
}

states::Foreground::~Foreground() {
	post_event(event::DisconnectAudio());
	post_event(event::DisconnectVideo());
}

// Idle state
states::Idle::Idle(my_context context) : my_base(context) {
	MediaObject & media_object = outermost_context();
	LOG_DEBUG(_log, "%s : <<Idle>>", media_object.id().c_str());
}
boost::statechart::result states::Idle::react(const event::Acquire & evt) {
	MediaObject & media_object = outermost_context();
	res_t adec, vdec;

	if (!evt.resources.adecs.empty()) {
		adec = *evt.resources.adecs.begin();
	}
	if (!evt.resources.vdecs.empty()) {
		vdec = *evt.resources.vdecs.begin();
	}
	media_object.store_event(evt);
	media_object.connector().vsm_register(media_object.id(), adec, vdec);
	return discard_event();
}

// Registered state
states::Registered::Registered(my_context context)
	: my_base(context), connector(outermost_context().connector())
	, policy(outermost_context().connection_policy()), id(outermost_context().id()) {
	LOG_DEBUG(_log, "%s : <<Registered>>", id.c_str());
	if (outermost_context().state_downcast<const states::Foreground *>())
		post_event(event::TryDisplay());
}
states::Registered::~Registered() {
	connector.vsm_unregister(id);
}

boost::statechart::result states::Registered::react(const event::Release & evt) {
	MediaObject & media_object = outermost_context();
	const event::Acquire & old_res =
			static_cast<const event::Acquire &>
				(media_object.retreive_event(MediaObject::RESOURCES_CONFIG));
	media_object.store_event(evt);
	const event::Acquire & new_res =
			static_cast<const event::Acquire &>
				(media_object.retreive_event(MediaObject::RESOURCES_CONFIG));
	// do we need to unregister?
	if (new_res.resources != old_res.resources) {
		// do we need to reregister?
		if (new_res.resources)
			post_event(new_res);
		return transit< states::Idle >();
	}
	return discard_event();
}

boost::statechart::result states::Registered::react(const event::Acquire & evt) {
	MediaObject & media_object = outermost_context();
	const event::Acquire & old_res =
			static_cast<const event::Acquire &>
				(media_object.retreive_event(MediaObject::RESOURCES_CONFIG));
	media_object.store_event(evt);
	const event::Acquire & new_res =
			static_cast<const event::Acquire &>
				(media_object.retreive_event(MediaObject::RESOURCES_CONFIG));
	if (old_res.resources != new_res.resources) {
		// do we need to reregister?
		if (new_res.resources)
			post_event(new_res);
		return transit< states::Idle >();
	}
	return discard_event();
}

// VideoConnected state
states::VideoConnected::VideoConnected(my_context context)
	: my_base(context), connector(outermost_context().connector())
	, policy(outermost_context().connection_policy()), id(outermost_context().id()) {
	MediaObject & media_object = outermost_context();
	LOG_DEBUG(_log, "%s : <<VideoConnected>>", media_object.id().c_str());
	policy.video().commit_connect(media_object.shared_from_this());
	media_object.emit_stored_event(MediaObject::VIDEO_INFO_CONFIG);
	media_object.emit_stored_event(MediaObject::OPACITY_CONFIG);
	post_event(event::TryDisplay());
}

states::VideoConnected::~VideoConnected() {
	connector.display_set_alpha(id, 1.);
	policy.video().disconnected(id);
}

boost::statechart::result states::VideoConnected::react(const event::DisconnectVideo &) {
	MediaObject & media_object = outermost_context();
	media_object.connector().vsm_disconnect(id);
	return discard_event();
}

boost::statechart::result states::VideoConnected::react(const event::SetOpacity & evt) {
	MediaObject & media_object = outermost_context();
	media_object.store_event(evt);
	media_object.connector().display_set_alpha(id, evt.alpha);
	return discard_event();
}

// VideoConnecting state
states::VideoConnecting::VideoConnecting(my_context context) : my_base(context) {
	MediaObject & media_object = outermost_context();
	LOG_DEBUG(_log, "%s : <<VideoConnecting>>", media_object.id().c_str());
}

// VideoMuting state
states::VideoMuting::VideoMuting(my_context context) : my_base(context) {
	MediaObject & media_object = outermost_context();
	LOG_DEBUG(_log, "%s : <<VideoMuting>>", media_object.id().c_str());
}

boost::statechart::result states::VideoMuting::react(const event::Muted &) {
	MediaObject & media_object = outermost_context();
	auto sink = media_object.connection_policy().video().try_connect(media_object.shared_from_this());
	if (sink != mdc::sink_t::VOID) {
		media_object.connector().vsm_connect(media_object.id(), sink);
		return transit< states::VideoConnecting >();
	} else {
		return transit< states::VideoDisconnected >();
	}
}

// VideoDisconnected state
states::VideoDisconnected::VideoDisconnected(my_context context) : my_base(context) {
	MediaObject & media_object = outermost_context();
	LOG_DEBUG(_log, "%s : <<VideoDisconnected>>", media_object.id().c_str());
}

boost::statechart::result states::VideoDisconnected::react(const event::TryDisplay &) {
	MediaObject & media_object = outermost_context();
	if ( media_object.hasVideo() && outermost_context().state_downcast<const states::Foreground *>() ) {
		media_object.connector().avblock_mute(media_object.id(), ITVDisplay::VIDEO_CHANNEL);
		return transit< states::VideoMuting >();
	}
	return forward_event();
}

// OffScreen state
states::OffScreen::OffScreen(my_context context)
	: my_base(context) {
	MediaObject & media_object = outermost_context();
	LOG_DEBUG(_log, "%s : <<Offscreen>>", media_object.id().c_str());
}

boost::statechart::result states::OffScreen::react(const event::TryDisplay &) {
	if  (  outermost_context().state_downcast<const states::Foreground *>()
		&& outermost_context().state_downcast<const states::ContentReady *>()
		&& outermost_context().state_downcast<const states::MediaVideoDataConfigured *>()
		&& outermost_context().state_downcast<const states::DisplayWinConfigured *>()) {
		return transit<states::OnScreen>();
	}
	return forward_event();
}

// OnScreen state
states::OnScreen::OnScreen(my_context context)
	: my_base(context), connector(outermost_context().connector()), id(outermost_context().id()) {
	LOG_DEBUG(_log, "%s : <<OnScreen>>", id.c_str());
	outermost_context().commit_unmute(ITVDisplay::VIDEO_CHANNEL);
	post_event(event::TryDisplay());
}

states::OnScreen::~OnScreen() {
	connector.avblock_mute(id, ITVDisplay::VIDEO_CHANNEL);
}

boost::statechart::result states::OnScreen::react(const event::Mute &) {
	post_event(event::Mute());
	return transit<states::OffScreen>();
}

// MediaVideoDataNotConfigured state
states::MediaVideoDataNotConfigured::MediaVideoDataNotConfigured(my_context context) : my_base(context) {
	MediaObject & media_object = outermost_context();
	LOG_DEBUG(_log, "%s : <<MediaVideoDataNotConfigured>>", media_object.id().c_str());
}

boost::statechart::result states::MediaVideoDataNotConfigured::react(const event::MediaVideoData & evt) {
	MediaObject & media_object = outermost_context();
	media_object.store_event(evt);
	return transit<states::MediaVideoDataConfigured>();
}

// MediaVideoDataConfigured state
states::MediaVideoDataConfigured::MediaVideoDataConfigured(my_context context) : my_base(context) {
	MediaObject & media_object = outermost_context();
	LOG_DEBUG(_log, "%s : <<MediaVideoDataConfigured>>", media_object.id().c_str());
	media_object.emit_stored_event(MediaObject::VIDEO_INFO_CONFIG);
	media_object.emit_stored_event(MediaObject::DISPLAY_WIN_CONFIG);
	post_event(event::TryDisplay());
}

boost::statechart::result states::MediaVideoDataConfigured::react(const event::MediaVideoData & evt) {
	MediaObject & media_object = outermost_context();
	media_object.store_event(evt);
	media_object.connector().display_set_video_info(media_object.id(), evt.video_info);
	post_event(event::TryDisplay());
	return discard_event();
}

// DisplayWinNotConfigured state
states::DisplayWinNotConfigured::DisplayWinNotConfigured(my_context context) : my_base(context) {
	MediaObject & media_object = outermost_context();
	LOG_DEBUG(_log, "%s : <<DisplayWinNotConfigured>>", media_object.id().c_str());
}

boost::statechart::result states::DisplayWinNotConfigured::react(const event::SetDisplayWindow & evt) {
	MediaObject & media_object = outermost_context();
	media_object.store_event(evt);
	return transit<states::DisplayWinConfiguring>();
}

boost::statechart::result states::DisplayWinNotConfigured::react(const event::SwitchToFullscreen & evt) {
	MediaObject & media_object = outermost_context();
	media_object.store_event(evt);
	return transit<states::DisplayWinConfiguring>();
}

boost::statechart::result states::DisplayWinNotConfigured::react(const event::SwitchToAutoLayout & evt) {
	MediaObject & media_object = outermost_context();
	media_object.store_event(evt);
	return transit<states::DisplayWinConfiguring>();
}

// DisplayWinConfiguring state
states::DisplayWinConfiguring::DisplayWinConfiguring(my_context context) : my_base(context) {
	MediaObject & media_object = outermost_context();
	LOG_DEBUG(_log, "%s : <<DisplayWinNotConfiguring>>", media_object.id().c_str());
	media_object.emit_stored_event(MediaObject::DISPLAY_WIN_CONFIG);
}

boost::statechart::result states::DisplayWinConfiguring::react(const event::SetDisplayWindow & evt) {
	MediaObject & media_object = outermost_context();
	media_object.store_event(evt);
	media_object.connector().display_set_window(media_object.id(),
												display_out_t(evt.src_rect, evt.out_rect, false));
	return discard_event();
}

boost::statechart::result states::DisplayWinConfiguring::react(const event::SwitchToFullscreen & evt) {
	MediaObject & media_object = outermost_context();
	media_object.store_event(evt);
	media_object.connector().display_set_window(media_object.id(), display_out_t(true));
	return discard_event();
}

boost::statechart::result states::DisplayWinConfiguring::react(const event::SwitchToAutoLayout & evt) {
	MediaObject & media_object = outermost_context();
	media_object.store_event(evt);
	const event::MediaVideoData & mvd =
			static_cast<const event::MediaVideoData &>(media_object.retreive_event(MediaObject::VIDEO_INFO_CONFIG));
	display_out_t dpy_out = media_object.layout_manager().suggest_layout(mvd.video_info, media_object.id());
	media_object.connector().display_set_window(media_object.id(), dpy_out);
	return discard_event();
}

// DisplayWinConfigured state
states::DisplayWinConfigured::DisplayWinConfigured(my_context context) : my_base(context) {
	MediaObject & media_object = outermost_context();
	LOG_DEBUG(_log, "%s : <<DisplayWinConfigured>>", media_object.id().c_str());
	post_event(event::TryDisplay());
}

boost::statechart::result states::DisplayWinConfigured::react(const event::SetDisplayWindow & evt) {
	MediaObject & media_object = outermost_context();
	media_object.store_event(evt);
	media_object.connector().display_set_window(media_object.id(),
												display_out_t(evt.src_rect, evt.out_rect, false));
	return discard_event();
}

boost::statechart::result states::DisplayWinConfigured::react(const event::SwitchToFullscreen & evt) {
	MediaObject & media_object = outermost_context();
	media_object.store_event(evt);
	media_object.connector().display_set_window(media_object.id(), display_out_t(true));
	return discard_event();
}

boost::statechart::result states::DisplayWinConfigured::react(const event::SwitchToAutoLayout & evt) {
	MediaObject & media_object = outermost_context();
	media_object.store_event(evt);
	const event::MediaVideoData & mvd =
			static_cast<const event::MediaVideoData &>(media_object.retreive_event(MediaObject::VIDEO_INFO_CONFIG));
	display_out_t dpy_out = media_object.layout_manager().suggest_layout(mvd.video_info, media_object.id());
	media_object.connector().display_set_window(media_object.id(), dpy_out);
	return discard_event();
}

// ContentNotReady state
states::ContentNotReady::ContentNotReady(my_context context) : my_base(context) {
	MediaObject & media_object = outermost_context();
	LOG_DEBUG(_log, "%s : <<ContentNotReady>>", media_object.id().c_str());
}

// ContentReady state
states::ContentReady::ContentReady(my_context context) : my_base(context) {
	MediaObject & media_object = outermost_context();
	LOG_DEBUG(_log, "%s : <<ContentReady>>", media_object.id().c_str());
	post_event(event::TryDisplay());
}

states::ContentReady::~ContentReady() {
	post_event(event::Mute());
}

// Unfocused state
states::Unfocused::Unfocused(my_context context) : my_base(context) {
	MediaObject & media_object = outermost_context();
	LOG_DEBUG(_log, "%s : <<Unfocused>>", media_object.id().c_str());
}

// Focused state
states::Focused::Focused(my_context context) : my_base(context) {
	MediaObject & media_object = outermost_context();
	LOG_DEBUG(_log, "%s : <<Focused>>", media_object.id().c_str());
	post_event(event::TryDisplay());
}

// Visible state
states::Visible::Visible(my_context context) : my_base(context) {
	MediaObject & media_object = outermost_context();
	LOG_DEBUG(_log, "%s : <<Visible>>", media_object.id().c_str());
	post_event(event::SetOpacity(1.0));
}

states::Visible::~Visible() {
	post_event(event::SetOpacity(0.0));
}

boost::statechart::result states::Visible::react(const event::SetVisibility & evt) {
	return evt.visible ? discard_event() : transit<states::Hidden>();
}

// Hidden state
states::Hidden::Hidden(my_context context) : my_base(context) {
	MediaObject & media_object = outermost_context();
	LOG_DEBUG(_log, "%s : <<Hidden>>", media_object.id().c_str());
}

boost::statechart::result states::Hidden::react(const event::SetVisibility & evt) {
	return evt.visible ? transit<states::Visible>() : discard_event();
}

// AudioConnected state
states::AudioConnected::AudioConnected(my_context context)
	: my_base(context), connector(outermost_context().connector())
	, policy(outermost_context().connection_policy()), id(outermost_context().id()) {
	MediaObject & media_object = outermost_context();
	LOG_DEBUG(_log, "%s : <<AudioConnected>>", media_object.id().c_str());
	policy.audio().commit_connect(media_object.shared_from_this());
	post_event(event::TryDisplay());
}

states::AudioConnected::~AudioConnected() {
	connector.sound_disconnect(id);
	policy.audio().disconnected(id);
}

boost::statechart::result states::AudioConnected::react(const event::DisconnectAudio &) {
	return transit<states::AudioDisconnected>();
}

// AudioConnecting state
states::AudioConnecting::AudioConnecting(my_context context) : my_base(context) {
	MediaObject & media_object = outermost_context();
	LOG_DEBUG(_log, "%s : <<AudioConnecting>>", media_object.id().c_str());
}

// AudioDisconnected state
states::AudioDisconnected::AudioDisconnected(my_context context) : my_base(context) {
	MediaObject & media_object = outermost_context();
	LOG_DEBUG(_log, "%s : <<AudioDisconnected>>", media_object.id().c_str());
	// reestablish audio connection for focused pipeline and music widget
	// TODO: remove once PLAT-14073 is fixed
	if (media_object.foreground() && media_object.focus()) {
		post_event(event::TryDisplay());
	}
}

boost::statechart::result states::AudioDisconnected::react(const event::TryDisplay &) {
	MediaObject & media_object = outermost_context();
	auto sink = media_object.connection_policy().audio().try_connect(media_object.shared_from_this());
	if (sink != mdc::sink_t::VOID) {
		media_object.connector().sound_connect(media_object.id());
		return transit< states::AudioConnecting >();
	}
	return forward_event();
}

// OffAir state
states::OffAir::OffAir(my_context context)
	: my_base(context) {
	MediaObject & media_object = outermost_context();
	LOG_DEBUG(_log, "%s : <<OffAir>>", media_object.id().c_str());
}

boost::statechart::result states::OffAir::react(const event::TryDisplay &) {
	return transit<states::OnAir>();
}

// OnAir state
states::OnAir::OnAir(my_context context)
	: my_base(context), connector(outermost_context().connector()), id(outermost_context().id()) {
	LOG_DEBUG(_log, "%s : <<OnAir>>", id.c_str());
	outermost_context().commit_unmute(ITVDisplay::AUDIO_CHANNEL);
	post_event(event::TryDisplay());
}
states::OnAir::~OnAir() {
	connector.avblock_mute(id, ITVDisplay::AUDIO_CHANNEL);
}

boost::statechart::result states::OnAir::react(const event::Mute &) {
	post_event(event::Mute());
	return transit<states::OffAir>();
}


}}
