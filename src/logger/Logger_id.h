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

// Error classification tags
// config_file_reader
#define MSGERR_CONFIG            "CONFIG_ERR"            // configuration error
#define MSGERR_CONFIG_OPEN       "CONFIG_OPEN_ERR"       // configuration file open error
// utils
#define MSGERR_THREAD_CREATE     "THREAD_CREATE_ERR"     // thread creation failed
#define MSGERR_THREAD_JOIN       "THREAD_JOIN_ERR"       // thread join failed
#define MSGERR_COND_TIMEDWAIT    "COND_TIMEDWAIT"        // condition wait timed out
#define MSGERR_DEADLOCK          "DEADLOCK_ERR"          // deadlock detected
#define MSGERR_OUTOFMEMORY       "OUTOFMEMORY_ERR"       // OOM condition
// ums_connector
#define MSGERR_RESOLVE_HCMD      "RESOLVE_HCMD_ERR"      // resolving command handler failed
#define MSGERR_RESOLVE_HREP      "RESOLVE_HREP_ERR"      // resolving replay handler failed
#define MSGERR_RESOLVE_HSUB      "RESOLVE_HSUB_ERR"      // resolving subscription handler failed
#define MSGERR_JSON_SERIALIZE    "JSON_SERIALIZE_ERR"    // json serialization failed
#define MSGERR_JSON_PARSE        "JSON_PARSE_ERR"        // json parsing failed
#define MSGERR_JSON_SCHEMA       "JSON_SCHEMA_ERR"       // json schema error
#define MSGERR_JSON_UNMARSHALL   "JSON_UNMARSHALL_ERR"   // json object unmarshalling error
#define MSGERR_SERVICE_REGISTER  "SERVICE_REGISTER_ERR"  // luna service registration failed
#define MSGERR_UNREGISTER        "UNREGISTER_ERR"        // luna unregister failed
#define MSGERR_CATEGORY_REGISTER "CATEGORY_REGISTER_ERR" // luna category registration failed
#define MSGERR_CATEGORY_APPEND   "CATEGORY_APPEND_ERR"   // luna category append failed
#define MSGERR_CATEGORY_DATA     "CATEGORY_DATA_ERR"     // luna set category data failed
#define MSGERR_GMAIN_ATTACH      "GMAIN_ATTACH_ERR"      // luna main loop attaching failed
#define MSGERR_GMAIN_NOT_RUNNING "GMAIN_NOT_RUNNING"     // gmain loop is not running
#define MSGERR_SERVICE_NOTREG    "SERVICE_NOTREG_ERR"    // luna service wasn't registered
#define MSGERR_COMM_REPLAY       "COMM_REPLAY_ERR"       // luna replay failed
#define MSGERR_COMM_SEND         "COMM_SEND_ERR"         // luna send failed
#define MSGERR_COMM_SUBSCRIBE    "COMM_SUBSCRIBE_ERR"    // luna subscription failed
#define MSGERR_COMM_NOTIFY       "COMM_NOTIFY_ERR"       // luna notification failed
#define MSGERR_BUS_TYPE          "BUS_TYPE"              // an invalid bus type was given
#define MSGERR_SUBS_FIND         "SUBS_FIND_ERR"         // failed to find subscription

