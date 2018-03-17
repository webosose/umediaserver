#define BOOST_TEST_MODULE MediaClientTest
#include <unistd.h>
#include <boost/test/included/unit_test.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/lexical_cast.hpp>
#include <uMediaClient.h>

using namespace boost::unit_test::framework;

#define NUM_COLORS  6

struct MediaClientTestFixture {
	MediaClientTestFixture()
		: test_counter(max_test_runs), test_runs(max_test_runs)
		, test_color(colors[fixture_counter++ % NUM_COLORS]) {
		try {
			test_counter = test_runs = boost::lexical_cast<size_t>
					(master_test_suite().argv[master_test_suite().argc - 1]);
		} catch (...) {}
	}

	void report() {
		std::cerr << "\033[1;3" << test_color << "mrunning "
				  << boost::unit_test::framework::current_test_case().p_name
				  << " # " << (test_runs - test_counter)
				  << " of " << test_runs << "...\033[0m" << std::endl;
	}

	static const size_t max_test_runs = 100;
	static size_t fixture_counter;
	static const std::string request;
	static const char colors[NUM_COLORS];

	size_t test_counter;
	size_t test_runs;
	const char test_color;
};

const char MediaClientTestFixture::colors[] = {'1','2','3','4','5','6'};
size_t MediaClientTestFixture::fixture_counter = 0;

struct MediaPlayer : public uMediaServer::uMediaClient {
	MediaPlayer() : unload_completed(false), uMediaServer::uMediaClient(false, UMS_CONNECTOR_PRIVATE_BUS) {}
	virtual bool onUnloadCompleted() {
		unload_completed = true;
	}
	bool unload_completed;
};

BOOST_FIXTURE_TEST_CASE(umc_lifetime, MediaClientTestFixture) {

	while (test_counter--) {
		report();
		MediaPlayer();
	}
}

BOOST_FIXTURE_TEST_CASE(double_unload, MediaClientTestFixture) {
	const size_t one_millisecond = 1000;
	while (test_counter--) {
		report();
		MediaPlayer umc;
		BOOST_CHECK(umc.load("dummy_uri", "sim"));
		BOOST_CHECK(umc.unload());
		// wait for unload completed
		size_t breakout_counter = 1000;
		while (--breakout_counter && !umc.unload_completed)
			usleep(one_millisecond);
		BOOST_CHECK(breakout_counter);
		BOOST_CHECK(umc.unload());
	}
}

BOOST_FIXTURE_TEST_CASE(mdc_display_iface, MediaClientTestFixture) {
	while (test_counter--) {
		report();
		MediaPlayer umc;
		BOOST_CHECK(umc.load("dummy_uri", "sim"));
		BOOST_CHECK(umc.setDisplayWindow({0, 0, 800, 600}));
		BOOST_CHECK(umc.setDisplayWindow({0, 18, 720, 540},{0, 0, 1280, 720}));
		BOOST_CHECK(umc.switchToFullscreen());
		BOOST_CHECK(umc.setFocus());
		BOOST_CHECK(umc.setFocus());
	}
}
