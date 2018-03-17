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

#ifndef __UMS_MDC_STATES_H__
#define __UMS_MDC_STATES_H__

namespace uMediaServer { namespace mdc { namespace states {

// initial state
struct Init;
	// app foreground orthogonal states
	struct Foreground;
	struct Background;
	// content readyness
	struct ContentReady;
	struct ContentNotReady;
	// focus related states
	struct Focused;
	struct Unfocused;
	// media visibility states
	struct Visible;
	struct Hidden;
	// registration states
	struct Idle;
	struct Registered;
		// substates - video
		struct VideoDisconnected;
		struct VideoMuting;
		struct VideoConnecting;
		struct VideoConnected;
			// substates - ortho regions
			// video meta info configuration
			struct MediaVideoDataNotConfigured;
			struct MediaVideoDataConfigured;
				// display output configuration
				struct DisplayWinNotConfigured;
				struct DisplayWinConfiguring;
				struct DisplayWinConfigured;
					// on screen states
					struct OffScreen;
					struct OnScreen;
		// substates - audio
		struct AudioDisconnected;
		struct AudioConnecting;
		struct AudioConnected;
			struct OffAir;
			struct OnAir;
}}} // uMediaServer::mdc::state

#endif // __UMS_MDC_STATES_H__