// umedia-api
#define MSGERR_GETTIMEOFDAY      "GETTIMEOFDAY_ERR"      // gettimeofday call failed
#define MSGERR_LOAD_TIMEOUT      "LOAD_TIMEOUT_ERR"      // media load wait timeout expired
// process_controller
#define MSGERR_PROC_KILL         "PROC_KILL_ERR"         // process controller failed to kill child
#define MSGERR_RESTART_RETRY     "RESTART_RETRY_ERR"     // too many restart retries for child process
#define MSGERR_RESTART_FAIL      "RESTART_FAIL_ERR"      // failed to restart child process
#define MSGERR_PROC_TERM         "PROC_TERM_ERR"         // failed to send TERM to process
#define MSGERR_QUEUE_OVERFLOW    "QUEUE_OVERFLOW_ERR"    // failed to send TERM to process
#define MSGERR_PROC_NOT_FOUND    "PROC_NOT_FOUND_ERR"    // got SIGCHLD with unknown pid
#define MSGERR_SEND_SIGTERM_WARN "SEND_SIGTERM_WARN"     // warning message, send SIGTERM to process
#define MSGERR_SEND_SIGKILL_WARN "SEND_SIGKILL_WARN"     // warning message, send SIGKILL to process
#define MSGNFO_PROC_SIGCHLD      "PROC_SIGCHLD"          // got SIGCHLD
#define MSGERR_POOL_INIT         "POOL_INIT_ERR"         // pool initialization failure
#define MSGERR_PROC_EXIT         "PROC_EXIT_ERR"         // unexpected process exit
#define MSGNFO_PROC_STARTED      "PROCESS_STARTED"       // new pipeline process started

