// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/sqlite_helper
//

#include <daw/sqlite/sqlite3_class.h>
#include <daw/daw_print.h>

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

// requires-expressions only SFINAE inside a template
template<typename Statement, typename Func>
concept can_visit = requires( Statement const &s, Func f ) { s.visit( f ); };

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
	  "SELECT name FROM sqlite_schema WHERE type=? ORDER BY name;";
	{
		// Test that an error occurs when more than 1 row is returned from db.exec
		// without callback and without specifying to ignore them
		auto it = db.exec( sql, "table" );
		auto const d = std::distance( it, it.end( ) );
		assert( d == 2 );

		std::cout << "Table names 2\n";
		for( auto const &row : db.exec( sql, "table" ) ) {
			auto value = row["name"];
			daw::println( "value.get_text( ): {}", value.get_text( ) );
			daw::println( "row.front( ).value.get_text( ): {}", row.front( ).value.get_text( ) );
		}
	}

	{
		// Round trip each bindable type through sqlite3_bind_*64/prepare_v3
		db.exec( "CREATE TABLE vals ( t TEXT, b BLOB, i INTEGER, r REAL );" );
		static constexpr auto bytes =
		  std::array{ std::byte{ 0x01 }, std::byte{ 0x00 }, std::byte{ 0xFF } };
		auto ins = daw::sqlite::shared_prepared_statement(
		  db,
		  "INSERT INTO vals VALUES( ?, ?, ?, ? );",
		  daw::sqlite::prepare_flags::persistent );
		ins.bind( 1, daw::sqlite::cell_value( daw::string_view( "hello" ) ) );
		ins.bind( 2, daw::sqlite::cell_value( daw::sqlite::types::blob_t(
		               bytes.data( ), bytes.size( ) ) ) );
		ins.bind( 3, daw::sqlite::cell_value( std::int64_t{ 42 } ) );
		ins.bind( 4, daw::sqlite::cell_value( 1.5 ) );
		(void)db.exec( ins );
		ins.reset( );
		ins.bind( 1, daw::sqlite::cell_value( daw::string_view( ) ) );
		ins.bind( 2, daw::sqlite::cell_value( daw::sqlite::types::blob_t( ) ) );
		ins.bind( 3 );
		ins.bind( 4 );
		(void)db.exec( ins );

		auto it = db.exec( "SELECT t, b, i, r FROM vals ORDER BY rowid;" );
		assert( it->size( ) == 4 );
		assert( ( *it )["t"].get_text( ) == "hello" );
		auto const blob = ( *it )["b"].get_blob( );
		assert( blob.size( ) == 3 and blob[0] == std::byte{ 0x01 } and
		        blob[1] == std::byte{ 0x00 } and blob[2] == std::byte{ 0xFF } );
		assert( ( *it )["i"].get_integer( ) == 42 );
		assert( ( *it )["r"].get_float( ) == 1.5 );
		++it;
		// Empty values must stay empty, not become NULL
		assert( ( *it )["t"].get_type( ) == daw::sqlite::column_type::Text );
		assert( ( *it )["t"].get_text( ).empty( ) );
		assert( ( *it )["b"].get_type( ) == daw::sqlite::column_type::Blob );
		assert( ( *it )["i"].is_null( ) );
		assert( ( *it )["r"].is_null( ) );
		++it;
		assert( it == it.end( ) );
		daw::println( "bind round trip ok" );
	}
	{
		// Prepare errors carry the code and sqlite3_errmsg text
		bool threw = false;
		try {
			(void)db.exec( "SELECT * FROM no_such_table;" );
		} catch( daw::sqlite::sqlite3_exception const &ex ) {
			threw = true;
			assert( ex.error( ) == SQLITE_ERROR );
			assert( daw::string_view( ex.what( ) ).find( "no_such_table" ) !=
			        daw::string_view::npos );
		}
		assert( threw );
	}
	{
		// sqlite3_open_v2 honours flags; READONLY without CREATE must fail
		bool threw = false;
		try {
			auto ro = daw::sqlite::database( "/nonexistent/dir/x.sqlite",
			                                 SQLITE_OPEN_READONLY );
		} catch( daw::sqlite::sqlite3_exception const &ex ) {
			threw = true;
			assert( ex.error( ) == SQLITE_CANTOPEN );
		}
		assert( threw );
	}
	{
		// sqlite3_close_v2 lets a statement outlive its database
		auto stmt = daw::sqlite::shared_prepared_statement( );
		{
			auto tmp = daw::sqlite::database( ":memory:" );
			stmt = daw::sqlite::shared_prepared_statement( tmp, "SELECT 1;" );
		}
		auto it = daw::sqlite::query_iterator( stmt );
		assert( it->front( ).value.get_integer( ) == 1 );
	}
	{
		// prepare_flags on every construction path
		using daw::sqlite::prepare_flags;
		constexpr auto flags = prepare_flags::persistent | prepare_flags::no_vtab;
		static_assert( ( flags & prepare_flags::no_vtab ) == prepare_flags::no_vtab );
		auto f = prepare_flags::none;
		f |= prepare_flags::persistent;
		assert( f == prepare_flags::persistent );

		// unique statement with flags and several params
		auto ps = daw::sqlite::prepared_statement(
		  db,
		  "SELECT name FROM sqlite_schema WHERE type=? AND name=?;",
		  flags,
		  "table",
		  "tbl" );
		assert( db.exec( std::move( ps ) ).count( ) == 1 );
		// shared statement with flags and params
		auto sps = daw::sqlite::shared_prepared_statement(
		  db, "SELECT ? + ?;", flags, std::int64_t{ 2 }, std::int64_t{ 3 } );
		assert( db.exec( sps )->front( ).value.get_integer( ) == 5 );
		// exec with flags, with and without params
		assert( db.exec( sql, flags, "table" ).count( ) == 3 ); // tbl, tbl2, vals
		assert( db.exec( "SELECT 1;", prepare_flags::persistent ).count( ) == 1 );

		// no_vtab must reject virtual tables such as pragma_table_info
		auto const vtab_sql = daw::string_view(
		  "SELECT name FROM pragma_table_info('tbl');" );
		assert( db.exec( vtab_sql ).count( ) == 2 );
		bool threw = false;
		try {
			(void)db.exec( vtab_sql, prepare_flags::no_vtab );
		} catch( daw::sqlite::sqlite3_exception const & ) { threw = true; }
		assert( threw );
		daw::println( "prepare flags ok" );
	}
	{
		// Typed row access and visitation, validated once per row
		using daw::sqlite::types::blob_t;
		using daw::sqlite::types::text_t;
		auto it = db.exec( "SELECT t, b, i, r FROM vals ORDER BY rowid;" );
		auto [t, b, i, r] = it.get_row<text_t, blob_t, std::int64_t, double>( );
		assert( t == "hello" and b.size( ) == 3 and i == 42 and r == 1.5 );
		// Conversions and owning copies
		auto [ts, bs, i32, rf] = it.get_row<std::string, blob_t, int, float>( );
		assert( ts == "hello" and bs.size( ) == 3 and i32 == 42 and rf == 1.5f );
		assert( to_string( ( *it )["i"] ) == "42" );
		// Reading a prefix of the columns is fine
		assert( std::get<0>( it.get_row<std::string>( ) ) == "hello" );

		std::size_t visited = 0;
		it.visit_row( [&]<typename T>( std::size_t column, T const &value ) {
			using daw::sqlite::types::integer_t;
			if( column == 2 ) {
				assert( ( std::same_as<T, integer_t> ) );
				if constexpr( std::same_as<T, integer_t> ) {
					assert( value == 42 );
				}
			}
			++visited;
		} );
		assert( visited == 4 );

		++it;
		// NULL needs std::optional, empty text is not NULL
		auto [t2, b2, i2, r2] =
		  it.get_row<std::optional<std::string>, std::optional<blob_t>,
		             std::optional<std::int64_t>, std::optional<double>>( );
		assert( t2 and t2->empty( ) and b2 and b2->empty( ) and not i2 and
		        not r2 );
		bool threw = false;
		try {
			(void)it.get_row<std::string, blob_t, std::int64_t>( );
		} catch( daw::sqlite::sqlite3_exception const & ) { threw = true; }
		assert( threw );
		// The variant cell_value reads NULL fine
		assert( std::get<2>( it.get_row<text_t, blob_t, daw::sqlite::cell_value>( ) )
		          .is_null( ) );
	}
	{
		// Range checks: narrowing integers and too many columns
		auto it = db.exec( "SELECT 300, 1;" );
		bool threw = false;
		try {
			(void)it.get_row<std::uint8_t>( );
		} catch( daw::sqlite::sqlite3_exception const & ) { threw = true; }
		assert( threw );
		threw = false;
		try {
			(void)it.get_row<int, int, int>( );
		} catch( daw::sqlite::sqlite3_exception const & ) { threw = true; }
		assert( threw );
		auto [a, flag] = it.get_row<short, bool>( );
		assert( a == 300 and flag );

		// Statement level single column visit
		auto stmt = daw::sqlite::shared_prepared_statement( db, "SELECT 2.5;" );
		auto row = daw::sqlite::query_iterator( stmt );
		auto const is_real = stmt.visit_column( 0, []<typename T>( T ) {
			return std::same_as<T, daw::sqlite::types::real_t>;
		} );
		assert( is_real );
		daw::println( "typed rows ok" );
	}
	{
		// typed_statement: column types in the type, checked once at prepare
		using daw::sqlite::types::text_t;
		auto stmt = daw::sqlite::typed_statement<std::int64_t, text_t,
		                                         std::optional<double>>(
		  db, "SELECT i, t, r FROM vals ORDER BY rowid;" );
		static_assert( decltype( stmt )::column_count == 3 );
		// The second row has a NULL i, which is not optional, so the row after
		// the first throws
		std::size_t n = 0;
		bool threw = false;
		try {
			for( auto [i, t, r] : stmt.rows( ) ) {
				assert( i == 42 and t == "hello" and r and *r == 1.5 );
				++n;
			}
		} catch( daw::sqlite::sqlite3_exception const & ) { threw = true; }
		assert( threw and n == 1 );
		// The range reset the statement while unwinding, so it can run again
		n = 0;
		threw = false;
		try {
			for( auto row : stmt.rows( ) ) {
				(void)row;
				++n;
			}
		} catch( daw::sqlite::sqlite3_exception const & ) { threw = true; }
		assert( threw and n == 1 );

		// Rerunning with new bindings, and early exit resets via the range
		auto by_t = daw::sqlite::typed_statement<std::string, std::optional<std::int64_t>>(
		  db, "SELECT t, i FROM vals WHERE t = ?1;", "hello" );
		for( auto [t, i] : by_t.rows( ) ) {
			assert( t == "hello" and i and *i == 42 );
			break;
		}
		by_t.bind( 1, daw::sqlite::cell_value( text_t( "" ) ) );
		auto rows = std::vector<std::tuple<std::string, std::optional<std::int64_t>>>( );
		for( auto row : by_t.rows( ) ) {
			rows.push_back( row );
		}
		assert( rows.size( ) == 1 and std::get<0>( rows[0] ).empty( ) and
		        not std::get<1>( rows[0] ) );

		// Manual stepping
		auto one = daw::sqlite::typed_statement<int, bool>( db, "SELECT 7, 1;" );
		assert( one.step( ) );
		assert( one.row( ) == std::tuple( 7, true ) );
		assert( not one.step( ) );

		// Column count mismatch is caught when prepared
		threw = false;
		try {
			auto bad = daw::sqlite::typed_statement<int>( db, "SELECT 1, 2;" );
		} catch( daw::sqlite::sqlite3_exception const & ) { threw = true; }
		assert( threw );
		daw::println( "typed_statement ok" );
	}
	{
		// typed_statement::visit calls func with the current row's columns
		using daw::sqlite::types::text_t;
		auto stmt = daw::sqlite::typed_statement<std::int64_t, text_t, double>(
		  db, "SELECT 42, 'hello', 1.5;" );
		assert( stmt.step( ) );
		// Arguments arrive as the declared column types
		auto const sum = stmt.visit(
		  []<typename I, typename T, typename R>( I i, T t, R r ) {
			  static_assert( std::same_as<I, std::int64_t> );
			  static_assert( std::same_as<T, text_t> );
			  static_assert( std::same_as<R, double> );
			  return static_cast<double>( i ) + static_cast<double>( t.size( ) ) + r;
		  } );
		assert( sum == 48.5 );
		// The result is returned as is, including references
		int target = 0;
		int &ref = stmt.visit( [&]( std::int64_t, text_t, double ) -> int & {
			return target;
		} );
		assert( &ref == &target );
		// A void visitor works
		std::int64_t seen = 0;
		stmt.visit( [&]( std::int64_t i, text_t, double ) { seen = i; } );
		assert( seen == 42 );
		assert( not stmt.step( ) );

		// The callable must accept the column types
		auto const sum_like = []( std::int64_t, text_t, double ) {};
		auto const wrong_arity = []( std::int64_t, text_t ) {};
		auto const wrong_type = []( std::int64_t, std::int64_t, double ) {};
		static_assert( can_visit<decltype( stmt ), decltype( sum_like )> );
		static_assert( not can_visit<decltype( stmt ), decltype( wrong_arity )> );
		static_assert( not can_visit<decltype( stmt ), decltype( wrong_type )> );

		// NULL: std::optional gets nullopt, other types throw
		auto nullable =
		  daw::sqlite::typed_statement<std::optional<std::int64_t>, std::string>(
		    db, "SELECT i, t FROM vals ORDER BY rowid;" );
		std::size_t rows = 0;
		while( nullable.step( ) ) {
			nullable.visit( [&]( std::optional<std::int64_t> i, std::string t ) {
				if( rows == 0 ) {
					assert( i and *i == 42 and t == "hello" );
				} else {
					assert( not i and t.empty( ) );
				}
			} );
			++rows;
		}
		assert( rows == 2 );
		auto strict = daw::sqlite::typed_statement<std::int64_t>(
		  db, "SELECT i FROM vals ORDER BY rowid DESC;" );
		assert( strict.step( ) );
		bool threw = false;
		try {
			(void)strict.visit( []( std::int64_t i ) { return i; } );
		} catch( daw::sqlite::sqlite3_exception const & ) { threw = true; }
		assert( threw );
		strict.reset( );

		// cell_value columns pass through the variant
		auto dynamic = daw::sqlite::typed_statement<daw::sqlite::cell_value>(
		  db, "SELECT NULL;" );
		assert( dynamic.step( ) );
		assert( dynamic.visit( []( daw::sqlite::cell_value const &c ) {
			return c.is_null( );
		} ) );
		dynamic.reset( );

		// cell_value::visit with an overload set
		auto const kind = []( daw::sqlite::cell_value const &c ) {
			return c.visit(
			  []( daw::sqlite::types::real_t ) { return 'r'; },
			  []( daw::sqlite::types::integer_t ) { return 'i'; },
			  []( text_t ) { return 't'; },
			  []( daw::sqlite::types::blob_t const & ) { return 'b'; },
			  []( daw::sqlite::cell_value::null_cell_t ) { return 'n'; } );
		};
		assert( kind( daw::sqlite::cell_value( 1.5 ) ) == 'r' );
		assert( kind( daw::sqlite::cell_value( std::int64_t{ 1 } ) ) == 'i' );
		assert( kind( daw::sqlite::cell_value( text_t( "x" ) ) ) == 't' );
		assert( kind( daw::sqlite::cell_value( daw::sqlite::types::blob_t( ) ) ) ==
		        'b' );
		assert( kind( daw::sqlite::cell_value( ) ) == 'n' );
		daw::println( "visit ok" );
	}
	daw::println( "All good" );
}
