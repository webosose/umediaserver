#ifndef __DBI_QUERY_H__
#define __DBI_QUERY_H__

#include <iostream>
#include <sstream>
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

#include "into.h"
#include "from.h"
#include "exception.h"

namespace uMediaServer { namespace DBI {

class SQLiteDBI;

/**
 * SQL query class
 */
class Query {
public:

	/**
	 * Temporary SQL query wrapper object
	 */
	class Shell {
	public:
		Shell(Query * query);
		/**
		 * Query execution routine
		 * @throws DBI::Exception on error
		 */
		~Shell() noexcept(false);

		// noncopyable
		Shell(const Shell &) = delete;
		Shell & operator = (const Shell &) = delete;

		// movable
		Shell(Shell && other);
		Shell & operator = (Shell && other);

		/**
		 * Appends value to query
		 * @param IN template value that could be serialized to std::ostream
		 * @returns Reference to itself for lazy streaming
		 */
		template<typename T>
		Shell & operator << (const T & t) {
			_query->_query_stream << t;
			return *this;
		}

		/**
		 * Appends receving placeholder
		 * @param IN value reference wrapper object
		 * @returns Reference to itself
		 */
		Shell & operator , (IntoWrapper && into);
		/**
		 * Appends query binding placeholder
		 * @param IN value const reference wrapper object
		 * @returns Reference to itself
		 */
		Shell & operator , (FromWrapper && from);

	private:
		Query * _query;
	};

	Query(SQLiteDBI * dbi) : _dbi(dbi) {}

	/**
	 * Starts new query
	 * @param IN template value that could be serialized to std::ostream
	 * @returns Temporary query wrapper object
	 */
	template <typename T>
	Shell operator << (const T & t) {
		_query_stream << t;
		return Shell(this);
	}

	/**
	 * Assigns value to user provided data
	 * @param IN template value to assign
	 */
	template <typename Value>
	void set_next(const Value & val) {
		if (_into == _intos.end()) {
			throw RangeError() << boost::throw_file(__FILE__)
							   << boost::throw_line(__LINE__)
							   << boost::throw_function(BOOST_CURRENT_FUNCTION);
		}
		bool full  = !_into->set_next_value(val);
		if  (full) ++_into;
	}

	/**
	 * Extracts next binding placeholders value
	 * @param IN template value to be assigned
	 */
	template <typename Value>
	void get_next(Value & val) {
		if (_from == _froms.end()) {
			throw RangeError() << boost::throw_file(__FILE__)
							   << boost::throw_line(__LINE__)
							   << boost::throw_function(BOOST_CURRENT_FUNCTION);
		}
		bool full = !_from->get_next_value(val);
		if  (full) ++_from;
	}

	/**
	 * Provides runtime type information of holding value
	 * @returns std::type_info of wrapped reference
	 */
	const std::type_info & next_into_type() const {
		if (_into == _intos.end()) {
			throw RangeError() << boost::throw_file(__FILE__)
							   << boost::throw_line(__LINE__)
							   << boost::throw_function(BOOST_CURRENT_FUNCTION);
		}
		return _into->get_next_type();
	}

	/**
	 * Provides runtime type information of holding value
	 * @returns std::type_info of wrapped reference
	 */
	const std::type_info & next_from_type() const {
		if (_from == _froms.end()) {
			throw RangeError() << boost::throw_file(__FILE__)
							   << boost::throw_line(__LINE__)
							   << boost::throw_function(BOOST_CURRENT_FUNCTION);
		}
		return _from->get_next_type();
	}

	std::string query();

private:
	friend class Shell;

	void exec();
	void reset();

	SQLiteDBI * _dbi;
	std::stringstream _query_stream;
	std::vector<IntoWrapper> _intos;
	std::vector<IntoWrapper>::iterator _into;
	std::vector<FromWrapper> _froms;
	std::vector<FromWrapper>::iterator _from;
};

}}

#endif
