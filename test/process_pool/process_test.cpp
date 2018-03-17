#define BOOST_TEST_MODULE ProcessTest

#include <iostream>
#include <fstream>
#include <csignal>
#include <glib.h>
#include <boost/test/included/unit_test.hpp>
#include <boost/filesystem.hpp>

#include <Process.h>

using namespace uMediaServer;

BOOST_AUTO_TEST_CASE(procedure) {
	GMainLoop * loop = g_main_loop_new(nullptr, false);
	boost::filesystem::remove("/tmp/process_test.tmp");
	BOOST_CHECK(!boost::filesystem::exists("/tmp/process_test.tmp"));
	Process p([](){
		std::fstream fs;
		fs.open("/tmp/process_test.tmp", std::ios::out);
		fs.close();
	}, [loop](pid_t, int status){
		BOOST_CHECK(WIFEXITED(status));
		BOOST_CHECK_EQUAL(WEXITSTATUS(status), 0);
		BOOST_CHECK(boost::filesystem::exists("/tmp/process_test.tmp"));
		g_main_loop_quit(loop);
	});
	g_main_loop_run(loop);
	g_main_loop_unref(loop);
	boost::filesystem::remove("/tmp/process_test.tmp");
}

BOOST_AUTO_TEST_CASE(command_line) {
	GMainLoop * loop = g_main_loop_new(nullptr, false);
	boost::filesystem::remove("/tmp/process_test.tmp");
	BOOST_CHECK(!boost::filesystem::exists("/tmp/process_test.tmp"));
	Process p("/usr/bin/touch /tmp/process_test.tmp", [loop](pid_t, int status){
		BOOST_CHECK(WIFEXITED(status));
		BOOST_CHECK_EQUAL(WEXITSTATUS(status), 0);
		BOOST_CHECK(boost::filesystem::exists("/tmp/process_test.tmp"));
		g_main_loop_quit(loop);
	});
	g_main_loop_run(loop);
	g_main_loop_unref(loop);
	boost::filesystem::remove("/tmp/process_test.tmp");
}

BOOST_AUTO_TEST_CASE(term_process) {
	GMainLoop * loop = g_main_loop_new(nullptr, false);
	Process p([](){
		while(true);
	}, [loop](pid_t, int status){
		BOOST_CHECK(WIFSIGNALED(status));
		BOOST_CHECK_EQUAL(WTERMSIG(status), SIGTERM);
		g_main_loop_quit(loop);
	});
	p.stop();
	g_main_loop_run(loop);
	g_main_loop_unref(loop);
}

BOOST_AUTO_TEST_CASE(kill_process) {
	GMainLoop * loop = g_main_loop_new(nullptr, false);
	Process p([](){
		std::signal(SIGTERM, [](int){});
		while(true);
	}, [loop](pid_t, int status){
		BOOST_CHECK(WIFSIGNALED(status));
		BOOST_CHECK_EQUAL(WTERMSIG(status), SIGKILL);
		g_main_loop_quit(loop);
	});
	uMediaServer::GMainTimer timer(500, [&p](){ p.stop(); });
	g_main_loop_run(loop);
	g_main_loop_unref(loop);
}

BOOST_AUTO_TEST_CASE(crash_process) {
	GMainLoop * loop = g_main_loop_new(nullptr, false);
	Process p([](){
		raise(SIGSEGV);
	}, [loop](pid_t, int status){
		BOOST_CHECK(WIFSIGNALED(status));
		BOOST_CHECK_EQUAL(WTERMSIG(status), SIGSEGV);
		g_main_loop_quit(loop);
	});
	g_main_loop_run(loop);
	g_main_loop_unref(loop);
}

BOOST_AUTO_TEST_CASE(no_callback) {
	GMainLoop * loop = g_main_loop_new(nullptr, false);
	Process p([](){
		std::fstream fs;
		fs.open("/tmp/process_test.tmp", std::ios::out);
		fs.close();
	});
	uMediaServer::GMainTimer timer(500, [loop](){ g_main_loop_quit(loop); });
	g_main_loop_run(loop);
	g_main_loop_unref(loop);
	BOOST_CHECK(boost::filesystem::exists("/tmp/process_test.tmp"));
	boost::filesystem::remove("/tmp/process_test.tmp");
}

BOOST_AUTO_TEST_CASE(no_loop) {
	Process p([](){ std::cout << "I'm alive!" << std::endl;	});
}

BOOST_AUTO_TEST_CASE(environment) {
	GMainLoop * loop = g_main_loop_new(nullptr, false);
	boost::filesystem::remove("/tmp/process_test.tmp");
	BOOST_CHECK(!boost::filesystem::exists("/tmp/process_test.tmp"));
	Process p([](){
		std::fstream fs;
		fs.open(std::getenv("PP_FILE_TO_CREATE"), std::ios::out);
		fs.close();
	}, [loop](pid_t, int status){
		BOOST_CHECK(WIFEXITED(status));
		BOOST_CHECK_EQUAL(WEXITSTATUS(status), 0);
		BOOST_CHECK(boost::filesystem::exists("/tmp/process_test.tmp"));
		g_main_loop_quit(loop);
	}, {{"PP_FILE_TO_CREATE", "/tmp/process_test.tmp", "REPLACE"}});
	g_main_loop_run(loop);
	g_main_loop_unref(loop);
	boost::filesystem::remove("/tmp/process_test.tmp");
}

