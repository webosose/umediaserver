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

#define BOOST_TEST_MODULE ResourceManagerClientTest
#include <boost/test/included/unit_test.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <ResourceManagerClient.h>
#include <atomic>
#include <sstream>
#include <unistd.h>
#include <thread>

using namespace boost::unit_test::framework;

#define INDEXED_RESOURCE   "VDEC"
// NB: assuming that system has exactly 2 resource units of the kind
#define ANONYMOUS_RESOURCE "ADEC"

struct ResourceManagerClientMock {
	// resource ownership info
	// assuming that client owns only 1 resource of a type
	struct ROI {
		ROI() : indexed(false), anonymous(-1) {}
		std::atomic<bool>   indexed;
		std::atomic<size_t> anonymous;
	};
	ResourceManagerClientMock()
		: deny_policy_action(false)
		, hang_in_policy_action(false) {
		BOOST_REQUIRE(client.registerPipeline("ref"));
		client.registerPolicyActionHandler(
				[this](const char *, const char * res,
						const char *, const char *, const char *)->bool {
			while(hang_in_policy_action){ usleep(500); }
			if (deny_policy_action) {
				deny_policy_action = false;
				return false;
			}
			using namespace boost::property_tree;
			std::string resources = (res + 1);
			resources.erase(resources.length() - 1);
			std::stringstream resources_stream(resources);
			ptree json;
			read_json(resources_stream, json);
			return release(json.get<std::string>("resource") == INDEXED_RESOURCE);
		});
	}
	~ResourceManagerClientMock() {
		BOOST_REQUIRE(client.unregisterPipeline());
	}

	bool tryAcquire(bool indexed = true) {
		std::string response;
		if (indexed)
			return owned_resources.indexed =
					client.tryAcquire(indexed_request, response);

		using boost::format;
		using namespace boost::property_tree;

		// shouldn't try to acquire if already owns
		BOOST_REQUIRE_EQUAL(owned_resources.anonymous, -1);

		std::stringstream request;
		request << format(anonymous_request) % -1;
		if (client.tryAcquire(request.str(), response)) {
			std::stringstream response_stream(response);
			ptree json;
			read_json(response_stream, json);
			owned_resources.anonymous = json.get<size_t>("resources..index");
			return true;
		}
		return false;
	}

	bool acquire(bool indexed = true) {
		std::string response;
		if (indexed)
			return owned_resources.indexed = client.acquire(indexed_request, response);

		using boost::format;
		using namespace boost::property_tree;

		// shouldn't try to acquire if already owns
		BOOST_REQUIRE_EQUAL(owned_resources.anonymous, -1);

		std::stringstream request;
		request << format(anonymous_request) % -1;
		if (client.acquire(request.str(), response)) {
			std::stringstream response_stream(response);
			ptree json;
			read_json(response_stream, json);
			owned_resources.anonymous = json.get<size_t>("resources..index");
			return true;
		}
		return false;
	}
	bool release(bool indexed = true) {
		if (indexed) {
			owned_resources.indexed = false;
			return client.release(indexed_request);
		}

		using boost::format;

		// shouldn't try to release not owned resource
		BOOST_REQUIRE_PREDICATE(std::not_equal_to<size_t>(), (owned_resources.anonymous)(-1));

		std::stringstream request;
		request << format(anonymous_request) % owned_resources.anonymous;
		owned_resources.anonymous = -1;
		if (client.release(request.str())) {
			return true;
		}
		return false;
	}
	bool notifyActivity() {
		return client.notifyActivity();
	}
	void denyPolicyAction() {
		deny_policy_action = true;
	}
	void hangInPolicyAction( bool hang = true ) {
		hang_in_policy_action = hang;
	}

	// detail
	static const std::string indexed_request;
	static const std::string anonymous_request;
	uMediaServer::ResourceManagerClient client;
	ROI owned_resources;
	bool deny_policy_action;
	bool hang_in_policy_action;
};

#define NUM_CLIENTS 3
#define NUM_COLORS  6

