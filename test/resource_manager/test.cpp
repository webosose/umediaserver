#define BOOST_TEST_MODULE ResourceManagerTest
#include <boost/test/included/unit_test.hpp>

#include <pbnjson.hpp>

// public morozov pattern
// http://en.wikipedia.org/wiki/Pavlik_Morozov
#define private public
#define protected public

#include <ResourceManager.h>

#undef private
#undef protected

#include <Logger_macro.h>

using namespace uMediaServer;

const char * test_config = R"__(
		version = "1.0"
		resources = (
			{
				id = "VDEC";
				name = "Digital Video Decoder";
				qty = 2;
			},
			{
				id = "ADEC";
				name = "Digital Audio Decoder";
				qty = 2;
			},
			{
				id = "PRIMARY_SCALER";
				name = "Primary Scaler";
				qty = 1;
			},
			{
				id = "SECONDARY_SCALER";
				name = "Secondary Scaler";
				qty = 1;
			},
			{
				id = "VDEC_BUS";
				name = "Video Decoder Core Bus";
				qty = 8;
			},
			{
				id = "MSVC";
				name = "Multi Standard Video Codec";
				qty = 2;
			},
			{
				id = "VP9";
				name = "VP9 Decoder Core";
				qty = 4;
			},
			{
				type = "MUTUALLY_EXCLUSIVE";
				resources = (
					{
						id = "HEVC";
						name = "High Efficiency Video Codec";
						qty = 8;
					},
					{
						id = "G1";
						name = "G1 Decoder Core";
						qty = 4;
					}
				);
			}
		);
		pipelines = (
			{
				type = "file";
				name = "Video playback pipeline.";
				bin  = "/usr/local/webos/usr/sbin/umediapipeline";
				schema_file = "/usr/local/webos/etc/umediaserver/fileschema.js";
				resources = "VDEC,ADEC";
				priority = 4;
				# select policy candidate on contention
				policy_select = "select pipeline from active_pipelines where (priority < THIS_PRIORITY AND time(LRU)";
				policy_action = "unload";  # this pipeline only supports unload at policy select
			},
			{
				type = "media";
				name = "Video playback pipeline.";
				bin  = "/usr/local/webos/usr/sbin/starfish-media-pipeline";
				resources = "VDEC,ADEC";
				priority = 3;
				# select policy candidate on contention
				policy_select = "priority < ??PRIORITY?? AND time(??LRU??)";
				policy_action = "unload";  # this pipeline only supports unload at policy select
			}
		);
		)__";


Logger _log(UMS_LOG_CONTEXT_RESOURCE_MANAGER);
struct ResourceManagerFixture {
	ResourceManagerFixture() {
		_log.setLogLevel("DEBUG");
		libconfig::Config config;
		config.readString(test_config);
		rm = std::unique_ptr<ResourceManager>(new ResourceManager(config));
	}
	std::unique_ptr<ResourceManager> rm;
};

BOOST_FIXTURE_TEST_CASE(policy, ResourceManagerFixture) {
	LOG_DEBUG(_log, "===================== TEST policy interface : START");

	// register pipelines
	const std::string pipelines[] = { "pipeline#0", "pipeline#1", "pipeline#2" };
	for (size_t i = 0; i < 3; ++i) { BOOST_REQUIRE(rm->registerPipeline(pipelines[i], "file")); }

	// acquire 2 VDECS
	for (size_t i = 0; i < 2; ++i) {
		std::string request = "[{\"resource\":\"VDEC\",\"qty\":1}]";
		std::string response;
		dnf_request_t failures;
		BOOST_REQUIRE(rm->acquire(pipelines[i], request, failures, response));
	}
	// select policy candidate for VDEC[*]=1 - should be pipeline#0, pipeline#1 (by LRU)
	resource_request_t shortage(std::list<resource_descriptor_t>{{"VDEC", 1}});
	std::list<std::string> candidates;
	std::list<std::string> expected_candidates = {pipelines[0]};

	// bump LRU/MRU
	BOOST_CHECK(rm->notifyActivity(pipelines[2]));
	BOOST_CHECK(rm->selectPolicyCandidates(pipelines[2], shortage, candidates));
	BOOST_CHECK_EQUAL_COLLECTIONS(candidates.begin(), candidates.end(),
			expected_candidates.begin(), expected_candidates.end());

	// now set pipeline#1 as not displaying
	// policy action after this will pick pipeline#1 as candidate even though
	// it is higher in the LRU list
	BOOST_CHECK(rm->notifyVisibility(pipelines[1], false));
	shortage.front().index = 0;
	expected_candidates = {pipelines[1]};
	BOOST_CHECK(rm->selectPolicyCandidates(pipelines[2], shortage, candidates));
	BOOST_CHECK_EQUAL_COLLECTIONS(candidates.begin(), candidates.end(),
		expected_candidates.begin(), expected_candidates.end());
	BOOST_CHECK(rm->notifyVisibility(pipelines[1], true));

	// select policy candidate for VDEC[0]=1 - should be pipeline#1
	shortage.front().index = 0;
	expected_candidates = {pipelines[1]};
	BOOST_CHECK(rm->selectPolicyCandidates(pipelines[2], shortage, candidates));
	BOOST_CHECK_EQUAL_COLLECTIONS(candidates.begin(), candidates.end(),
			expected_candidates.begin(), expected_candidates.end());

	// select policy candidate for VDEC[*]=2 - should be {pipeline#0, pipeline#1}
	shortage.front() = {"VDEC", 2};
	expected_candidates = {pipelines[0], pipelines[1]};
	BOOST_CHECK(rm->notifyActivity(pipelines[2]));
	BOOST_CHECK(rm->selectPolicyCandidates(pipelines[2], shortage, candidates));
	BOOST_CHECK_EQUAL_COLLECTIONS(candidates.begin(), candidates.end(),
			expected_candidates.begin(), expected_candidates.end());

	// select policy candidate for VDEC[2]=1 - invalid resource
	shortage.front() = {"VDEC", 1, 2};
	BOOST_CHECK(!rm->selectPolicyCandidates(pipelines[2], shortage, candidates));

	// select policy candidate for ADEC[*]=1 - not allocated resource
	shortage.front() = {"ADEC", 1};
	BOOST_CHECK(!rm->selectPolicyCandidates(pipelines[2], shortage, candidates));
	LOG_DEBUG(_log, "===================== TEST policy: END");
}

