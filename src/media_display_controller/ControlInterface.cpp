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

#include <ControlInterface.h>

// Command Line luna-send examples:
//
// ---------------------------------
// LSM register for window foreground events
// luna-send -n 100 palm://com.webos.surfacemanager/getForegroundAppInfo '{"subscribe":true}'
//
// example return event from LSM
// {
//    "errorCode": 0,
//    "foregroundAppInfo": [
//        {
//            "appId": "eos.bare",
//            "processId": "1112",
//            "windowId": "",
//            "windowType": "_WEBOS_WINDOW_TYPE_CARD"
//        }
//    ],
//    "returnValue": true,
//    "subscribed": true
// }
//
// ---------------------------
// VSM Register
// luna-send -n 1 -f palm://com.webos.service.videosinkmanager/private/register '{"context”:”<MID>", "resourceList":[{"type":"VDEC","portNumber":<NUMBER<0,1>>},{"type":"ADEC","portNumber":<NUMBER<0,1>>},],"audioType":"media"}'
//
// ---------------------------
// VSM Unregister
// luna-send -n 1 -f palm://com.webos.service.videosinkmanager/private/unregister '{"context”:”<MID>"}'
//
// ---------------------------
// VSM Set Window sizes input and output
// luna-send -n 1 -f luna://com.webos.service.tv.display/private/setCustomDisplayWindow
//   '{"context" : "pipeline_1"
//        "sourceInput" :   {"positionX" : 0, "positionY" : 0, "width" : 1920, "height" : 1080},
//        "displayOutput" : {"positionX" : 0, "positionY" : 0, "width" : 1920, "height" : 1080}}'
//
// ---------------------------
// VSM Set Window Full Screen
// luna-send -n 1 -f luna://com.webos.service.tv.display/private/setDisplayWindow
//   '{"context" : "pipeline_1", "fullScreen":true
//        "displayOutput" : {"positionX" : 0, "positionY" : 0, "width" : 0, "height" : 0}}'
//
// ---------------------------
// Set Video Info
//luna-send -n 1 -f luna://com.webos.service.tv.display/private/setMediaVideoData '{"context" : "<MID>", "content" : "movie", "video" : {"frameRate" : 60, "scanType" : "VIDEO_PROGRESSIVE", "width" : 800, "height" : 600, "pixelAspectRatio" : {"width" : 1, "height" : 1}, "data3D" : {"originalPattern" : "side_side_half", "currentPattern" : "side_side_half", "typeLR" : "LR"}, "bitRate" : 2315496 , "adaptive" : true, "path" : "file" }}'
//
// ---------------------------
// Connect media object to MAIN sound output
//
// luna://com.webos.service.tv.sound/private/connect '{"context": "_LRI33vLWja9s2x", "sinkType": "main"}'
//
// ---------------------------
// startMute: start muting audio or video. set to true
// Mute audio only
//luna-send -n 1 -f luna://com.webos.service.tv.avblock/startMute '{"context”:”<MID>", "muteOwner”:”do_not_care", "audio":true, "video”:false}'
//
// ---------------------------
// Unmute audio
//luna-send -n 1 -f luna://com.webos.service.tv.avblock/stopMute '{"context”:”<MID>", "muteOwner”:”do_not_care", "audio":true, "video”:false}'
//
// ---------------------------
// Blank video only
//luna-send -n 1 -f luna://com.webos.service.tv.avblock/startMute '{"context”:”<MID>", "muteOwner”:”do_not_care", "audio”:false, "video”:true}'
//
// ---------------------------
// Unblank video
//luna-send -n 1 -f luna://com.webos.service.tv.avblock/stopMute '{"context”:”<MID>", "muteOwner”:”do_not_care", "audio”:false, "video”:true}'
//
// ---------------------------
// getScreenStatus
// luna-send -n 1 -f luna://com.webos.service.tv.display/getScreenStatus '{}'
//
// Example response for a successful call:
// {
//     "returnValue": true,
//     "screenStatus": [
//         {
//             "window": {
//                 "width": 1280,
//                 "positionX": 0,
//                 "positionY": 0,
//                 "height": 720
//             },
//             "sinkType": "MAIN",
//             "connected": true,
//             "fullScreen": true
//         },
//         {
//             "sinkType": "SUB",
//             "connected": false
//         }
//     ]
// }
//
// Subscription Response :
// {
//     "returnValue": true,
//     "screenStatus": [
//         {
//             "window": {
//                 "width": 1280,
//                 "positionX": 0,
//                 "positionY": 0,
//                 "height": 720
//             },
//             "sinkType": "MAIN",
//             "connected": true,
//             "fullScreen": true
//         },
//         {
//             "sinkType": "SUB",
//             "connected": false
//         }
//     ]
// }
//
// Example response for a fail call:
// {
//     "returnValue": false,
//     "errorCode" : "ERROR_06",
//     "errorText" : "Invalid argument"
// }
//
//
// ---------------------------------
// LSM register for window foreground events
// getForegroundAppInfo '{"subscribe":true}'
//
// example return event from LSM
// {
//    "errorCode": 0,
//    "foregroundAppInfo": [
//        {
//            "appId": "eos.bare",
//            "processId": "1112",
//            "windowId": "",
//            "windowType": "_WEBOS_WINDOW_TYPE_CARD"
//        }
//    ],
//    "returnValue": true,
//    "subscribed": true
// }
//
const char * ControlInterface::application_events = "palm://com.webos.surfacemanager/getForegroundAppInfo";