struct ResourceManagerClientFixture {
	ResourceManagerClientFixture()
		: test_counter(max_test_runs), test_runs(max_test_runs)
		, test_color(colors[fixture_counter++ % NUM_COLORS]) {
		try {
			test_counter = test_runs = boost::lexical_cast<size_t>
					(master_test_suite().argv[master_test_suite().argc - 1]);
		} catch (...) {}
	}
	bool tryAcquire(size_t c = 0, bool indexed = true) {
		auto & client = clients[c % NUM_CLIENTS];
		return client.tryAcquire(indexed);
	}
	bool acquire(size_t c = 0, bool indexed = true) {
		auto & client = clients[c % NUM_CLIENTS];
		return client.acquire(indexed);
	}
	bool release(size_t c = 0, bool indexed = true) {
		auto & client = clients[c % NUM_CLIENTS];
		return client.release(indexed);
	}
	size_t owner(bool anonymous = false, size_t index = 0) {
		size_t owner = -1;
		auto is_owner = [] (const ResourceManagerClientMock & client,
				bool anon, size_t idx)->bool {
			return (( anon && client.owned_resources.anonymous == idx) ||
					(!anon && client.owned_resources.indexed             ));
		};
		for (size_t i = 0; i < NUM_CLIENTS; ++i) {
			if (is_owner(clients[i], anonymous, index))
				owner = owner == -1 ? i : -1;
		}
		return owner;
	}
	const ResourceManagerClientMock::ROI & roi(size_t c = 0) {
		return clients[c % NUM_CLIENTS].owned_resources;
	}
	bool notifyActivity(size_t c = 0) {
		return clients[c % NUM_CLIENTS].notifyActivity();
	}
	void denyPolicyAction(size_t c = 0) {
		clients[c % NUM_CLIENTS].denyPolicyAction();
	}
	void hangInPolicyAction(size_t c = 0, bool hang = true) {
		clients[c % NUM_CLIENTS].hangInPolicyAction(hang);
	}

	void report() {
		std::cerr << "\033[1;3" << test_color << "mrunning "
				  << boost::unit_test::framework::current_test_case().p_name
				  << " # " << (test_runs - test_counter)
				  << " of " << test_runs << "...\033[0m" << std::endl;
	}
	// static data
	static const size_t max_test_runs = 100;
	static size_t fixture_counter;
	static const std::string request;
	static const char colors[NUM_COLORS];

	// internals
	ResourceManagerClientMock clients[NUM_CLIENTS];
	size_t test_counter;
	size_t test_runs;
	const char test_color;
};
const std::string ResourceManagerClientMock::indexed_request =
		"[{\"resource\":\"" INDEXED_RESOURCE "\",\"qty\":1,\"index\":0}]";
const std::string ResourceManagerClientMock::anonymous_request =
		"[{\"resource\":\"" ANONYMOUS_RESOURCE "\",\"qty\":1,\"index\":%1%}]";
const char ResourceManagerClientFixture::colors[] = {'1','2','3','4','5','6'};
size_t ResourceManagerClientFixture::fixture_counter = 0;

BOOST_FIXTURE_TEST_CASE(acquire_release, ResourceManagerClientFixture) {
	while(test_counter--) {
		report();
		BOOST_CHECK(acquire());
		BOOST_CHECK(release());
		// anonymous resource case
		BOOST_CHECK(acquire(0, false));
		BOOST_CHECK(release(0, false));
	}
}

BOOST_FIXTURE_TEST_CASE(try_acquire, ResourceManagerClientFixture) {
	while(test_counter--) {
		report();
		// indexed resource
		BOOST_CHECK(tryAcquire(0));
		// second request should fail
		BOOST_CHECK(!tryAcquire(1));
		// client # 0 holds resource
		BOOST_CHECK(roi(0).indexed);
		// client # 1 has no resource
		BOOST_CHECK(!roi(1).indexed);

		// anonymous resource
		BOOST_CHECK(tryAcquire(0, false));
		BOOST_CHECK(tryAcquire(1, false));
		// third request should fail
		BOOST_CHECK(!tryAcquire(2, false));
		BOOST_CHECK_PREDICATE(std::not_equal_to<size_t>(), (roi(0).anonymous)(-1));
		BOOST_CHECK_PREDICATE(std::not_equal_to<size_t>(), (roi(1).anonymous)(-1));
		BOOST_CHECK_EQUAL(roi(2).anonymous, -1);

		// release resources to restore state
		BOOST_CHECK(release(0));
		BOOST_CHECK(release(0, false));
		BOOST_CHECK(release(1, false));
	}
}

#include <unistd.h>

