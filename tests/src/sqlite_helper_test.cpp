// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/sqlite_helper
//

#include <daw/daw_ensure.h>
#include <daw/daw_iterator_traits.h>
#include <daw/daw_print.h>
#include <daw/pipelines/map.h>
#include <daw/pipelines/pipeline.h>
#include <daw/sqlite/sqlite3_class.h>

#include <array>
#include <cstddef>
#include <optional>
#include <ranges>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

// requires-expressions only SFINAE inside a template
template<typename Statement, typename Func>
concept can_visit = requires( Statement const &s, Func f ) { s.visit( f ); };

template<typename Statement, typename T>
concept can_row_as =
  requires( Statement const &s ) { s.template row_as<T>( ); };

struct typed_result {
	std::optional<std::int64_t> id;
	std::string text;
	std::optional<double> real;
};

struct wrong_typed_result {
	wrong_typed_result( std::int64_t, std::string );
};

struct RowType {
	int id;
	std::string name;

	friend bool operator==( RowType const &, RowType const & ) = default;
};

using iterator_contract_statement = daw::sqlite::typed_statement<int>;
using row_iterator = iterator_contract_statement::row_iterator;
using row_range = iterator_contract_statement::row_range;
static_assert( std::input_iterator<row_iterator> );
static_assert( not std::forward_iterator<row_iterator> );
static_assert( daw::InputIterator<row_iterator> );
static_assert( std::ranges::input_range<row_range> );

using object_statement = daw::sqlite::typed_statement<int, std::string>;
using object_range =
  decltype( std::declval<object_statement &>( ).rows_as<RowType>( ) );
static_assert( std::ranges::input_range<object_range> );
static_assert(
  std::same_as<std::ranges::range_value_t<object_range>, RowType> );

