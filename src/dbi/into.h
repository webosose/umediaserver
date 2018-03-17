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
//

#ifndef __DBI_INTO_H__
#define __DBI_INTO_H__

#include "value_wrapper.h"
#include <list>
#include <functional>

namespace uMediaServer { namespace DBI {

/**
 * Receiving reference wrapper class
 */
class IntoWrapper : public ValueWrapper {
public:
	class insert_visitor {
	public:
		insert_visitor(IntoWrapper & into) : _into(into) {}
		template <typename T>
		void operator ()(T & t) const {
			_into.add_new_value(t);
		}
	private:
		IntoWrapper & _into;
	};

	// fix to support gcc-4.7.2
	IntoWrapper() {}
	IntoWrapper(IntoWrapper && other) noexcept : ValueWrapper(std::move(other)),
		extend(std::move(other.extend)), list(std::move(other.list)) {}

	const std::type_info & get_next_type() const {
		if (_values.empty())
			extend(const_cast<IntoWrapper *>(this));
		return _values[_current_value_index % _values.size()].type();
	}
	template <typename Value>
	bool set_next_value(const Value & val) {
		if (_current_value_index == _values.size()) {
			if (extend)
				extend(this);
			else
				return false;
		}
		boost::any_cast<std::reference_wrapper<Value>>
						(_values[_current_value_index++]).get() = val;
		return extend || _current_value_index < _values.size();
	}

	// TODO: change access to private
	std::function<void(IntoWrapper*)> extend;
	boost::any list;

private:
	template <typename Value>
	void add_new_value(Value & val) {
		_values.push_back(std::ref(val));
	}
};

template <typename T, typename std::enable_if
			<traits::is_primitive<T>::value>::type* = nullptr>
IntoWrapper into(T & t) {
	IntoWrapper into;
	IntoWrapper::insert_visitor insert(into);
	insert(t);
	return into;
}

template <typename T, typename std::enable_if
			<traits::is_sequence<T>::value>::type* = nullptr>
IntoWrapper into(T & t) {
	IntoWrapper into;
	IntoWrapper::insert_visitor insert(into);
	boost::fusion::for_each(t, insert);
	return into;
}

// the only reason for this wrapper to exist
// is to avoid gdb crash on reading symbols.
// TODO: remove this
template <typename T, typename std::enable_if
		  <traits::is_primitives_list<T>::value>::type* = nullptr>
void gdb_workaround(IntoWrapper & into) {
	into.extend = [](IntoWrapper * self){
		auto & list = boost::any_cast<std::reference_wrapper<T>>(self->list).get();
		list.push_back({});
		IntoWrapper::insert_visitor insert(*self);
		insert(list.back());
	};
}

template <typename T, typename std::enable_if
			<traits::is_primitives_list<T>::value>::type* = nullptr>
IntoWrapper into(T & t) {
	IntoWrapper into;
	into.list = std::reference_wrapper<T>(t);
	gdb_workaround<T>(into);
	return into;
}

// TODO: remove this
template <typename T, typename std::enable_if
		  <traits::is_sequences_list<T>::value>::type* = nullptr>
void gdb_workaround(IntoWrapper & into) {
	into.extend = [](IntoWrapper * self){
		auto & list = boost::any_cast<std::reference_wrapper<T>>(self->list).get();
		list.push_back({});
		IntoWrapper::insert_visitor insert(*self);
		boost::fusion::for_each(list.back(), insert);
	};
}

template <typename T, typename std::enable_if
			<traits::is_sequences_list<T>::value>::type* = nullptr>
IntoWrapper into(T & t) {
	IntoWrapper into;
	into.list = std::reference_wrapper<T>(t);
	gdb_workaround<T>(into);
	return into;
}

}}

#endif