BOOST_FIXTURE_TEST_CASE(resource_cycling, ResourceManagerClientFixture) {
	BOOST_CHECK(notifyActivity(NUM_CLIENTS - 1));
	BOOST_CHECK(acquire(NUM_CLIENTS - 1));
	BOOST_CHECK_EQUAL(owner(), NUM_CLIENTS - 1);
	while(test_counter--) {
		report();
		for (size_t i = 0; i < NUM_CLIENTS; ++i) {
			BOOST_CHECK(notifyActivity(i));
			BOOST_CHECK(acquire(i));
			BOOST_CHECK_EQUAL(owner(), i);
		}
	}
}

BOOST_FIXTURE_TEST_CASE(full_lifecycle, ResourceManagerClientFixture) {
	while(test_counter--) {
		report();
		ResourceManagerClientMock rmc;
		BOOST_CHECK(rmc.acquire());
		BOOST_CHECK(rmc.release());
	}
}

BOOST_FIXTURE_TEST_CASE(keep_alive, ResourceManagerClientFixture) {
	while(test_counter--) {
		report();
		// resource conflict w/o notifyActivity
		BOOST_CHECK(notifyActivity(0));
		BOOST_CHECK(acquire(0, false));
		// grab indexed resource for sync
		BOOST_CHECK(notifyActivity(1));
		BOOST_CHECK(acquire(1));
		BOOST_CHECK(acquire(1, false));
		BOOST_CHECK(notifyActivity(2));
		BOOST_CHECK(acquire(2, false));
		// client # 0 should give up resource
		BOOST_CHECK_EQUAL(roi(0).anonymous, -1);
		// resource conflict with notifyActivity
		BOOST_CHECK(notifyActivity(1));
		// invoke policy action on client # 1 with indexed resource
		// this should sync notifyActivity with consequental acquire
		usleep(200);
		BOOST_CHECK(notifyActivity(0));
		BOOST_CHECK(acquire(0));
		BOOST_CHECK(acquire(0, false));
		// client # 2 should give up resource
		BOOST_CHECK_EQUAL(roi(2).anonymous, -1);
		// client # 1 should preserve resource
		BOOST_CHECK_PREDICATE(std::not_equal_to<size_t>(), (roi(1).anonymous)(-1));
		// free acquired resources to restore state
		BOOST_CHECK(release(0, false));
		BOOST_CHECK(release(1, false));
		BOOST_CHECK(release(0));
	}
}

BOOST_FIXTURE_TEST_CASE(multiple_select, ResourceManagerClientFixture) {
	while(test_counter--) {
		report();
		BOOST_CHECK(acquire(0, false));
		BOOST_CHECK(acquire(1, false));
		// setting policy action deny to client # 0
		denyPolicyAction(0);
		BOOST_CHECK(acquire(2, false));
		// client # 1 should give up resource
		BOOST_CHECK_EQUAL(roi(1).anonymous, -1);
		// client # 0 should preserve resource
		BOOST_CHECK_PREDICATE(std::not_equal_to<size_t>(), (roi(0).anonymous)(-1));
		// deny all
		denyPolicyAction(0);
		denyPolicyAction(2);
		BOOST_CHECK(!acquire(1, false));
		BOOST_CHECK_EQUAL(roi(1).anonymous, -1);
		// free acquired resources to restore state
		BOOST_CHECK(release(0, false));
		BOOST_CHECK(release(2, false));
	}
}

BOOST_FIXTURE_TEST_CASE(selected_flag, ResourceManagerClientFixture) {
	while(test_counter--) {
		report();
		BOOST_CHECK(notifyActivity(0));
		BOOST_CHECK(acquire(0, false));
		BOOST_CHECK(notifyActivity(1));
		BOOST_CHECK(acquire(1, false));
		// as the result client # 1 should go to selected list
		BOOST_CHECK(notifyActivity(2));
		BOOST_CHECK(acquire(2, false));
		// client # 0 should give up resource
		BOOST_CHECK_EQUAL(roi(0).anonymous, -1);
		// client # 1 should preserve resource
		BOOST_CHECK_PREDICATE(std::not_equal_to<size_t>(), (roi(1).anonymous)(-1));
		// now client # 1 should give up
		BOOST_CHECK(notifyActivity(0));
		BOOST_CHECK(acquire(0, false));
		// client # 1 should give up resource
		BOOST_CHECK_EQUAL(roi(1).anonymous, -1);
		// client # 2 should preserve resource
		BOOST_CHECK_PREDICATE(std::not_equal_to<size_t>(), (roi(2).anonymous)(-1));
		// free acquired resources to restore state
		BOOST_CHECK(release(0, false));
		BOOST_CHECK(release(2, false));
	}
}