int main( ) {
	// auto db = database( "db.sqlite" );
	auto db = daw::sqlite::database( ":memory:" );
	if( not db.has_table( "tbl" ) ) {
		daw::println( "creating table" );
		db.exec( "CREATE TABLE tbl ( ID NUMBER, FOO VARCHAR(100) );" );
		db.exec( "CREATE TABLE tbl2 ( ID NUMBER, FOO VARCHAR(100) );" );
	}
	daw_ensure( db.has_table( "tbl" ) );
	daw::println( "table exists" );
	daw::println( "table names" );
	for( auto const &tbl : db.tables( ) ) {
		daw::println( "{}", tbl );
	}

	static constexpr daw::string_view sql =
	  "SELECT name FROM sqlite_schema WHERE type=? ORDER BY name;";
	{
		// Test that an error occurs when more than 1 row is returned from db.exec
		// without callback and without specifying to ignore them
		auto it = db.exec( sql, "table" );
		auto const d = std::distance( it, it.end( ) );
		daw_ensure( d == 2 );

		daw::println( "Table names 2" );
		for( auto const &row : db.exec( sql, "table" ) ) {
			auto value = row["name"];
			daw::println( "value.get_text( ): {}", value.get_text( ) );
			daw::println( "row.front( ).value.get_text( ): {}",
			              row.front( ).value.get_text( ) );
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
		ins.bind( 2,
		          daw::sqlite::cell_value(
		            daw::sqlite::types::blob_t( bytes.data( ), bytes.size( ) ) ) );
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
		daw_ensure( it->size( ) == 4 );
		daw_ensure( ( *it )["t"].get_text( ) == "hello" );
		auto const blob = ( *it )["b"].get_blob( );
		daw_ensure( blob.size( ) == 3 and blob[0] == std::byte{ 0x01 } and
		            blob[1] == std::byte{ 0x00 } and blob[2] == std::byte{ 0xFF } );
		daw_ensure( ( *it )["i"].get_integer( ) == 42 );
		daw_ensure( ( *it )["r"].get_float( ) == 1.5 );
		++it;
		// Empty values must stay empty, not become NULL
		daw_ensure( ( *it )["t"].get_type( ) == daw::sqlite::column_type::Text );
		daw_ensure( ( *it )["t"].get_text( ).empty( ) );
		daw_ensure( ( *it )["b"].get_type( ) == daw::sqlite::column_type::Blob );
		daw_ensure( ( *it )["i"].is_null( ) );
		daw_ensure( ( *it )["r"].is_null( ) );
		++it;
		daw_ensure( it == it.end( ) );
		daw::println( "bind round trip ok" );
	}
	{
		// Prepare errors carry the code and sqlite3_errmsg text
		bool threw = false;
		try {
			(void)db.exec( "SELECT * FROM no_such_table;" );
		} catch( daw::sqlite::sqlite3_exception const &ex ) {
			threw = true;
			daw_ensure( ex.error( ) == SQLITE_ERROR );
			daw_ensure( daw::string_view( ex.what( ) ).find( "no_such_table" ) !=
			            daw::string_view::npos );
		}
		daw_ensure( threw );
	}
	{
		// sqlite3_open_v2 honours flags; READONLY without CREATE must fail
		bool threw = false;
		try {
			auto ro = daw::sqlite::database( "/nonexistent/dir/x.sqlite",
			                                 SQLITE_OPEN_READONLY );
		} catch( daw::sqlite::sqlite3_exception const &ex ) {
			threw = true;
			daw_ensure( ex.error( ) == SQLITE_CANTOPEN );
		}
		daw_ensure( threw );
	}
	{
		// sqlite3_close_v2 lets a statement outlive its database
		auto stmt = daw::sqlite::shared_prepared_statement( );
		{
			auto tmp = daw::sqlite::database( ":memory:" );
			stmt = daw::sqlite::shared_prepared_statement( tmp, "SELECT 1;" );
		}
		auto it = daw::sqlite::query_iterator( stmt );
		daw_ensure( it->front( ).value.get_integer( ) == 1 );
	}
	{
		// prepare_flags on every construction path
		using daw::sqlite::prepare_flags;
		constexpr auto flags = prepare_flags::persistent | prepare_flags::no_vtab;
		static_assert( ( flags & prepare_flags::no_vtab ) ==
		               prepare_flags::no_vtab );
		auto f = prepare_flags::none;
		f |= prepare_flags::persistent;
		daw_ensure( f == prepare_flags::persistent );

		// unique statement with flags and several params
		auto ps = daw::sqlite::prepared_statement(
		  db,
		  "SELECT name FROM sqlite_schema WHERE type=? AND name=?;",
		  flags,
		  "table",
		  "tbl" );
		daw_ensure( db.exec( std::move( ps ) ).count( ) == 1 );
		// shared statement with flags and params
		auto sps = daw::sqlite::shared_prepared_statement(
		  db, "SELECT ? + ?;", flags, std::int64_t{ 2 }, std::int64_t{ 3 } );
		daw_ensure( db.exec( sps )->front( ).value.get_integer( ) == 5 );
		// exec with flags, with and without params
		daw_ensure( db.exec( sql, flags, "table" ).count( ) ==
		            3 ); // tbl, tbl2, vals
		daw_ensure( db.exec( "SELECT 1;", prepare_flags::persistent ).count( ) ==
		            1 );

		// no_vtab must reject virtual tables such as pragma_table_info
		auto const vtab_sql =
		  daw::string_view( "SELECT name FROM pragma_table_info('tbl');" );
		daw_ensure( db.exec( vtab_sql ).count( ) == 2 );
		bool threw = false;
		try {
			(void)db.exec( vtab_sql, prepare_flags::no_vtab );
		} catch( daw::sqlite::sqlite3_exception const & ) { threw = true; }
		daw_ensure( threw );
		daw::println( "prepare flags ok" );
	}
	{
		// Typed row access and visitation, validated once per row
		using daw::sqlite::types::blob_t;
		using daw::sqlite::types::text_t;
		auto it = db.exec( "SELECT t, b, i, r FROM vals ORDER BY rowid;" );
		auto [t, b, i, r] = it.get_row<text_t, blob_t, std::int64_t, double>( );
		daw_ensure( t == "hello" and b.size( ) == 3 and i == 42 and r == 1.5 );
		// Conversions and owning copies
		auto [ts, bs, i32, rf] = it.get_row<std::string, blob_t, int, float>( );
		daw_ensure( ts == "hello" and bs.size( ) == 3 and i32 == 42 and
		            rf == 1.5f );
		daw_ensure( to_string( ( *it )["i"] ) == "42" );
		// Reading a prefix of the columns is fine
		daw_ensure( std::get<0>( it.get_row<std::string>( ) ) == "hello" );

		std::size_t visited = 0;
		it.visit_row( [&]<typename T>( std::size_t column, T const &value ) {
			using daw::sqlite::types::integer_t;
			daw_ensure( column != 2 or ( std::same_as<T, integer_t> ) );
			if( column == 2 ) {
				if constexpr( std::same_as<T, integer_t> ) {
					daw_ensure( value == 42 );
				}
			}
			++visited;
		} );
		daw_ensure( visited == 4 );

		++it;
		// NULL needs std::optional, empty text is not NULL
		auto [t2, b2, i2, r2] = it.get_row<std::optional<std::string>,
		                                   std::optional<blob_t>,
		                                   std::optional<std::int64_t>,
		                                   std::optional<double>>( );
		daw_ensure( t2 and t2->empty( ) and b2 and b2->empty( ) and not i2 and
		            not r2 );
		bool threw = false;
		try {
			(void)it.get_row<std::string, blob_t, std::int64_t>( );
		} catch( daw::sqlite::sqlite3_exception const & ) { threw = true; }
		daw_ensure( threw );
		// The variant cell_value reads NULL fine
		daw_ensure(
		  std::get<2>( it.get_row<text_t, blob_t, daw::sqlite::cell_value>( ) )
		    .is_null( ) );
	}
	{
		// Range checks: narrowing integers and too many columns
		auto it = db.exec( "SELECT 300, 1;" );
		bool threw = false;
		try {
			(void)it.get_row<std::uint8_t>( );
		} catch( daw::sqlite::sqlite3_exception const & ) { threw = true; }
		daw_ensure( threw );
		threw = false;
		try {
			(void)it.get_row<int, int, int>( );
		} catch( daw::sqlite::sqlite3_exception const & ) { threw = true; }
		daw_ensure( threw );
		auto [a, flag] = it.get_row<short, bool>( );
		daw_ensure( a == 300 and flag );

		// Statement level single column visit
		auto stmt = daw::sqlite::shared_prepared_statement( db, "SELECT 2.5;" );
		auto row = daw::sqlite::query_iterator( stmt );
		auto const is_real = stmt.visit_column( 0, []<typename T>( T ) {
			return std::same_as<T, daw::sqlite::types::real_t>;
		} );
		daw_ensure( is_real );
		daw::println( "typed rows ok" );
	}
	{
		// typed_statement: column types in the type, checked once at prepare
		using daw::sqlite::types::text_t;
		auto stmt =
		  daw::sqlite::typed_statement<std::int64_t, text_t, std::optional<double>>(
		    db, "SELECT i, t, r FROM vals ORDER BY rowid;" );
		static_assert( decltype( stmt )::column_count == 3 );
		// The second row has a NULL i, which is not optional, so the row after
		// the first throws
		std::size_t n = 0;
		bool threw = false;
		try {
			for( auto [i, t, r] : stmt.rows( ) ) {
				daw_ensure( i == 42 and t == "hello" and r and *r == 1.5 );
				++n;
			}
		} catch( daw::sqlite::sqlite3_exception const & ) { threw = true; }
		daw_ensure( threw and n == 1 );
		// The range reset the statement while unwinding, so it can run again
		n = 0;
		threw = false;
		try {
			for( auto row : stmt.rows( ) ) {
				(void)row;
				++n;
			}
		} catch( daw::sqlite::sqlite3_exception const & ) { threw = true; }
		daw_ensure( threw and n == 1 );

		// Rerunning with new bindings, and early exit resets via the range
		auto by_t =
		  daw::sqlite::typed_statement<std::string, std::optional<std::int64_t>>(
		    db, "SELECT t, i FROM vals WHERE t = ?1;", "hello" );
		for( auto [t, i] : by_t.rows( ) ) {
			daw_ensure( t == "hello" and i and *i == 42 );
			break;
		}
		by_t.bind( 1, daw::sqlite::cell_value( text_t( "" ) ) );
		auto rows =
		  std::vector<std::tuple<std::string, std::optional<std::int64_t>>>( );
		for( auto row : by_t.rows( ) ) {
			rows.push_back( row );
		}
		daw_ensure( rows.size( ) == 1 and std::get<0>( rows[0] ).empty( ) and
		            not std::get<1>( rows[0] ) );

		// The iterator also meets daw::pipelines' iterator conventions.  Keep the
		// reset-owning range alive while a pipeline view refers to it.
		auto records = daw::sqlite::typed_statement<int, std::string>(
		  db, "SELECT 1, 'one' UNION ALL SELECT 2, 'two' ORDER BY 1;" );
		auto record_rows = records.rows( );
		auto converted_rows = daw::pipelines::pipeline(
		  record_rows, daw::pipelines::FromTuple<RowType> );
		auto converted = std::vector<RowType>( );
		for( auto row : converted_rows ) {
			converted.push_back( std::move( row ) );
		}
		daw_ensure(
		  ( converted == std::vector<RowType>{ { 1, "one" }, { 2, "two" } } ) );

		auto direct_records = daw::sqlite::typed_statement<int, std::string>(
		  db, "SELECT 3, 'three' UNION ALL SELECT 4, 'four' ORDER BY 1;" );
		auto direct = std::vector<RowType>( );
		for( RowType row : direct_records.rows_as<RowType>( ) ) {
			direct.push_back( std::move( row ) );
		}
		daw_ensure(
		  ( direct == std::vector<RowType>{ { 3, "three" }, { 4, "four" } } ) );

		auto post_increment = daw::sqlite::typed_statement<int>(
		  db, "SELECT 3 UNION ALL SELECT 4 ORDER BY 1;" );
		auto post_increment_rows = post_increment.rows( );
		auto row_it = post_increment_rows.begin( );
		daw_ensure( *row_it++ == std::tuple{ 3 } );
		daw_ensure( *row_it == std::tuple{ 4 } );
		daw_ensure( ++row_it == post_increment_rows.end( ) );

		// Manual stepping
		auto one = daw::sqlite::typed_statement<int, bool>( db, "SELECT 7, 1;" );
		daw_ensure( one.step( ) );
		daw_ensure( one.row( ) == std::tuple( 7, true ) );
		daw_ensure( not one.step( ) );

		// Column count mismatch is caught when prepared
		threw = false;
		try {
			auto bad = daw::sqlite::typed_statement<int>( db, "SELECT 1, 2;" );
		} catch( daw::sqlite::sqlite3_exception const & ) { threw = true; }
		daw_ensure( threw );
		daw::println( "typed_statement ok" );
	}
	{
		// typed_statement::row_as constructs a domain type from the current row
		auto stmt = daw::sqlite::typed_statement<std::optional<std::int64_t>,
		                                         std::string,
		                                         std::optional<double>>(
		  db, "SELECT i, t, r FROM vals ORDER BY rowid;" );
		static_assert( can_row_as<decltype( stmt ), typed_result> );
		static_assert( not can_row_as<decltype( stmt ), wrong_typed_result> );

		daw_ensure( stmt.step( ) );
		auto const first = stmt.row_as<typed_result>( );
		daw_ensure( first.id and *first.id == 42 and first.text == "hello" and
		            first.real and *first.real == 1.5 );
		daw_ensure( stmt.step( ) );
		auto const second = stmt.row_as<typed_result>( );
		daw_ensure( not second.id and second.text.empty( ) and not second.real );
		daw_ensure( not stmt.step( ) );
		daw::println( "row_as ok" );
	}
	{
		// typed_statement::visit calls func with the current row's columns
		using daw::sqlite::types::text_t;
		auto stmt = daw::sqlite::typed_statement<std::int64_t, text_t, double>(
		  db, "SELECT 42, 'hello', 1.5;" );
		daw_ensure( stmt.step( ) );
		// Arguments arrive as the declared column types
		auto const sum =
		  stmt.visit( []<typename I, typename T, typename R>( I i, T t, R r ) {
			  static_assert( std::same_as<I, std::int64_t> );
			  static_assert( std::same_as<T, text_t> );
			  static_assert( std::same_as<R, double> );
			  return static_cast<double>( i ) + static_cast<double>( t.size( ) ) + r;
		  } );
		daw_ensure( sum == 48.5 );
		// The result is returned as is, including references
		int target = 0;
		int &ref = stmt.visit( [&]( std::int64_t, text_t, double ) -> int & {
			return target;
		} );
		daw_ensure( &ref == &target );
		// A void visitor works
		std::int64_t seen = 0;
		stmt.visit( [&]( std::int64_t i, text_t, double ) {
			seen = i;
		} );
		daw_ensure( seen == 42 );
		daw_ensure( not stmt.step( ) );

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
					daw_ensure( i and *i == 42 and t == "hello" );
				} else {
					daw_ensure( not i and t.empty( ) );
				}
			} );
			++rows;
		}
		daw_ensure( rows == 2 );
		auto strict = daw::sqlite::typed_statement<std::int64_t>(
		  db, "SELECT i FROM vals ORDER BY rowid DESC;" );
		daw_ensure( strict.step( ) );
		bool threw = false;
		try {
			(void)strict.visit( []( std::int64_t i ) {
				return i;
			} );
		} catch( daw::sqlite::sqlite3_exception const & ) { threw = true; }
		daw_ensure( threw );
		strict.reset( );

		// cell_value columns pass through the variant
		auto dynamic = daw::sqlite::typed_statement<daw::sqlite::cell_value>(
		  db, "SELECT NULL;" );
		daw_ensure( dynamic.step( ) );
		daw_ensure( dynamic.visit( []( daw::sqlite::cell_value const &c ) {
			return c.is_null( );
		} ) );
		dynamic.reset( );

		// cell_value::visit with an overload set
		auto const kind = []( daw::sqlite::cell_value const &c ) {
			return c.visit(
			  []( daw::sqlite::types::real_t ) {
				  return 'r';
			  },
			  []( daw::sqlite::types::integer_t ) {
				  return 'i';
			  },
			  []( text_t ) {
				  return 't';
			  },
			  []( daw::sqlite::types::blob_t const & ) {
				  return 'b';
			  },
			  []( daw::sqlite::cell_value::null_cell_t ) {
				  return 'n';
			  } );
		};
		daw_ensure( kind( daw::sqlite::cell_value( 1.5 ) ) == 'r' );
		daw_ensure( kind( daw::sqlite::cell_value( std::int64_t{ 1 } ) ) == 'i' );
		daw_ensure( kind( daw::sqlite::cell_value( text_t( "x" ) ) ) == 't' );
		daw_ensure(
		  kind( daw::sqlite::cell_value( daw::sqlite::types::blob_t( ) ) ) == 'b' );
		daw_ensure( kind( daw::sqlite::cell_value( ) ) == 'n' );
		daw::println( "visit ok" );
	}
	daw::println( "All good" );
}
