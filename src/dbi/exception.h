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

#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__

#include <boost/exception/all.hpp>

namespace uMediaServer { namespace DBI {

/**
 * Generic DB access error
 */
struct Exception : std::runtime_error, virtual boost::exception {
	Exception(const std::string & error = "database access error")
		: std::runtime_error(error) {}
};
/**
 * DB open error
 */
struct OpenError  : Exception {
	OpenError(const std::string & error = "database open error")
		: Exception(error) {}
};
/**
 * SQL query execution error
 */
struct ExecError  : Exception {
	ExecError(const std::string & error = "query exec error")
		: Exception(error) {}
};
/**
 * Data binding / conversion error
 */
struct ConvError  : Exception {
	ConvError(const std::string & error = "data conversion error")
		: Exception(error) {}
};
/**
 * Placeholders range mismatch
 */
struct RangeError : Exception {
	RangeError(const std::string & error = "argument range error")
		: Exception(error) {}
};

/**
 * Exception tag that delivers DB URI
 */
typedef boost::error_info<struct db_uri_tag,   std::string > throw_db_uri;
/**
 * Exception tag that represents original SQLite3 error
 */
typedef boost::error_info<struct db_error_tag, const char *> throw_db_error;
/**
 * Exception tag that represents typeid of failed placeholder
 */
typedef boost::error_info<struct typeid_tag,   std::string > throw_typeid;

}}

#endif // __EXCEPTION_H__
