#define BOOST_TEST_MODULE TestRegistry
#include <boost/test/included/unit_test.hpp>
#include <boost/fusion/adapted.hpp>
#include <list>

#define PROFILE_SQL_INTERFACE 0
#if PROFILE_SQL_INTERFACE
	#define private public
#endif
#include <Registry.h>
#if PROFILE_SQL_INTERFACE
	#undef private
#endif

#define USE_DB_FILE 0
#if USE_DB_FILE
const char * db_uri = "/tmp/registry.db";
#else
const char * db_uri = ":memory:";
#endif

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
				priority = 4;
			},
			{
				type = "media";
				bin  = "/usr/local/webos/usr/sbin/starfish-media-pipeline";
				name = "Video playback pipeline.";
				priority = 3;
			}
		);
)__";

const char * dynamic_config = R"__(
		pipelines = (
			{
				type = "dynamic";
				name = "Dynamic Pipeline";
				bin  = "/usr/local/webos/usr/sandbox/bin/dyn_pipeline";
				priority = 4;
				environment = (
					{
						name = "LD_LIBRARY_PATH";
						value = "some_lib_path";
						op = "PREPEND";
					},
					{
						name = "PATH";
						value = "some_bin_path";
						op = "APPEND";
					},
					{
						name = "SOME_VAR";
						value = "some_value";
						op = "REPLACE";
					}
				);
			}
		);
)__";

namespace reg = uMediaServer::Reg;

static reg::Registry * g_registry = nullptr;

struct RegistryFixture {
	RegistryFixture() {
		if (!g_registry) {
			g_registry = new reg::Registry(db_uri);
			libconfig::Config config;
			config.readString(test_config);
			BOOST_REQUIRE(g_registry->apply(config));
		};
	}
};

namespace test {
	struct EnvConfig {
		std::string name;
		std::string value;
		std::string op;
	};
	struct ResourceConfig {
		std::string id;
		std::string name;
		size_t qty;
	};
	struct PipelineConfig {
		std::string type;
		std::string name;
		std::string bin;
		std::string schema_file;
		size_t priority;
		size_t max_restarts;
		std::list<EnvConfig> env;
	};
}

BOOST_FUSION_ADAPT_STRUCT (
		test::EnvConfig,
		(std::string, name)
		(std::string, value)
		(std::string, op)
)

BOOST_FUSION_ADAPT_STRUCT (
		test::ResourceConfig,
		(std::string, id)
		(std::string, name)
		(size_t, qty)
)

BOOST_FUSION_ADAPT_STRUCT (
		test::PipelineConfig,
		(std::string, type)
		(std::string, name)
		(std::string, bin)
		(std::string, schema_file)
		(size_t, priority)
		(size_t, max_restarts)
)

using namespace uMediaServer::DBI;

BOOST_FIXTURE_TEST_CASE(registry_init, RegistryFixture) {
	size_t resources, pipelines;
	auto & sql = *(g_registry->dbi());

	sql << "select count(*) from resources;", into(resources);
	BOOST_CHECK_EQUAL(resources, 7);
	sql << "select count(*) from pipelines;", into(pipelines);
	BOOST_CHECK_EQUAL(pipelines, 2);

	test::ResourceConfig msvc, expected_msvc{"MSVC", "Multi Standard Video Codec", 2};
	sql << "select id, name, qty from resources where id=?", from(expected_msvc.id), into(msvc);
	BOOST_CHECK(boost::fusion::equal_to(msvc, expected_msvc));

	test::PipelineConfig file,
		expected_file{"file", "Video playback pipeline.", "/usr/local/webos/usr/sbin/umediapipeline",
					  "/usr/local/webos/etc/umediaserver/fileschema.js", 4, 0};
	sql << "select type, name, bin, schema_file, priority, max_restarts from pipelines where type=?;",
			from(expected_file.type), into(file);
	BOOST_CHECK(boost::fusion::equal_to(file, expected_file));
}

BOOST_FIXTURE_TEST_CASE(registry_interface, RegistryFixture) {
	auto & sql = *(g_registry->dbi());
	BOOST_CHECK(g_registry->del("resources", "VDEC"));
	size_t resources_count;
	sql << "select count(*) from resources;", into(resources_count);
	BOOST_CHECK_EQUAL(resources_count, 6);
	test::ResourceConfig adec, expected_adec{"ADEC", "Digital Audio Decoder", 2};
	BOOST_CHECK(g_registry->get("resources", "ADEC", adec));
	BOOST_CHECK(boost::fusion::equal_to(adec, expected_adec));

	test::ResourceConfig vdec, expected_vdec{"VDEC", "VideoDecoder", 2};
	BOOST_CHECK(g_registry->set("resources", expected_vdec));
	BOOST_CHECK(g_registry->get("resources", "VDEC", vdec));
	BOOST_CHECK(boost::fusion::equal_to(vdec, expected_vdec));
	expected_vdec.qty = 8;
	BOOST_CHECK(g_registry->set("resources", expected_vdec));
	BOOST_CHECK(g_registry->get("resources", "VDEC", vdec));
	BOOST_CHECK(boost::fusion::equal_to(vdec, expected_vdec));
}