// pipeline
#define MSGERR_PIPELINE_SEND     "PIPELINE_SEND_ERR"     // sending message to pipeline failed
#define MSGERR_PL_CREATE_OOM     "PL_CREATE_OOM_ERR"     // pipeline creation faied due to OOM
#define MSGERR_PL_START_PROC     "PL_START_PROC_ERR"     // pipeline process start failure
#define MSGERR_PIPELINE_ADD      "PIPELINE_ADD_ERR"      // pipeline add failed
#define MSGERR_PIPELINE_FIND     "PIPELINE_FIND_ERR"     // pipeline not found
// server
#define MSGERR_SCHEMA_FILE       "SCHEMA_FILE_ERR"       // schema file doesn't exist or mallformatted
#define MSGERR_PIPELINE_TYPE     "PIPELINE_TYPE_ERR"     // pipeline type not supported
#define MSGERR_NO_MEDIA_ID       "NO_MEDIA_ID_ERR"       // media id not specified in command
#define MSGERR_NO_APP_ID         "NO_APP_ID_ERR"         // app id not specified in command
#define MSGERR_INVALID_MEDIA_ID  "NO_MEDIA_ID_ERR"       // invalid media media id
#define MSGERR_NO_MEDIA_POS      "NO_MEDIA_POS_ERR"      // media position not specified
#define MSGERR_NO_PRELOAD_ATTR   "NO_PRELOAD_ATTR_ERR"   // preload attribute not specified
#define MSGERR_NO_MEDIA_RATE     "NO_MEDIA_RATE_ERR"     // media playback rate not specified
#define MSGERR_NO_AUDIO_OUTPUT   "NO_AUDIO_OUTPUT_ERR"   // media audio output not specified
#define MSGERR_NO_PIPELINE_TYPE  "NO_PIPELINE_TYPE_ERR"  // pipeline type not specified
#define MSGERR_NO_MEDIA_URI      "NO_MEDIA_URI_ERR"      // media URI not specified
#define MSGERR_NO_VOLUME         "NO_VOLUME_ERR"         // volume not specified
#define MSGERR_NO_TLXS           "NO_TLXS_ERR"           // top left X src not specified
#define MSGERR_NO_TLYS           "NO_TLYS_ERR"           // top left Y src not specified
#define MSGERR_NO_BRXS           "NO_BRXS_ERR"           // bottom right X src not specified
#define MSGERR_NO_BRYS           "NO_BRYS_ERR"           // bottom right Y src not specified
#define MSGERR_NO_TLXD           "NO_TLXD_ERR"           // top left X dst not specified
#define MSGERR_NO_TLYD           "NO_TLYD_ERR"           // top left Y dst not specified
#define MSGERR_NO_BRXD           "NO_BRXD_ERR"           // bottom right X dst not specified
#define MSGERR_NO_BRYD           "NO_BRYD_ERR"           // bottom right Y dst not specified
#define MSGERR_NO_FS             "NO_FS_ERR"             // fullscreen attribute not specified
#define MSGERR_NO_TRACK_TYPE     "NO_TRACK_TYPE_ERR"     // track type not specified
#define MSGERR_NO_TRACK_INDEX    "NO_TRACK_INDEX_ERR"    // track index not specified
#define MSGERR_NO_ENABLE         "NO_ENABLE_ERR"         // subtitle enable not specified
#define MSGERR_NO_SYNC           "NO_SYNC_ERR"           // subtitle sync flag not specified
#define MSGERR_NO_FONT_SIZE      "NO_FONT_SIZE_ERR"      // subtitle font size not specified
#define MSGERR_NO_COLOR          "NO_COLOR_ERR"          // subtitle color not specified
#define MSGERR_INV_SUB_PAYLOAD   "INVALID_SUB_PAYLOAD"   // subtitle payload not specified properly
#define MSGERR_NO_PAYLOAD        "NO_PAYLOAD_ERR"        // payload not specified
#define MSGERR_NO_MODULE_NAME    "NO_MODULE_NAME_ERR"    // module name not specified
#define MSGERR_NO_LOG_LEVEL      "NO_LOG_LEVEL_ERR"      // logging level not specified
#define MSGERR_NO_CONN_TYPE      "NO_CONN_TYPE_ERR"      // connection type not specified
#define MSGERR_NO_CONN_ID        "NO_CONN_ID_ERR"        // connection id not specified
#define MSGERR_NO_SVC_NAME       "NO_SVC_NAME_ERR"       // service name not specified
#define MSGERR_NO_RESOURCES      "NO_RESOURCES_ERR"      // resources not specified
#define MSGERR_POLCAND_FIND      "POLCAND_FIND_ERR"      // policy candidate no found
#define MSGERR_ACQUIRE_FAILED    "ACQUIRE_FAILED_ERR"    // resource acquire failed
#define MSGERR_CLARG_PARSE       "CLARG_PARSE_ERR"       // command line arguments parsing failed
#define MSGERR_NO_RES_CONF       "NO_RES_CONF_ERR"       // resource configuration file not provided
#define MSGERR_SRV_OPERATE       "SRV_OPERATE_ERR"       // general media server operation failure
#define MSGERR_UNKNOWN_MODULE    "UNKNOWN_MODULE_ERR"    // unknown module
#define MSGERR_NO_LOCATION       "NO_LOCATION_ERR"       // location not specified
#define MSGERR_NO_FORMAT         "NO_FORMAT_ERR"         // format not specified
#define MSGERR_NO_WIDTH          "NO_WIDTH_ERR"          // width not specified
#define MSGERR_NO_HEIGHT         "NO_HEIGHT_ERR"         // height not specified
#define MSGERR_NO_BITRATE        "NO_BITRATE_ERR"        // bitrate not specified
#define MSGERR_NO_INIT           "NO_INIT_ERR"           // init not specified
#define MSGERR_NO_QUALITY        "NO_QUALITY_ERR"        // picture quality not specified
#define MSGERR_NO_BUF_RANGE_INT  "NO_VALUE_INT"      // buffer range interval must be provided
#define MSGERR_NO_CUR_TIME_INT   "NO_KEY_INT"       // current time interval must be provided
#define MSGERR_NO_VALUE_INT  "NO_VALUE_INT"      // value for interval must be provided
#define MSGERR_NO_KEY_INT   "NO_KEY_INT"       // key for interval must be provided
#define MSGERR_POWERD_REG_FAILED   "POWERD_REGISTRATION_FAILED"       // powerd shut down registration
#define MSGERR_NO_IP             "NO_IP_ERR"             // ip address not specified
#define MSGERR_NO_PORT           "NO_PORT_ERR"           //port number not specified
#define MSGERR_NO_BASE_TIME      "NO_BASE_TIME_ERR"      // base time not specified
#define MSGERR_NO_AUDIO_MODE     "NO_AUDIO_MODE_ERR"     // audio mode not specified
#define MSGERR_NO_BUF_RANGE_INT  "NO_VALUE_INT"          // buffer range interval must be provided
#define MSGERR_NO_CUR_TIME_INT   "NO_KEY_INT"            // current time interval must be provided
#define MSGERR_NO_VALUE_INT      "NO_VALUE_INT"          // value for interval must be provided
#define MSGERR_NO_KEY_INT        "NO_KEY_INT"            // key for interval must be provided
#define MSGERR_POWERD_REG_FAILED   "POWERD_REGISTRATION_FAILED" // powerd shut down registration
// resource_manager
#define MSGERR_UMC_CREATE        "UMC_CREATE_ERR"        // umedia connector creation failed
#define MSGERR_CONN_OPEN         "CONN_OPEN_ERR"         // already connected
#define MSGERR_CONN_TIMEOUT      "CONN_TIMEOUT_ERR"      // connection timeout
#define MSGERR_CONN_CLOSED       "CONN_CLOSED_ERR"       // connection closed
#define MSGERR_TRANS_LOCKED      "TRANS_LOCKED_ERR"      // transaction locked
#define MSGERR_UNLOCK_TIMEOUT    "UNLOCK_TIMEOUT_ERR"    // transaction unlock timeout
#define MSGERR_NO_TRANS_LOCK     "NO_TRANS_LOCK_ERR"     // no transaction lock specified
#define MSGERR_NO_POL_ACT        "NO_POL_ACT_ERR"        // no policy action specified
#define MSGERR_NO_ACQ_COMPLETE   "NO_ACQ_COMPLETE_ERR"   // no acquire complete specified
#define MSGERR_EVENT_TIMEOUT     "EVENT_TIMEOUT_ERR"     // event timeout error
#define MSGERR_NO_POLICY_TYPE    "NO_POLICY_TYPE_ERR"    // no resource policy type specified
#define MSGERR_NO_POLICY_PRIO    "NO_POLICY_PRIO_ERR"    // no resource policy priority specified
#define MSGERR_POLICY_FIND       "POLICY_FIND_ERR"       // policy not found
#define MSGERR_INVALID_CONN      "INVALID_CONN_ERR"      // invalid connection
#define MSGERR_BAD_SPLIT         "BAD_SPLIT_ERR"         // bad split occurred
#define MSGERR_RESOURCE_FIND     "RESOURCE_FIND_ERR"     // system resource not found
#define MSGERR_PIPELINE_REM      "PIPELINE_REM_ERR"      // pipeline removal failed
#define MSGERR_CONN_FIND         "CONN_FIND_ERR"         // connection not found
#define MSGERR_LOCK_OWNERSHIP    "LOCK_OWNERSHIP_ERR"    // lock ownership error
#define MSGERR_NOT_ENOUGH_RES    "NOT_ENOUGH_RES_ERR"    // not enough free resources
#define MSGERR_RES_ACQUIRE       "RES_ACQUIRE_ERR"       // failed to acquire resources
#define MSGERR_BAD_CAST          "BAD_CAST_ERR"          // bad cast occurred
#define MSGERR_NO_RESOURCE_REQ   "NO_RESOURCE_REQ_ERR"   // no resource request specified
#define MSGERR_POLICY_FAILED     "POLICY_FAILED_ERR"     // failed to send policy action
#define MSGERR_SEND_ACQUIRE_RESULT "SEND_ACQUIRE_RESULT_ERR" // failed to send acquire result
#define MSGNFO_POLICY_REQUEST    "POLICY_REQUEST"        // send policy action to candidate
// ums dbi
#define MSGERR_DBI_ERROR          "DBI_ACCESS_ERR"       // registry access error
// simulated pipeline
#define MSGERR_INIT_FAILED       "INIT_FAILED_ERR"       // initialization failed
// reference pipeline
#define MSGERR_PIPELINE_INIT     "PIPELINE_INIT_ERR"     // initialization failed
#define MSGERR_GST_STATE_CHANGE  "GST_STATE_CHANGE_ERR"  // gstreamer state change error
#define MSGERR_PLAYBACK_THR      "PLAYBACK_THR_ERR"      // playback thread error
#define MSGERR_AUDIOSINK_SETUP   "AUDIOSINK_SETUP_ERR"   // audio sink setup error
#define MSGERR_VIDEOSINK_SETUP   "VIDEOSINK_SETUP_ERR"   // video sink setup error
#define MSGERR_PLAY              "PLAY_ERR"              // play failed
#define MSGERR_PAUSE             "PAUSE_ERR"             // pause failed
#define MSGERR_SEEK              "SEEK_ERR"              // seek failed
#define MSGERR_PLAYRATE          "PLAYRATE_ERR"          // set playrate failed
#define MSGERR_SELECT_TRACK      "SELECT_TRACK_ERR"      // select track failed
#define MSGERR_SUBTITLE_SRC      "SUBTITLE_SRC_ERR"      // set subtitle source failed
#define MSGERR_SUBTITLE_ENABLE   "SUBTITLE_ENABLE_ERR"   // enable subtitle failed
#define MSGERR_SUBTITLE_POS      "SUBTITLE_POS_ERR"      // set subtitle position failed
#define MSGERR_SUBTITLE_SYNC     "SUBTITLE_SYNC_ERR"     // subtitle sync failed
#define MSGERR_SUBTITLE_FONT_SIZE "SUBTITLE_FONT_SIZE_ERR"// set subtitle font size failed
#define MSGERR_SUBTITLE_COLOR    "SUBTITLE_COLOR_ERR"   // set subtitle color failed
#define MSGERR_UPDATE_INT        "UPDATE_INT_ERR"        // set update interval failed
#define MSGERR_PROPERTY          "PROPERTY_ERR"          // set property failed
#define MSGERR_INVALID_ARG       "INVALID_ARG_ERR"       // invalid argument error
#define MSGERR_INVALID_LOG_LVL   "INVALID_LOG_LVL_ERR"   // invalid log level
#define MSGERR_STATE_WAIT        "STATE_WAIT_ERR"        // set state and wait error
#define MSGERR_STATE             "STATE_ERR"             // set state error
#define MSGERR_GST_MSG           "GST_MSG_ERR"           // gstreamer error message
#define MSGERR_GST_MSG_WARN      "GST_MSG_WARN"          // gstreamer warning message
#define MSGERR_SNAPSHOT          "SNAPSHOT_ERR"          // take snapshot failed
#define MSGERR_PIPELINE_LOAD     "PIPELINE_LOAD_ERR"     // pipeline failed to load media
#define MSGERR_CHANGE_RESOLUTION "CHANGE_RESOLUTION_ERR" // pipeline failed to change camera resolution
#define MSGERR_STREAM_QUALITY    "STREAM_QUALITY_ERR"    // pipeline failed to set input stream quality
#define MSGERR_RMC_CREATE        "RMC_CREATE_ERR"        // resource manager creation failed
#define MSGERR_NO_POLICY_ACT     "NO_POLICY_ACT_ERR"     // no policy action supplied
// media display controller
#define MSGERR_UNHANDLED_REPLY   "UNHANDLED_REPLY_ERR"   // reply message not handled
#define MSGERR_VSM_REGISTER_ERR  "VSM_REGISTER_ERR"      // failed to register media with vsm
#define MSGERR_SKIPPED_EVENT     "SKIPPED_EVENT_ERR"     // unprocessed mdc event
#define MSGERR_FG_RESUBSCRIBE    "FG_RESUBSCRIBE_ERR"    // getForegroudnAppInfo Re-Subscription
#define MSGNFO_ACQUIRE_REQUEST   "ACQUIRE_REQUEST"       // acquire resources
#define MSGNFO_ACQUIRE_COMPLETED "ACQUIRE_COMPLETED"     // acquire completed
#define MSGNFO_FOREGROUND_REQUEST "FOREGROUND_REQUEST"   // notifyForeground request
#define MSGNFO_BACKGROUND_REQUEST "BACKGROUND_REQUEST"   // notifyBackground request

