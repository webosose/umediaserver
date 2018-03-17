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

#ifndef __DBI_FROM_H__
#define __DBI_FROM_H__

#include "value_wrapper.h"

namespace uMediaServer { namespace DBI {

/**
 * Binding const reference wrapper class
 */
class FromWrapper : public ValueWrapper {
public:
	class insert_visitor {
	public:
		insert_visitor(FromWrapper & from) : _from(from) {}
		template <typename T>
		void operator ()(const T & t) const {
			_from.add_new_value(t);
		}
	private:
		FromWrapper & _from;
	};

	const std::type_info & get_next_type() const {
		return _values[_current_value_index % _values.size()].type();
	}
	template <typename Value>
	bool get_next_value(Value & val) {
		val = boost::any_cast<std::reference_wrapper<const Value>>
						(_values[_current_value_index++]).get();
		return _current_value_index < _values.size();
	}

private:
	template <typename Value>
	void add_new_value(const Value & val) {
		_values.push_back(std::cref(val));
	}
};

template <typename T, typename std::enable_if
			<traits::is_primitive<T>::value>::type* = nullptr>
FromWrapper from(const T & t) {
	FromWrapper from;
	FromWrapper::insert_visitor insert(from);
	insert(t);
	return from;
}

template <typename T, typename std::enable_if
			<traits::is_sequence<T>::value>::type* = nullptr>
FromWrapper from(T & t) {
	FromWrapper from;
	FromWrapper::insert_visitor insert(from);
	boost::fusion::for_each(t, insert);
	return from;
}

}}

#endif