BOOST_FIXTURE_TEST_CASE(parse_release_request, ResourceManagerFixture) {
	// happy path
	std::string release_request = R"__([{"resource":"VDEC","index":0}, {"resource":"ADEC","index":1}])__";
	resource_list_t release_list;
	resource_list_t expected_list = {{"VDEC", 0}, {"ADEC", 1}};
	BOOST_CHECK(rm->decodeReleaseRequest(release_request, release_list));
	BOOST_CHECK_EQUAL_COLLECTIONS(release_list.begin(), release_list.end(),
			expected_list.begin(), expected_list.end());
	// junk request
	BOOST_CHECK(!rm->decodeReleaseRequest("some junk", release_list));
	// malformed request (no index)
	release_request = R"__([{"resource":"VDEC","qty":2}, {"resource":"ADEC","index":1}])__";
	BOOST_CHECK(!rm->decodeReleaseRequest(release_request, release_list));
	// malformed request (not an array)
	release_request = R"__({"resource":"ADEC","index":1})__";
	BOOST_CHECK(!rm->decodeReleaseRequest(release_request, release_list));
}

BOOST_FIXTURE_TEST_CASE(parse_acquire_request, ResourceManagerFixture) {
	// legacy request
	std::string acquire_request = R"__([{"resource":"ADEC", "qty":1, "index":0}, {"resource":"VDEC", "qty":4}])__";
	dnf_request_t dnf_request;
	dnf_request_t expected_request{std::list<resource_descriptor_t>{{"ADEC", 1, 0}, {"VDEC", 4}}};
	BOOST_CHECK(rm->decodeAcquireRequest(acquire_request, dnf_request));
	BOOST_CHECK_EQUAL_COLLECTIONS(dnf_request.front().begin(), dnf_request.front().end(),
			expected_request.front().begin(), expected_request.front().end());

	// 3-level request
	// request [(HEVC=1 and BW=1) or (MSVC=1 and BW=4)] and
	//         [(G1=1 and BW=2) or (MSVC=1 and BW=4)]   and
	//         [ADEC=1]
	acquire_request = R"__(
			[
				[
					[{"resource":"HEVC", "qty":1}, {"resource":"VDEC_BUS", "qty":1}],
					[{"resource":"MSVC", "qty":1}, {"resource":"VDEC_BUS", "qty":4}]
				],
				[
					[{"resource":"G1", "qty":1},   {"resource":"VDEC_BUS", "qty":2}],
					[{"resource":"MSVC", "qty":1}, {"resource":"VDEC_BUS", "qty":3}]
				],
				{"resource":"ADEC", "qty":1}
			]
	)__";
	// DNF expanded request
	expected_request = {
		std::list<resource_descriptor_t>{{"HEVC", 1}, {"VDEC_BUS", 1}, {"G1", 1}, {"VDEC_BUS", 2}, {"ADEC", 1}},
		std::list<resource_descriptor_t>{{"MSVC", 1}, {"VDEC_BUS", 4}, {"G1", 1}, {"VDEC_BUS", 2}, {"ADEC", 1}},
		std::list<resource_descriptor_t>{{"HEVC", 1}, {"VDEC_BUS", 1}, {"MSVC", 1}, {"VDEC_BUS", 3}, {"ADEC", 1}},
		std::list<resource_descriptor_t>{{"MSVC", 1}, {"VDEC_BUS", 4}, {"MSVC", 1}, {"VDEC_BUS", 3}, {"ADEC", 1}}
	};
	BOOST_CHECK(rm->decodeAcquireRequest(acquire_request, dnf_request));
	BOOST_REQUIRE_EQUAL(dnf_request.size(), expected_request.size());
	auto dnf_it = dnf_request.begin();
	auto exp_it = expected_request.begin();
	for (; dnf_it != dnf_request.end(); ++dnf_it, ++exp_it) {
		BOOST_CHECK_EQUAL_COLLECTIONS(dnf_it->begin(), dnf_it->end(), exp_it->begin(), exp_it->end());
	}

	// junk request
	BOOST_CHECK(!rm->decodeAcquireRequest("this } is junk", dnf_request));

	// malformed request (not an array)
	BOOST_CHECK(!rm->decodeAcquireRequest("{\"resource\":\"HEVC\", \"qty\":1}", dnf_request));
}