// Avoutputd display controller
#define MSGERR_AVOUTPUT_SUBSCRIBE  "AVOUTPUT_SUBSCRIBE"   // subscribe to avoutput/sink/getStatus
#define MSGERR_MEDIA_ID_NOT_CONNECTED "MEDIA_ID_NOT_CONNECTED"  // cannot perform action on media id, not connected
#define MSGERR_MEDIA_ID_NOT_REGISTERD  "MEDIA_ID_NOT_REGISTERED"  // cannot perform action on media id, not registerd
#define MSGERR_AVOUTPUTD_INVALID_PARAMS "AVOUTPUTD_INVALID_PARAMS"  // invalid parameters for avoutputd call
#define MSGWARN_UNEXPECTED_AUDIO_CONNECTION "UNEXPECTED_AUDIO_CONNECTION"

// Performance metrics classification tags
#define MSGNFO_LOAD_REQUEST      "LOAD_REQUEST"          // got load request
#define MSGNFO_PRELOAD_REQUEST   "PRELOAD_REQUEST"       // got preload request
#define MSGNFO_ATTACH_REQUEST    "ATTACH_REQUEST"        // got attach request
#define MSGNFO_PIPELINE_LOADED   "PIPELINE_LOADED"       // pipeline loaded media
#define MSGNFO_UNLOAD_REQUEST    "UNLOAD_REQUEST"        // got unload request
#define MSGNFO_PIPELINE_UNLOADED "PIPELINE_UNLOADED"     // pipeline unloaded
#define MSGNFO_MDC_REGISTRATION  "MDC_REGISTRATION"      // mdc media registration
#define MSGNFO_APP_STATUS_UPDATE "APP_STATUS_UPDATE"     // app status update
#define MSGNFO_MDC_AUTO_LAYOUT   "MDC_AUTO_LAYOUT"       // mdc layout management

