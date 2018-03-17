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

#include <list>
#include <set>
#include <algorithm>
#include <boost/fusion/adapted.hpp>
#include "Registry.h"
#include "db_schema.h"


namespace uMediaServer { namespace Reg {
struct _TableInfo {
	size_t cid;
	std::string name;
	std::string type;
	bool notnull;
	std::string dflt_value;
	bool pk;
};
}}

BOOST_FUSION_ADAPT_STRUCT(
		uMediaServer::Reg::_TableInfo,
		(size_t, cid)
		(std::string, name)
		(std::string, type)
		(bool, notnull)
		(std::string, dflt_value)
		(bool, pk))

namespace uMediaServer { namespace Reg {

using namespace uMediaServer::DBI;
const std::string Registry::globals_table = "globals";
const std::string Registry::globals_key   = "key";
const std::string Registry::globals_value = "value";

Registry::Registry(const dbi_ptr_t & dbi)
	:_sql(dbi), _log(UMS_LOG_CONTEXT_SERVER) {
	*_sql << registry_schema;
}

Registry::dbi_ptr_t Registry::dbi() const {
	return _sql;
}

bool Registry::apply(const libconfig::Config & config) {
	*_sql << "begin transaction;";
	try {
		const libconfig::Setting & root = config.getRoot();
		for (size_t index = 0; index < root.getLength(); ++index) {
			const libconfig::Setting & section = root[index];
			if ( section.getLength() == 0 ) {                  /* scalar setting */
				try {
					set_global_entry(section);
				} catch (const Exception & e) {
					log_exception(e, kPmLogLevel_Warning);
				}
			} else
				apply_section(section);                       /* composite setting */
		}
	} catch (const Exception & e) {
		*_sql << "rollback;";
		log_exception(e, kPmLogLevel_Error);
		return false;
	}
	*_sql << "commit;";
	return true;
}

void Registry::apply_section(const libconfig::Setting & section,
							 const std::string & foreign_key,
							 const std::string & foreign_val) {
	std::string table_name = section.getName();
	// store given config entry into section table
	auto append_entry = [&](const libconfig::Setting & entry) {
		std::list<size_t> subsections;
		{
			std::list<std::string> strings;
			std::list<int> integers;
			std::list<double> floats;
			std::string comma;
			std::stringstream column_names, placeholders;
			auto query = *_sql << "replace into ";
			query << table_name;
			if (!foreign_key.empty()) {
				column_names << comma << foreign_key;
				placeholders << comma << ":" << foreign_key;
				query , from(foreign_val);
				comma = ", ";
			}
			for (size_t i = 0; i < entry.getLength(); ++i) {
				const libconfig::Setting & atom = entry[i];
				if (atom.getType() == libconfig::Setting::TypeList) {
					subsections.push_back(i);
				} else {
					column_names << comma << atom.getName();
					placeholders << comma << ":" << atom.getName();
					comma = ", ";
					switch (atom.getType()) {
						case libconfig::Setting::TypeString:
							try {
									strings.push_back((const char *)atom);
									query , from(strings.back());
							} catch (...) {} break;
						case libconfig::Setting::TypeInt:
						case libconfig::Setting::TypeInt64:
							try {
									integers.push_back(atom);
									query , from(integers.back());
							} catch (...) {} break;
						case libconfig::Setting::TypeFloat:
							try {
									floats.push_back(atom);
									query , from(floats.back());
							} catch (...) {} break;
					}
				}
			}
			query << "(" << column_names.str() << ")"
				  << " values" << "(" << placeholders.str() << ");";
		}
		for (auto s : subsections) {
			try {
				apply_section(entry[s], entry[0].getName(), (const char *)entry[0]);
			} catch (...) {
				// failed to apply_section but we can recover
			}
		}
	};
	// fill table
	for (size_t index = 0; index < section.getLength(); ++index) {
		const auto & entry = section[index];
		try {
			append_entry(entry);
		} catch (const Exception & e) {
			log_exception(e, kPmLogLevel_Warning);
		}
	}
}

std::map<std::string, Registry::TableSchema>::iterator
		Registry::update_table_info(const std::string &table) {
	std::list<_TableInfo> info;
	*_sql << "pragma table_info(" << table << ");", into(info);
	TableSchema schema;
	schema.has_primary_key = false;
	for (auto it = info.begin(); it != info.end(); ++it) {
		if (it->pk)
			schema.has_primary_key = true;
		schema.columns.push_back({it->name, it->type});
	}
	return _db_schema.insert({table, schema}).first;
}

const Registry::TableSchema & Registry::get_table_schema(const std::string & table) {
	auto ti_it = _db_schema.find(table);
	if (ti_it == _db_schema.end())
		ti_it = update_table_info(table);
	return ti_it->second;
}

bool Registry::del(const std::string & section, const std::string & key) {
	try {
		const auto & ts = get_table_schema(section);
		*_sql << "delete from " << section << " where "
			  << ts.columns.front().name << "=?;", from(key);
	} catch (const Exception & e) {
		log_exception(e, kPmLogLevel_Warning);
		return false;
	}
	return true;
}

void Registry::set_global_entry(const libconfig::Setting & entry) {
	std::string entry_name(entry.getName()), entry_value;
	try {
		entry_value = (const char *)entry;
	} catch (...) {
		throw SchemaError() << boost::throw_file(__FILE__)
							<< boost::throw_line(__LINE__)
							<< boost::throw_function(BOOST_CURRENT_FUNCTION);
	}
	this->set(entry_name, entry_value);
}

bool Registry::get(const std::string & key, std::string & value) {
	try {
		*_sql << "select " << globals_value << " from " << globals_table
			  << " where " << globals_key << "=?;", from(key), into(value);
	} catch (const Exception & e) {
		log_exception(e, kPmLogLevel_Warning);
		return false;
	}
	return true;

}

bool Registry::set(const std::string & key, const std::string & value) {
	try {
		*_sql << "replace into " << globals_table
			  << " values(?,?);", from(key), from(value);
	} catch (const Exception & e) {
		log_exception(e, kPmLogLevel_Warning);
		return false;
	}
	return true;
}

bool Registry::del(const std::string & key) {
	try {
		*_sql << "delete from " << globals_table
			  << " where " << globals_key << "=?;", from(key);
	} catch (const Exception & e) {
		log_exception(e, kPmLogLevel_Warning);
		return false;
	}
	return true;
}

void Registry::log_exception(const Exception & e, LogLevel l) {
	__KV extras;
	if (auto * info_ptr = boost::get_error_info<throw_db_error>(e))
		extras["DB_ERR"] = *info_ptr;
	if (auto * info_ptr = boost::get_error_info<throw_db_uri>(e))
		extras["DB_URI"] = *info_ptr;
	__log_print_ex(_log, l, MSGERR_DBI_ERROR, extras,
				   __EXTRACT_FILE(*boost::get_error_info<boost::throw_file>(e)),
				   *boost::get_error_info<boost::throw_function>(e),
				   *boost::get_error_info<boost::throw_line>(e), "%s", e.what());
}

}} // namespace uMediaServer::Registry
