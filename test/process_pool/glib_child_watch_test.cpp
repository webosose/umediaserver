#include <glib.h>
#include <cassert>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <thread>
#include <unistd.h>
#include <set>
#include <map>
#include <vector>
#include <mutex>
#include <signal.h>
#include <fstream>

const int TTL_UNIT  = 100000; /* 100 ms */
const int MIN_TTL   = 10;     /* min proc lifetime = 1 s */
const int MAX_TTL   = 100;    /* max proc lifetime = 10 s */
const int NO_PROC   = 1000;   /* number of processes to start */
const int NO_THREAD = 100;    /* number of threads to start */

static GMainLoop * loop = nullptr;
static void * cookie = nullptr;

int rand_range(int min, int max) {
	unsigned int random_value = 0;
	size_t size = sizeof(random_value);
	std::ifstream urandom("/dev/urandom", std::ios::in|std::ios::binary);
	if(urandom.is_open()) {
	        urandom.read(reinterpret_cast<char*>(&random_value), size);
	        urandom.close(); //close stream
	} else {
		std::cerr << "Failed to open /dev/urandom" << std::endl;
	}
	return min + int(double(max - min) * double(random_value) / double(UINT_MAX));
}

int rand_sleep() {
	int ttl = rand_range(MIN_TTL, MAX_TTL);
	return ttl * TTL_UNIT;
}

int child() {
	usleep(rand_sleep());
	return 0;
}

int main() {
	static std::mutex guard;
	static std::set<pid_t> pids;
	static std::map<int, int> exit_info;

	std::vector<std::thread> threads(NO_THREAD);

	static GMainLoop * loop = g_main_loop_new(nullptr, false);

	for (size_t p = 0; p < NO_PROC; ++p) {
		pid_t pid = fork();
		if (pid) {
			std::lock_guard<std::mutex> l(guard);
			pids.insert(pid);
			g_child_watch_add(pid, [](GPid pid, gint status, gpointer){
				std::lock_guard<std::mutex> l(guard);
				std::cout << "proc [" << pid << "] exited with " << status << " status." << std::endl;
				auto it = pids.find(pid);
				if (it != pids.end()) pids.erase(it);
				auto ei = exit_info.find(status);
				if (ei == exit_info.end())
					exit_info[status] = 1;
				else
					ei->second++;
				if (pids.empty()) g_main_loop_quit(loop);
			}, nullptr);
		} else
			return child();
	}

	for (size_t t = 0; t < NO_THREAD; ++t) {
		threads[t] = std::thread([](){
			usleep(rand_sleep());
			std::lock_guard<std::mutex> l(guard);
			if (!pids.empty()) {
				int n = rand_range(0, pids.size() - 1);
				auto it = pids.begin();
				while (n--) { ++it; }
				auto pid = *it;
				std::cout << "killing proc [" << pid << "] ..." << std::endl;
				kill(pid, SIGKILL);
			}
		});
	}

	g_main_loop_run(loop);

	for (size_t t = 0; t < NO_THREAD; ++t) {
		threads[t].join();
	}

	for (const auto & ei : exit_info) {
		std::cout << "status " << ei.first << " = " << ei.second << std::endl;
	}

	return 0;
}
