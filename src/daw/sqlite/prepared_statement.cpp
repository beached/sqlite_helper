// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/sqlite_helper
//

#include "daw/sqlite/query_iterator.h"
#include "daw/sqlite/sqlite3_class.h"

#include <daw/daw_contiguous_view.h>
#include <daw/daw_string_view.h>

#include <cstddef>
#include <iostream>
#include <sqlite3.h>
#include <sstream>
#include <string>

namespace daw::sqlite {
	static_assert( static_cast<unsigned>( prepare_flags::persistent ) ==
	               SQLITE_PREPARE_PERSISTENT );
	static_assert( static_cast<unsigned>( prepare_flags::normalize ) ==
	               SQLITE_PREPARE_NORMALIZE );
	static_assert( static_cast<unsigned>( prepare_flags::no_vtab ) ==
	               SQLITE_PREPARE_NO_VTAB );
#if defined( SQLITE_PREPARE_DONT_LOG )
	static_assert( static_cast<unsigned>( prepare_flags::dont_log ) ==
	               SQLITE_PREPARE_DONT_LOG );
#endif

	namespace {
		sqlite3_stmt *prepare( database &db, daw::string_view sql,
		                       prepare_flags flags ) {
			assert( sql.size( ) <= std::numeric_limits<int>::max( ) );
			sqlite3_stmt *st = nullptr;
			auto rc = sqlite3_prepare_v3( db.get_handle( ),
			                              sql.data( ),
			                              static_cast<int>( sql.size( ) ),
			                              static_cast<unsigned>( flags ),
			                              &st,
			                              nullptr );
			if( rc != SQLITE_OK ) {
				throw sqlite3_exception( rc, sqlite3_errmsg( db.get_handle( ) ) );
			}
			return st;
		}

		void bind_value( sqlite3_stmt *stmt, std::size_t index,
		                 cell_value const &value ) {
			assert( index <= std::numeric_limits<int>::max( ) );
			auto const idx = static_cast<int>( index );
			auto rc = value.visit(
			  [&]( types::real_t v ) {
				  return sqlite3_bind_double( stmt, idx, v );
			  },
			  [&]( types::integer_t v ) {
				  return sqlite3_bind_int64( stmt, idx, v );
			  },
			  [&]( types::text_t v ) {
				  // A null pointer would bind NULL instead of an empty string
				  auto const *first = v.data( ) ? v.data( ) : "";
				  return sqlite3_bind_text64( stmt,
				                              idx,
				                              first,
				                              static_cast<sqlite3_uint64>( v.size( ) ),
				                              SQLITE_TRANSIENT,
				                              SQLITE_UTF8 );
			  },
			  [&]( types::blob_t const &v ) {
				  if( v.empty( ) ) {
					  // A null pointer would bind NULL instead of an empty blob
					  return sqlite3_bind_zeroblob64( stmt, idx, 0 );
				  }
				  return sqlite3_bind_blob64( stmt,
				                              idx,
				                              v.data( ),
				                              static_cast<sqlite3_uint64>( v.size( ) ),
				                              SQLITE_TRANSIENT );
			  },
			  [&]( cell_value::null_cell_t ) {
				  return sqlite3_bind_null( stmt, idx );
			  } );
			if( rc != SQLITE_OK ) {
				throw sqlite3_exception( rc );
			}
		}

		void bind_null( sqlite3_stmt *stmt, std::size_t index ) {
			assert( index <= std::numeric_limits<int>::max( ) );
			auto rc = sqlite3_bind_null( stmt, static_cast<int>( index ) );
			if( rc != SQLITE_OK ) {
				throw sqlite3_exception( rc );
			}
		}
	} // namespace

	prepared_statement::prepared_statement( database &db, daw::string_view sql,
	                                        prepare_flags flags )
	  : m_statement( prepare( db, sql, flags ) ) {}

	namespace {
		inline void validate( prepared_statement &statement, std::size_t column ) {
			if( not statement.is_good( ) ) {
				throw sqlite3_exception( "Attempt to use an invalid statement" );
			}
			if( statement.get_column_count( ) <= column ) {
				throw sqlite3_exception( "Column specified is out of range" );
			}
		}
	} // namespace

	daw::string_view prepared_statement::get_column_name( std::size_t column ) {
		validate( *this, column );
		return { sqlite3_column_name( get( ), static_cast<int>( column ) ) };
	}

	types::real_t prepared_statement::get_column_float( std::size_t column ) {
		validate( *this, column );
		return sqlite3_column_double( get( ), static_cast<int>( column ) );
	}

	types::integer_t
	prepared_statement::get_column_integer( std::size_t column ) {
		validate( *this, column );
		return sqlite3_column_int64( get( ), static_cast<int>( column ) );
	}

	types::text_t prepared_statement::get_column_text( std::size_t column ) {
		validate( *this, column );
		auto first = reinterpret_cast<char const *>(
		  sqlite3_column_text( get( ), static_cast<int>( column ) ) );
		return daw::string_view(
		  first, sqlite3_column_bytes( get( ), static_cast<int>( column ) ) );
	}

	bool prepared_statement::is_column_null( std::size_t column ) {
		return column_type::Null == get_column_type( column );
	}

	types::blob_t prepared_statement::get_column_blob( std::size_t column ) {
		validate( *this, column );
		auto first = reinterpret_cast<std::byte const *>(
		  sqlite3_column_blob( get( ), static_cast<int>( column ) ) );
		return types::blob_t( first,
		                      static_cast<std::size_t>( sqlite3_column_bytes(
		                        get( ), static_cast<int>( column ) ) ) );
	}

