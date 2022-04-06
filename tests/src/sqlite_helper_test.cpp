// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/sqlite_helper
//

#include <daw/sqlite_helper/sqlite3_class.h>

#include <iostream>

int main( ) {
	using namespace daw::db;
	auto db = daw::db::Sqlite3Db( );
	db.open( "db.sqlite" );
	if( not db.has_table( "tbl" ) ) {
		std::cout << "creating table\n";
		db.exec( "CREATE TABLE tbl ( ID NUMBER, FOO VARCHAR(100) );" );
	}
	assert( db.has_table( "tbl" ) );
	std::cout << "table exists\n";
}
