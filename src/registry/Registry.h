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

#ifndef __REGISTRY_H__
#define __REGISTRY_H__

#include <string>
#include <memory>
#include <map>
#include <boost/fusion/adapted.hpp>
#include <boost/mpl/range_c.hpp>
#include <libconfig.h++>
#include <dbi.h>
#include "Logger_macro.h"

namespace uMediaServer { namespace Reg {

namespace traits = uMediaServer::DBI::traits;

struct SchemaError : DBI::Exception {
	SchemaError(const std::string & error = "database schema error") :
		DBI::Exception(error) {}
};

/**
 * DB powered Registry interface
 */
class Registry {
	typedef DBI::SQLiteDBI dbi_t;
	typedef std::shared_ptr<dbi_t> dbi_ptr_t;

	struct TableSchema {
		struct ColumnSchema {
			std::string name;
			std::string type;
		};
		bool has_primary_key;
		std::vector<ColumnSchema> columns;
	};

public:
	static std::shared_ptr<Registry> instance(const std::string & dbi_uri = ":memory:") {
		static std::shared_ptr<Registry> _instance;
		if (_instance == nullptr) {
			_instance.reset(new Registry(dbi_uri));
		}
		return _instance;
	}

	/**
	 * Creates Registry
	 * @param IN pointer to SQLiteDBI connection
	 */
	Registry(const dbi_ptr_t & dbi);
	/**
	 * Creates Registry and DBI Engine
	 * @param IN DBI uri
	 */
	Registry(const std::string & db_uri) : Registry(dbi_ptr_t(new dbi_t(db_uri))) {}

	// noncopyable
	Registry(const Registry &) = delete;
	Registry & operator = (const Registry &) = delete;

	/**
	 * Apply Configuration to Registry from given config
	 * @param IN const reference to config object
	 * @returns true in case of success otherwise false
	 */
	bool apply(const libconfig::Config & config);

	/**
	 * Provides access to DBI engine
	 * @returns pointer to SQLiteDBI connection
	 */
	dbi_ptr_t dbi() const;

	// regular object interface
	/**
	 * Gets stored in registry object by its section and key
	 * @param IN registry section (aka table)
	 * @param IN object key (aka primary key)
	 * @param OUT templated object to fill
	 * @returns true in case of success and false otherwise
	 */
	template <typename POD, typename
			  std::enable_if<traits::is_sequence<POD>::value>::type* = nullptr>
	bool get(const std::string & section, const std::string & key, POD & pod);
	/**
	 * Insers / updates registry object
	 * @param IN registry section (aka table)
	 * @param IN templated object to store
	 * @returns true in case of success and false otherwise
	 */
	template <typename POD, typename
			  std::enable_if<traits::is_sequence<POD>::value>::type* = nullptr>
	bool set(const std::string & section, const POD & pod);
	/**
	 * Removess registry object by its section and key
	 * @param IN registry section (aka table)
	 * @param IN object key (aka primary key)
	 * @returns true in case of success and false otherwise
	 */
	bool del(const std::string & section, const std::string & key);
	// collection interface
	/**
	 * Gets list of stored object attachements by its section and foreign key
	 * @param IN registry section (aka table)
	 * @param IN parent object key (aka foreign key)
	 * @param OUT templated list of attachements to fill
	 * @returns true in case of success and false otherwise
	 */
	template <typename POD, typename
			  std::enable_if<traits::is_sequence<POD>::value>::type* = nullptr>
	bool get(const std::string & section, const std::string & key, std::list<POD> & list);
	// globals
	/**
	 * Gets global registry object by its key
	 * @param IN object key
	 * @param OUT global value stored
	 * @returns true in case of success and false otherwise
	 */
	bool get(const std::string & key, std::string & value);
	/**
	 * Updates / inserts global registry object by its key
	 * @param IN object key (aka primary key)
	 * @param IN global value to store
	 * @returns true in case of success and false otherwise
	 */
	bool set(const std::string & key, const std::string & value);
	/**
	 * Deletes global registry object by its key
	 * @param IN object key (aka primary key)
	 * @returns true in case of success and false otherwise
	 */
	bool del(const std::string & key);

private:
	static const std::string globals_table;
	static const std::string globals_key;
	static const std::string globals_value;

	void apply_section(const libconfig::Setting & section,
					   const std::string & foreign_key = std::string(),
					   const std::string & foreign_val = std::string());
	void set_global_entry(const libconfig::Setting & entry);

	std::map<std::string, TableSchema>::iterator update_table_info(const std::string & table);
	const TableSchema & get_table_schema(const std::string & table);

	void log_exception(const uMediaServer::DBI::Exception &, LogLevel);

	std::map<std::string, TableSchema> _db_schema;
	dbi_ptr_t _sql;
	Logger _log;
};

namespace detail {

template <typename Sequence>
struct columns_folder {
	columns_folder(DBI::Query::Shell & q)
		: _query(q) {}
	template<typename Index>
	void operator ()(const Index &) const {
		using namespace boost::fusion::extension;
		_query << (Index::value ? ", " : "") << prefix
			   << struct_member_name<Sequence, Index::value>::call();
	}
	DBI::Query::Shell & _query;
	std::string prefix;
};

}

template <typename POD, typename
		  std::enable_if<traits::is_sequence<POD>::value>::type*>
inline bool Registry::get(const std::string & section, const std::string & key, POD & pod) {
	using namespace uMediaServer::DBI;
	using namespace boost::fusion;
	try {
		auto query = *_sql << "select ";
		boost::mpl::range_c<size_t, 0, result_of::size<POD>::value> indices;
		detail::columns_folder<POD> fold_columns(query);
		for_each(indices, fold_columns);
		query << " from " << section << " where "
			  << extension::struct_member_name<POD, 0>::call()
			  << "=?;", from(key), into(pod);
	} catch (const Exception & e) {
		log_exception(e, kPmLogLevel_Warning);
		return false;
	}
	return true;
}

template <typename POD, typename
		  std::enable_if<traits::is_sequence<POD>::value>::type*>
inline bool Registry::set(const std::string & section, const POD & pod) {
	using namespace uMediaServer::DBI;
	using namespace boost::fusion;
	try {
		auto query = *_sql << "replace into ";
		query << section << "(";
		boost::mpl::range_c<size_t, 0, result_of::size<POD>::value> indices;
		detail::columns_folder<POD> fold_columns(query);
		for_each(indices, fold_columns);
		query << ") values (";
		fold_columns.prefix = ":";
		for_each(indices, fold_columns);
		query << ");", from(pod);
	} catch (const Exception & e) {
		log_exception(e, kPmLogLevel_Warning);
		return false;
	}
	return true;
}

template <typename POD, typename
		  std::enable_if<traits::is_sequence<POD>::value>::type*>
inline bool Registry::get(const std::string & section, const std::string & key, std::list<POD> & list) {
	using namespace uMediaServer::DBI;
	using namespace boost::fusion;
	try {
		// TODO: consider to deprecate usage of TableSchema
		const auto & ts = get_table_schema(section);
		auto query = *_sql << "select ";
		boost::mpl::range_c<size_t, 0, result_of::size<POD>::value> indices;
		detail::columns_folder<POD> fold_columns(query);
		for_each(indices, fold_columns);
		query << " from " << section << " where "
			  << ts.columns.front().name << "=?;", from(key), into(list);
	} catch (const Exception & e) {
		log_exception(e, kPmLogLevel_Warning);
		return false;
	}
	return true;
}

}} // namespace uMediaServer::Registry

#endif