BOOST_FIXTURE_TEST_CASE(resource_pool, ResourceManagerFixture) {
	// check initial state
	std::map<std::string, system_resource_cfg_t> expected_pool = {
			{"VDEC",             {"VDEC",             2, "Digital Video Decoder"                    }},
			{"ADEC",             {"ADEC",             2, "Digital Audio Decoder"                    }},
			{"PRIMARY_SCALER",   {"PRIMARY_SCALER",   1, "Primary Scaler"                           }},
			{"SECONDARY_SCALER", {"SECONDARY_SCALER", 1, "Secondary Scaler"                         }},
			{"VDEC_BUS",         {"VDEC_BUS",         8, "Video Decoder Core Bus"                   }},
			{"MSVC",             {"MSVC",             2, "Multi Standard Video Codec"               }},
			{"VP9",              {"VP9",              4, "VP9 Decoder Core"                         }},
			{"HEVC",             {"HEVC",             8, "High Efficiency Video Codec", {"G1"}      }},
            {"G1",               {"G1",               4, "G1 Decoder Core",             {"HEVC"}    }}
	};
	auto & sr = *rm->system_resources;
	// unfortunately BOOST_CHECK_EQUAL_COLLECTIONS fails to compile with maps
	BOOST_CHECK_EQUAL(sr.pool.size(), expected_pool.size());
	for (const auto & cfg_res : expected_pool) {
		BOOST_CHECK_EQUAL(sr.pool.find(cfg_res.first)->second, cfg_res.second);
	}

	// check acquire (happy path)
	resource_request_t request(std::list<resource_descriptor_t>{{"VDEC", 1}, {"ADEC", 2}, {"PRIMARY_SCALER", 1}});
	resource_request_t failures;
	resource_list_t expected_list = { // {id, index}
			{"VDEC", 1}, {"ADEC", 1}, {"ADEC", 0}, {"PRIMARY_SCALER", 0}
	};

	resource_list_t allocated = sr.acquire(request, failures);
	BOOST_CHECK(failures.empty());
	BOOST_CHECK_EQUAL_COLLECTIONS(allocated.begin(), allocated.end(),
			expected_list.begin(), expected_list.end());
	// resource pool should be updated
	expected_pool.find("VDEC"          )->second.units = 1; // set to 0...0001
	expected_pool.find("ADEC"          )->second.units = 0; // set to 0...0000
	expected_pool.find("PRIMARY_SCALER")->second.units = 0; // set to 0...0000
	for (const auto & cfg_res : expected_pool) {
		BOOST_CHECK_EQUAL(sr.pool.find(cfg_res.first)->second, cfg_res.second);
	}

	// check_acquire (failed allocation)
	request = {{"VDEC", 1}, {"ADEC", 1}, {"SECONDARY_SCALER", 1}};
	resource_request_t expected_failures(std::list<resource_descriptor_t>{{"ADEC", 1}});
	allocated = sr.acquire(request, failures);
	BOOST_CHECK(allocated.empty());
	BOOST_CHECK_EQUAL_COLLECTIONS(failures.begin(), failures.end(),
			expected_failures.begin(), expected_failures.end());
	// resource pool shouldn't be updated
	for (const auto & cfg_res : expected_pool) {
		BOOST_CHECK_EQUAL(sr.pool.find(cfg_res.first)->second, cfg_res.second);
	}

	// check release
	resource_list_t release = {{"VDEC", 1}, {"ADEC", 0}, {"VDEC", 0}};
	resource_list_t expected_releases = {{"VDEC", 1}, {"ADEC", 0}};
	resource_list_t released = sr.release(release);
	BOOST_CHECK_EQUAL_COLLECTIONS(released.begin(), released.end(),
			expected_releases.begin(), expected_releases.end());
	// resource pool should be updated
	expected_pool.find("VDEC"          )->second.units = 3; // set to 0...0011
	expected_pool.find("ADEC"          )->second.units = 1; // set to 0...0001
	for (const auto & cfg_res : expected_pool) {
		BOOST_CHECK_EQUAL(sr.pool.find(cfg_res.first)->second, cfg_res.second);
	}

	// check indexed request
	request = {{"VDEC", 1, 0}, {"ADEC", 1}};
	expected_list = {{"VDEC", 0}, {"ADEC", 0}};
	allocated = sr.acquire(request, failures);
	BOOST_CHECK(failures.empty());
	BOOST_CHECK_EQUAL_COLLECTIONS(allocated.begin(), allocated.end(),
			expected_list.begin(), expected_list.end());
	// resource pool should be updated
	expected_pool.find("VDEC"          )->second.units = 2; // set to 0...0010
	expected_pool.find("ADEC"          )->second.units = 0; // set to 0...0000
	for (const auto & cfg_res : expected_pool) {
		BOOST_CHECK_EQUAL(sr.pool.find(cfg_res.first)->second, cfg_res.second);
	}

	// check request with too many resources
	request = {{"VDEC", 8}};
	expected_failures = {{"VDEC", 7}};
	allocated = sr.acquire(request, failures);
	BOOST_CHECK(allocated.empty());
	BOOST_CHECK_EQUAL_COLLECTIONS(failures.begin(), failures.end(),
			expected_failures.begin(), expected_failures.end());
	// resource pool shouldn't be updated
	for (const auto & cfg_res : expected_pool) {
		BOOST_CHECK_EQUAL(sr.pool.find(cfg_res.first)->second, cfg_res.second);
	}

	// mixed request with indexed and anonymouse request of the same resource kind
	request = {{"HEVC", 3}, {"HEVC", 1, 7}};
	expected_list = {{"HEVC", 6}, {"HEVC", 5}, {"HEVC", 4}, {"HEVC", 7}};
	allocated = sr.acquire(request, failures);
	BOOST_CHECK(failures.empty());
	BOOST_CHECK_EQUAL_COLLECTIONS(allocated.begin(), allocated.end(),
			expected_list.begin(), expected_list.end());
}

