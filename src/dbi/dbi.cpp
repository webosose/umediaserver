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

#include "dbi.h"
#include <unistd.h>

namespace uMediaServer { namespace DBI {

static const char * bind_err = "unable to bind value";

namespace {
typedef class SQLiteStatement {
public:
	SQLiteStatement() : stmt(nullptr) {}
	~SQLiteStatement() {
		sqlite3_finalize(stmt);
	}
	sqlite3_stmt ** operator &() {
		sqlite3_finalize(stmt);
		return &stmt;
	}
	operator sqlite3_stmt * () {
		return stmt;
	}
	operator bool() const {
		return stmt == nullptr;
	}
private:
	sqlite3_stmt * stmt;
} stmt_t;
}

SQLiteDBI::SQLiteDBI(const std::string & db_uri)
	: _db(nullptr), _query(this) {
	int sql_result = sqlite3_open(db_uri.c_str(), &_db);
	if (sql_result != SQLITE_OK) {
		throw OpenError() << boost::throw_file(__FILE__)
						  << boost::throw_line(__LINE__)
						  << boost::throw_function(BOOST_CURRENT_FUNCTION)
						  << throw_db_uri(db_uri)
						  << throw_db_error(sqlite3_errstr(sql_result));
	}
}

SQLiteDBI::~SQLiteDBI() {
	sqlite3_close(_db);
}

void SQLiteDBI::exec() {
	std::string query = _query.query();
	stmt_t stmt;
	const char * tail = query.c_str();
	int sql_result = SQLITE_OK;

	auto dispatch_int = [this](int val) {
		auto & store_type = _query.next_into_type();
		if (store_type == typeid(std::reference_wrapper<int>)) {
			_query.set_next(val);
		} else if (store_type == typeid(std::reference_wrapper<size_t>)) {
			_query.set_next(size_t(val));
		} else if (store_type == typeid(std::reference_wrapper<bool>)) {
			_query.set_next(bool(val));
		} else {
			throw ConvError() << boost::throw_file(__FILE__)
							  << boost::throw_line(__LINE__)
							  << boost::throw_function(BOOST_CURRENT_FUNCTION)
							  << throw_db_error(bind_err)
							  << throw_typeid(store_type.name());
		}
	};
	auto dispatch_text = [this](const unsigned char * val) {
		auto & store_type = _query.next_into_type();
		if (store_type == typeid(std::reference_wrapper<std::string>)) {
			_query.set_next(std::string((const char *)val));
		} else {
			throw ConvError() << boost::throw_file(__FILE__)
							  << boost::throw_line(__LINE__)
							  << boost::throw_function(BOOST_CURRENT_FUNCTION)
							  << throw_db_error(bind_err)
							  << throw_typeid(store_type.name());
		}
	};
	auto dispatch_float = [this](double val) {
		auto & store_type = _query.next_into_type();
		if (store_type == typeid(std::reference_wrapper<float>)) {
			_query.set_next(float(val));
		} else if (store_type == typeid(std::reference_wrapper<double>)) {
			_query.set_next(val);
		} else {
			throw ConvError() << boost::throw_file(__FILE__)
							  << boost::throw_line(__LINE__)
							  << boost::throw_function(BOOST_CURRENT_FUNCTION)
							  << throw_db_error(bind_err)
							  << throw_typeid(store_type.name());
		}
	};
	auto dispatch_null = [this]() {
		auto & store_type = _query.next_into_type();
		if (store_type == typeid(std::reference_wrapper<float>)) {
			_query.set_next(0.0f);
		} else if (store_type == typeid(std::reference_wrapper<double>)) {
			_query.set_next(0.0);
		} else if (store_type == typeid(std::reference_wrapper<int>)) {
			_query.set_next(0);
		} else if (store_type == typeid(std::reference_wrapper<size_t>)) {
			_query.set_next(0);
		} else if (store_type == typeid(std::reference_wrapper<bool>)) {
			_query.set_next(false);
		} else if (store_type == typeid(std::reference_wrapper<std::string>)) {
			_query.set_next(std::string());
		} else {
			throw ConvError() << boost::throw_file(__FILE__)
							  << boost::throw_line(__LINE__)
							  << boost::throw_function(BOOST_CURRENT_FUNCTION)
							  << throw_db_error(bind_err)
							  << throw_typeid(store_type.name());
		}
	};

	auto dispatch_value = [&](int index) {
		sql_result = sqlite3_column_type(stmt, index);
		switch (sql_result) {
			case SQLITE_TEXT:
				dispatch_text(sqlite3_column_text(stmt, index)); break;
			case SQLITE_INTEGER:
				dispatch_int(sqlite3_column_int(stmt, index)); break;
			case SQLITE_FLOAT:
				dispatch_float(sqlite3_column_double(stmt, index)); break;
			case SQLITE_NULL:
				dispatch_null(); break;
			default:
				throw ConvError() << boost::throw_file(__FILE__)
								  << boost::throw_line(__LINE__)
								  << boost::throw_function(BOOST_CURRENT_FUNCTION)
								  << throw_db_error(bind_err)
								  << throw_typeid(typeid(void*).name());
		}
	};

	auto bind_value = [this, &stmt](int index) {
		++index; // what? they count indices starting from 1
		auto & store_type = _query.next_from_type();
		if (store_type == typeid(std::reference_wrapper<const int>)) {
			int val; _query.get_next(val);
			return sqlite3_bind_int(stmt, index, val);
		} else if (store_type == typeid(std::reference_wrapper<const size_t>)) {
			size_t val; _query.get_next(val);
			return sqlite3_bind_int(stmt, index, int(val));
		} else if (store_type == typeid(std::reference_wrapper<const bool>)) {
			bool val; _query.get_next(val);
			return sqlite3_bind_int(stmt, index, int(val));
		} else if (store_type == typeid(std::reference_wrapper<const std::string>)) {
			std::string val; _query.get_next(val);
			return sqlite3_bind_text(stmt, index, val.c_str(), -1, SQLITE_TRANSIENT);
		} else if (store_type == typeid(std::reference_wrapper<const double>)) {
			double val; _query.get_next(val);
			return sqlite3_bind_double(stmt, index, val);
		} else if (store_type == typeid(std::reference_wrapper<const float>)) {
			float val; _query.get_next(val);
			return sqlite3_bind_double(stmt, index, double(val));
		}
		throw ConvError() << boost::throw_file(__FILE__)
						  << boost::throw_line(__LINE__)
						  << boost::throw_function(BOOST_CURRENT_FUNCTION)
						  << throw_db_error(bind_err)
						  << throw_typeid(store_type.name());
	};

	do {
		sql_result = sqlite3_prepare_v2(_db, tail, -1, &stmt, &tail);
		if (sql_result != SQLITE_OK) {
			throw ExecError() << boost::throw_file(__FILE__)
							  << boost::throw_line(__LINE__)
							  << boost::throw_function(BOOST_CURRENT_FUNCTION)
							  << throw_db_error(sqlite3_errstr(sql_result));
		}
		if (stmt) {
			// binding values
			int placeholders_count = sqlite3_bind_parameter_count(stmt);
			for (int placeholder_index = 0; placeholder_index < placeholders_count; ++placeholder_index) {
				sql_result = bind_value(placeholder_index);
				if (sql_result != SQLITE_OK) {
					throw ConvError() << boost::throw_file(__FILE__)
									  << boost::throw_line(__LINE__)
									  << boost::throw_function(BOOST_CURRENT_FUNCTION)
									  << throw_db_error(sqlite3_errstr(sql_result))
									  << throw_typeid(typeid(void*).name());
				}
			}
			// executing query
			sql_result = sqlite3_step(stmt);
			while (sql_result == SQLITE_ROW) {
				// collecting data
				int column_count = sqlite3_data_count(stmt);
				for (int column_index = 0; column_index < column_count; ++column_index)
					dispatch_value(column_index);
				sql_result = sqlite3_step(stmt);
			};
			if(sql_result != SQLITE_DONE) {
				throw ExecError() << boost::throw_file(__FILE__)
								  << boost::throw_line(__LINE__)
								  << boost::throw_function(BOOST_CURRENT_FUNCTION)
								  << throw_db_error(sqlite3_errstr(sql_result));
			}
			sql_result = sqlite3_reset(stmt);
			if(sql_result != SQLITE_OK) {
				throw ExecError() << boost::throw_file(__FILE__)
								  << boost::throw_line(__LINE__)
								  << boost::throw_function(BOOST_CURRENT_FUNCTION)
								  << throw_db_error(sqlite3_errstr(sql_result));
			}
		}
	} while (strlen(tail));
}

}}