BOOST_FIXTURE_TEST_CASE(registry_globals, RegistryFixture) {
	std::string version, expected_version("1.0");
	BOOST_CHECK(g_registry->get("version", version));
	BOOST_CHECK_EQUAL(version, expected_version);

	const std::string key = "test_key";
	std::string value, expected_value("new_value");
	BOOST_CHECK(g_registry->set(key, expected_value));
	BOOST_CHECK(g_registry->get(key, value));
	BOOST_CHECK_EQUAL(value, expected_value);
	expected_value = "updated_value";
	BOOST_CHECK(g_registry->set(key, expected_value));
	BOOST_CHECK(g_registry->get(key, value));
	BOOST_CHECK_EQUAL(value, expected_value);
}

namespace test {

struct PipelineConfigBase {
	std::string type;
	std::string name;
};

struct PipelineConfigExe {
	std::string bin;
	std::string schema_file;
	size_t max_restarts;
};

struct PipelineConfigRes {
	std::string resources;
	size_t priority;
	std::string policy_select;
	std::string policy_action;
};

struct PipelineConfigInherited
		: PipelineConfigBase
		, PipelineConfigExe
		, PipelineConfigRes {};

struct PipelineConfigSparse {
	std::string type;
	std::string name;
	void * misc_data;
	std::string bin;
	std::string schema_file;
	size_t misc_param;
	std::string resources;
	size_t max_restarts;
	size_t priority;
	std::string policy_select;
	std::string policy_action;
};

}

BOOST_FUSION_ADAPT_STRUCT (
		test::PipelineConfigInherited,
		(std::string, type)
		(std::string, name)
		(std::string, bin)
		(std::string, schema_file)
		(size_t, priority)
		(size_t, max_restarts)
)

BOOST_FUSION_ADAPT_STRUCT (
		test::PipelineConfigSparse,
		(std::string, type)
		(std::string, name)
		(std::string, bin)
		(std::string, schema_file)
		(size_t, priority)
		(size_t, max_restarts)
)


BOOST_FIXTURE_TEST_CASE(composite_structures, RegistryFixture) {
	test::PipelineConfigInherited config1, expected_config1;
	expected_config1.type = "file";
	expected_config1.name = "Video playback pipeline.";
	expected_config1.bin = "/usr/local/webos/usr/sbin/umediapipeline";
	expected_config1.schema_file = "/usr/local/webos/etc/umediaserver/fileschema.js";
	expected_config1.resources = "HEVC,VDEC_BUS";
	expected_config1.priority = 4;
	expected_config1.max_restarts = 0;
	expected_config1.policy_select =
			"select pipeline from active_pipelines where \(priority < THIS_PRIORITY AND time(LRU)";
	expected_config1.policy_action =  "unload";
	BOOST_CHECK(g_registry->get("pipelines", "file", config1));
	BOOST_CHECK(boost::fusion::equal_to(config1, expected_config1));
	expected_config1.max_restarts = 2;
	BOOST_CHECK(g_registry->set("pipelines", expected_config1));
	BOOST_CHECK(g_registry->get("pipelines", "file", config1));
	BOOST_CHECK_EQUAL(config1.max_restarts, expected_config1.max_restarts);
	expected_config1.max_restarts = 0;
	BOOST_CHECK(g_registry->set("pipelines", expected_config1));

	test::PipelineConfigSparse config2,
			expected_config2{"file", "Video playback pipeline.", nullptr, "/usr/local/webos/usr/sbin/umediapipeline",
							"/usr/local/webos/etc/umediaserver/fileschema.js", 1, "", 0, 4};
	config2.misc_param = 132;
	BOOST_CHECK(g_registry->get("pipelines", "file", config2));
	BOOST_CHECK(boost::fusion::equal_to(config2, expected_config2));
	BOOST_CHECK_EQUAL(config2.misc_param, 132);
}

BOOST_FIXTURE_TEST_CASE(one_to_many, RegistryFixture) {
	libconfig::Config dyn_config;
	dyn_config.readString(dynamic_config);
	BOOST_CHECK(g_registry->apply(dyn_config));
	test::PipelineConfig pipeline,
		expected_pipeline{"dynamic", "Dynamic Pipeline", "/usr/local/webos/usr/sandbox/bin/dyn_pipeline",
						  "", 4, 0, {{"LD_LIBRARY_PATH", "some_lib_path", "PREPEND"},
			{"PATH", "some_bin_path", "APPEND"}, {"SOME_VAR", "some_value", "REPLACE"} }};
	BOOST_CHECK(g_registry->get("pipelines", expected_pipeline.type, pipeline));
	BOOST_CHECK(boost::fusion::equal_to(pipeline, expected_pipeline));
	// get environment
	BOOST_CHECK_EQUAL(pipeline.env.size(), 0);
	BOOST_CHECK(g_registry->get("environment", pipeline.type, pipeline.env));
	BOOST_CHECK_EQUAL(pipeline.env.size(), 3);
	auto pit = pipeline.env.begin();
	auto eit = expected_pipeline.env.begin();
	for (; pit != pipeline.env.end(); ++pit, ++eit) {
		const auto & env = *pit;
		const auto & expected_env = *eit;
		BOOST_CHECK(boost::fusion::equal_to(env, expected_env));
	}
}