BOOST_FIXTURE_TEST_CASE(activity_notification, ResourceManagerFixture) {
	const std::string pipelines[] = { "pipeline#0", "pipeline#1", "pipeline#2" };
	for (size_t i = 0; i < 3; ++i) { rm->registerPipeline(pipelines[i], "file"); }
	auto & connections = rm->connections;
	BOOST_CHECK_EQUAL(connections.size(), 3);
	// lets assign some resources to pipelines
	dnf_request_t failures;
	std::string acquire_response;
	rm->notifyActivity(pipelines[0]);
	rm->acquire(pipelines[0], "[{\"resource\":\"VDEC\", \"qty\":1}]", failures, acquire_response);
	BOOST_CHECK(failures.empty());
	rm->notifyActivity(pipelines[1]);
	rm->acquire(pipelines[1], "[{\"resource\":\"VDEC\", \"qty\":1}]", failures, acquire_response);
	BOOST_CHECK(failures.empty());
	rm->notifyActivity(pipelines[2]);
	rm->acquire(pipelines[2], "[{\"resource\":\"ADEC\", \"qty\":2}]", failures, acquire_response);
	BOOST_CHECK(failures.empty());
	// check active pipelines order => expecting {2,1,0}
	BOOST_CHECK_GE(connections[pipelines[2]].timestamp, connections[pipelines[1]].timestamp);
	BOOST_CHECK_GE(connections[pipelines[1]].timestamp, connections[pipelines[0]].timestamp);
	// give some notifications
	rm->notifyActivity(pipelines[2]);
	rm->notifyActivity(pipelines[1]);
	rm->notifyActivity(pipelines[0]);
	rm->notifyActivity(pipelines[2]);
	// check active pipelines order => expecting {2,0,1}
	BOOST_CHECK_GE(connections[pipelines[2]].timestamp, connections[pipelines[0]].timestamp);
	BOOST_CHECK_GE(connections[pipelines[0]].timestamp, connections[pipelines[1]].timestamp);
	// release one pipeline
	rm->unregisterPipeline(pipelines[0]);
	BOOST_CHECK_EQUAL(connections.size(), 2);
	// check active pipelines order => expecting {2,1}
	BOOST_CHECK_GE(connections[pipelines[2]].timestamp, connections[pipelines[1]].timestamp);
}

BOOST_FIXTURE_TEST_CASE(encode_resource_request, ResourceManagerFixture) {
	resource_request_t request(std::list<resource_descriptor_t>
	{{"VDEC", 2, 1}, {"ADEC", 1}, {"VDEC", 2, size_t(-1), "DPY0"}});
	const std::string & expected_str = "["
			"{\"qty\":2,\"resource\":\"VDEC\",\"attribute\":\"\",\"index\":1},"
			"{\"qty\":1,\"resource\":\"ADEC\",\"attribute\":\"\",\"index\":-1},"
			"{\"qty\":2,\"resource\":\"VDEC\",\"attribute\":\"DPY0\",\"index\":-1}"
			"]";
	std::string request_str;
	BOOST_REQUIRE(rm->encodeResourceRequest(request, request_str));
	BOOST_CHECK_EQUAL(request_str, expected_str);
}

BOOST_FIXTURE_TEST_CASE(acquire, ResourceManagerFixture) {
	const std::string pipeline = "pipeline#0";
	const std::string type = "file";
	BOOST_REQUIRE(rm->registerPipeline(pipeline, type));

	// check acquire (unhappy case - too many resources requested)
	std::string request = "["
			"{\"resource\":\"VDEC\",\"qty\":8},"
			"{\"resource\":\"ADEC\",\"qty\":4}"
			"]";
	dnf_request_t expected_failures({std::list<resource_descriptor_t>{{"VDEC", 6}, {"ADEC", 2}}});
	dnf_request_t failures;
	std::string expected_response = "{"
			"\"resources\":["
			"{\"qty\":6,\"resource\":\"VDEC\",\"index\":-1},"
			"{\"qty\":2,\"resource\":\"ADEC\",\"index\":-1}"
			"],\"state\":false,\"connectionId\":\"pipeline#0\""
			"}";
	std::string response;
	// acquire return value doesn't mean allocation success or failure
	BOOST_CHECK(rm->acquire(pipeline, request, failures, response));
	BOOST_CHECK_EQUAL_COLLECTIONS(failures.front().begin(), failures.front().end(),
			expected_failures.front().begin(), expected_failures.front().end());
	BOOST_CHECK_EQUAL(response, expected_response);

	// check acquire (happy path)
	request = "["
			"{\"resource\":\"VDEC\",\"qty\":2},"
			"{\"resource\":\"ADEC\",\"index\":1,\"qty\":1}"
			"]";
	BOOST_CHECK(rm->acquire(pipeline, request, failures, response));
	BOOST_CHECK(failures.empty());
	expected_response = "{"
			"\"resources\":["
			"{\"qty\":1,\"resource\":\"VDEC\",\"index\":1},"
			"{\"qty\":1,\"resource\":\"VDEC\",\"index\":0},"
			"{\"qty\":1,\"resource\":\"ADEC\",\"index\":1}"
			"],\"state\":true,\"connectionId\":\"pipeline#0\""
			"}";
	BOOST_CHECK_EQUAL(response, expected_response);
}

