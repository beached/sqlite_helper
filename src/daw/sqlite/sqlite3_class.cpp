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
#include <daw/daw_out_ptr.h>
#include <daw/daw_string_view.h>

#include <cstddef>
#include <format>
#include <iostream>
#include <sqlite3.h>
#include <sstream>
#include <string>
#include <utility>

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
	  : m_error( err_no )
	  , m_message( sqlite3_errstr( err_no ) ) {}

	sqlite3_exception::sqlite3_exception( int err_no, std::string message )
	  : m_error( err_no )
	  , m_message( std::move( message ) ) {}

	char const *sqlite3_exception::what( ) const noexcept {
		return m_message.c_str( );
	}

	int sqlite3_exception::error( ) const {
		return m_error;
	}

	sqlite3_exception::sqlite3_exception( std::string message )
	  : m_message( std::move( message ) ) {}

	void database::open( std::filesystem::path const &filename, int flags ) {
		// sqlite expects UTF-8 filenames on every platform
		auto const fname = filename.u8string( );
		auto ptr = unique_sqlite_ptr( );
		auto rc = sqlite3_open_v2( reinterpret_cast<char const *>( fname.c_str( ) ),
		                           daw::out_ptr( ptr ),
		                           flags,
		                           nullptr );
		if( rc != SQLITE_OK ) {
			// A handle is usually allocated even on failure and must be closed
			auto message = std::format(
			  "Could not open database {}: {}",
			  filename.string( ),
			  ( ptr ? sqlite3_errmsg( ptr.get( ) ) : sqlite3_errstr( rc ) ) );
			throw sqlite3_exception( rc, std::move( message ) );
		}
		m_db = std::move( ptr );
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
		auto first = exec( sql );
		auto last = first.end( );
		auto const row_count = first.count( );
		return daw::vector<std::string>(
		  do_resize_and_overwrite,
		  row_count,
		  [&]( std::string *ptr, std::size_t sz ) {
			  while( first != last ) {
				  std::construct_at(
				    ptr,
				    static_cast<std::string>( first->front( ).value.get_text( ) ) );
				  ++first;
				  ++ptr;
			  }
			  return sz;
		  } );
	}

	bool database::has_table( daw::string_view table_name ) {
		static constexpr daw::string_view sql =
		  "SELECT name FROM sqlite_schema WHERE type='table' and name=?;";
		auto const row_count = exec( sql, table_name ).count( );
		assert( row_count <= 1 );
		return row_count == 1;
	}

	query_iterator database::exec( prepared_statement statement ) {
		assert( m_db );
		return query_iterator( std::move( statement ) );
	}

	query_iterator database::exec( shared_prepared_statement statement ) {
		assert( m_db );
		return query_iterator( std::move( statement ) );
	}

	database::database( std::filesystem::path const &filename, int flags ) {
		open( filename, flags );
	}

	sqlite3 *database::release( ) {
		m_is_open.reset( );
		return m_db.release( );
	}

	database::database( sqlite3 *db )
	  : m_db( db )
	  , m_is_open( true ) {
		assert( db );
	}

	namespace {
		constexpr auto to_cell_value = []( auto value ) {
			return cell_value( value );
		};
	} // namespace

	cell_value::cell_value( prepared_statement &statement, size_t column )
	  : cell_value( statement.visit_column( column, to_cell_value ) ) {}

	cell_value::cell_value( shared_prepared_statement &statement, size_t column )
	  : cell_value( statement.visit_column( column, to_cell_value ) ) {}

	std::string to_string( cell_value const &value ) {
		return value.visit(
		  []( types::real_t v ) {
			  return std::to_string( v );
		  },
		  []( types::integer_t v ) {
			  return std::to_string( v );
		  },
		  []( types::text_t v ) {
			  return static_cast<std::string>( v );
		  },
		  []( types::blob_t const &v ) {
			  return types::to_string( v );
		  },
		  []( cell_value::null_cell_t ) {
			  return std::string( "{Null}" );
		  } );
	}
} // namespace daw::sqlite
