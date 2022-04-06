// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/sqlite_helper
//

#include <daw/sqlite/sqlite3_class.h>

#include <iostream>

int main( ) {
	using namespace daw::sqlite;
	auto db = database( "db.sqlite" );
	if( not db.has_table( "tbl" ) ) {
		std::cout << "creating table\n";
		db.exec( "CREATE TABLE tbl ( ID NUMBER, FOO VARCHAR(100) );" );
		db.exec( "CREATE TABLE tbl2 ( ID NUMBER, FOO VARCHAR(100) );" );
	}
	assert( db.has_table( "tbl" ) );
	std::cout << "table exists\n";
	std::cout << "table names\n";
	for( auto const &tbl : db.tables( ) ) {
		std::cout << tbl << '\n';
	}

	{
		// Test that an error occurs when more than 1 row is returned from db.exec without callback and
		// without specifying to ignore them
		bool only_one = true;
		try {
			(void)db.exec(
			  "SELECT name FROM sqlite_schema WHERE type='table' ORDER BY name;" );
		} catch( std::exception const &ex ) {
			(void)ex;
			only_one = false;
		} catch( ... ) {
			std::cerr << "WTF\n";
			throw;
		}
		assert( not only_one );
	}
}
