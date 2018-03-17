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

#ifndef __DBI_DBI_H__
#define __DBI_DBI_H__

#include <sqlite3.h>
#include <string>
#include "query.h"

namespace uMediaServer { namespace DBI {

/**
 * SQLite3 DB Interface
 */
class SQLiteDBI {
public:
	/**
	 * Opens / creates SQLite3 DB connection
	 * @param IN SQLite3 DB URI
	 * @throws OpenError on failure
	 */
	SQLiteDBI(const std::string & db_uri);
	~SQLiteDBI();

	/**
	 * Starts preparation of the new query
	 * @param IN template value that could be serialized to std::ostream
	 * @returns SQL query temporary wrapper object
	 */
	template<typename T>
	Query::Shell operator << (const T & t) {
		return _query << t;
	}

	SQLiteDBI(const SQLiteDBI &) = delete;
	SQLiteDBI & operator = (const SQLiteDBI &) = delete;

private:
	friend class Query;
	void exec();

	sqlite3 * _db;
	Query _query;
};


}} // namespace uMediaServer::DBI

#endif