BOOST_FIXTURE_TEST_CASE(release, ResourceManagerFixture) {
	const std::string pipeline = "pipeline#0";
	const std::string type = "file";
	BOOST_REQUIRE(rm->registerPipeline(pipeline, type));

	// acquire some resources
	std::string request = "["
			"{\"resource\":\"VDEC\",\"qty\":2},"
			"{\"resource\":\"ADEC\",\"qty\":1,\"index\":0}"
			"]";
	std::string response;
	dnf_request_t failures;
	// should acquire VDEC0, VDEC1 and ADEC0
	BOOST_REQUIRE(rm->acquire(pipeline, request, failures, response));

	// check release (releasing some extras - ADEC1)
	request = "["
			"{\"resource\":\"VDEC\",\"index\":0,\"qty\":1},"
			"{\"resource\":\"VDEC\",\"index\":1,\"qty\":1},"
			"{\"resource\":\"ADEC\",\"index\":1,\"qty\":1}"
			"]";
	BOOST_CHECK(rm->release(pipeline, request));
	// should be able to acquire 2 VDECS
	request = "[{\"resource\":\"VDEC\",\"qty\":2}]";
	BOOST_CHECK(rm->acquire(pipeline, request, failures, response));
	// should fail on acquire ADEC0 as it wasn't freed
	request = "[{\"resource\":\"ADEC\",\"qty\":1,\"index\":0}]";
	BOOST_CHECK(rm->acquire(pipeline, request, failures, response));
}

BOOST_FIXTURE_TEST_CASE(lru_priority, ResourceManagerFixture) {
	const std::string pipelines[] = { "low#0", "high#1", "high#2", "high#3" };
	for (size_t i = 0; i < 4; ++i) {
		rm->registerPipeline(pipelines[i], (i ? "file" : "media"));
	}
	auto & connections = rm->connections;
	BOOST_CHECK_EQUAL(connections.size(), 4);
	// lets assign some resources to pipelines
	dnf_request_t failures;
	std::string acquire_response;
	BOOST_CHECK(rm->notifyActivity(pipelines[1]));
	BOOST_CHECK(rm->acquire(pipelines[1], "[{\"resource\":\"VDEC\", \"qty\":1}]",
			failures, acquire_response));
	BOOST_CHECK(failures.empty());
	BOOST_CHECK(rm->notifyActivity(pipelines[0]));
	BOOST_CHECK(rm->acquire(pipelines[0], "[{\"resource\":\"VDEC\", \"qty\":1}]",
			failures, acquire_response));
	BOOST_CHECK(failures.empty());
	BOOST_CHECK(rm->notifyActivity(pipelines[2]));
	BOOST_CHECK(rm->acquire(pipelines[2], "[{\"resource\":\"ADEC\", \"qty\":1}]",
			failures, acquire_response));
	BOOST_CHECK(failures.empty());
	BOOST_CHECK(rm->notifyActivity(pipelines[3]));
	BOOST_CHECK(rm->acquire(pipelines[3], "[{\"resource\":\"ADEC\", \"qty\":1}]",
			failures, acquire_response));
	BOOST_CHECK(failures.empty());
	BOOST_CHECK_EQUAL(connections.size(), 4);
	// check active pipelines order => expecting {3,2,0,1}
	BOOST_CHECK_GE(connections[pipelines[3]].timestamp, connections[pipelines[2]].timestamp);
	BOOST_CHECK_GE(connections[pipelines[2]].timestamp, connections[pipelines[0]].timestamp);
	BOOST_CHECK_GE(connections[pipelines[2]].timestamp, connections[pipelines[1]].timestamp);
	// give some notifications
	BOOST_CHECK(rm->notifyActivity(pipelines[2]));
	BOOST_CHECK(rm->notifyActivity(pipelines[1]));
	BOOST_CHECK(rm->notifyActivity(pipelines[0]));
	BOOST_CHECK(rm->notifyActivity(pipelines[2]));
	// check active pipelines order => expecting {2,0,1,3}
	BOOST_CHECK_GE(connections[pipelines[2]].timestamp, connections[pipelines[0]].timestamp);
	BOOST_CHECK_GE(connections[pipelines[0]].timestamp, connections[pipelines[1]].timestamp);
	BOOST_CHECK_GE(connections[pipelines[1]].timestamp, connections[pipelines[3]].timestamp);
	// release one pipeline
	BOOST_CHECK(rm->unregisterPipeline(pipelines[2]));
	BOOST_CHECK_EQUAL(connections.size(), 3);
	// check active pipelines order => expecting {0,1,3}
	BOOST_CHECK_GE(connections[pipelines[0]].timestamp, connections[pipelines[1]].timestamp);
	BOOST_CHECK_GE(connections[pipelines[1]].timestamp, connections[pipelines[3]].timestamp);
	// check policy candidates selection
	BOOST_CHECK(rm->registerPipeline(pipelines[2], "file"));
	dnf_request_t shortage({std::list<resource_descriptor_t>{{"VDEC", 1}}});
	std::list<std::string> candidates;
	std::list<std::string> expected_candidates = {pipelines[0]};
	BOOST_CHECK(rm->selectPolicyCandidates(pipelines[2], shortage.front(), candidates));
	BOOST_CHECK_EQUAL_COLLECTIONS(candidates.begin(), candidates.end(),
			expected_candidates.begin(), expected_candidates.end());
}