// KVP tags
#define KVP_SESSION_ID           "SESSION_ID"            // unique session id
#define KVP_CODE_POINT           "CODE_POINT"            // code point (file, function, line)
#define KVP_TIMESTAMP            "TIMESTAMP"             // monotonic timestamp
// Extended set of KVP tags
#define KVP_ERROR                "ERROR"                 // error message
#define KVP_ERROR_CODE           "ERROR_CODE"            // error code
#define KVP_PIPELINE_TYPE        "PIPELINE_TYPE"         // registered pipeline type
#define KVP_FILE                 "FILE"                  // file on which operation was performed
#define KVP_CONFIG_KEY           "CONFIG_KEY"            // config key
#define KVP_SERVICE              "SERVICE"               // luna service name
#define KVP_MSG                  "MSG"                   // luna message
#define KVP_CHLDPID              "CHLDPID"               // child process pid
#define KVP_PROCESS              "PROCESS"               // child process name
#define KVP_PIPELINE_ID          "PIPELINE_ID"           // pipeline id
#define KVP_PIPELINE_TYPE        "PIPELINE_TYPE"         // pipeline type
#define KVP_MEDIA_ID             "MEDIA_ID"              // media addressing id
#define KVP_MEDIA_URI            "MEDIA_URI"             // media URI
#define KVP_POLICY_PRI           "POLICY_PRI"            // policy priority
#define KVP_OWNER                "OWNER"                 // resource owner
#define KVP_REQUESTER            "REQUESTER"             // resource requester
#define KVP_RESOURCE             "RESOURCE"              // resource name
#define KVP_RES_MAX_QTY          "RES_MAX_QTY"           // resource max quantity
#define KVP_RES_AVL_QTY          "RES_AVL_QTY"           // resource available quantity
#define KVP_RES_REQ_QTY          "RES_REQ_QTY"           // resource requested quantity
#define KVP_DATA                 "DATA"                  // some data
#define KVP_REASON               "REASON"                // error reason
