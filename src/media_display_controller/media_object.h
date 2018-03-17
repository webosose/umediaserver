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

#ifndef __UMS_MDC_MEDIA_OBJECT_H__
#define __UMS_MDC_MEDIA_OBJECT_H__

#include <vector>
#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/state.hpp>
#include <boost/statechart/transition.hpp>
#include <boost/statechart/deferral.hpp>
#include <boost/statechart/custom_reaction.hpp>
#include <boost/statechart/shallow_history.hpp>
#include <boost/statechart/in_state_reaction.hpp>
#include "events.h"
#include "states.h"
#include "interfaces.h"

namespace uMediaServer { namespace mdc {

class MediaObject : public IMediaObject, public std::enable_shared_from_this< const MediaObject >,
		public boost::statechart::state_machine< MediaObject, states::Init > {
public:
	enum EventType {
		VIDEO_INFO_CONFIG = 0,
		DISPLAY_WIN_CONFIG,
		RESOURCES_CONFIG,
		OPACITY_CONFIG
	};

	MediaObject(const std::string & id, const std::string & app_id,
				ITVDisplay & c, IConnectionPolicy & p, ILayoutManager & lm);

	// IMediaObject interface
	virtual const std::string & id() const;
	virtual const std::string & appId() const;
	virtual bool foreground() const;
	virtual bool focus() const;
	virtual bool hasAudio() const;
	virtual bool hasVideo() const;
	virtual bool autoLayout() const;

	ITVDisplay & connector();
	IConnectionPolicy & connection_policy();
	ILayoutManager & layout_manager();

	template <typename T>
	void store_event(const T &) {}
	void emit_stored_event(EventType event_type);
	const event_base_type & retreive_event(EventType event_type);
	void unconsumed_event(const event_base_type & event);

	void process_event(const event_base_type & evt);
	std::vector<std::string> getStates() const;
	void DisplayStateConfiguration() const;
	void enableDebug(bool state);
	void commit_unmute(size_t channel);

private:
	std::string _id;
	std::string _app_id;
	ITVDisplay & _connector;
	IConnectionPolicy & _policy;
	ILayoutManager & _layout_manager;
	event_base_ptr_type _config[4];
	static bool debug_state;
};

/**
 * @brief The states::Init struct
 * Initial state of media object state machine.
 * The state has 5 orthogonal regions.
 * No event reactions handled by this state directly.
 * By default state machine enters Background, ContentNotReady, Unfocused, Visible and Idle sub states.
 *        __________________________________________________
 *       |__________________ Init __________________________|
 *       |                                                  |
 *       |----------------- Region #0 ----------------------|
 *       |        ____________       ____________           |
 *       |       |            |<----|            |          |
 *       | O---->| Background |     | Foreground |          |
 * O---->|       |____________|---->|____________|          |
 *       |                                                  |
 *       |----------------- Region #1 ----------------------|
 *       |        _________________       _______________   |
 *       |       |                 |<----|               |  |
 *       | O---->| ContentNotReady |     | ContentReady  |  |
 *       |       |_________________|---->|_______________|  |
 *       |                                                  |
 *       |----------------- Region #2 ----------------------|
 *       |        ____________       ____________           |
 *       |       |            |<----|            |          |
 *       | O---->| Unfocused  |     |   Focused  |          |
 *       |       |____________|---->|____________|          |
 *       |                                                  |
 *       |----------------- Region #3 ----------------------|
 *       |        ____________       ____________           |
 *       |       |            |<----|            |          |
 *       | O---->|   Visible  |     |   Hidden   |          |
 *       |       |____________|---->|____________|          |
 *       |                                                  |
 *       |----------------- Region #4 ----------------------|
 *       |        ____________       ____________           |
 *       |       |            |<----|            |          |
 *       | O---->|    Idle    |     | Registered |          |
 *       |       |____________|---->|____________|          |
 *       |__________________________________________________|
 *
 */
struct states::Init : boost::statechart::state< states::Init, MediaObject,
		boost::mpl::list< states::Background,
						  states::ContentNotReady,
						  states::Unfocused,
						  states::Visible,
						  states::Idle > > {
	Init(my_context context);
};

/**
 * @brief The states::Background struct
 * This state corresponds to media element of a background app.
 * This state is a default for current orthogonal region.
 * The state reacts on ToForeground event by transition to Foreground state.
 * The state emits Mute, DisconnectAudio & DisconnectVideo events on entry.
 *              ____________                ____________
 * O---------->|            |              |            |
 *  Mute       | Background | ToForeground | Foreground |
 *  Disconnect |            |              |            |
 * <-----------|____________|------------->|____________|
 *
 */
struct states::Background : boost::statechart::state < states::Background, states::Init::orthogonal<0> > {
	typedef boost::statechart::transition< event::ToForeground, states::Foreground > reactions;
	Background(my_context context);
};

/**
 * @brief The states::Foreground struct
 * This state corresponds to media element of a foreground app.
 * This state emits TryDisplay event as an entry action.
 * The state reacts on ToBackground event by transition to Background state.
 *               ____________                ____________
 *   TryDisplay |            |              |            |
 * <------------| Foreground | ToBackground | Foreground |
 *              |____________|------------->|____________|
 *
 */
struct states::Foreground : boost::statechart::state < states::Foreground, states::Init::orthogonal<0> > {
	typedef boost::statechart::transition< event::ToBackground, states::Background > reactions;
	Foreground(my_context context);
	~Foreground();
};

/**
 * @brief The states::ContentNotReady struct
 * This state represents media element awaiting ContentReady event from a pipeline.
 * This state is a default for current region.
 * The state reacts on MediaContentReady event by transition to ContentReady state.
 * The state emits Mute event on entry.
 *        _________________                        ______________
 *       |                 |   MediaContentReady  |              |
 * o---->| ContentNotReady |--------------------->| ContentReady |
 *       |                 | MediaContentNotReady |              |
 *       |_________________|<---------------------|______________|
 *
 */
struct states::ContentNotReady : boost::statechart::state< states::ContentNotReady, states::Init::orthogonal<1> > {
	typedef boost::statechart::transition< event::MediaContentReady, states::ContentReady > reactions;
	ContentNotReady(my_context context);
};

/**
 * @brief The states::ContentReady struct
 * This state represents media element with media content ready for display.
 * The state reacts on MediaContentNotReady event by transition to ContentNotReady state.
 * The state emits TryDisplay event on entry.
 *  ______________                         _________________
 * |              | MediaContentNotReady  |                 |
 * | ContentReady |---------------------->| ContentNotReady |
 * |              |   MediaContentReady   |                 |
 * |______________|<----------------------|_________________|
 *
 */
struct states::ContentReady : boost::statechart::state< states::ContentReady, states::Init::orthogonal<1> > {
	typedef boost::statechart::transition< event::MediaContentNotReady, states::ContentNotReady > reactions;
	ContentReady(my_context context);
	~ContentReady();
};

/**
 * @brief The states::Unfocused struct
 * This state represents media element out of the focus.
 * This state is a default for current region.
 * The state reacts on SetFocus event by transition to Focused state.
 *        _________________                        ______________
 *       |                 |       SetFocus       |              |
 * o---->|    Unfocused    |--------------------->|    Focused   |
 *       |                 |       LostFocus      |              |
 *       |_________________|<---------------------|______________|
 *
 */
struct states::Unfocused : boost::statechart::state< states::Unfocused, states::Init::orthogonal<2> > {
	typedef boost::statechart::transition< event::SetFocus, states::Focused > reactions;
	Unfocused(my_context context);
};

/**
 * @brief The states::Focused struct
 * This states represents media element with active focus.
 * The state reacts on LostFocus event by transition to Unfocused state.
 *  ______________                         _________________
 * |              |       LostFocus       |                 |
 * |    Focused   |---------------------->|    Unfocused    |
 * |              |       SetFocus        |                 |
 * |______________|<----------------------|_________________|
 *
 */
struct states::Focused : boost::statechart::state< states::Focused, states::Init::orthogonal<2> > {
	typedef boost::statechart::transition< event::LostFocus, states::Unfocused > reactions;
	Focused(my_context context);
};

/**
 * @brief The states::Visible struct
 * This state represents visible media element.
 * This state is a default for current region.
 * The state reacts on SetVisibility(false) event by transition to Hidden state.
 *        _________________                        ______________
 *       |                 |    SetVisibility     |              |
 * o---->|     Visible     |--------------------->|    Hidden    |
 *       |                 |    SetVisibility     |              |
 *       |_________________|<---------------------|______________|
 *
 */
struct states::Visible : boost::statechart::state< states::Visible, states::Init::orthogonal<3> > {
	typedef boost::statechart::custom_reaction< event::SetVisibility > reactions;
	Visible(my_context context);
	~Visible();
	boost::statechart::result react(const event::SetVisibility & evt);
};

/**
 * @brief The states::Hidden struct
 * This states represents hidden media element.
 * The state reacts on SetVisibility(true) event by transition to Visible state.
 *  ______________                         _________________
 * |              |     SetVisibility     |                 |
 * |    Hidden    |---------------------->|     Visible     |
 * |              |     SetVisibility     |                 |
 * |______________|<----------------------|_________________|
 *
 */
struct states::Hidden : boost::statechart::state< states::Hidden, states::Init::orthogonal<3> > {
	typedef boost::statechart::custom_reaction< event::SetVisibility > reactions;
	Hidden(my_context context);
	boost::statechart::result react(const event::SetVisibility & evt);
};

/**
 * @brief The states::Idle struct
 * This states corresponds to media element created but not registered on VSM side.
 * This state is a default for current orthogonal region.
 * The state reacts on Registered event by transition to Registered state.
 * The state reacts on Acquire event by triggering VSM registration.
 * The state defers and retriggers on exit Resources and TryDisplay events.
 *        ____________                ____________
 *       |            |--+           |            |
 *       |            |  | Acquire   |            |
 * O---->|    Idle    |<-+           | Registered |
 *       |            |              |            |
 *       |            | Registered   |            |
 *       |____________|------------->|____________|
 *
 */
struct states::Idle : boost::statechart::state< states::Idle, states::Init::orthogonal<4> > {
	typedef boost::mpl::list < boost::statechart::transition< event::Registered, states::Registered >,
							   boost::statechart::custom_reaction< event::Acquire >,
							   boost::statechart::deferral< event::TryDisplay >
							 > reactions;
	Idle(my_context context);
	boost::statechart::result react(const event::Acquire & evt);
};

/**
 * @brief The states::Registered struct
 * The state reflects media object with valid VSM registration.
 * The state has 2 orthogonal regions representing Video and Audio connections.
 * First region has 4 substates: VideoDisconnected, VideoMuting, VideoConnecting and VideoConnected.
 * Second region has 3 substates: AudioDisconnected, AudioConnecting and AudioConnected.
 * The state reacts on Release event by transition to Idle state.
 * The state unregisters from VSM on exit.
 *  _______________________________________________________________
 * |__________________________ Registered _________________________|
 * |                                                               |
 * |-------------------------- Region #0 --------------------------|
 * |        ___________________                 _________________  | Acquire  ______
 * |       |                   |               |                 | | Release |      |
 * | O---->| VideoDisconnected |<--------------| VideoConnected  | |-------->| Idle |
 * |       |___________________|  Video-       |_________________| |         |______|
 * |              |               Disconnected    ^                |
 * |   TryDisplay |                               | VideoConnected |
 * |       _______V_________                    __|______________  |
 * |      |                 |       Muted      |                 | |
 * |      |   VideoMuting   |----------------->| VideoConnecting | |
 * |      |_________________|                  |_________________| |
 * |                                                               |
 * |------------------------- Region #1 ---------------------------|
 * |        ___________________                                    |
 * |       |                   |    AudioDisconnected              |
 * | O---->| AudioDisconnected |<---------------------+            |
 * |       |___________________|                      |            |
 * |              |                                   |            |
 * |   TryDisplay |                                   |            |
 * |       _______V_________                   _______|________    |
 * |      |                 |  AudioConnected |                |   |
 * |      | AudioConnecting |---------------->| AudioConnected |   |
 * |      |_________________|                 |________________|   |
 * |                                                               |
 * |_______________________________________________________________|
 *
 */
struct states::Registered : boost::statechart::state< states::Registered, states::Init::orthogonal<4>,
		boost::mpl::list < states::VideoDisconnected, states::AudioDisconnected > > {
	typedef boost::mpl::list < boost::statechart::custom_reaction< event::Release >,
							   boost::statechart::custom_reaction< event::Acquire >
							 > reactions;
	Registered(my_context context);
	~Registered();
	boost::statechart::result react(const event::Release & evt);
	boost::statechart::result react(const event::Acquire & evt);
	ITVDisplay & connector;
	IConnectionPolicy & policy;
	std::string id;
};

/**
 * @brief The states::VideoDisconnected struct
 * This states corresponds to media element not connected to VSM.
 * This state is a default for current region.
 * The state reacts on TryDisplay event by transition into Connecting state.
 *        ___________________                 _____________
 *       |                   |  TryDisplay   |             |
 * O---->| VideoDisconnected |-------------->| VideoMuting |
 *       |___________________|               |_____________|
 *
 */
struct states::VideoDisconnected : boost::statechart::state< states::VideoDisconnected,
															 states::Registered::orthogonal<0> > {
	typedef boost::mpl::list < boost::statechart::custom_reaction< event::TryDisplay >,
							   boost::statechart::deferral< event::VideoConnected >
							 > reactions;
	VideoDisconnected(my_context context);
	boost::statechart::result react(const event::TryDisplay &);
};

/**
 * @brief The states::VideoMuting struct
 * This states corresponds to media element avaiting avblock response.
 * On entry this state initiates avblock video muting.
 * The state reacts on Muted event by transition to Connecting state.
 *  _____________           _________________
 * |             |  Muted  |                 |
 * | VideoMuting |-------->| VideoConnecting |
 * |_____________|         |_________________|
 *
 */
struct states::VideoMuting : boost::statechart::state< states::VideoMuting,
													   states::Registered::orthogonal<0> > {
	typedef boost::statechart::custom_reaction< event::Muted > reactions;
	VideoMuting(my_context context);
	boost::statechart::result react(const event::Muted &);
};

/**
 * @brief The states::VideoConnecting struct
 * This states corresponds to media element connecting to VSM.
 * On entry this state initiates connection to VSM.
 * The state reacts on Connected event by transition to Connected state.
 *  _________________                    ________________
 * |                 |  VideoConnected  |                |
 * | VideoConnecting |----------------->| VideoConnected |
 * |_________________|                  |________________|
 *
 */
struct states::VideoConnecting : boost::statechart::state< states::VideoConnecting,
														   states::Registered::orthogonal<0> > {
	typedef boost::statechart::transition< event::VideoConnected, states::VideoConnected > reactions;
	VideoConnecting(my_context context);
};

/**
 * @brief The states::VideoConnected struct
 * The state reflects media object connected to VSM.
 * The state has 1 orthogonal region.
 * The state reacts on VideoDisconnected event by trnsition to VideoDisconnected state.
 * By default state machine enters MediaVideoDataNotConfigured state or restore previous state.
 *     ________________________________________________________________________
 *    |___________________________ VideoConnected _____________________________|
 *    |        _____________________________       __________________________  |
 *    |       |                             |     |                          | |                    ___________________
 * -->| (H)-->| MediaVideoDataNotConfigured |---->| MediaVideoDataConfigured | | VideoDisconnected |                   |
 *    |       |_____________________________|     |__________________________| |------------------>| VideoDisconnected |
 *    |                                                                        |                   |___________________|
 *    |________________________________________________________________________|
 *
 */
struct states::VideoConnected : boost::statechart::state < states::VideoConnected, states::Registered::orthogonal<0>,
		boost::mpl::list< boost::statechart::shallow_history < states::MediaVideoDataNotConfigured > >,
		boost::statechart::has_shallow_history > {
	typedef boost::mpl::list < boost::statechart::custom_reaction< event::DisconnectVideo >,
							   boost::statechart::custom_reaction< event::SetOpacity >,
							   boost::statechart::transition< event::VideoDisconnected, states::VideoDisconnected >
							 > reactions;
	VideoConnected(my_context context);
	~VideoConnected();
	boost::statechart::result react(const event::DisconnectVideo &);
	boost::statechart::result react(const event::SetOpacity &);
	ITVDisplay & connector;
	IConnectionPolicy & policy;
	std::string id;
};

/**
 * @brief The states::MediaVideoDataNotConfigured struct
 * This states represents media element awaiting MediaVideoData to be configured.
 * This state is a default for current region.
 * The state reacts on MediaVideoData event by storing this event and transition to MediaVideoDataConfigured state.
 *        _____________________________                  __________________________
 *       |                             | MediaVideoData |                          |
 * (H)-->| MediaVideoDataNotConfigured |--------------->| MediaVideoDataConfigured |
 *       |_____________________________|                |__________________________|
 *
 */
struct states::MediaVideoDataNotConfigured : boost::statechart::state< states::MediaVideoDataNotConfigured,
																	   states::VideoConnected > {
	typedef boost::statechart::custom_reaction< event::MediaVideoData > reactions;
	MediaVideoDataNotConfigured(my_context context);
	boost::statechart::result react(const event::MediaVideoData & evt);
};

/**
 * @brief The states::MediaVideoDataConfigured struct
 * This states represents media element with valid MediaVideoData configuration.
 * This state could be reentered immediately due to history.
 * The state has 1 orthogonal region.
 * The state retriggers stored MediaVideoData event on entry.
 * The state emits SetDisplayWindow for auto layout on entry.
 * By default state machine enters DisplayWinNotConfigured state.
 *     _________________________________________________________________
 *    |___________________ MediaVideoDataConfigured ____________________|
 *    |        _________________________       _______________________  |
 *    |       |                         |     |                       | |---+
 * -->|   O-->| DisplayWinNotConfigured |---->| DisplayWinConfiguring | |   | MediaVideoData
 *    |       |_________________________|     |_______________________| |<--+
 *    |                                                           |     |
 *    |        _________________________                          |     |
 *    |       |                         |      DisplayConfigured  |     |
 *    |       |   DisplayWinConfigured  |<------------------------+     |
 *    |       |_________________________|                               |
 *    |_________________________________________________________________|
 *
 */

struct states::MediaVideoDataConfigured : boost::statechart::state< states::MediaVideoDataConfigured,
																	states::VideoConnected,
																	states::DisplayWinNotConfigured > {
	typedef boost::statechart::custom_reaction< event::MediaVideoData > reactions;
	MediaVideoDataConfigured(my_context context);
	boost::statechart::result react(const event::MediaVideoData & evt);
};

/**
 * @brief The states::DisplayWinNotConfigured struct
 * This states represents media element awaiting display output to be configured.
 * This state is a default for current region.
 * The state reacts on SetDisplayWindow and SwitchToFullscreen events by storing event and transition
 * to DisplayWinConfiguring state.
 *        _________________________  SwitchToAutoLayout  _______________________
 *       |                         | SwitchToFullscreen |                       |
 *  O--->| DisplayWinNotConfigured | SetDisplayWindow   | DisplayWinConfiguring |
 *       |_________________________|------------------->|_______________________|
 *
 */
struct states::DisplayWinNotConfigured : boost::statechart::state< states::DisplayWinNotConfigured,
																   states::MediaVideoDataConfigured > {
	typedef boost::mpl::list < boost::statechart::custom_reaction< event::SetDisplayWindow >,
							   boost::statechart::custom_reaction< event::SwitchToFullscreen >,
							   boost::statechart::custom_reaction< event::SwitchToAutoLayout >
							 > reactions;
	DisplayWinNotConfigured(my_context context);
	boost::statechart::result react(const event::SetDisplayWindow & evt);
	boost::statechart::result react(const event::SwitchToFullscreen & evt);
	boost::statechart::result react(const event::SwitchToAutoLayout & evt);
};

/**
 * @brief The states::DisplayWinConfiguring struct
 * This states represents media element awaiting display output to be configured.
 * The state reacts on DisplayConfigured event by transition into DisplayWinConfigured state.
 * to DisplayWinConfiguring state.
 *        _________________________  SwitchToAutoLayout  _______________________
 *       |                         | SwitchToFullscreen |                       |
 *  O--->| DisplayWinNotConfigured | SetDisplayWindow   | DisplayWinConfiguring |
 *       |_________________________|------------------->|_______________________|
 *
 */
struct states::DisplayWinConfiguring : boost::statechart::state< states::DisplayWinConfiguring,
																 states::MediaVideoDataConfigured > {
	typedef boost::mpl::list < boost::statechart::custom_reaction< event::SetDisplayWindow >,
							   boost::statechart::custom_reaction< event::SwitchToFullscreen >,
							   boost::statechart::custom_reaction< event::SwitchToAutoLayout >,
							   boost::statechart::transition< event::DisplayConfigured, states::DisplayWinConfigured >
							 > reactions;
	DisplayWinConfiguring(my_context context);
	boost::statechart::result react(const event::SetDisplayWindow & evt);
	boost::statechart::result react(const event::SwitchToFullscreen & evt);
	boost::statechart::result react(const event::SwitchToAutoLayout & evt);
};

/**
 * @brief The states::DisplayWinConfigured struct
 * This states represents media element with valid display output configuration.
  * This state could be reentered immediately due to history.
 * The state has 1 orthogonal region.
 * The state retriggers stored SetDisplayWindow or SwitchToFullscreen events on entry.
 * By default state machine enters Offscreen state.
 *     _____________________________________________
 *    |___________ DisplayWinConfigured ____________|
 *    |      ___________   TryDisplay   __________  |
 *    |     |           |------------->|          | |---+ SwitchToFullscreen
 * -->| O-->| Offscreen |     Mute     | OnScreen | |   | SetDisplayWindow
 *    |     |___________|<-------------|__________| |<--+ SwitchToAutoLayout
 *    |                                             |
 *    |_____________________________________________|
 *
 */

struct states::DisplayWinConfigured : boost::statechart::state< states::DisplayWinConfigured,
																states::MediaVideoDataConfigured,
																states::OffScreen > {
	typedef boost::mpl::list < boost::statechart::custom_reaction< event::SetDisplayWindow >,
							   boost::statechart::custom_reaction< event::SwitchToFullscreen >,
							   boost::statechart::custom_reaction< event::SwitchToAutoLayout >
							 > reactions;
	DisplayWinConfigured(my_context context);
	boost::statechart::result react(const event::SetDisplayWindow & evt);
	boost::statechart::result react(const event::SwitchToFullscreen & evt);
	boost::statechart::result react(const event::SwitchToAutoLayout & evt);
};

/**
 * @brief The states::OffScreen struct
 * This states represents AV muted media element.
 * This state is a default for current region.
 * The state reacts on TryDisplay event by conditional transition to OnScreen state.
 *        ______________                  ___________
 *       |              |       Mute     |           |
 *       |              |<---------------|           |
 * O---->|   OffScreen  |     TryDisplay |  OnScreen |
 *       |              |___/\---------->|           |
 *       |______________|   \/           |___________|
 *
 */
struct states::OffScreen : boost::statechart::state< states::OffScreen, states::DisplayWinConfigured > {
	typedef boost::statechart::custom_reaction < event::TryDisplay > reactions;
	boost::statechart::result react(const event::TryDisplay &);
	OffScreen(my_context context);
};

/**
 * @brief The states::OnScreen struct
 * This states represents media element visible on the screen.
 * The state does AV unmute on entry and AV mute on exit.
 * The state reacts on Mute by transiotion to OffScreen state.
 *  __________                ___________
 * |          |     Mute     |           |
 * | OnScreen |------------->| OffScreen |
 * |          | TryDisplay   |           |
 * |__________|<-------------|___________|
 *
 */
struct states::OnScreen : boost::statechart::state< states::OnScreen, states::DisplayWinConfigured > {
	typedef boost::statechart::custom_reaction< event::Mute > reactions;
	OnScreen(my_context context);
	~OnScreen();
	boost::statechart::result react(const event::Mute &);
	ITVDisplay & connector;
	std::string id;
};

/**
 * @brief The states::AudioConnected struct
 * The state reflects media object connected to sound.
 * The state reacts on AudioDisconnected event by trnsition to AudioDisconnected state.
 * By default state machine enters OffAir state.
 *     ____________________________________
 *    |_________ AudioConnected ___________|
 *    |                                    |
 *    |        ________       _______      |
 *    |       |        |<----|       |     |                    ___________________
 *    | O---->| OffAir |     | OnAir |     | AudioDisconnected |                   |
 * -->|       |________|---->|_______|     |------------------>| AudioDisconnected |
 *    |                                    |                   |___________________|
 *    |____________________________________|
 *
 */
struct states::AudioConnected : boost::statechart::state < states::AudioConnected, states::Registered::orthogonal<1>,
														   states::OffAir > {
	typedef boost::mpl::list < boost::statechart::custom_reaction< event::DisconnectAudio >,
							   boost::statechart::transition< event::AudioDisconnected, states::AudioDisconnected >
							 > reactions;
	AudioConnected(my_context context);
	~AudioConnected();
	boost::statechart::result react(const event::DisconnectAudio &);
	ITVDisplay & connector;
	IConnectionPolicy & policy;
	std::string id;
};

/**
 * @brief The states::AudioDisconnected struct
 * This states corresponds to media element not connected to sound.
 * This state is a default for current region.
 * The state reacts on TryDisplay event by transition into Connecting state.
 *        ___________________                 _________________
 *       |                   |  TryDisplay   |                 |
 * O---->| AudioDisconnected |-------------->| AudioConnecting |
 *       |___________________|               |_________________|
 *
 */
struct states::AudioDisconnected : boost::statechart::state< states::AudioDisconnected,
															 states::Registered::orthogonal<1> > {
	typedef boost::mpl::list < boost::statechart::custom_reaction< event::TryDisplay >,
							   boost::statechart::transition< event::AudioConnected, states::OffAir >
							 > reactions;
	AudioDisconnected(my_context context);
	boost::statechart::result react(const event::TryDisplay &);
};

/**
 * @brief The states::AudioConnecting struct
 * This states corresponds to media element connecting to SOUND.
 * On entry this state initiates connection to SOUND.
 * The state reacts on Connected event by transition to Connected state.
 *  _________________                    ________________
 * |                 |  AudioConnected  |                |
 * | AudioConnecting |----------------->| AudioConnected |
 * |_________________|                  |________________|
 *
 */
struct states::AudioConnecting : boost::statechart::state< states::AudioConnecting,
														   states::Registered::orthogonal<1> > {
	typedef boost::mpl::list < boost::statechart::transition< event::AudioConnected, states::OffAir >,
							   boost::statechart::transition< event::AudioDisconnected, states::AudioDisconnected >
							 > reactions;
	AudioConnecting(my_context context);
};

/**
 * @brief The states::OffAir struct
 * This states represents audio muted media element.
 * The state reacts on TryDisplay event by conditional transition to OnAir state.
 *        ______________                  ___________
 *       |              |       Mute     |           |
 *       |              |<---------------|           |
 * O---->|    OffAir    |     TryDisplay |   OnAir   |
 *       |              |___/\---------->|           |
 *       |______________|   \/           |___________|
 *
 */
struct states::OffAir : boost::statechart::state< states::OffAir, states::AudioConnected > {
	typedef boost::statechart::custom_reaction < event::TryDisplay > reactions;
	boost::statechart::result react(const event::TryDisplay &);
	OffAir(my_context context);
};

/**
 * @brief The states::OnAir struct
 * This states represents media element visible on the screen.
 * The state does audio unmute on entry and audio mute on exit.
 * The state reacts on Mute by transiotion to OffAir state.
 *  __________                ___________
 * |          |     Mute     |           |
 * |   OnAir  |------------->|   OffAir  |
 * |          | TryDisplay   |           |
 * |__________|<-------------|___________|
 *
 */
struct states::OnAir : boost::statechart::state< states::OnAir, states::AudioConnected > {
	typedef boost::statechart::custom_reaction< event::Mute > reactions;
	OnAir(my_context context);
	~OnAir();
	boost::statechart::result react(const event::Mute &);
	ITVDisplay & connector;
	std::string id;
};


}} // uMediaServer::mdc

#endif // __MDC_HFSM_MEDIA_OBJECT_H__
