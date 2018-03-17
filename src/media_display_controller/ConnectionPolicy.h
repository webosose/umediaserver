#ifndef __MDC_CONNECTION_POLICY_H__
#define __MDC_CONNECTION_POLICY_H__

#include <map>
#include "interfaces.h"

namespace uMediaServer {

class ConnectionPolicy;

class VideoChannelConnection : public mdc::IChannelConnection {
public:
	VideoChannelConnection(ConnectionPolicy & policy);
	virtual mdc::sink_t try_connect(mdc::IMediaObject::ptr_t media);
	virtual void commit_connect(mdc::IMediaObject::ptr_t media);
	virtual mdc::sink_t connected(const std::string &id) const;
	virtual mdc::sink_t requested(const std::string &id) const;
	virtual void disconnected(const std::string &id);
private:
	ConnectionPolicy & _policy;
};

class AudioChannelConnection : public mdc::IChannelConnection {
public:
	AudioChannelConnection(ConnectionPolicy & policy);
	virtual mdc::sink_t try_connect(mdc::IMediaObject::ptr_t media);
	virtual void commit_connect(mdc::IMediaObject::ptr_t media);
	virtual mdc::sink_t connected(const std::string &id) const;
	virtual mdc::sink_t requested(const std::string &id) const;
	virtual void disconnected(const std::string &id);
private:
	ConnectionPolicy & _policy;
};

class ConnectionPolicy : public mdc::IConnectionPolicy {
public:
	ConnectionPolicy(const mdc::IAcbObserver &, const std::pair<std::string, std::string> &);

	virtual mdc::IChannelConnection & audio();
	virtual const mdc::IChannelConnection & audio() const;
	virtual mdc::IChannelConnection & video();
	virtual const mdc::IChannelConnection & video() const;
	virtual mdc::IMediaObject::ptr_t connected(mdc::sink_t sink) const;
	virtual mdc::IMediaObject::ptr_t requested(mdc::sink_t sink) const;

private:
	friend class AudioChannelConnection;
	friend class VideoChannelConnection;

	typedef std::map<mdc::sink_t, mdc::IMediaObject::weak_ptr_t> output_map_t;
	typedef output_map_t::iterator iter_t;
	typedef output_map_t::const_iterator const_iter_t;
	static mdc::IMediaObject::ptr_t get(const_iter_t b, const_iter_t e, mdc::sink_t sink);
	static mdc::sink_t get(const_iter_t b, const_iter_t e, const std::string & id);

	void log_connections(const std::string & prefix, const std::string & id) const;

	AudioChannelConnection _audio_channel;
	VideoChannelConnection _video_channel;

	output_map_t _requested;
	output_map_t _connected;

	const mdc::IAcbObserver & _acb_spy;
	const std::pair<std::string, std::string> & _audio_stack;

	// placeholder selection checks
	static bool free_or_background(output_map_t::const_reference);
	static bool free_or_no_focus(output_map_t::const_reference);
	static bool sound_connection_test(mdc::IMediaObject::ptr_t in, mdc::IMediaObject::ptr_t out,
									  const std::pair<std::string, std::string> &);
};

}


#endif // __MDC_CONNECTION_POLICY_H__