namespace test {
	struct PipelineConfigMin {
		std::string type;
		size_t priority;
		std::string bin;
	};
}

BOOST_FUSION_ADAPT_STRUCT (
		test::PipelineConfigMin,
		(std::string, type)
		(size_t, priority)
		(std::string, bin)
)

BOOST_FIXTURE_TEST_CASE(partial_request, RegistryFixture) {
	test::PipelineConfigMin config,
			expected_config{"media", 3, "/usr/local/webos/usr/sbin/starfish-media-pipeline"};
	BOOST_CHECK(g_registry->get("pipelines", "media", config));
	BOOST_CHECK(boost::fusion::equal_to(config, expected_config));
	expected_config.priority = 5;
	BOOST_CHECK(g_registry->set("pipelines", expected_config));
	BOOST_CHECK(g_registry->get("pipelines", "media", config));
	BOOST_CHECK_EQUAL(config.priority, expected_config.priority);
	expected_config.priority = 3;
	BOOST_CHECK(g_registry->set("pipelines", expected_config));
}

#include <chrono>
BOOST_FIXTURE_TEST_CASE(registry_performance, RegistryFixture) {
	std::vector<std::string> keys{"file", "media"};
	std::map<std::string, test::PipelineConfig> raw_configs;
	for(const auto & key : keys) {
		test::PipelineConfig config;
		g_registry->get("pipelines", key, config);
		raw_configs[key] = config;
	}
	const size_t iterations = 1000;
	auto tic = std::chrono::steady_clock::now();
	for (size_t i = 0; i < iterations; ++i) {
		for (const auto & key : keys) {
			auto config = raw_configs[key];
		}
	}
	auto toc = std::chrono::steady_clock::now();
	auto us = std::chrono::duration_cast<std::chrono::microseconds>(toc - tic).count();
	std::cout << "raw access took " << us << " us for " << iterations * keys.size() << " iterations"
			  << ", mean = " << ((double)us / (iterations * keys.size())) << std::endl;
	tic = std::chrono::steady_clock::now();
	for (size_t i = 0; i < iterations; ++i) {
		for (const auto & key : keys) {
			test::PipelineConfig config;
			g_registry->get("pipelines", key, config);
		}
	}
	toc = std::chrono::steady_clock::now();
	us = std::chrono::duration_cast<std::chrono::microseconds>(toc - tic).count();
	std::cout << "registry access took " << us << " us for " << iterations * keys.size() << " iterations"
			  << ", mean = " << ((double)us / (iterations * keys.size())) << std::endl;

#define PROFILE_SQL_INTERFACE 0
#if PROFILE_SQL_INTERFACE
	sqlite3 * sql_raw = registry.dbi()->_db;

	tic = std::chrono::steady_clock::now();
	const char * query = "select * from pipelines where type=?;";
	for (size_t i = 0; i < iterations; ++i) {
		for (const auto & key : keys) {
			test::PipelineConfig config;
			const char * tail = query;
			sqlite3_stmt * stmt;
			sqlite3_prepare_v2(sql_raw, tail, -1, &stmt, &tail);
			sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
			sqlite3_step(stmt);
			config.type = (const char *)sqlite3_column_text(stmt, 1);
			config.name = (const char *)sqlite3_column_text(stmt, 2);
			config.bin = (const char *)sqlite3_column_text(stmt, 3);
			config.resources = (const char *)sqlite3_column_text(stmt, 4);
			config.priority = sqlite3_column_int(stmt, 5);
			config.policy_select = (const char *)sqlite3_column_text(stmt, 6);
			config.policy_action = (const char *)sqlite3_column_text(stmt, 7);
			sqlite3_finalize(stmt);
		}
	}
	toc = std::chrono::steady_clock::now();
	us = std::chrono::duration_cast<std::chrono::microseconds>(toc - tic).count();
	std::cout << "sql raw access took " << us << " us for " << iterations * keys.size() << " iterations"
			  << ", mean = " << ((double)us / (iterations * keys.size())) << std::endl;

	tic = std::chrono::steady_clock::now();
	for (size_t i = 0; i < iterations; ++i) {
		for (const auto & key : keys) {
			test::PipelineConfig config;
			const char * tail = query;
			sqlite3_stmt * stmt;
			sqlite3_prepare_v2(sql_raw, tail, -1, &stmt, &tail);
			sqlite3_finalize(stmt);
		}
	}
	toc = std::chrono::steady_clock::now();
	us = std::chrono::duration_cast<std::chrono::microseconds>(toc - tic).count();
	std::cout << "sql prepare took " << us << " us for " << iterations * keys.size() << " iterations"
			  << ", mean = " << ((double)us / (iterations * keys.size())) << std::endl;
#endif
}