const char * ControlInterface::acb_service_observer = "palm://com.webos.service.acb/getForegroundAppInfo";

// ---------------------------
// Register/Unregister
//
// VSM Register
//
// {
//   "context":"<MID>",
//   "resourceList":
//   [
//     {"type":"VDEC","portNumber":<NUMBER<0,1>>},
//     {"type":"ADEC","portNumber":<NUMBER<0,1>>},
//   ],
//   "audioType":"media"
// }
//
const char * ControlInterface::register_media = "palm://com.webos.service.videosinkmanager/private/register";

// ---------------------------
// VSM UNRegister
// { "context":"<MID>" }
//
const char * ControlInterface::unregister_media = "palm://com.webos.service.videosinkmanager/private/unregister";

// ---------------------------
// Connect
// { "context":"<MID>", "sinkType":"MAIN/SUB" }
//
const char * ControlInterface::connect_media = "luna://com.webos.service.videosinkmanager/connect";

// Disconnect
// { "context":"<MID>"}
const char * ControlInterface::disconnect_media = "luna://com.webos.service.videosinkmanager/disconnect";

// getConnectionState
// { "context":"<MID>", "subscribe":true/false }
const char * ControlInterface::get_connection_state = "palm://com.webos.service.videosinkmanager/getConnectionState";

// ---------------------------
// Set Window Dimensions
//   {
//     "context":"<MID>",
//     "sourceInput":{"positionX" : 0, "positionY" : 0, "width" : 1920, "height" : 1080},
//     "displayOutput":{"positionX" : 0, "positionY" : 0, "width" : 1920, "height" : 1080}
//   }
//
const char * ControlInterface::display_custom_window =
		"luna://com.webos.service.tv.display/setCustomDisplayWindow";

// ---------------------------
// Set Full Screen.  Same call as window dimensions except fullScreen = true
//   {
//     "context":"<MID>",
//     "fullScreen":true,
//     "displayOutput":{"positionX" : 0, "positionY" : 0, "width" : 0, "height" : 0}
//   }
//
const char * ControlInterface::display_window = "luna://com.webos.service.tv.display/setDisplayWindow";

