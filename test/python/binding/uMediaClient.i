/* uMediaClient.i */


%module uMediaClient
%{
#include <pthread.h>
#include "uMediaClient.h"
#include <UMSConnector.h>

using namespace uMediaServer;

namespace uMediaServer {

class py_lock {
	PyGILState_STATE gstate_;
public:
	py_lock()  { gstate_ = PyGILState_Ensure(); }
	~py_lock() { PyGILState_Release(gstate_);   }
};

class MediaPlayer : public uMediaClient {
public:
	MediaPlayer() : uMediaClient(false, UMS_CONNECTOR_PRIVATE_BUS), queue_(NULL) {
		if (!PyEval_ThreadsInitialized())
			PyEval_InitThreads();

		pthread_create(&in_thr_, NULL, inputThread, this);
	}

	~MediaPlayer() {
		py_lock _;
		setQueue(NULL);
		PyThreadState* tstate = PyEval_SaveThread();
		stop();   // exit uMS API event loop
		pthread_join(in_thr_, NULL);
		PyEval_RestoreThread(tstate);
	}

	void sendDebugMsg(char* m) {
		std::string dst = "com.webos.pipeline." + media_id + "/debug";
		connection->sendMessage(dst, m, NULL, NULL);
	}

	// Thread to run event loop for subscription and command messages
	static void * inputThread(void *ctx) {
		MediaPlayer * self = static_cast<MediaPlayer *>(ctx);
		self->run();
		return NULL;
	}

	// XXX isn't better to overload stateChange instead of wrapping every callback?
	virtual bool   onLoadCompleted() {
		return pushMsg("load", true);
	}
	virtual bool onUnloadCompleted() { return pushMsg("unload", true); }
#if UMS_INTERNAL_API_VERSION == 2
	virtual bool       onSourceInfo(const ums::source_info_t& si) {
		pushMsg("duration", (long long)si.duration);
		return true;
	}
#else
	virtual bool       onSourceInfo(const source_info_t& si) {
		for (int i = 0; i < si.numPrograms; i++)
		pushMsg("duration", (long long)si.programInfo[i].duration);
		return true;
	}
#endif

	virtual bool onCurrentTime(long long currentTime) { return pushMsg( "time", currentTime); }
	virtual bool     onEndOfStream(/*bool    state*/) { return pushMsg(  "eos",        true); }
	virtual bool          onPaused(/*bool   paused*/) { return pushMsg("pause",        true); }
	virtual bool         onPlaying(/*bool  playing*/) { return pushMsg( "play",        true); }
	virtual bool        onSeekDone(/*bool  seeking*/) { return pushMsg( "seek",        true); }

	virtual bool onError(long long err, const std::string& msg) { return pushMsg("error", msg.c_str()); }
	virtual bool onUserDefinedChanged(const char* message) { return pushMsg("unknown", message); }

	// TODO probably we need to add check type of received object
	void setQueue(PyObject* queue) {
		py_lock _;

		if (queue_) Py_DECREF(queue_);

		queue_ = queue;

		if (queue_) Py_INCREF(queue_);
	}

	bool pushMsg(const char *ev_name,          bool a1) { py_lock _; return pushMsg(ev_name, Py_BuildValue("b", a1)); }
	bool pushMsg(const char *ev_name,     long long a1) { py_lock _; return pushMsg(ev_name, Py_BuildValue("L", a1)); }
	bool pushMsg(const char *ev_name, long a1, long a2) { py_lock _; return pushMsg(ev_name, Py_BuildValue("[ll]", a1, a2)); }
	bool pushMsg(const char *ev_name,   const char* a1) { py_lock _; return pushMsg(ev_name, Py_BuildValue("s", a1)); }

	bool pushMsg(const char *ev_name, PyObject *args) { // TODO check if we need to Py_DECREF(args);
		if (!queue_)
			return false;

// FIXME remove pragmas once bug http://bugs.python.org/issue9369 will be closed
#pragma push
#pragma GCC diagnostic ignored "-Wwrite-strings"
		PyObject* res = PyObject_CallMethod(queue_, "put_nowait", "((sO))", ev_name, args);
#pragma pop
		if (res) {
			// TODO check if queue is not overrun
			Py_DECREF(res);
			return true;
		} else {
			fprintf(stderr, "pushMsg: error calling queue->put_nowait:\n");
			PyErr_Print();
			setQueue(NULL); // If this call fail then all others will fail too
			return false;
		}
	}

private:
	pthread_t in_thr_;
	PyObject *queue_;
};
}
%}

%include "exception.i"
%exception {
	try {
        // FIXME no matching function for onSourceInfo.
//		$action
	} catch(std::bad_alloc&) {
		SWIG_exception(SWIG_MemoryError,  "Out of memory");
	} catch(std::exception& e) {
		SWIG_exception(SWIG_RuntimeError, e.what());
	}
}

%include "std_string.i"
%include "uMediaTypes.h"

%rename($ignore, %$isconstant) "";
%rename($ignore, %$isclass) "";
namespace uMediaServer {
	%rename("%s") "uMediaClient";
	%rename("%s") "MediaPlayer";
}

%include "uMediaClient.h"

namespace uMediaServer {
class MediaPlayer : public uMediaClient {
public:
	MediaPlayer();
	~MediaPlayer();
	void setQueue(PyObject *);
	void sendDebugMsg(char*);
};
}
