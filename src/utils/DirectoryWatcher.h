// @@@LICENSE

#ifndef DIRECTORYWATCHER_H_
#define DIRECTORYWATCHER_H_
#include <sys/inotify.h>
#include <thread>
#include <signal.h>
#include <csignal>

namespace uMediaServer {

class dwexception : public std::exception
{
private:
	std::string dir_;
public:
	dwexception(std::string dir) : dir_(dir) {};
	~dwexception() throw() {};
	virtual const char* what() const throw()
	{
		std::string retval = "inotify exception, can't watch directory" + dir_;
		return retval.c_str();
	}
};

template <class F> // F must be functor
class DirectoryWatcher {
private:
	void watch()
	{
		sigset_t set;
		sigemptyset(&set);
		sigaddset(&set, SIGUSR1);
		sigprocmask(SIG_UNBLOCK, &set, NULL);

		while (true) {
			if (watch_)
			{
				watching_ = true;
				bzero(event_, bufsiz_);
				ssize_t val = read(fd_, event_, bufsiz_);
				if (val != -1 && (reinterpret_cast<inotify_event*>(event_)->mask &
						(IN_CLOSE_WRITE | IN_DELETE))) {
					cb_();
				}
			}
			else
			{
				watching_ = false;
				break;
			}
		}
	}

	std::string directory_;
	std::thread t_;
	F cb_;
	int fd_;
	inotify_event* event_;
	size_t bufsiz_;
	std::atomic<bool> watch_;
	std::atomic<bool> watching_;
	bool changed_handler_;
	void (*old_handler_)(int);
	void (*dummy_handler_)(int);

public:
	typedef std::unique_ptr<DirectoryWatcher> Ptr;
	DirectoryWatcher(std::string directory, F cb) :
		directory_(directory), cb_(cb), event_(nullptr), bufsiz_(0), watch_(true), watching_(false)
	{
		fd_ = inotify_init();
		if (inotify_add_watch(fd_, directory.c_str(), IN_CLOSE_WRITE | IN_DELETE) < 0) {
			close(fd_);
			throw dwexception(directory);
		}
		bufsiz_ = sizeof(struct inotify_event) + PATH_MAX + 1;
		event_ = static_cast<inotify_event*>(malloc(bufsiz_));

		if (event_ != nullptr) {
			// We need to have a sig handler installed in order to exit the blocking read
			// If no-one is already installed we install a dummy handler

			// Block SIGUSR1 while manipulating the handler to not incorrectly handle a
			// potential signal
			sigset_t set;
			sigemptyset(&set);
			sigaddset(&set, SIGUSR1);
			sigprocmask(SIG_BLOCK, &set, NULL);

			dummy_handler_ = [](int arg) {;};
			old_handler_ = std::signal(SIGUSR1, dummy_handler_);
			if (old_handler_ != SIG_DFL && old_handler_ != SIG_IGN) {
				changed_handler_ = false;
				std::signal(SIGUSR1, old_handler_);
			}
			else {
				changed_handler_ = true;
			}
			t_ = std::thread([this] {this->watch();});
			// Unblock SIGUSR1
			sigprocmask(SIG_UNBLOCK, &set, NULL);
		}
		else
		{
			close(fd_);
			throw dwexception(directory_);
		}
	}

	~DirectoryWatcher ()
	{
		watch_ = false;
		close(fd_);
		if (t_.joinable()) {
			if (watching_) {
				pthread_kill(t_.native_handle(), SIGUSR1);
			}
			t_.join();
			if (changed_handler_) {
				// Block SIG_USR1 while restoring with the handler
				sigset_t set;
				sigemptyset(&set);
				sigaddset(&set, SIGUSR1);

				sigprocmask(SIG_BLOCK, &set, NULL);
				auto cur_handler = std::signal(SIGUSR1, old_handler_);
				if (cur_handler != dummy_handler_) {
					// Oops someone else has changed handler, so let's reinstall it
					std::signal(SIGUSR1, cur_handler);
				}
				// Unblock SIG_USR1
				sigprocmask(SIG_UNBLOCK, &set, NULL);
			}

		}
		free(event_);
	}

	DirectoryWatcher (DirectoryWatcher&&) = delete;
	DirectoryWatcher (const DirectoryWatcher&) = delete;
	DirectoryWatcher&& operator = (DirectoryWatcher&&) = delete;
	DirectoryWatcher&  operator = (const DirectoryWatcher&) = delete;
};
}
#endif /* DIRECTORYWATCHER_H_ */