// ---------------------------
// Set video overlays z order and alpha. Stateless, sets all params at once.
// NB: As of Jan'21 2016 main and sub are reverted in this call
// NB: main_zorder and sub1_zorder have to have different values for this call to succeed
//   {
//     "main_alpha":[0..255],
//     "main_zorder":[0..1],
//     "sub1_alpha":[0..255],
//     "sub1_zorder":[0..1]
//   }
//
const char * ControlInterface::display_z_order = "luna://com.webos.service.tv.display/setWindowZorder";

// ---------------------------
// Set Full Screen.  Same call as window dimensions except fullScreen = true
//   {
//     "broadcastId":"<MID>",
//     "setSubWindowMode":("none","pbp","pip"),
//   }
//
const char * ControlInterface::display_sub_window_mode = "luna://com.webos.service.tv.display/setSubWindowMode";

// ---------------------------
// Mute and Blanking Control
//
// MUTE audio : startMute {"context”:”<MID>", "muteOwner”:”do_not_care", "audio":true, "video”:false}
// UNMUTE audio : stopMute {"context”:”<MID>", "muteOwner”:”do_not_care", "audio":true, "video”:false}
// BLANK video :  startMute {"context”:”<MID>", "muteOwner”:”do_not_care", "audio”:false, "video”:true}
// BLANK video :  stopMute {"context”:”<MID>", "muteOwner”:”do_not_care", "audio”:false, "video”:true}
//
const char * ControlInterface::start_avmute = "luna://com.webos.service.tv.avblock/startMute";
const char * ControlInterface::stop_avmute = "luna://com.webos.service.tv.avblock/stopMute";

// ---------------------------
// Connect media object to MAIN sound output
//
// luna://com.webos.service.tv.sound/private/connect '{"context": "<mid>", "sinkType": "main"}'
const char * ControlInterface::connect_sound = "luna://com.webos.service.tv.sound/private/connect";


// ---------------------------
// Disconnect media object from sound output
//
// luna://com.webos.service.tv.sound/private/disconnect '{"context": "<mid>"}'
const char * ControlInterface::disconnect_sound = "luna://com.webos.service.tv.sound/private/disconnect";

// ---------------------------
// Sound connection state observer
//
// luna://com.webos.service.tv.sound/private/getConnectionStatus '{"subscribe":true}'
const char * ControlInterface::sound_connection_state =
		"palm://com.webos.service.tv.sound/private/getConnectionStatus";

// ---------------------------
// Set Video Info
// {
//   "context" : "<MID>",
//   "content" : "movie",
//   "video" :
//     {
//       "frameRate" : 60, "scanType" : "VIDEO_PROGRESSIVE", "width" : 800, "height" : 600,
//       "pixelAspectRatio" : {"width" : 1, "height" : 1},
//       "data3D" : {"originalPattern" : "side_side_half", "currentPattern" : "side_side_half", "typeLR" : "LR"},
//       "bitRate" : 2315496 ,
//       "adaptive" : true,
//       "path" : "file"
//     }
// }
//
const char * ControlInterface::set_video_info = "luna://com.webos.service.tv.display/private/setMediaVideoData";

// ---------------------------
// getScreenStatus
// luna://com.webos.service.tv.display/getScreenStatus '{}'
const char * ControlInterface::get_screen_status = "luna://com.webos.service.tv.display/getScreenStatus";

//----------------------------
//turnOnscreen
//palm://com.webos.service.tvpower/power/turnOnScreen '{}'
const char * ControlInterface::turn_on_screen = "palm://com.webos.service.tvpower/power/turnOnScreen";

//-----------------------------
//removeNopTimer
//luna://com.webos.service.nop/removeNopTimer
const char * ControlInterface::remove_nop_timer = "luna://com.webos.service.nop/removeNopTimer";

//-----------------------------
//getTimerInfo
//luna://com.webos.service.nop/getTimerInfo
const char * ControlInterface::get_timer_info = "luna://com.webos.service.nop/getTimerInfo";