BOOST_FIXTURE_TEST_CASE(mutex_resources, ResourceManagerFixture) {
	const std::string pipelines[] = { "pipeline#0", "pipeline#1", "pipeline#2", "pipeline#3" };

	auto register_pipelines = [&] () {
		for (size_t i = 0; i < sizeof(pipelines) / sizeof(pipelines[0]); ++i)
			rm->registerPipeline(pipelines[i], "media");
	};

	auto unregister_pipelines = [&] () {
		for (size_t i = 0; i < sizeof(pipelines) / sizeof(pipelines[0]); ++i)
			rm->unregisterPipeline(pipelines[i]);
	};

	register_pipelines();

	dnf_request_t failures;
	std::string acquire_response;
	BOOST_CHECK(rm->acquire(pipelines[0], "[{\"resource\":\"HEVC\", \"qty\":4}]", failures, acquire_response));
	BOOST_CHECK(failures.empty());
	BOOST_CHECK(rm->acquire(pipelines[1], "[{\"resource\":\"HEVC\", \"qty\":2}]", failures, acquire_response));
	BOOST_CHECK(failures.empty());
	BOOST_CHECK(rm->acquire(pipelines[2], "[{\"resource\":\"VDEC\", \"qty\":1, \"index\":0}]",
				failures, acquire_response));
	BOOST_CHECK(failures.empty());

	dnf_request_t expected_failures({std::list<resource_descriptor_t>{{"HEVC", 6}, {"VDEC", 1, 0}}});
	// conflicting request VDEC=0 G1/HEVC
	BOOST_CHECK(rm->acquire(pipelines[3], "[{\"resource\":\"G1\", \"qty\":2},\
		{\"resource\":\"VDEC\", \"qty\":1, \"index\":0}]",
		failures, acquire_response));
	BOOST_CHECK_EQUAL_COLLECTIONS(failures.front().begin(), failures.front().end(),
				expected_failures.front().begin(), expected_failures.front().end());

	std::list<std::string> candidates;
	std::list<std::string> expected_candidates = { pipelines[0], pipelines[1], pipelines[2] };
	BOOST_CHECK(rm->selectPolicyCandidates(pipelines[3], failures.front(), candidates));
	BOOST_CHECK_EQUAL_COLLECTIONS(candidates.begin(), candidates.end(),
				expected_candidates.begin(), expected_candidates.end());

	unregister_pipelines();
}

