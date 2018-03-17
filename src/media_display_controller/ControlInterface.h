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

#ifndef CONTROL_INTERFACE_H
#define CONTROL_INTERFACE_H

struct ControlInterface
{
	// LSM Interface
	static const char * application_events;
	// ACB service
	static const char * acb_service_observer;

	// VSM interface
	static const char * register_media;
	static const char * unregister_media;
	static const char * connect_media;
	static const char * disconnect_media;
	static const char * get_connection_state;
	// TV display interface
	static const char * display_window;
	static const char * display_custom_window;
	static const char * display_z_order;
	static const char * display_sub_window_mode;
	// AvBlock interface
	static const char * start_avmute;           // mute audio OR video
	static const char * stop_avmute;            // unmute audio OR video
	static const char * set_video_info;
	static const char * get_screen_status;

	// TV sound interface
	static const char * connect_sound;
	static const char * disconnect_sound;
	static const char * sound_connection_state;

	//nop timer interface
        static const char *remove_nop_timer;
	static const char *get_timer_info;

	//TV power interface
        static const char *turn_on_screen;
};


#endif // CONTROL_INTERFACE_H
