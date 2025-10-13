// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/sqlite_helper
//

#include <daw/sqlite/sqlite3_class.h>
#include <daw/daw_print.h>

int main( ) {
	// auto db = database( "db.sqlite" );
	auto db = daw::sqlite::database( ":memory:" );
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

	static constexpr daw::string_view sql =
	  "SELECT name FROM sqlite_schema WHERE type='table' ORDER BY name;";
	{
		// Test that an error occurs when more than 1 row is returned from db.exec
		// without callback and without specifying to ignore them
		auto it = db.exec( sql );
		auto const d = std::distance( it, it.end( ) );
		assert( d == 2 );

		std::cout << "Table names 2\n";
		for( auto const &row : db.exec( sql ) ) {
			auto value = row["name"];
			daw::println( "value.get_text( ): {}", value.get_text( ) );
			daw::println( "row.front( ).value.get_text( ): {}", row.front( ).value.get_text( ) );
		}
	}
}
