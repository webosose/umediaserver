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

//->Start of API documentation comment block
/**
@page com_webos_media com.webos.media
@brief Provides media playback and record functionality
@{
@}
 */
//->End of API documentation comment block

#include <Logger_macro.h>
#include <PipelineManager.h>
#include <Pipeline.h>
#include <UMSConnector.h>
#include <uMediaserver.h>
#include <GenerateUniqueID.h>
#include <MediaDisplayController.h>
#include <sstream>
#include <fstream>
#include <functional>
#include <boost/filesystem/operations.hpp>
#include <sys/inotify.h>
#include <libconfig.h++>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <umsTrace.h>

using namespace uMediaServer;
using namespace uMediaServer::Reg;
using namespace pbnjson;
using namespace std;
using namespace libconfig;

namespace fs = boost::filesystem;

#define RETURN_IF(exp,rv,msgid,format,args...) \
		{ if(exp) { \
			LOG_ERROR(log, msgid, format, ##args); \
			return rv; \
		} \
		}

namespace {
// filter ADECs and VDECs out of resource_list_t and convert to res_info_t
mdc::res_info_t convert_to_res_info(const resource_list_t & resources) {
	mdc::res_info_t res_info;
	for (const auto & resource_unit : resources) {
		res_info.add_resource(resource_unit.id, resource_unit.index);
	}
	return res_info;
}

} // namespace

uMediaserver::uMediaserver(const std::string& conf_file)
: log(UMS_LOG_CONTEXT_SERVER), dynamic_config_dir_watcher_(nullptr),
  senderForSetMaster(nullptr),
  messageForSetMaster(nullptr)
{
	LOG_TRACE(log, "uMediaserver connection: %s", UMEDIASERVER_CONNECTION_ID);
	LOG_TRACE(log, "uMediaserver resource file: %s", conf_file.c_str());

	// read configuration file for pipeline type definitions
	//  store pipelines types in system_pipelines queue
	Config cfg;
	auto registry = Registry::instance();

	LOG_DEBUG(log, "+ reading configuration file: %s",  conf_file.c_str());
	if (!readConfigFile(conf_file, cfg)) {
		LOG_CRITICAL(log, MSGERR_CONFIG,
				"uMS main configuration file broken or missing. %s",
				conf_file.c_str());
		exit(EXIT_FAILURE);
	}

	if (!registry->apply(cfg)) {
		LOG_CRITICAL(log, MSGERR_CONFIG,
				"uMS main configuration file broken. %s",
				conf_file.c_str());
		exit(EXIT_FAILURE);
	}

	string dir_path;
	registry->get("dynamic_pipeline_dir", dir_path);
	auto dynamic_config_dir = fs::path(dir_path);

	connector = new UMSConnector(UMEDIASERVER_CONNECTION_ID,
			NULL,static_cast<void*>(this), UMS_CONNECTOR_DUAL_BUS);
	sr_watcher = new ServiceReadyWatcher(connector);

	// TODO: switch resource manager to registry interface
	rm = new ResourceManager(cfg);
	pm = new PipelineManager(*sr_watcher);

	initAcquireQueue();

	if (!dynamic_config_dir.empty()) {
		readDynamicPipelineConfigs();
		try {
			auto cb = [this] {
				removeDynamicPipelines();
				readDynamicPipelineConfigs();
			};
			dynamic_config_dir_watcher_.reset(new DirectoryWatcher<function<void()>>(dynamic_config_dir.string(), cb));
		}
		catch (dwexception ex) {
			LOG_ERROR(log, MSGERR_CONFIG, "%s", ex.what());
		}
	}

	unload_functor_ = [this] (string connection_id) {
		LOG_DEBUG(log, "unloadFunctor(%s)", connection_id.c_str());
		acquire_queue.removeWaiter(connection_id);
		pm->stateChange(connection_id, false);
		if (pm->unload(connection_id)) {
			connector->unrefMessage(connection_message_map_[connection_id]);
			connection_message_map_.erase(connection_id);
		}
		mdc_->unregisterMedia(connection_id);
	};

	acquire_callback_ = [this](const std::string & id, const resource_list_t & resources) {
		// notify mdc
		std::string pipeline_service = pm->getPipelineServiceName(id);
		mdc_->acquired(id, pipeline_service, convert_to_res_info(resources));
	};

	rm->setAcquireCallback(acquire_callback_);
	rm->setReleaseCallback([this](const std::string & id, const resource_list_t & resources) {
		// notify mdc
		mdc_->released(id, convert_to_res_info(resources));
		// notify acquire queue
		acquire_queue.resourceReleased();
	});
	rm->setAcquireDisplayResourceCallback([this](const std::string & id, const int32_t & index, ums::disp_res_t & res) {
		mdc_->acquireDisplayResource(id, index, res);
	});
	rm->setReleaseDisplayResourceCallback([this](const std::string & id, const int32_t & index) {
		mdc_->releaseDisplayResource(id, index);
	});
	// release managed pipeline resources and unregister at exit
	pm->pipeline_exited.connect([&](const std::string &id) {
		rm->resetPipeline(id);
	});

	pm->pipeline_removed.connect([&](const std::string & id) {
		rm->unregisterPipeline(id);
	});

	pm->pipeline_pid_update.connect([this](const string &appid, pid_t pid, bool exec) {
		JGenerator serializer;
		JValue payload = Object();
		string payload_serialized;
		payload.put("appId", JValue(appid));
		payload.put("pid", JValue(pid));
		payload.put("exec", JValue(exec));
		JValue event = Object();
		event.put("procUpdate", payload);

		if (!serializer.toString(event,  pbnjson::JSchema::AllSchema(), payload_serialized)) {
			LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failure to serializer.toString()");
		}
		connector->sendChangeNotificationJsonString(payload_serialized, "trackAppProcesses");

	});

	// ---
	// uMediaserver public API

	connector->addEventHandler("load",loadCallback);
	connector->addEventHandler("attach",attachCallback);
	connector->addEventHandler("unload",unloadCallback);

	// media operations
	connector->addEventHandler("play",playCallback);
	connector->addEventHandler("pause",pauseCallback);
	connector->addEventHandler("seek",seekCallback);
	connector->addEventHandler("subscribe",stateChangeCallback);
	connector->addEventHandler("unsubscribe",unsubscribeCallback);
	connector->addEventHandler("setPlayRate",setPlayRateCallback);
	connector->addEventHandler("setVolume",setVolumeCallback);

	// Resource Manager API
	connector->addEventHandler("registerPipeline",registerPipelineCallback, UMS_CONNECTOR_PRIVATE_BUS);
	connector->addEventHandler("unregisterPipeline",unregisterPipelineCallback, UMS_CONNECTOR_PRIVATE_BUS);

	connector->addEventHandler("acquire",acquireCallback, UMS_CONNECTOR_PRIVATE_BUS);
	connector->addEventHandler("tryAcquire",tryAcquireCallback, UMS_CONNECTOR_PRIVATE_BUS);
	connector->addEventHandler("release",releaseCallback, UMS_CONNECTOR_PRIVATE_BUS);
	connector->addEventHandler("notifyForeground",notifyForegroundCallback);
	connector->addEventHandler("notifyBackground",notifyBackgroundCallback);
	connector->addEventHandler("notifyActivity",notifyActivityCallback);
	connector->addEventHandler("trackAppProcesses", trackAppProcessesCallback, UMS_CONNECTOR_PRIVATE_BUS);

	// MDC API
	connector->addEventHandler("registerMedia", registerMediaCallback, UMS_CONNECTOR_PUBLIC_BUS);

	// pipeline state query API
	connector->addEventHandler("getActivePipelines", getActivePipelinesCallback, UMS_CONNECTOR_PRIVATE_BUS);

	// ---
	mdc_ = MediaDisplayController::instance(connector);

	// register for visibility events to update Resource Manager state and LRU score
	mdc_->registerEventNotify(mdc::event::EventSignalType::VISIBLE, [this](mdc::event::EventSignalType,
			const std::string &id, const mdc::event::EventDataBaseType &state) {
		const mdc::event::VisibilityEvent &visibility_event = static_cast<const mdc::event::VisibilityEvent&>(state);

		LOG_TRACE(log, "MDC::VISIBLE id = %s, state = %s.",
				id.c_str(), visibility_event.state ? "TRUE" : "FALSE");

		rm->notifyVisibility(id, visibility_event.state);
	});

	// register for focus events to update Resource Manager LRU score
	mdc_->registerEventNotify(mdc::event::EventSignalType::FOCUS, [this](mdc::event::EventSignalType,
			const std::string &id, const mdc::event::EventDataBaseType &state) {
		const mdc::event::FocusEvent &focus_event = static_cast<const mdc::event::FocusEvent&>(state);

		LOG_TRACE(log, "MDC::FOCUS id = %s, state = %s. Update RM LRU.",
				id.c_str(), focus_event.state ? "TRUE" : "FALSE");

		// update LRU and focus state
		if (focus_event.state) {
			rm->notifyActivity(id);
			rm->notifyFocus(id);
			pm->resume(id);
		}
	});

	mdc_->registerEventNotify(mdc::event::EventSignalType::FOREGROUND, [this](mdc::event::EventSignalType,
			const std::string &id, const mdc::event::EventDataBaseType &state) {
		const mdc::event::ForegroundEvent &event = static_cast<const mdc::event::ForegroundEvent&>(state);

		// update LRU and focus state
		if (event.state) {
			rm->notifyActivity(id);
			rm->notifyForeground(id);
			pm->resume(id);
		} else {
			rm->notifyBackground(id);
		}
	});

	mdc_->registerEventNotify(mdc::event::EventSignalType::REGISTERED, [this](mdc::event::EventSignalType,
							  const std::string &id, const mdc::event::EventDataBaseType &) {
		string active_pipeline;
		if(pm->getActivePipeline(id, active_pipeline))
			rm->setManaged(id);
	});

	mdc_->registerEventNotify(mdc::event::EventSignalType::SOUND_DISCONNECTED,
			[this] (mdc::event::EventSignalType, const std::string & id, const mdc::event::EventDataBaseType &) {
		pm->pause(id);
	});

	mdc_->registerEventNotify(mdc::event::EventSignalType::PLANE_ID,
			[this] (mdc::event::EventSignalType, const std::string & id, const mdc::event::EventDataBaseType &state) {
		const mdc::event::PlaneIdEvent &event =  static_cast<const mdc::event::PlaneIdEvent&>(state);
		if (event.plane_id >= 0) {
			auto connection = rm->findConnection(id);
			if (connection && !connection->is_managed) {
				pbnjson::JValue args = pbnjson::JObject {{"planeID", event.plane_id}};
				std::string message;
				if (pbnjson::JGenerator(nullptr).toString(args, pbnjson::JSchema::AllSchema(), message)) {
					std::string cmd = id + "/SetPlane";
					connector->sendMessage(cmd, message, nullptr, nullptr);
				}
			} else {
				pm->setPlane(id, event.plane_id);
			}
		}
	});

} // end uMediaServer

uMediaserver::~uMediaserver()
{
	LOG_TRACE(log, "uMediaserver dtor.");

	delete pm;
	delete rm;
	delete mdc_;
	delete connector;
}

bool uMediaserver::readConfigFile(const string& conf_file, Config& cfg)
{
	try {
		cfg.readFile(conf_file.c_str());
	}
	catch(const FileIOException &fioex) {
		LOG_ERROR(log,MSGERR_CONFIG_OPEN,
				"uMS configuration file not found. %s",
				conf_file.c_str());
		return false;
	}
	catch(const ParseException &pex) {
		LOG_ERROR(log,MSGERR_CONFIG,
				"uMS configuration has an error. Parse error at %s, %d, %s.",
				pex.getFile(), pex.getLine(), pex.getError());
		return false;
	}
	return true;
}

void uMediaserver::readDynamicPipelineConfigs()
{
	auto registry = Reg::Registry::instance();
	std::string dir_path;
	if (!registry->get("dynamic_pipeline_dir", dir_path)) {
		LOG_ERROR(log, MSGERR_CONFIG, "unable to find dynamic pipeline config directory");
		return;
	}
	auto dynamic_config_dir = fs::path(dir_path);
	if (fs::exists(dynamic_config_dir) && fs::is_directory(dynamic_config_dir)) {
		fs::directory_iterator end_iter;
		for (fs::directory_iterator dir_iter(dynamic_config_dir); dir_iter!=end_iter; dir_iter++) {
			if (dir_iter->status().type() != fs::file_type::regular_file) {
				continue;
			}

			Config cfg;
			string conf_file = dir_iter->path().string();
			LOG_DEBUG(log, "+ reading dynamic config file: %s",  conf_file.c_str());
			if (readConfigFile(conf_file, cfg)) {
				if (!registry->apply(cfg)) {
					LOG_ERROR(log, MSGERR_CONFIG, "%s : wrong config schema.", conf_file.c_str());
				}
				// TODO: remove when rm switched to registry iface
				try {
					const auto & pipelines_config = cfg.getRoot()["pipelines"];
					for (size_t p = 0; p < pipelines_config.getLength(); ++p) {
						const auto & pipeline_config = pipelines_config[p];
						std::string pipeline_type = (const char *)pipeline_config["type"];
						uint32_t pipeline_priority = pipeline_config["priority"];
						dynamic_pipeline_types_.push_back(pipeline_type);
						rm->setPriority(pipeline_type, pipeline_priority);
					}
				} catch(...) {}
			} else {
				LOG_ERROR(log, MSGERR_CONFIG, "%s : corrupted config file.", conf_file.c_str());
			}
		}
	}
	else if (!fs::create_directories(dynamic_config_dir)) {
			LOG_ERROR(log, MSGERR_CONFIG, "Could not create non-existing dynamic_pipeline_dir: %s",
				dynamic_config_dir.c_str());
	}
}

void uMediaserver::removeDynamicPipelines()
{
	auto registry = Reg::Registry::instance();
	for(const auto & type : dynamic_pipeline_types_)
	{
		// TODO: can we inore errors here?
		registry->del("environment", type);
		registry->del("pipelines", type);
		rm->removePriority(type);
	}
	dynamic_pipeline_types_.clear();
}

//->Start of API documentation comment block
/**
@page com_webos_media com.webos.media
@{
@section com_webos_media_load load

Requests the media server to load a new media object for the specified URI.

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
uri     | yes | String | Location of media file
type    | yes | String | Pipeline type to launch
payload | yes | String | JSON object containing pipeline specific parameters

@par Returns(Call)
Name | Required | Type | Description
-----|--------|------|----------
greturnValue | yes | Boolean | true if successful, false otherwise.
errorCode   | no  | Integer | errorCode only if returnValue is false.
errorText   | no  | String  | errorText only if returnValue is false.
mediaId     | yes | String  | media id assigned to this media.

@par Returns(Subscription)
None
@}
 */
//->End of API documentation comment block
bool uMediaserver::loadCommand(UMSConnectorHandle* sender,
		UMSConnectorMessage* message, void* ctxt)
{
	static auto is_nabs = [](const std::string & transport) {
		static const std::string NABS_TRANSPORT_TYPES[2] = {"NABS-ROUTE", "NABS-MMT"};
		for (const auto & nabs : NABS_TRANSPORT_TYPES)
			if (nabs == transport)
				return true;
		return false;
	};
	JDomParser parser;

	string cmd = connector->getMessageText(message);
	if (!parser.parse(cmd, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. cmd=%s ", cmd.c_str());
		return false;
	}

	JValue parsed = parser.getDom();
	RETURN_IF(!(parsed.hasKey("uri") && parsed["uri"].isString()), false, MSGERR_NO_MEDIA_URI, "client must specify uri");
	RETURN_IF(!(parsed.hasKey("type") && parsed["type"].isString()), false, MSGERR_NO_PIPELINE_TYPE, "client must specify type");

	string uri = parsed["uri"].asString();
	string type = parsed["type"].asString();

	RETURN_IF(!rm->isValidType(type), false, MSGERR_NO_PIPELINE_TYPE, "specified type is not valid");

	string app_id = parsed["payload"]["option"]["appId"].asString();
	string transport = parsed["payload"]["mediaTransportType"].asString();
	string payload = JGenerator::serialize(parsed["payload"], pbnjson::JSchema::AllSchema());

	string connection_id;   // id returned by load
	bool preloaded = false;
	bool isPreload = false;

	if (parsed.hasKey("mediaId")) {
		connection_id = parsed["mediaId"].asString();   // id returned by dispatch
		preloaded = true;
	}

	LOG_DEBUG(log, "connection_id : %s", connection_id.c_str());

	if(preloaded)
		isPreload = false;

	bool rv = pm->load(connection_id, type, uri, payload, app_id, connector, isPreload);

	if (!parsed["payload"]["option"]["preload"]) {
		preloaded = false;
	}

	if (!preloaded) {
		// register pipeline as managed with Resource Manager
		UMSTRACE_BEFORE((connection_id+"_load").c_str());
		rm->registerPipeline(connection_id, type);

		// register with Media Display Controller
		if (!is_nabs(transport))
		{
			LOG_DEBUG(log, "registerMedia by umediaserver media_id:%s, app_id:%s",
					connection_id.c_str(), app_id.c_str());
			mdc_->registerMedia(connection_id, app_id);
		}

		LOG_INFO_EX(log, MSGNFO_LOAD_REQUEST, __KV({ {KVP_MEDIA_ID, connection_id},
					{KVP_PIPELINE_TYPE, type} }), "");

		connector->addClientWatcher(sender, message, bind(unload_functor_,connection_id));
		connection_message_map_[connection_id] = message;
		connector->refMessage(message);
	}

	string retObject = createRetObject(rv, connection_id);
	connector->sendResponseObject(sender,message,retObject);
	rm->notifyActivity(connection_id);
	return true;
}

//->Start of API documentation comment block
/**
@page com_webos_media com.webos.media
@{
@section com_webos_media_attach attacj

Requests the media server to attach to an existing pipeline

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
mediaId     | yes | String | Media ID of pipeline to attach to

@par Returns(Call)
Name | Required | Type | Description
-----|--------|------|----------
returnValue | yes | Boolean | true if successful, false otherwise.
errorCode   | no  | Integer | errorCode only if returnValue is false.
errorText   | no  | String  | errorText only if returnValue is false.
mediaId     | yes | String  | media id assigned to this media.

@par Returns(Subscription)
None
@}
 */
//->End of API documentation comment block
bool uMediaserver::attachCommand(UMSConnectorHandle* sender,
		UMSConnectorMessage* message, void* ctxt)
{
	JDomParser parser;

	string cmd = connector->getMessageText(message);
	if (!parser.parse(cmd, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. cmd=%s ", cmd.c_str());
		return false;
	}

	JValue parsed = parser.getDom();
	RETURN_IF(!parsed.hasKey("mediaId"), false, MSGERR_NO_MEDIA_ID, "mediaId must be specified");

	string connection_id = parsed["mediaId"].asString();

	LOG_DEBUG(log, "attach. cmd=%s,connection_id=%s",
			cmd.c_str(), connection_id.c_str());

	LOG_INFO_EX(log, MSGNFO_ATTACH_REQUEST, __KV({{KVP_MEDIA_ID, connection_id}}), "");

	string retObject;
	if (connection_message_map_.find(connection_id) == connection_message_map_.end()) {
		retObject = createRetObject(false, connection_id);
	}
	else {
		// Since the unload_functor_ runs in the same thread as this command, there is no
		// need to make this atomic
		connector->delClientWatcher(sender, connection_message_map_[connection_id]);
		connector->addClientWatcher(sender, message, bind(unload_functor_, connection_id));
		connector->unrefMessage(connection_message_map_[connection_id]);
		connection_message_map_[connection_id] = message;
		connector->refMessage(message);
		retObject = createRetObject(true, connection_id);
	}
	JValue detached_msg(Object()), payload(Object());
	string json_message;
	payload.put("state", true);
	payload.put("mediaId", connection_id);
	detached_msg.put("detached", payload);
	JGenerator().toString(detached_msg,  pbnjson::JSchema::AllSchema(), json_message);

	connector->sendChangeNotificationJsonString(json_message, connection_id);

	connector->sendResponseObject(sender,message,retObject);

	return true;
}

//->Start of API documentation comment block
/**
@page com_webos_media com.webos.media
@{
@section com_webos_media_unload unload

Unloads the media object and releases all shared AV resources.

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
mediaId | yes | String | media id assigned to this media.

@par Returns(Call)
Name | Required | Type | Description
-----|--------|------|----------
returnValue | yes | Boolean | true if successful, false otherwise.
errorCode   | no  | Integer | errorCode only if returnValue is false.
errorText   | no  | String  | errorText only if returnValue is false.

@par Returns(Subscription)
None
@}
 */
//->End of API documentation comment block

bool uMediaserver::unloadCommand(UMSConnectorHandle* sender, UMSConnectorMessage* message, void* ctxt)
{
	JDomParser parser;

	string cmd = connector->getMessageText(message);
	string type;

	if (!parser.parse(cmd, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", cmd.c_str());
		return false;
	}

	JValue parsed = parser.getDom();
	RETURN_IF(!parsed.hasKey("mediaId"), false, MSGERR_NO_MEDIA_ID, "mediaId must be specified");

	string connection_id = parsed["mediaId"].asString();

	LOG_DEBUG(log, "unload. cmd=%s,connection_id=%s",
			cmd.c_str(), connection_id.c_str());

	LOG_INFO_EX(log, MSGNFO_UNLOAD_REQUEST, __KV({{KVP_MEDIA_ID, connection_id}}), "");

	// unregister with Media Display Controller
	mdc_->unregisterMedia(connection_id);
	acquire_queue.removeWaiter(connection_id);
	bool rv = pm->unload(connection_id);
	if (rv) {
		connector->delClientWatcher(sender, message);
		connector->unrefMessage(connection_message_map_[connection_id]);
		connection_message_map_.erase(connection_id);
	}

	string retObject = createRetObject(rv, connection_id);
	connector->sendResponseObject(sender,message,retObject);
	return true;
}

//->Start of API documentation comment block
/**
@page com_webos_media com.webos.media
@{
@section com_webos_media_play play

Plays the media object.

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
mediaId | yes | String | media id assigned to this media.

@par Returns(Call)
Name | Required | Type | Description
-----|--------|------|----------
returnValue | yes | Boolean | true if successful, false otherwise.
errorCode   | no  | Integer | errorCode only if returnValue is false.
errorText   | no  | String  | errorText only if returnValue is false.
mediaId     | yes | String  | media id assigned to this media.

@par Returns(Subscription)
None
@}
 */
//->End of API documentation comment block
bool uMediaserver::playCommand(UMSConnectorHandle* sender, UMSConnectorMessage* message, void* ctxt)
{
	JDomParser parser;

	string cmd = connector->getMessageText(message);

	if (!parser.parse(cmd, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", cmd.c_str());
		return false;
	}

	JValue parsed = parser.getDom();
	RETURN_IF(!parsed.hasKey("mediaId"), false, MSGERR_NO_MEDIA_ID, "mediaId must be specified");

	string connection_id = parsed["mediaId"].asString();

	LOG_TRACE(log, "play. cmd=%s,connection_id=%s",
			cmd.c_str(), connection_id.c_str());

	rm->notifyActivity(connection_id);
	bool rv = pm->play(connection_id);

	string retObject = createRetObject(rv, connection_id);
	connector->sendResponseObject(sender,message,retObject);
	return true;
}

//->Start of API documentation comment block
/**
@page com_webos_media com.webos.media
@{
@section com_webos_media_pause pause

Pauses playback.

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
mediaId | yes | String | media id assigned to this media.

@par Returns(Call)
Name | Required | Type | Description
-----|--------|------|----------
returnValue | yes | Boolean | true if successful, false otherwise.
errorCode   | no  | Integer | errorCode only if returnValue is false.
errorText   | no  | String  | errorText only if returnValue is false.
mediaId     | yes | String  | media id assigned to this media.

@par Returns(Subscription)
None
@}
 */
//->End of API documentation comment block
bool uMediaserver::pauseCommand(UMSConnectorHandle* sender, UMSConnectorMessage* message, void* ctxt)
{
	JDomParser parser;

	string cmd = connector->getMessageText(message);

	if (!parser.parse(cmd, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", cmd.c_str());
		return false;
	}

	// TODO remove debug statement
	LOG_DEBUG(log, "%s ", cmd.c_str());

	JValue parsed = parser.getDom();
	RETURN_IF(!parsed.hasKey("mediaId"), false, MSGERR_NO_MEDIA_ID, "mediaId must be specified");

	string connection_id = parsed["mediaId"].asString();

	LOG_TRACE(log, "pause. cmd=%s,connection_id=%s", cmd.c_str(), connection_id.c_str());

	bool rv = pm->pause(connection_id);
	string retObject = createRetObject(rv, connection_id);

	connector->sendResponseObject(sender,message,retObject);
	return true;
}


//->Start of API documentation comment block
/**
@page com_webos_media com.webos.media
@{
@section com_webos_media_seek seek

Seeks to specified time position.

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
mediaId  | yes | String  | media id assigned to this media.
position | yes | Integer | position in milliseconds from the start to seek to.

@par Returns(Call)
Name | Required | Type | Description
-----|--------|------|----------
returnValue | yes | Boolean | true if successful, false otherwise.
errorCode   | no  | Integer | errorCode only if returnValue is false.
errorText   | no  | String  | errorText only if returnValue is false.
mediaId     | yes | String  | media id assigned to this media.

@par Returns(Subscription)
None
@}
 */
//->End of API documentation comment block
bool uMediaserver::seekCommand(UMSConnectorHandle* sender, UMSConnectorMessage* message, void* ctxt)
{
	JDomParser parser;

	string cmd = connector->getMessageText(message);
	if (!parser.parse(cmd, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. cmd=%s ", cmd.c_str());
		return false;
	}

	// TODO remove debug statement
	LOG_TRACE(log, "%s ", cmd.c_str());

	JValue parsed = parser.getDom();
	RETURN_IF(!parsed.hasKey("mediaId"), false, MSGERR_NO_MEDIA_ID, "mediaId must be specified");
	RETURN_IF(!parsed.hasKey("position"), false, MSGERR_NO_MEDIA_POS, "position must be specified");

	string connection_id = parsed["mediaId"].asString();
	JValue jposition = parsed["position"];

	int64_t pos = jposition.asNumber<int64_t>();
	long long position = static_cast<long long>(pos);

	LOG_TRACE(log, "cmd=%s,connection_id=%s, position=%" PRId64,
			cmd.c_str(),connection_id.c_str(), pos);

	rm->notifyActivity(connection_id);
	bool rv = pm->seek(connection_id, position);

	string retObject = createRetObject(rv, connection_id);
	connector->sendResponseObject(sender,message,retObject);
	return true;
}

//->Start of API documentation comment block
/**
@page com_webos_media com.webos.media
@{
@section com_webos_media_subscribe subscribe

subscribe events from a media pipeline.

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
mediaId  | yes | String  | media id assigned to this media.

@par Returns(Call)
Name | Required | Type | Description
-----|--------|------|----------
returnValue | yes | Boolean | true if successful, false otherwise.
errorCode   | no  | Integer | errorCode only if returnValue is false.
errorText   | no  | String  | errorText only if returnValue is false.
mediaId     | yes | String  | media id assigned to this media.

@par Returns(Subscription)
Name | Required | Type | Description
-----|--------|------|----------
loadCompleted | yes | String | JSON string in the form of {"loadCompleted" : {"mediaId":string}}
trackSelected | yes | String | JSON string in the form of {"trackSelected" : {"type":string,"index":integer,"mediaId":string}}
seekDone      | yes | String | JSON string in the form of {"seekDone"      : {"mediaId":string}}
endOfStream   | yes | String | JSON string in the form of {"endOfStream"   : {"mediaId":string}}
currentTime   | yes | String | JSON string in the form of {"currentTime"   : {"currentTime":integer,"mediaId":string}}
bufferRange   | yes | String | JSON string in the form of {"bufferRange"   : {"beginTime":integer,"endTime":integer,"remainingTime":integer,"percent":integer,"mediaId":string}}
bufferingStart| yes | String | JSON string in the form of {"bufferingStart": {"mediaId":string}}
bufferingEnd  | yes | String | JSON string in the form of {"bufferingEnd"  : {"mediaId":string}}
videoFrame    | yes | String | JSON string in the form of {"videoFrame"    : {"valid":boolean,"mediaId":string}}
sourceInfo    | yes | String | see sourceInfo JSON object below
streamingInfo | yes | String | see streamingInfo JSON object below
videoInfo     | yes | String | see videoInfo JSON object below
audioInfo     | yes | String | see audioInfo JSON object below
error     	| yes | String | see error JSON object below

@par sourceInfo JSON object
@code
 {"sourceInfo" :
   {
    "container"          : string,
    "numPrograms"        : integer,
     "seekable"           : boolean,
     "trickable"           : boolean,
    "programInfo"
     [{
         "duration"           : number, // in milli-seconds
         "numAudioTracks"     : integer,
         "audioTrackInfo"     :
          [{
              "language"      : string,
              "codec"         : string,
              "profile"       : string,
              "level"         : string,
              "bitRate"       : integer,
              "sampleRate"    : number,
              "channels"      : number,
              "audioType"     : integer
          }],
          "numVideoTracks"    : integer,
          "videoTrackInfo"    :
           [{
              "angleNumber"   : integer,
              "codec"         : string,
              "profile"       : string,
              "level"         : number,
              "width"         : integer,
              "height"        : integer,
              "aspectRatio"   : string,
              "frameRate"     : number,
              "bitRate"       : integer
              "progressive" : boolean
          }],
         "numSubtitleTracks"  : integer,
         "subtitleType"       : string, // undefined / dvb / jcap / ass / dxsa / dxsb / text_plain
         "subtitleTrackInfo"  :
          [{
              "language"          : string,
              "pid"               : integer,
              "ctag"              : integer,
              "type"              : integer, // not used currently
              "compositionPageId" : integer, // not used currently
              "ancilaryPageId"    : integer, // not used currently
              "hearingImpared"    : boolean  // not used currently
         }],
     }],
    "mediaId"                 : string
   }
 }
@endcode

@par streamingInfo JSON object
@code
 {"streamingInfo" :
   {
    "instantBitrate" : integer,
    "totalBitrate"   : integer,
    "mediaId"        : string
   }
 }
@endcode

@par videoInfo JSON object
@code
{"videoInfo" :
   {
    "width"            : integer,
    "height"           : integer,
    "aspectRatio"      : string,
    "pixelAspectRatio" : string,
    "frameRate"        : number,
    "bitRate"          : integer,
    "mode3D"           : string,
    "actual3D"         : string,
    "scanType"         : string,
    "SEI"              :
     [{
        "transferCharacteristics"     : integer,
        "colorPrimaries"              : integer,
        "matrixCoeffs"                : integer,
        "displayPrimariesX0"          : integer,
        "displayPrimariesX1"          : integer,
        "displayPrimariesX2"          : integer,
        "displayPrimariesY0"          : integer,
        "displayPrimariesY1"          : integer,
        "displayPrimariesY2"          : integer,
        "whitePointX"                 : integer,
        "whitePointY"                 : integer,
        "minDisplayMasteringLuminance : integer,
        "maxDixplayMasteringLuminance : integer
     }],
	 "VUI"              :
	 [{
        "transferCharacteristics"     : integer,
        "colorPrimaries"              : integer,
        "matrixCoeffs"                : integer
     }],
    "mediaId"      : string
   }
 }
@endcode

@par audioInfo JSON object
@code
 {"audioInfo" :
   {
    "sampleRate"   : number,
    "channels"     : number,
    "mediaId"      : integer
   }
 }
@endcode

@par error JSON object below
@code
{"error" :
   {
    "errorCode" : integer,
    "errorText" : string,
    "mediaId"   : string
   }
 }
@endcode
@}
 */
//->End of API documentation comment block
bool uMediaserver::stateChangeCommand(UMSConnectorHandle* sender, UMSConnectorMessage* message, void* ctxt)
{
	JDomParser parser;
	string cmd = connector->getMessageText(message);

	if (!parser.parse(cmd, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", cmd.c_str());
		return false;
	}

	JValue parsed = parser.getDom();

	RETURN_IF(!(parsed.hasKey("mediaId") && parsed["mediaId"].isString()), false, MSGERR_NO_MEDIA_ID, "mediaId must be specified");

	string connection_id = parsed["mediaId"].asString();

	LOG_TRACE(log, "stateChangeEvent. cmd=%s,connection_id=%s",
			cmd.c_str(), connection_id.c_str());

	bool rv = false;
	if (connector->addSubscriber(sender, message, connection_id)) {
		auto connection = rm->findConnection(connection_id);
		if (connection && !connection->is_managed) {
			rv = true;
			LOG_DEBUG(log, "Unmanaged client connected: stateChangeEvent. cmd=%s,connection_id=%s",
					cmd.c_str(), connection_id.c_str());
		} else {
			rv = pm->stateChange(connection_id, true);
		}
	}

	string retObject = createRetObject(rv, connection_id);
	connector->sendResponseObject(sender,message,retObject);
	return true;
}

//->Start of API documentation comment block
/**
@page com_webos_media com.webos.media
@{
@section com_webos_media_unsubscribe unsubscribe

stop subscription events from a media pipeline.

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
mediaId  | yes | String  | media id assigned to this media.

@par Returns(Call)
Name | Required | Type | Description
-----|--------|------|----------
returnValue | yes | Boolean | true if successful, false otherwise.
errorCode   | no  | Integer | errorCode only if returnValue is false.
errorText   | no  | String  | errorText only if returnValue is false.
mediaId     | yes | String  | media id assigned to this media.

@par Returns(Subscription)
None
@}
 */
//->End of API documentation comment block
bool uMediaserver::unsubscribeCommand(UMSConnectorHandle* sender, UMSConnectorMessage* message, void* ctxt)
{
	JDomParser parser;
	string cmd = connector->getMessageText(message);

	if (!parser.parse(cmd, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", cmd.c_str());
		return false;
	}

	JValue parsed = parser.getDom();

	RETURN_IF(!parsed.hasKey("mediaId"), false, MSGERR_NO_MEDIA_ID, "mediaId must be specified");

	string connection_id = parsed["mediaId"].asString();

	LOG_TRACE(log, "unSubscribeEvent. cmd=%s,connection_id=%s",
			cmd.c_str(), connection_id.c_str());

	bool rv = false;
	if (connector->removeSubscriber(sender, message, connection_id)) {
		auto connection = rm->findConnection(connection_id);
		if (connection && connection->is_managed) {
			rv = pm->stateChange(connection_id, false);
		}
	}

	string retObject = createRetObject(rv, connection_id);
	connector->sendResponseObject(sender,message,retObject);
	return true;
}

//->Start of API documentation comment block
/**
@page com_webos_media com.webos.media
@{
@section com_webos_media_setPlayRate setPlayRate

Change play rate.

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
mediaId     | yes | String  | media id assigned to this media.
playRate    | yes | Integer | rate for playback.
audioOutput | yes | Boolean | determine to mute audio

@par Returns(Call)
Name | Required | Type | Description
-----|--------|------|----------
returnValue | yes | Boolean | true if successful, false otherwise.
errorCode   | no  | Integer | errorCode only if returnValue is false.
errorText   | no  | String  | errorText only if returnValue is false.
mediaId     | yes | String  | media id assigned to this media.

@par Returns(Subscription)
None
@}
 */
//->End of API documentation comment block
bool uMediaserver::setPlayRateCommand(UMSConnectorHandle* sender, UMSConnectorMessage* message, void* ctx)
{
	JDomParser parser;

	string cmd = connector->getMessageText(message);

	// TODO remove debug statement
	LOG_DEBUG(log, "%s ", cmd.c_str());

	if (!parser.parse(cmd, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", cmd.c_str());
		return false;
	}

	JValue parsed = parser.getDom();
	RETURN_IF(!parsed.hasKey("mediaId"), false, MSGERR_NO_MEDIA_ID, "mediaId must be specified");
	RETURN_IF(!parsed.hasKey("playRate"), false, MSGERR_NO_MEDIA_RATE, "client must specify playback rate");

	string connection_id = parsed["mediaId"].asString();
	double rate;
	parsed["playRate"].asNumber(rate);

	LOG_TRACE(log, "cmd=%s,connection_id=%s", cmd.c_str(), connection_id.c_str());

	bool rv = pm->setPlayRate(connection_id,rate,true);
	string retObject = createRetObject(rv, connection_id);
	connector->sendResponseObject(sender,message,retObject);
	return true;
}

bool uMediaserver::pipelineCmdEventSetMaster(UMSConnectorHandle* handle, UMSConnectorMessage* message, void* ctx)
{
	uMediaserver *self = static_cast<uMediaserver *>(ctx);
	const char *receivedMsg = self->connector->getMessageText(message);
	LOG_DEBUG(self->log, "pipelineCmdEvent command : %s", receivedMsg);

	// reply back the as-is msg to the caller.
	string retJsonString = receivedMsg;
	LOG_TRACE(self->log, "createRetObject retObjectString =  %s", retJsonString.c_str());
	self->connector->sendResponseObject(self->senderForSetMaster, self->messageForSetMaster, retJsonString);

	UMSConnector::unrefMessage(self->messageForSetMaster);
	self->senderForSetMaster = nullptr;
	self->messageForSetMaster = nullptr;
	return true;
}

//->Start of API documentation comment block
/**
@page com_webos_media com.webos.media
@{
@section com_webos_media_setVolume setVolume

control input gain

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
mediaId  | yes | String  | media id assigned to this media.
volume   | yes | Integer | value of input gain
ease     | no  | string  | JSON object containing pipeline specific parameters

@par Returns(Call)
Name | Required | Type | Description
-----|--------|------|----------
returnValue | yes | Boolean | true if successful, false otherwise.
errorCode   | no  | Integer | errorCode only if returnValue is false.
errorText   | no  | String  | errorText only if returnValue is false.
mediaId     | yes | String  | media id assigned to this media.

@par Returns(Subscription)
None
@}
 */
//->End of API documentation comment block
bool uMediaserver::setVolumeCommand(UMSConnectorHandle* sender, UMSConnectorMessage* message, void* ctx)
{
	JDomParser parser;

	string cmd = connector->getMessageText(message);

	if (!parser.parse(cmd, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", cmd.c_str());
		return false;
	}

	JValue parsed = parser.getDom();
	RETURN_IF(!parsed.hasKey("mediaId"), false, MSGERR_NO_MEDIA_ID, "mediaId must be specified");
	RETURN_IF(!parsed.hasKey("volume"), false, MSGERR_NO_VOLUME, "client must specify volume");

	string connection_id = parsed["mediaId"].asString();
	int32_t volume;
	int32_t easeDuration = 0;
	string easeType;
	JValue param = parsed["volume"];
	RETURN_IF(CONV_OK != param.asNumber<int32_t>(volume), false, MSGERR_NO_VOLUME, "client must specify volume");

	if (parsed.hasKey("ease")) {
		JValue param = parsed["ease"]["duration"];
		if (!param.isNull()) {
			param.asNumber(easeDuration);
		}
		easeType = parsed["ease"]["type"].asString();
	}

	LOG_TRACE(log, "cmd=%s,connection_id=%s", cmd.c_str(), connection_id.c_str());

	bool rv = pm->setVolume(connection_id, volume, easeDuration, string_to_ease_type(easeType.c_str()));
	string retObject = createRetObject(rv, connection_id);
	connector->sendResponseObject(sender,message,retObject);
	return true;
}

// @f getActivePipelinesCommand
// @brief get the json string representing the running pipelines and its resources
//
//
bool uMediaserver::getActivePipelinesCommand(UMSConnectorHandle* sender,
		UMSConnectorMessage* message, void*)
{
	const resource_manager_connection_map_t& connections = rm->getConnections();

	if (connections.empty()) {
		LOG_DEBUG(log, "active_pipelines: EMPTY\n");
		string retString = createRetObject(true, "<anonymous>", "pipeline empty");
		connector->sendResponseObject(sender,message,retString);
		return true;
	}

	JValue retObject = Array();

	LOG_DEBUG(log,"============ Active Pipelines =======");
	for (auto i = connections.begin(); i != connections.end(); ++i) {
		JValue pipeline_obj = Object();
		JValue resources_array = Array();

		for (auto j : i->second.resources) {
			JValue resource_obj = Object();
			resource_obj.put("resource", j.id);
			resource_obj.put("index", (int)j.index);
			resources_array.append(resource_obj);
		}
		pipeline_obj.put("resources", resources_array);
		pipeline_obj.put("type", i->second.type.c_str());
		pipeline_obj.put("id", i->second.connection_id.c_str());
		pipeline_obj.put("is_managed", JValue((bool)i->second.is_managed));
		pipeline_obj.put("policy_state", JValue((int)i->second.policy_state));
		pipeline_obj.put("is_foreground", JValue((bool)i->second.is_foreground));
		pipeline_obj.put("is_focus", JValue((bool)i->second.is_focus));
		pipeline_obj.put("timestamp", JValue((int64_t)i->second.timestamp));

		LOG_DEBUG(log,"+");
		LOG_DEBUG(log,"\tid = %s", i->second.connection_id.c_str());
		LOG_DEBUG(log,"\tis_managed = %d", i->second.is_managed);
		LOG_DEBUG(log,"\tpolicy_state = %d", i->second.policy_state);
		LOG_DEBUG(log,"\tis_foreground = %d", i->second.is_foreground);

		// add more information for managed pipeline
		string active_pipeline_out;
		if (pm->getActivePipeline(i->second.connection_id, active_pipeline_out)) {
			JDomParser parser;
			if (!parser.parse(active_pipeline_out,  pbnjson::JSchema::AllSchema())) {
				LOG_ERROR(log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. string=%s ", active_pipeline_out.c_str());
				return false;
			}

			JValue parsed = parser.getDom();
			if (parsed.isObject()) {
				pipeline_obj.put("uri", parsed["uri"].asString());
				pipeline_obj.put("pid", parsed["pid"]);
				pipeline_obj.put("processState", parsed["processState"].asString());
				pipeline_obj.put("mediaState", parsed["mediaState"].asString());
				pipeline_obj.put("appId", parsed["appId"].asString());
			}
		}

		// gather mdc info
		auto mdc_info = mdc_->getMediaElementState(i->second.connection_id.c_str());
		std::pair<std::string, std::string> sink_name = mdc_->getConnectedSinkname(i->second.connection_id.c_str());
		if (mdc_info) {
			JValue mdc_states = Array();
			for (const auto & state : mdc_info.states) {
				mdc_states << state;
			};
			JValue mdc_connections = Array();
			if (mdc_info.connections.first >= 0)
				mdc_connections << sink_name.first;
			if (mdc_info.connections.second >= 0)
				mdc_connections << sink_name.second;
			JValue mdc_obj = JObject{{"states", mdc_states}, {"connections", mdc_connections}};
			pipeline_obj.put("mdc", mdc_obj);
		}

		retObject << pipeline_obj;
	}

	string payload = JGenerator::serialize(retObject,  pbnjson::JSchema::AllSchema());
	LOG_DEBUG(log,"payload = %s", payload.c_str());

	connector->sendResponseObject(sender,message,payload);
	return true;
}



// -------------------------------------
// ResourceManager API (luna)
//


//->Start of API documentation comment block
/**
@page com_webos_media com.webos.media
@{
@section com_webos_media_registerPipeline registerPipeline

Register Pipeline Resource Manager.
Register with Resource Manager. Session is persistent across all start/end transaction and acquire/release cycles.
Registered clients and their current resource requirements will be tracked by Resource Manager.
Param type as specified in Resource Manager configuration file pipeline settings

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
type | yes | String  | connection type.

@par Returns(Call)
Name | Required | Type | Description
-----|--------|------|----------
connectionId | yes | String  | connection id for this connection.

@par Returns(Subscription)
None
@}
 */
//->End of API documentation comment block
bool uMediaserver::registerPipelineCommand(UMSConnectorHandle* sender,
		UMSConnectorMessage* message,
		void* ctx)
{
	JDomParser parser;

	string cmd = connector->getMessageText(message);
	if (!parser.parse(cmd, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", cmd.c_str());
		return false;
	}

	JValue parsed = parser.getDom();
	RETURN_IF(!parsed.hasKey("type"), false, MSGERR_NO_CONN_TYPE, "Connection type must be specified");
	string type = parsed["type"].asString();

	string connection_id = GenerateUniqueID()();
	bool rv = rm->registerPipeline(connection_id, type);
	if( rv == false ) {
		// TODO: do we need to send response or luna will do it for us?
		connector->sendSimpleResponse(sender,message,rv);
		return false;
	}

	const char * service = connector->getSenderServiceName(message);
	if( service == NULL ) {
		LOG_ERROR(log, MSGERR_NO_SVC_NAME,
				"Resource Manager connections must specify a service name.");
		rm->unregisterPipeline(connection_id);
		connector->sendSimpleResponse(sender,message,rv);
		return false;
	}

	string service_name = service;
	rm->setServiceName(connection_id,service_name);

	LOG_DEBUG(log, "connection_id=%s, type = %s, service_name=%s",
			connection_id.c_str(), type.c_str(), service);

	connector->addClientWatcher(sender, message, [this, connection_id] {
		LOG_DEBUG(log, "RM Client disconnected. Unregister(%s).",
				connection_id.c_str());
		rm->unregisterPipeline(connection_id);
        mdc_->unregisterMedia(connection_id);
	});

	connector->sendResponse(sender,message,"connectionId", connection_id);
	rm->notifyActivity(connection_id);

	return true;
}

//->Start of API documentation comment block
/**
@page com_webos_media com.webos.media
@{
@section com_webos_media_unregisterPipeline unregisterPipeline

unregister Pipeline with Resource Manager.

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
connectionId | yes | String  | connection id for this connection.

@par Returns(Call)
Name | Required | Type | Description
-----|--------|------|----------
returnValue | yes | Boolean | true if successful, false otherwise.

@par Returns(Subscription)
None
@}
 */
//->End of API documentation comment block
bool uMediaserver::unregisterPipelineCommand(UMSConnectorHandle* sender,
		UMSConnectorMessage* message,
		void* ctx)
{
	JDomParser parser;

	string cmd = connector->getMessageText(message);
	if (!parser.parse(cmd, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", cmd.c_str());
		return false;
	}

	JValue parsed = parser.getDom();
	RETURN_IF(!(parsed.hasKey("connectionId") && parsed["connectionId"].isString()), false,
			MSGERR_NO_CONN_ID, "connectionId must be specified");
	string connection_id = parsed["connectionId"].asString();

	LOG_TRACE(log, "connection_id=%s", connection_id.c_str());

	connector->delClientWatcher(sender, message);

	string retObject = createRetObject(true, connection_id);
	connector->sendResponseObject(sender,message,retObject);

	return rm->unregisterPipeline(connection_id);
}

//->Start of API documentation comment block
/**
@page com_webos_media com.webos.media
@{
@section com_webos_media_acquire acquire

Acquire resources.

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
connectionId | yes | String  | connection id for this connection.
resources    | yes | String  | resource list to be allocated.

@par Returns(Call)
Name | Required | Type | Description
-----|--------|------|----------
returnValue | yes | Boolean | true if successful, false otherwise.

@par Returns(Subscription)
None
@}
 */
//->End of API documentation comment block
bool uMediaserver::acquireCommand(UMSConnectorHandle* sender,
		UMSConnectorMessage* message,
		void* ctx)
{
	JDomParser parser;
	pbnjson::JValue msg = pbnjson::Object();
	pbnjson::JValue payload = pbnjson::Object();

	string cmd = connector->getMessageText(message);
	if (!parser.parse(cmd, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ",cmd.c_str());
		return false;
	}

	const char * service = connector->getSenderServiceName(message);
	RETURN_IF(service == NULL, false, MSGERR_NO_RESOURCES,"Unable to obtain service name.");

	JValue parsed = parser.getDom();
	RETURN_IF(!parsed.hasKey("connectionId"),
			false, MSGERR_NO_CONN_ID, "connectionId must be specified");
	string connection_id = parsed["connectionId"].asString();

	RETURN_IF(!parsed.hasKey("resources"),
			false, MSGERR_NO_RESOURCES, "resources must be specified");

	string resource_request = parsed["resources"].asString();

	// TODO: optimize to avoid double lookup - later we'll do same map
	// lookup with rm->findConnection(...)
	rm->setServiceName(connection_id,service);

	// get connection information
	auto connection = rm->findConnection(connection_id);
	RETURN_IF(nullptr == connection, false, MSGERR_CONN_FIND, "Invalid connection");

	// enqueue resource request
	acquire_queue.enqueueRequest(connection_id, connection->service_name, resource_request);

	connector->sendSimpleResponse(sender,message,true);

	return true;
}

void uMediaserver::initAcquireQueue() {
	// assign resource manager pointer
	acquire_queue.setResourceManager(rm);

	// set policy action calback
	acquire_queue.setPolicyActionCallback([this] (const std::string & connection_id,
										  const std::string & candidate_id,
										  const resource_request_t & failed_resources)->bool {
		std::string failed_resources_encoded;
		if (!rm->encodeResourceRequest(failed_resources, failed_resources_encoded)) {
			LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "failed to serialize request");
			return false;
		}

		auto candidate_connection = rm->findConnection(candidate_id);
		if (nullptr == candidate_connection) {
			LOG_ERROR(log, MSGERR_PIPELINE_FIND, "policy action target not found");
			return false;
		}

		LOG_INFO(log, MSGNFO_POLICY_REQUEST, "+ Invoking policy on %s, service_name=%s",
				candidate_id.c_str(), candidate_connection->service_name.c_str());

		// suspend managed pipeline
		if (candidate_connection->is_managed) {
			// TODO: handle failed case
			pm->suspend(candidate_id);
			// TODO: are we expecting policy action denial from managed pipelines?
			mdc_->contentReady(candidate_id, false);
			//Need to inform the mdc when pipeline suspended beacuse of policy action
			mdc_->updatePipelineState(candidate_id, PLAYBACK_STOPPED);
		}

		auto rm_connection = rm->findConnection(connection_id);
		if (nullptr == rm_connection) {
			LOG_ERROR(log, MSGERR_PIPELINE_FIND, "policy action target not found");
			return false;
		}

		// send policy action against unmanaged pipeline
		pbnjson::JValue msg = pbnjson::Object();
		pbnjson::JValue payload = pbnjson::Object();

		payload.put("action","release");

		// tell candidate request verbose name for UI/UX informational purposes
		auto & sql = *Registry::instance()->dbi();
		std::string pipeline_name("unknown");
		try {
			// TODO: registry interface for partial requests
			sql << "select name from pipelines where type=?;",
					DBI::from(rm_connection->type), DBI::into(pipeline_name);
		} catch(...) {}
		payload.put("requestor_type", rm_connection->type);
		payload.put("requestor_name", pipeline_name);
		payload.put("resources", failed_resources_encoded);
		payload.put("connectionId", candidate_id);

		// add payload to policyAction message
		msg.put("policyAction", payload);

		pbnjson::JGenerator serializer(NULL);
		std::string serialized_request;

		if (!serializer.toString(msg, pbnjson::JSchema::AllSchema(), serialized_request)) {
			LOG_ERROR(log, MSGERR_JSON_SERIALIZE, "json serialization failed");
			return false;
		}

		// inform candidates of the bad news
		LOG_INFO(log, MSGNFO_POLICY_REQUEST, "inform policy candidate. %s", candidate_id.c_str());
		std::string cmd = candidate_connection->service_name + "/policyAction";
		// handle policy action response
		return connector->sendMessage(cmd, serialized_request,
					uMediaserver::policyResponseCallback, NULL);
	});

	// set acquire result callback
	acquire_queue.setAcquireResultCallback([this] (const std::string & service_name,
										   const std::string & response)->bool {
		std::string cmd = service_name + "/acquireComplete";
		return connector->sendMessage(cmd, response, NULL, NULL);
	});
}

bool uMediaserver::policyResponseCallback(UMSConnectorHandle * sender,
		UMSConnectorMessage * message, void *) {
	uMediaserver * server = uMediaserver::instance();

	std::string response = server->connector->getMessageText(message);
	JDomParser parser;
	if (!parser.parse(response,  pbnjson::JSchema::AllSchema())) {
		LOG_WARNING(server->log, MSGERR_JSON_PARSE,
				"json parsing failed : raw = %s", response.c_str());
		return false;
	}
	const JValue & dom = parser.getDom();
	std::string candidate_id;
	if ( ! (dom.hasKey("mediaId") && CONV_OK == dom["mediaId"].asString(candidate_id)) ) {
		LOG_WARNING(server->log, MSGERR_JSON_SCHEMA,
				"json schema validation failed : raw = %s", response.c_str());
		return false;
	}
	bool result;
	if ( ! (dom.hasKey("returnValue") && CONV_OK == dom["returnValue"].asBool(result)) ) {
		LOG_WARNING(server->log, MSGERR_JSON_SCHEMA,
				"json schema validation failed : raw = %s", response.c_str());
		return false;
	}

	server->acquire_queue.policyActionResult(candidate_id, result);

	return true;
}

//->Start of API documentation comment block
/**
@page com_webos_media com.webos.media
@{
@section com_webos_media_acquire tryAcquire

Acquire resources.

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
connectionId | yes | String  | connection id for this connection.
resources    | yes | String  | resource list to be allocated.

@par Returns(Call)
Name | Required | Type | Description
-----|--------|------|----------
returnValue | yes | Boolean | true if successful, false otherwise.

@par Returns(Subscription)
None
@}
 */
//->End of API documentation comment block
bool uMediaserver::tryAcquireCommand(UMSConnectorHandle* sender,
		UMSConnectorMessage* message,
		void* ctx)
{
	JDomParser parser;
	pbnjson::JValue msg = pbnjson::Object();
	pbnjson::JValue payload = pbnjson::Object();

	string cmd = connector->getMessageText(message);
	if (!parser.parse(cmd, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ",cmd.c_str());
		return false;
	}

	const char * service = connector->getSenderServiceName(message);
	RETURN_IF(service == NULL, false, MSGERR_NO_RESOURCES,"Unable to obtain service name.");

	JValue parsed = parser.getDom();
	RETURN_IF(!parsed.hasKey("connectionId"),
			false, MSGERR_NO_CONN_ID, "connectionId must be specified");
	string connection_id = parsed["connectionId"].asString();

	RETURN_IF(!parsed.hasKey("resources"),
			false, MSGERR_NO_RESOURCES, "resources must be specified");

	string resource_request = parsed["resources"].asString();

	// TODO: optimize to avoid double lookup - later we'll do same map
	// lookup with rm->findConnection(...)
	rm->setServiceName(connection_id,service);

	// get connection information
	auto connection = rm->findConnection(connection_id);
	RETURN_IF(nullptr == connection, false, MSGERR_CONN_FIND, "Invalid connection");

	// enqueue resource request
	acquire_queue.enqueueRequest(connection_id, connection->service_name, resource_request, false);

	connector->sendSimpleResponse(sender,message,true);

	return true;
}

//->Start of API documentation comment block
/**
@page com_webos_media com.webos.media
@{
@section com_webos_media_release release

Acquire resources.

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
connectionId | yes | String  | connection id for this connection.
resources    | yes | String  | resource list to be released.

@par Returns(Call)
Name | Required | Type | Description
-----|--------|------|----------
returnValue | yes | Boolean | true if successful, false otherwise.

@par Returns(Subscription)
None
@}
 */
//->End of API documentation comment block
bool uMediaserver::releaseCommand(UMSConnectorHandle* sender,
		UMSConnectorMessage* message,
		void* ctx)
{
	JDomParser parser;

	string cmd = connector->getMessageText(message);
	if (!parser.parse(cmd, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ",cmd.c_str());
		return false;
	}

	JValue parsed = parser.getDom();
	RETURN_IF(!parsed.hasKey("connectionId"), false, MSGERR_NO_CONN_ID,
			"connectionId must be specified");
	string connection_id = parsed["connectionId"].asString();

	RETURN_IF(!parsed.hasKey("resources"), false, MSGERR_NO_RESOURCES,
			"resources must be specified");
	string resources = parsed["resources"].asString();

	bool ret = true;
	ret = rm->release(connection_id, resources);

	connector->sendSimpleResponse(sender,message,ret);

	return true;
}

//->Start of API documentation comment block
/**
@page com_webos_media com.webos.media
@{
@section com_webos_media_notifyForeground notifyForeground

Notify of resource manager client is in foreground and may not
be selected for policy action

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
connectionId | yes | String  | connection id for this connection.

@par Returns(Call)
Name | Required | Type | Description
-----|--------|------|----------
returnValue | yes | Boolean | true if successful, false otherwise.

@par Returns(Subscription)
None
@}
 */
//->End of API documentation comment block
bool uMediaserver::notifyForegroundCommand(UMSConnectorHandle* sender,
		UMSConnectorMessage* message,
		void* ctx)
{
	JDomParser parser;
	pbnjson::JValue msg = pbnjson::Object();
	pbnjson::JValue payload = pbnjson::Object();

	string cmd = connector->getMessageText(message);
	if (!parser.parse(cmd, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ",cmd.c_str());
		return false;
	}

	JValue parsed = parser.getDom();
	RETURN_IF(!parsed.hasKey("connectionId"),
			false, MSGERR_NO_CONN_ID, "connectionId must be specified");
	string connection_id = parsed["connectionId"].asString();

	bool rv = false;
	// for mdc managed clients we shouldn't update rm directly
	if (mdc_->getMediaElementState(connection_id)) {
		mdc_->inAppForegroundEvent(connection_id, true);
		rv = true;
	} else {
		rv = rm->notifyForeground(connection_id);
		if ( rv == false ) {
			LOG_ERROR(log, MSGERR_NO_CONN_ID,
					"Resource Manager: connection_id=%s not found",
					connection_id.c_str());
		}
	}

	connector->sendSimpleResponse(sender, message, rv);
	return true;
}

//->Start of API documentation comment block
/**
@page com_webos_media com.webos.media
@{
@section com_webos_media_notifyBackground notifyBackground

Notify of resource manager client is in background and may
be selected for policy action

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
connectionId | yes | String  | connection id for this connection.

@par Returns(Call)
Name | Required | Type | Description
-----|--------|------|----------
returnValue | yes | Boolean | true if successful, false otherwise.

@par Returns(Subscription)
None
@}
 */
//->End of API documentation comment block
bool uMediaserver::notifyBackgroundCommand(UMSConnectorHandle* sender,
		UMSConnectorMessage* message,
		void* ctx)
{
	JDomParser parser;
	pbnjson::JValue msg = pbnjson::Object();
	pbnjson::JValue payload = pbnjson::Object();

	string cmd = connector->getMessageText(message);
	if (!parser.parse(cmd, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ",cmd.c_str());
		return false;
	}

	JValue parsed = parser.getDom();
	RETURN_IF(!parsed.hasKey("connectionId"),
			false, MSGERR_NO_CONN_ID, "connectionId must be specified");
	string connection_id = parsed["connectionId"].asString();

	bool rv = rm->notifyBackground(connection_id);
	if ( rv == false ) {
		LOG_ERROR(log, MSGERR_NO_CONN_ID,
				"Resource Manager: connection_id=%s not found",
				connection_id.c_str());
	}

	mdc_->inAppForegroundEvent(connection_id, false);

	connector->sendSimpleResponse(sender, message, rv);
	return true;
}

//->Start of API documentation comment block
/**
@page com_webos_media com.webos.media
@{
@section com_webos_media_notifyActivity notifyActivity

update Resource Manager connection activity time stamp

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------
connectionId | yes | String  | connection id for this connection.

@par Returns(Call)
Name | Required | Type | Description
-----|--------|------|----------
returnValue | yes | Boolean | true if successful, false otherwise.

@par Returns(Subscription)
None
@}
 */
//->End of API documentation comment block
bool uMediaserver::notifyActivityCommand(UMSConnectorHandle* sender,
		UMSConnectorMessage* message,
		void* ctx)
{
	JDomParser parser;
	pbnjson::JValue msg = pbnjson::Object();
	pbnjson::JValue payload = pbnjson::Object();

	string cmd = connector->getMessageText(message);
	if (!parser.parse(cmd, pbnjson::JSchema::AllSchema())) {
		LOG_ERROR(log, MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ",cmd.c_str());
		return false;
	}

	JValue parsed = parser.getDom();
	RETURN_IF(!parsed.hasKey("connectionId"),
			false, MSGERR_NO_CONN_ID, "connectionId must be specified");
	string connection_id = parsed["connectionId"].asString();

	bool rv = rm->notifyActivity(connection_id);
	if ( rv == false ) {
		LOG_ERROR(log, MSGERR_NO_CONN_ID,
				"Resource Manager: connection_id=%s not found",
				connection_id.c_str());
	}

	connector->sendSimpleResponse(sender, message, rv);
	return true;
}

//->Start of API documentation comment block
/**
@page com_webos_media com.webos.media
@{
@section com_webos_media_trackAppProcesses trackAppProcesses

notify subscriber of pipeline pid <=> appId

@par Parameters
Name | Required | Type | Description
-----|--------|------|----------

@par Returns(Call)
Name | Required | Type | Description
-----|--------|------|----------
returnValue | yes | Boolean | true if successful, false otherwise.

@par Returns(Subscription)
Name | Required | Type | Description
-----|--------|------|----------
procUpdate | yes | string | JSON string in the form of {"appId":string, "pid":int, "exec":bool}
@}
 */
//->End of API documentation comment block
bool uMediaserver::trackAppProcessesCommand(UMSConnectorHandle* sender,
		UMSConnectorMessage* message,
		void* ctx)
{
	bool rv = false;
	JValue retObject = Object();
	JValue pipelineArray;

	if (connector->addSubscriber(sender, message, "trackAppProcesses")) {
		rv = true;
	}

	pm->getActivePipelines(pipelineArray, true);
	retObject.put("subscribed", rv);
	retObject.put("mediaPipelines", pipelineArray);
	retObject.put("returnValue", rv);

	string payload = JGenerator::serialize(retObject,  pbnjson::JSchema::AllSchema());
	LOG_DEBUG(log,"payload = %s", payload.c_str());

	connector->sendResponseObject(sender,message,payload);
	return true;
}

// @f registerMedia
// @b register new umnanaged media element
//
//  command:
//   {"mediaId":"<MID>", "appId":"<appId>"}
//
//  responses:
//  success = {"returnValue": true,"context : "pipeline_1"}
//  failure = {
//     "returnValue": false,
//     "errorCode": "DISPLAY_ERROR_0000",
//     "errorText": "Invalid Context",
//     "context": "pipeline_1"
//  }
//
bool uMediaserver::registerMediaCommand(UMSConnectorHandle* handle, UMSConnectorMessage* message, void*) {

	string cmd = connector->getMessageText(message);
	LOG_DEBUG(log, "cmd = %s", cmd.c_str());

	JDomParser parser;
	RETURN_IF(!parser.parse(cmd,  pbnjson::JSchema::AllSchema()), false,
			  MSGERR_JSON_PARSE, "ERROR JDomParser.parse. raw=%s ", cmd.c_str());

	JValue parsed = parser.getDom();
	RETURN_IF(!parsed.hasKey("mediaId"), false, MSGERR_NO_MEDIA_ID, "mediaId must be specified");
	RETURN_IF(!parsed.hasKey("appId"), false, MSGERR_NO_APP_ID, "appId must be specified");

	string media_id = parsed["mediaId"].asString();
	string app_id = parsed["appId"].asString();

	auto result = mdc_->registerMedia(media_id, app_id);

	// MDC and RM registrations aren't atomic so we should force resource update to avoid races
	if (result)
		rm->setAcquireCallback(acquire_callback_);

	connector->sendSimpleResponse(handle, message, result);
	return true;
}

// -------------------------------------
// General Utilities
//

// @f createRetObject
// @brief create return object
//
// @format
// {
//   "returnValue":<string [true/false]>,
//   "errorCode":<number>,
//   "errorText":<string>,
//   "mediaId":<string[mediaId]>
// }
//
string uMediaserver::createRetObject(bool returnValue,
		const string& mediaId,
		const int& errorCode,
		const std::string& errorText)
{
	JValue retObject = Object();
	JGenerator serializer(NULL);
	string retJsonString;

	retObject.put("returnValue", returnValue);
	retObject.put("errorCode", errorCode);
	retObject.put("errorText", errorText);
	retObject.put("mediaId", mediaId);
	serializer.toString(retObject,  pbnjson::JSchema::AllSchema(), retJsonString);

	LOG_TRACE(log, "createRetObject retObjectString =  %s", retJsonString.c_str());
	return retJsonString;
}

// @f createRetObject
// @brief create return object
//
// @note pass in custom json return object
//
string uMediaserver::createRetObject(bool returnValue, const string& mediaId, const string& returnJSONString)
{
	JValue retObject = Object();
	JGenerator serializer(NULL);
	string retObjectString;

	retObject.put("returnValue", returnValue);
	retObject.put("errorCode", 0);  // no error
	retObject.put("errorText", "No Error"); // no error
	retObject.put("mediaId", mediaId);
	retObject.put("data", returnJSONString);
	serializer.toString(retObject,  pbnjson::JSchema::AllSchema(), retObjectString);

	LOG_TRACE(log, "createRetObject retObjectString =  %s", retObjectString.c_str());
	return retObjectString;
}
