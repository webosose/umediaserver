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

namespace uMediaServer { namespace DBI {

Query::Shell::Shell(Query * query) : _query(query) {}

Query::Shell::~Shell() noexcept(false) {
	if (_query) {
		if (std::uncaught_exception())
			_query->reset();
		else
			_query->exec();
	}
}

Query::Shell::Shell(Shell && other) {
	_query = other._query;
	other._query = nullptr;
}

Query::Shell & Query::Shell::operator = (Shell && other) {
	if (this != &other) {
		_query = other._query;
		other._query = nullptr;
	}
	return *this;
}

Query::Shell & Query::Shell::operator , (IntoWrapper && into) {
	_query->_intos.push_back(std::move(into));
	return *this;
}

Query::Shell & Query::Shell::operator , (FromWrapper && from) {
	_query->_froms.push_back(std::move(from));
	return *this;
}

std::string Query::query() {
	return _query_stream.str();
}

void Query::reset() {
	_intos.clear(); _into = _intos.begin();
	_froms.clear(); _from = _froms.begin();
	_query_stream.str("");
}

void Query::exec() {
	_into = _intos.begin();
	_from = _froms.begin();

	try {
		_dbi->exec();
	} catch (...) {
		reset();
		throw;
	}
	reset();
}

}}
