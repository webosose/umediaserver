// Copyright (c) 2016-2019 LG Electronics, Inc.
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
//

#ifndef __MDC_CONNECTION_POLICY_H__
#define __MDC_CONNECTION_POLICY_H__

#include <map>
#include <mutex>
#include "interfaces.h"

namespace uMediaServer {

class ConnectionPolicy;

class VideoChannelConnection : public mdc::IChannelConnection {
public:
	VideoChannelConnection(ConnectionPolicy & policy);
	virtual int32_t try_connect(mdc::IMediaObject::ptr_t media);
	virtual void commit_connect(mdc::IMediaObject::ptr_t media);
	virtual int32_t connected(const std::string &id) const;
	virtual int32_t requested(const std::string &id) const;
	virtual std::string sink_name(const std::string & id, const int32_t type = 0) const;
	virtual void disconnected(const std::string &id);
private:
	ConnectionPolicy & _policy;
};

class AudioChannelConnection : public mdc::IChannelConnection {
public:
	AudioChannelConnection(ConnectionPolicy & policy);
	virtual int32_t try_connect(mdc::IMediaObject::ptr_t media);
	virtual void commit_connect(mdc::IMediaObject::ptr_t media);
	virtual int32_t connected(const std::string &id) const;
	virtual int32_t requested(const std::string &id) const;
	virtual std::string sink_name(const std::string & id, const int32_t type = 0) const;
	virtual void disconnected(const std::string &id);
private:
	ConnectionPolicy & _policy;
};

class ConnectionPolicy : public mdc::IConnectionPolicy {
public:
	ConnectionPolicy(const std::pair<std::string, std::string> &);

	virtual mdc::IChannelConnection & audio();
	virtual const mdc::IChannelConnection & audio() const;
	virtual mdc::IChannelConnection & video();
	virtual const mdc::IChannelConnection & video() const;
	virtual mdc::IMediaObject::ptr_t video_connected(const std::string & sink_name) const;
	virtual mdc::IMediaObject::ptr_t video_requested(const std::string & sink_name) const;
	virtual mdc::IMediaObject::ptr_t audio_connected(const std::string & sink_name) const;
	virtual mdc::IMediaObject::ptr_t audio_requested(const std::string & sink_name) const;
	virtual void set_video_object(int32_t sink_index, std::string sink_name);
	virtual void set_video_sink(const std::string & sink_name);
	virtual void set_audio_object(int32_t sink_index, std::string sink_name);

private:
	friend class AudioChannelConnection;
	friend class VideoChannelConnection;
	typedef struct {
		mdc::IMediaObject::weak_ptr_t wptr;
		std::string name;
	} sink_name_map_t;
	typedef std::map<int32_t, sink_name_map_t> output_map_t;
	typedef output_map_t::iterator iter_t;
	typedef output_map_t::const_iterator const_iter_t;
	static mdc::IMediaObject::ptr_t get(const_iter_t b, const_iter_t e, int32_t sink);
	static mdc::IMediaObject::ptr_t get(const std::string & sink_name, const_iter_t b, const_iter_t e);
	static int32_t get(const_iter_t b, const_iter_t e, const std::string & id);

	void log_connections(const std::string & prefix, const std::string & id) const;

	AudioChannelConnection _audio_channel;
	VideoChannelConnection _video_channel;

	std::string _acquired_sink_name;
	std::mutex _mtx;

	output_map_t _audio_requested;
	output_map_t _video_requested;
	output_map_t _audio_connected;
	output_map_t _video_connected;

	const std::pair<std::string, std::string> & _audio_stack;
	// placeholder selection checks

	static bool free_or_background(output_map_t::const_reference);
	static bool free_or_no_focus(output_map_t::const_reference);
	static bool sound_connection_test(mdc::IMediaObject::ptr_t in, mdc::IMediaObject::ptr_t out,
									const std::pair<std::string, std::string> &);
};

}


#endif // __MDC_CONNECTION_POLICY_H__
