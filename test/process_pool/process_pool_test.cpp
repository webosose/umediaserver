#define BOOST_TEST_MODULE ProcessPoolTest

#include <unistd.h>
#include <glib.h>
#include <boost/format.hpp>
#include <boost/test/included/unit_test.hpp>
#include <Registry.h>
#include <ProcessPool.h>
#include <GLibHelper.h>

using namespace uMediaServer;

static bool this_is_pipeline        = false;

struct Fixture {
	const char * config_tmpl = R"__(
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
				}
			)
			pipelines = (
				{
					type = "file";
					name = "Video playback pipeline.";
					bin  = "%s";
					priority = 4;
				},
				{
					type = "media";
					bin  = "%s";
					name = "Media playback pipeline.";
					priority = 4;
					pool_size = 2;
					pool_fill_delay = 0;
					pool_refill_delay = 0;
				}
			)
	)__";

	Fixture() {
		auto & suite = boost::unit_test::framework::master_test_suite();
		if (std::string(suite.argv[suite.argc - 1]).find("-s") == 0) {
			this_is_pipeline = true;
			pipeline_process();
		} else {
			std::string test_config = str(boost::format(config_tmpl) % suite.argv[0] % suite.argv[0]);
			libconfig::Config config;
			config.readString(test_config);
			Reg::Registry::instance()->apply(config);
		}
	}

	int pipeline_process() {
		std::cout << "process " << getpid() << " started..." << std::endl;
		if (getenv("PIPELINE_SHOULD_CRASH"))
			raise(SIGSEGV);
		if (getenv("PIPELINE_SHOULD_HANG"))
			while(true);
		return 0;
	}
};

BOOST_GLOBAL_FIXTURE(Fixture);

struct ReadyWatcherMock : IServiceReadyWatcher {
	void watch(const std::string &, std::function<void()> && ready_callback) {
		ready_callback();
	}
	void unwatch(const std::string &) { }
};

BOOST_AUTO_TEST_CASE(pool_process) {
	if (this_is_pipeline) return;
	setenv("PIPELINE_SHOULD_HANG", "TRUE", 1);
	GMainLoop * loop = g_main_loop_new(nullptr, false);
	std::string id = "test_id";
	ReadyWatcherMock watcher;
	ProcessPool pool(watcher, [loop](const std::string &) {});
	auto timer = new GMainTimer(500, [loop](){ g_main_loop_quit(loop); });
	g_main_loop_run(loop);
	BOOST_CHECK_EQUAL(pool.pooled("media"), 2);
	g_main_loop_unref(loop);
}

BOOST_AUTO_TEST_CASE(create_process) {
	if (this_is_pipeline) return;
	bool dispatched = false;
	GMainLoop * loop = g_main_loop_new(nullptr, false);
	std::string id = "test_id";
	ReadyWatcherMock watcher;
	ProcessPool pool(watcher, [loop](const std::string &) {});
	pool.hire("file", id, [&](const std::string & service_name, Process::ptr_t p){
		std::cout << "process " << p->pid() << "(" << service_name << ") ready..." << std::endl;
		dispatched = true;
		new GMainTimer(500, [loop](){ g_main_loop_quit(loop); });
	});
	g_main_loop_run(loop);
	g_main_loop_unref(loop);
	BOOST_CHECK(dispatched);
}

BOOST_AUTO_TEST_CASE(crash_process) {
	if (this_is_pipeline) return;
	setenv("PIPELINE_SHOULD_CRASH", "TRUE", 1);
	bool crashed = false;
	bool dispatched = false;
	GMainLoop * loop = g_main_loop_new(nullptr, false);
	std::string id = "test_id";
	ReadyWatcherMock watcher;
	ProcessPool pool(watcher, [&crashed](const std::string &) {
		crashed = true;
	});
	Process::ptr_t proc;
	pool.hire("file", id, [&](const std::string & service_name, Process::ptr_t p){
		std::cout << "process " << p->pid() << "(" << service_name << ") ready..." << std::endl;
		dispatched = true;
		new GMainTimer(500, [loop](){ g_main_loop_quit(loop); });
		proc = std::move(p);
	});
	g_main_loop_run(loop);
	g_main_loop_unref(loop);
	BOOST_CHECK(dispatched);
	BOOST_CHECK(crashed);
}

BOOST_AUTO_TEST_CASE(retire_process) {
	if (this_is_pipeline) return;
	setenv("PIPELINE_SHOULD_HANG", "TRUE", 1);
	bool retired = false;
	bool dispatched = false;
	GMainLoop * loop = g_main_loop_new(nullptr, false);
	std::string id = "test_id";
	ReadyWatcherMock watcher;
	ProcessPool pool(watcher, [&retired](const std::string &) {
		retired = true;
	});
	pid_t pid;
	pool.hire("file", id, [&](const std::string & service_name, Process::ptr_t p){
		std::cout << "process " << (pid = p->pid()) << "(" << service_name << ") ready..." << std::endl;
		dispatched = true;
		pool.retire(std::move(p));
		new GMainTimer(500, [loop](){ g_main_loop_quit(loop); });
	});
	g_main_loop_run(loop);
	g_main_loop_unref(loop);
	BOOST_CHECK(dispatched);
	BOOST_CHECK(retired);
	BOOST_CHECK_EQUAL(kill(pid, 0), -1);
	BOOST_CHECK_EQUAL(errno, ESRCH);
}
