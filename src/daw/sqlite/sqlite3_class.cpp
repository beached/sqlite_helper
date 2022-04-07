// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/sqlite_helper
//

#include "daw/sqlite/sqlite3_class.h"
#include "daw/sqlite/prepared_statement.h"
#include "daw/sqlite/query_iterator.h"

#include <daw/daw_contiguous_view.h>
#include <daw/daw_move.h>
#include <daw/daw_string_view.h>

#include <cstddef>
#include <iostream>
#include <sqlite3.h>
#include <sstream>
#include <string>

namespace daw::sqlite {
	namespace types {
		std::string to_string( blob_t const &blob ) {
			std::ostringstream ss;
			for( auto const &current_byte : blob ) {
				ss << std::hex << static_cast<unsigned char>( current_byte );
			}
			return ss.str( );
		}
	} // namespace types

	sqlite3_exception::sqlite3_exception( int err_no )
	  : m_message( sqlite3_errstr( err_no ) ) {}

	char const *sqlite3_exception::what( ) const noexcept {
		return m_message.c_str( );
	}

	int sqlite3_exception::error( ) const {
		return m_error;
	}

	sqlite3_exception::sqlite3_exception( std::string message )
	  : m_message( DAW_MOVE( message ) ) {}

	void database::open( std::filesystem::path filename ) {
		sqlite3 *ptr = nullptr;
		auto result = sqlite3_open( filename.c_str( ), &ptr );
		if( result ) {
			throw sqlite3_exception( "Could not open database " + static_cast<std::string>( filename ) +
			                         ": " + sqlite3_errmsg( ptr ) );
		}
		m_db.reset( ptr );
		m_is_open = true;
	}

	void database::close( ) {
		m_db.reset( );
		m_is_open.reset( );
	}

	sqlite3 const *database::get_handle( ) const {
		assert( m_db );
		return m_db.get( );
	}

	sqlite3 *database::get_handle( ) {
		assert( m_db );
		return m_db.get( );
	}

	daw::vector<std::string> database::tables( ) {
		static constexpr daw::string_view sql =
		  "SELECT name FROM sqlite_schema WHERE type='table' ORDER BY name;";
		auto first = exec( prepared_statement( *this, sql ) );
		auto last = first.end( );
		auto const row_count = first.count( );
		return daw::vector<std::string>(
		  do_resize_and_overwrite,
		  row_count,
		  [&]( std::string *ptr, std::size_t sz ) {
			  while( first != last ) {
				  std::construct_at( ptr, static_cast<std::string>( first->front( ).value.get_text( ) ) );
				  ++first;
				  ++ptr;
			  }
			  return sz;
		  } );
	}

	bool database::has_table( daw::string_view table_name ) {
		static constexpr daw::string_view sql =
		  "SELECT name FROM sqlite_schema WHERE type='table' and name=?;";
		auto const row_count = exec( prepared_statement( *this, sql, table_name ) ).count( );
		assert( row_count <= 1 );
		return row_count == 1;
	}

	query_iterator database::exec( prepared_statement statement ) {
		assert( m_db );
		return query_iterator( DAW_MOVE( statement ) );
	}

	query_iterator database::exec( shared_prepared_statement statement ) {
		assert( m_db );
		return query_iterator( DAW_MOVE( statement ) );
	}

	database::database( std::filesystem::path filename ) {
		open( filename );
	}

	namespace {
		void delete_text_or_blob( void *val ) {
			auto ptr = static_cast<char const *>( val );
			delete[] ptr;
		}
	} // namespace

	namespace {
		template<typename T>
		requires( requires { typename T::i_am_a_prepared_statement; } ) cell_value::value_t
		  get_column( T &statement, size_t column ) {
			switch( statement.get_column_type( column ) ) {
			case column_type::Float:
				return cell_value::value_t( std::in_place_type<types::real_t>,
				                            statement.get_column_float( column ) );
			case column_type::Integer:
				return cell_value::value_t( std::in_place_type<types::integer_t>,
				                            statement.get_column_integer( column ) );
			case column_type::Text:
				return cell_value::value_t( std::in_place_type<types::text_t>,
				                            statement.get_column_text( column ) );
			case column_type::Blob:
				return cell_value::value_t( std::in_place_type<types::blob_t>,
				                            statement.get_column_blob( column ) );
			case column_type::Null:
				return cell_value::value_t( std::in_place_type<cell_value::null_cell_t>,
				                            cell_value::null_cell_t{ } );
			default:
				std::cerr << "Unknown sqlite3 column type returned" << std::endl;
				std::terminate( );
			}
		}
	} // namespace

	cell_value::cell_value( prepared_statement &statement, size_t column )
	  : m_value{ get_column( statement, column ) } {}

	cell_value::cell_value( shared_prepared_statement &statement, size_t column )
	  : m_value{ get_column( statement, column ) } {}

	std::string to_string( cell_value const &value ) {
		switch( value.get_type( ) ) {
		case column_type::Float:
			return std::to_string( value.get_float( ) );
		case column_type::Integer:
			return std::to_string( value.get_integer( ) );
		case column_type::Text:
			return static_cast<std::string>( value.get_text( ) );
		case column_type::Blob:
			return types::to_string( value.get_blob( ) );
		case column_type::Null:
			return "{Null}";
		default:
			std::cerr << "Unknown sqlite3 column type returned" << std::endl;
			std::terminate( );
		}
	}
} // namespace daw::sqlite