BOOST_FIXTURE_TEST_CASE(resource_coalescing, ResourceManagerFixture) {
	const std::string pipelines[] = { "pipeline#0", "pipeline#1", "pipeline#2", "pipeline#3" };

	auto register_pipelines = [&] () {
		for (size_t i = 0; i < sizeof(pipelines) / sizeof(pipelines[0]); ++i)
			rm->registerPipeline(pipelines[i], "media");
	};

	auto unregister_pipelines = [&] () {
		for (size_t i = 0; i < sizeof(pipelines) / sizeof(pipelines[0]); ++i)
			rm->unregisterPipeline(pipelines[i]);
	};

	register_pipelines();

	/* basic scenario:
	 * 1. p0 acquires 1 BW unit
	 * 2. p1 acquires 2 BW unit
	 * 3. p2 acquires 4 BW unit
	 * 4. p3 acquires 4 BW unit
	 * 5. select policy candidates
	 * 6. check pipelines connections state
	 * expected result:
	 * 1. acquire p3 failes with BW[*]=3 failure
	 * 2. p0, p1 selected as policy candidates
	 * 3. p0 expected to release BW[*]=1, p1 - BW[*]=2
	 */

	dnf_request_t failures;
	std::string acquire_response;
	BOOST_CHECK(rm->acquire(pipelines[0], "[{\"resource\":\"VDEC_BUS\", \"qty\":1}]", failures, acquire_response));
	BOOST_CHECK(failures.empty());
	BOOST_CHECK(rm->acquire(pipelines[1], "[{\"resource\":\"VDEC_BUS\", \"qty\":2}]", failures, acquire_response));
	BOOST_CHECK(failures.empty());
	BOOST_CHECK(rm->acquire(pipelines[2], "[{\"resource\":\"VDEC_BUS\", \"qty\":4}]", failures, acquire_response));
	BOOST_CHECK(failures.empty());
	dnf_request_t expected_failures({std::list<resource_descriptor_t>{{"VDEC_BUS", 3}}});
	BOOST_CHECK(rm->acquire(pipelines[3], "[{\"resource\":\"VDEC_BUS\", \"qty\":4}]", failures, acquire_response));
	BOOST_CHECK_EQUAL_COLLECTIONS(failures.front().begin(), failures.front().end(),
				expected_failures.front().begin(), expected_failures.front().end());

	std::list<std::string> candidates;
	std::list<std::string> expected_candidates = { pipelines[0], pipelines[1] };
	BOOST_CHECK(rm->selectPolicyCandidates(pipelines[3], failures.front(), candidates));
	BOOST_CHECK_EQUAL_COLLECTIONS(candidates.begin(), candidates.end(),
				expected_candidates.begin(), expected_candidates.end());

	resource_request_t policy_res = rm->connections[pipelines[0]].policy_resources;
	resource_request_t expected_policy_res(std::list<resource_descriptor_t>{{"VDEC_BUS", 1}});
	BOOST_CHECK_EQUAL_COLLECTIONS(policy_res.begin(), policy_res.end(),
				expected_policy_res.begin(), expected_policy_res.end());
	policy_res = rm->connections[pipelines[1]].policy_resources;
	expected_policy_res = {{"VDEC_BUS", 2}};
	BOOST_CHECK_EQUAL_COLLECTIONS(policy_res.begin(), policy_res.end(),
				expected_policy_res.begin(), expected_policy_res.end());
	policy_res = rm->connections[pipelines[2]].policy_resources;
	expected_policy_res.clear();
	BOOST_CHECK_EQUAL_COLLECTIONS(policy_res.begin(), policy_res.end(),
				expected_policy_res.begin(), expected_policy_res.end());

	/* scenario with policy action denial
	 * 1. set policy action denial state for p1
	 * 2. p3 acquires 5 BW units
	 * 3. select policy candidates
	 * 4. check pipelines connections state
	 * expected result
	 * 1. acquire p3 failes with BW[*]=5 failure
	 * 2. p0, p1 selected as policy candidates
	 * 3. p0 expected to release BW[*]=1, p2 - BW[*]=3
	 */

	rm->connections[pipelines[1]].policy_state = resource_manager_connection_t::DENIED;
	expected_failures = {std::list<resource_descriptor_t>{{"VDEC_BUS", 4}}};
	BOOST_CHECK(rm->acquire(pipelines[3], "[{\"resource\":\"VDEC_BUS\", \"qty\":5}]", failures, acquire_response));
	BOOST_CHECK_EQUAL_COLLECTIONS(failures.front().begin(), failures.front().end(),
				expected_failures.front().begin(), expected_failures.front().end());

	expected_candidates = { pipelines[0], pipelines[2] };
	BOOST_CHECK(rm->selectPolicyCandidates(pipelines[3], failures.front(), candidates));
	BOOST_CHECK_EQUAL_COLLECTIONS(candidates.begin(), candidates.end(),
				expected_candidates.begin(), expected_candidates.end());

	policy_res = rm->connections[pipelines[0]].policy_resources;
	expected_policy_res = {{"VDEC_BUS", 1}};
	BOOST_CHECK_EQUAL_COLLECTIONS(policy_res.begin(), policy_res.end(),
				expected_policy_res.begin(), expected_policy_res.end());
	policy_res = rm->connections[pipelines[2]].policy_resources;
	expected_policy_res = {{"VDEC_BUS", 3}};
	BOOST_CHECK_EQUAL_COLLECTIONS(policy_res.begin(), policy_res.end(),
				expected_policy_res.begin(), expected_policy_res.end());

	unregister_pipelines();
}

