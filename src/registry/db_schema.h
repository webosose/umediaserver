#ifndef __DB_SCHEMA_H__
#define __DB_SCHEMA_H__

namespace uMediaServer { namespace Reg {

static const char * registry_schema = R"__(
		drop table if exists globals;
		create table globals (
			key         text primary key,
			value       text
		);
		drop table if exists resources;
		create table resources (
			id          text primary key,
			name        text default '',
			qty         integer default 0
		);
		drop table if exists pipelines;
		create table pipelines (
			type              text primary key,
			name              text default '',
			bin               text,
			pool_size         integer default 0,
			pool_fill_delay   integer default 0,
			pool_refill_delay integer default 0,
			schema_file       text default '',
			priority          integer default 4,
			max_restarts      integer default 0
		);
		drop table if exists environment;
		create table environment (
			type            text,
			name            text,
			value           text,
			op              text,
			unique(type, name) on conflict replace
		);
)__";

}} // namespace uMediaServer::Reg

#endif // __DB_SCHEMA_H__
