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

#ifndef __VALUE_WRAPPER_H__
#define __VALUE_WRAPPER_H__

#include <vector>
#include <list>
#include <boost/any.hpp>
#include <boost/fusion/sequence.hpp>
#include <boost/fusion/algorithm.hpp>

namespace uMediaServer { namespace DBI {

namespace traits {

template <typename>
struct is_string : std::false_type {};

template <>
struct is_string<std::string> : std::true_type {};

template <typename T>
struct is_primitive : std::integral_constant
		<bool, std::is_arithmetic<T>::value || is_string<T>::value> {};

using boost::fusion::traits::is_sequence;

template <typename>
struct is_primitives_list : std::false_type {};

template <typename T, typename A>
struct is_primitives_list<std::list<T,A>> : is_primitive<T> {};

template <typename>
struct is_sequences_list : std::false_type {};

template <typename T, typename A>
struct is_sequences_list<std::list<T,A>> : is_sequence<T> {};

} // namespace traits

/**
 * Base value reference wrapper class
 */
class ValueWrapper {
public:
	ValueWrapper() : _current_value_index(0) {}

	ValueWrapper(const ValueWrapper &) = delete;
	ValueWrapper & operator = (const ValueWrapper &) = delete;

	ValueWrapper(ValueWrapper && other) noexcept : _values(std::move(other._values)),
		_current_value_index(other._current_value_index) {}

protected:
	std::vector<boost::any> _values;
	size_t _current_value_index;
};

}} // namespace uMediaServer::DBI


#endif // __VALUE_WRAPPER_H__