BOOST_FIXTURE_TEST_CASE(or_and_request, ResourceManagerFixture) {
	const std::string pipelines[] = { "pipeline#0", "pipeline#1", "pipeline#2", "pipeline#3" };

	auto register_pipelines = [&] () {
		for (size_t i = 0; i < sizeof(pipelines) / sizeof(pipelines[0]); ++i)
			rm->registerPipeline(pipelines[i], "media");
	};

	auto unregister_pipelines = [&] () {
		for (size_t i = 0; i < sizeof(pipelines) / sizeof(pipelines[0]); ++i)
			rm->unregisterPipeline(pipelines[i]);
	};

	register_pipelines();

	/* "and" case scenario:
	 * 1. p0 acquires BW[*]=4, MSVC[*]=1
	 * 2. p1 acquires BW[*]=2, HEVC[*]=2
	 * 3. p2 acquires BW[*]=1, HEVC[*]=1
	 * 4. p3 acquires BW[*]=2, VP9[*]=1, BW[*]=4, MSVC[*]=1
	 * 5. select policy candidates
	 * 6. check pipelines connection state
	 * expected result:
	 * 1. p3 acquire failes with BW[*]=5 shortage
	 * 2. p0, p1 selected as a policy candidates
	 * 3. p0 expected to release BW[*]=4, p1 - BW[*]=1
	 */

	dnf_request_t failures;
	std::string acquire_response;
	BOOST_CHECK(rm->acquire(pipelines[0],
					"[{\"resource\":\"VDEC_BUS\", \"qty\":4}, {\"resource\":\"MSVC\", \"qty\":1}]",
					failures, acquire_response));
	BOOST_CHECK(failures.empty());
	BOOST_CHECK(rm->acquire(pipelines[1],
					"[{\"resource\":\"VDEC_BUS\", \"qty\":2}, {\"resource\":\"HEVC\", \"qty\":2}]",
					failures, acquire_response));
	BOOST_CHECK(failures.empty());
	BOOST_CHECK(rm->acquire(pipelines[2],
					"[{\"resource\":\"VDEC_BUS\", \"qty\":1}, {\"resource\":\"HEVC\", \"qty\":1}]",
					failures, acquire_response));
	BOOST_CHECK(failures.empty());
	dnf_request_t expected_failures({std::list<resource_descriptor_t>{{"VDEC_BUS", 5}}});
	BOOST_CHECK(rm->acquire(pipelines[3],
					"[{\"resource\":\"VDEC_BUS\", \"qty\":2}, {\"resource\":\"VP9\", \"qty\":1},\
					  {\"resource\":\"VDEC_BUS\", \"qty\":4}, {\"resource\":\"MSVC\", \"qty\":1}]",
					failures, acquire_response));
	BOOST_CHECK_EQUAL(failures.size(), 1);
	BOOST_CHECK_EQUAL_COLLECTIONS(failures.front().begin(), failures.front().end(),
					expected_failures.front().begin(), expected_failures.front().end());

	std::list<std::string> candidates;
	std::list<std::string> expected_candidates = { pipelines[0], pipelines[1] };
	BOOST_CHECK(rm->selectPolicyCandidates(pipelines[3], failures.front(), candidates));
	BOOST_CHECK_EQUAL_COLLECTIONS(candidates.begin(), candidates.end(),
					expected_candidates.begin(), expected_candidates.end());

	resource_request_t policy_res = rm->connections[pipelines[0]].policy_resources;
	resource_request_t expected_policy_res(std::list<resource_descriptor_t>{{"VDEC_BUS", 4}});
	BOOST_CHECK_EQUAL_COLLECTIONS(policy_res.begin(), policy_res.end(),
					expected_policy_res.begin(), expected_policy_res.end());
	policy_res = rm->connections[pipelines[1]].policy_resources;
	expected_policy_res = {{"VDEC_BUS", 1}};
	BOOST_CHECK_EQUAL_COLLECTIONS(policy_res.begin(), policy_res.end(),
					expected_policy_res.begin(), expected_policy_res.end());
	policy_res = rm->connections[pipelines[2]].policy_resources;
	expected_policy_res.clear();
	BOOST_CHECK_EQUAL_COLLECTIONS(policy_res.begin(), policy_res.end(),
					expected_policy_res.begin(), expected_policy_res.end());

	/* "or" case without resource conflict scenario:
	 * 1. p3 acquires BW[*]=4, MSVC[*]=1 or BW[*]=1, HEVC[*]=1
	 * 2. check acquired resources
	 * expected result:
	 * 1. p3 acquire succeeds with BW[*]=1, HEVC[*]=1
	 */

	BOOST_CHECK(rm->acquire(pipelines[3],
			"[\
				[\
					[{\"resource\":\"VDEC_BUS\", \"qty\":4}, {\"resource\":\"MSVC\", \"qty\":1}],\
					[{\"resource\":\"VDEC_BUS\", \"qty\":1}, {\"resource\":\"HEVC\", \"qty\":1}]\
				]\
			]", failures, acquire_response));
	BOOST_CHECK(failures.empty());

	/* "or" case with resource conflict scenario:
	 * 1. p3 releases BW[*]=1, HEVC[*]=1
	 * 2. p3 acquires BW[*]=4, MSVC[*]=1 or BW[*]=2, G1[*]=1
	 * 3. for each failed resources select policy candidates
	 * expected result:
	 * 1. p3 acquire failes with shortage BW[*]=1, HEVC[*]=3 or BW[*]=3
	 * 2. policy candidates for the first case are p0, p1, p2
	 * 3. policy candidates for the second case are p0
	 */
	BOOST_CHECK(rm->release(pipelines[3], "[{\"resource\":\"VDEC_BUS\", \"qty\":1, \"index\":0},\
											{\"resource\":\"HEVC\", \"qty\":1, \"index\":4}]"));

	expected_failures = {std::list<resource_descriptor_t>{{"HEVC", 3}, {"VDEC_BUS", 1}},
						 std::list<resource_descriptor_t>{{"VDEC_BUS", 3}}};
	BOOST_CHECK(rm->acquire(pipelines[3],
			"[\
				[\
					[{\"resource\":\"VDEC_BUS\", \"qty\":4}, {\"resource\":\"MSVC\", \"qty\":1}],\
					[{\"resource\":\"VDEC_BUS\", \"qty\":2}, {\"resource\":\"G1\", \"qty\":1}]\
				]\
			]", failures, acquire_response));
	BOOST_CHECK_EQUAL(failures.size(), 2);
	BOOST_CHECK_EQUAL_COLLECTIONS(failures.front().begin(), failures.front().end(),
						expected_failures.front().begin(), expected_failures.front().end());
	BOOST_CHECK_EQUAL_COLLECTIONS(failures.back().begin(), failures.back().end(),
						expected_failures.back().begin(), expected_failures.back().end());
	std::list<std::string> candidates_array[] = {{pipelines[0], pipelines[1], pipelines[2]}, {pipelines[0]}};
	size_t idx = 0;
	for (auto & branch : failures) {
		BOOST_CHECK(rm->selectPolicyCandidates(pipelines[3], branch, candidates));
		BOOST_CHECK_EQUAL_COLLECTIONS(candidates.begin(), candidates.end(),
							candidates_array[idx].begin(), candidates_array[idx].end());
		++idx;
	}

	unregister_pipelines();
}