	bool prepared_statement::is_good( ) const {
		return static_cast<bool>( m_statement );
	}

	void prepared_statement::reset( ) {
		auto rc = sqlite3_reset( m_statement.get( ) );
		if( rc != SQLITE_OK ) {
			throw sqlite3_exception( rc );
		}
	}

	void prepared_statement::reset_to_default_init( ) {
		m_statement.reset( );
	}

	void prepared_statement::bind( std::size_t index, cell_value const &value ) {
		bind_value( m_statement.get( ), index, value );
	}

	void prepared_statement::bind( std::size_t index ) {
		bind_null( m_statement.get( ), index );
	}

	column_type prepared_statement::get_column_type( std::size_t column ) {
		validate( *this, column );

		switch(
		  sqlite3_column_type( m_statement.get( ), static_cast<int>( column ) ) ) {
		case SQLITE_INTEGER:
			return column_type::Integer;
		case SQLITE_FLOAT:
			return column_type::Float;
		case SQLITE_TEXT:
			return column_type::Text;
		case SQLITE_BLOB:
			return column_type::Blob;
		case SQLITE_NULL:
			return column_type::Null;
		default:
			std::cerr << "Unknown sqlite3 column type returned" << std::endl;
			std::terminate( );
		}
	}

	shared_prepared_statement::shared_prepared_statement( database &db,
	                                                      daw::string_view sql,
	                                                      prepare_flags flags )
	  : m_statement( prepare( db, sql, flags ),
	                 ps_impl::sqlite3_stmt_deleter{ } ) {}

	shared_prepared_statement::shared_prepared_statement(
	  prepared_statement statement )
	  : m_statement( statement.m_statement.release( ),
	                 ps_impl::sqlite3_stmt_deleter{ } ) {}

	sqlite3_stmt *shared_prepared_statement::get( ) {
		return m_statement.get( );
	}

	std::size_t shared_prepared_statement::get_column_count( ) {
		auto const count = sqlite3_column_count( get( ) );
		assert( 0 <= count );
		return static_cast<size_t>( count );
	}

	namespace {
		inline void validate( shared_prepared_statement &statement,
		                      std::size_t column ) {
			if( not statement.is_good( ) ) {
				throw sqlite3_exception( "Attempt to use an invalid statement" );
			} else if( statement.get_column_count( ) <= column ) {
				throw sqlite3_exception( "Column specified is out of range" );
			}
		}
	} // namespace

	daw::string_view
	shared_prepared_statement::get_column_name( std::size_t column ) {
		validate( *this, column );
		return daw::string_view(
		  sqlite3_column_name( get( ), static_cast<int>( column ) ) );
	}

	types::real_t
	shared_prepared_statement::get_column_float( std::size_t column ) {
		validate( *this, column );
		return sqlite3_column_double( get( ), static_cast<int>( column ) );
	}

	types::integer_t
	shared_prepared_statement::get_column_integer( std::size_t column ) {
		validate( *this, column );
		return sqlite3_column_int64( get( ), static_cast<int>( column ) );
	}

	types::text_t
	shared_prepared_statement::get_column_text( std::size_t column ) {
		validate( *this, column );
		auto first = reinterpret_cast<char const *>(
		  sqlite3_column_text( get( ), static_cast<int>( column ) ) );
		return daw::string_view(
		  first, sqlite3_column_bytes( get( ), static_cast<int>( column ) ) );
	}

	bool shared_prepared_statement::is_column_null( std::size_t column ) {
		return column_type::Null == get_column_type( column );
	}

	types::blob_t
	shared_prepared_statement::get_column_blob( std::size_t column ) {
		validate( *this, column );
		auto first = reinterpret_cast<std::byte const *>(
		  sqlite3_column_blob( get( ), static_cast<int>( column ) ) );
		return types::blob_t( first,
		                      static_cast<std::size_t>( sqlite3_column_bytes(
		                        get( ), static_cast<int>( column ) ) ) );
	}

	bool shared_prepared_statement::is_good( ) const {
		return nullptr != m_statement;
	}

	void shared_prepared_statement::reset( ) {
		auto rc = sqlite3_reset( m_statement.get( ) );
		if( rc != SQLITE_OK ) {
			throw sqlite3_exception( rc );
		}
	}

	void shared_prepared_statement::reset_to_default_init( ) {
		m_statement.reset( );
	}

	void shared_prepared_statement::bind( std::size_t index,
	                                      cell_value const &value ) {
		bind_value( m_statement.get( ), index, value );
	}

	void shared_prepared_statement::bind( std::size_t index ) {
		bind_null( m_statement.get( ), index );
	}

	column_type shared_prepared_statement::get_column_type( std::size_t column ) {
		validate( *this, column );

		switch(
		  sqlite3_column_type( m_statement.get( ), static_cast<int>( column ) ) ) {
		case SQLITE_INTEGER:
			return column_type::Integer;
		case SQLITE_FLOAT:
			return column_type::Float;
		case SQLITE_TEXT:
			return column_type::Text;
		case SQLITE_BLOB:
			return column_type::Blob;
		case SQLITE_NULL:
			return column_type::Null;
		default:
			std::cerr << "Unknown sqlite3 column type returned" << std::endl;
			std::terminate( );
		}
	}

	void ps_impl::sqlite3_stmt_deleter::operator( )( sqlite3_stmt *ptr ) const {
		if( ptr ) {
			sqlite3_finalize( ptr );
		}
	}
} // namespace daw::sqlite