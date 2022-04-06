// The MIT License (MIT)
//
// Copyright (c) 2014-2017 Darrell Wright
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files( the "Software" ), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and / or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "daw/sqlite/query_iterator.h"
#include "daw/sqlite/sqlite3_class.h"

#include <daw/daw_contiguous_view.h>
#include <daw/daw_move.h>
#include <daw/daw_string_view.h>

#include <cstddef>
#include <iostream>
#include <sqlite3.h>
#include <sstream>
#include <string>

namespace daw::sqlite {
	prepared_statement::prepared_statement( database &db, daw::string_view sql ) {
		assert( sql.size( ) <= std::numeric_limits<int>::max( ) );
		sqlite3_stmt *st = nullptr;
		auto rc = sqlite3_prepare_v2( db.get_handle( ),
		                              sql.data( ),
		                              static_cast<int>( sql.size( ) ),
		                              &st,
		                              nullptr );
		if( rc != SQLITE_OK ) {
			throw sqlite3_exception( rc );
		}
		m_statement.reset( st );
	}

	sqlite3_stmt *prepared_statement::get( ) {
		return m_statement.get( );
	}

	size_t prepared_statement::get_column_count( ) {
		auto const count = sqlite3_column_count( get( ) );
		assert( 0 <= count );
		return static_cast<size_t>( count );
	}

	namespace {
		inline void validate( prepared_statement &statement, size_t column ) {
			if( not statement.is_good( ) ) {
				throw sqlite3_exception( "Attempt to use an invalid statement" );
			} else if( statement.get_column_count( ) <= column ) {
				throw sqlite3_exception( "Column specified is out of range" );
			}
		}
	} // namespace

	daw::string_view prepared_statement::get_column_name( size_t column ) {
		validate( *this, column );
		return daw::string_view( sqlite3_column_name( get( ), static_cast<int>( column ) ) );
	}

	types::real_t prepared_statement::get_column_float( size_t column ) {
		validate( *this, column );
		return sqlite3_column_double( get( ), static_cast<int>( column ) );
	}

	types::integer_t prepared_statement::get_column_integer( size_t column ) {
		validate( *this, column );
		return sqlite3_column_int64( get( ), static_cast<int>( column ) );
	}

	types::text_t prepared_statement::get_column_text( size_t column ) {
		validate( *this, column );
		auto first =
		  reinterpret_cast<char const *>( sqlite3_column_text( get( ), static_cast<int>( column ) ) );
		return daw::string_view( first, sqlite3_column_bytes( get( ), static_cast<int>( column ) ) );
	}

	bool prepared_statement::is_column_null( size_t column ) {
		return column_type::Null == get_column_type( column );
	}

	types::blob_t prepared_statement::get_column_blob( size_t column ) {
		validate( *this, column );
		auto first = reinterpret_cast<std::byte const *>(
		  sqlite3_column_blob( get( ), static_cast<int>( column ) ) );
		return types::blob_t(
		  first,
		  static_cast<std::size_t>( sqlite3_column_bytes( get( ), static_cast<int>( column ) ) ) );
	}

	bool prepared_statement::is_good( ) const {
		return nullptr != m_statement;
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

	namespace {
		void delete_text_or_blob( void *val ) {
			auto ptr = static_cast<char const *>( val );
			delete[] ptr;
		}
	} // namespace

	void prepared_statement::bind( size_t index, cell_value const &value ) {
		assert( index <= std::numeric_limits<int>::max( ) );
		auto rc = SQLITE_ERROR;
		switch( value.get_type( ) ) {
		case column_type::Float:
			rc = sqlite3_bind_double( m_statement.get( ), static_cast<int>( index ), value.get_float( ) );
			break;
		case column_type::Integer:
			rc =
			  sqlite3_bind_int64( m_statement.get( ), static_cast<int>( index ), value.get_integer( ) );
			break;
		case column_type::Text: {
			auto const &val = value.get_text( );
			auto *arry = new char[val.size( )]; // deleted by sqlite3_bind_text
			                                    // 5th parameter function
			std::copy( std::data( val ), daw::data_end( val ), arry );
			rc = sqlite3_bind_text( m_statement.get( ),
			                        static_cast<int>( index ),
			                        arry,
			                        val.size( ),
			                        delete_text_or_blob );
		} break;
		case column_type::Blob: {
			auto const &val = value.get_blob( );
			auto *arry = new std::byte[val.size( )]; // deleted by sqlite3_bind_blob 5th
			                                         // parameter function
			std::copy( val.begin( ), val.end( ), arry );
			rc = sqlite3_bind_blob( m_statement.get( ),
			                        static_cast<int>( index ),
			                        arry,
			                        val.size( ),
			                        delete_text_or_blob );
		} break;
		case column_type::Null:
			rc = sqlite3_bind_null( m_statement.get( ), static_cast<int>( index ) );
			break;
		default:
			std::cerr << "Unknown sqlite3 column type returned" << std::endl;
			std::terminate( );
		}
		if( rc != SQLITE_OK ) {
			throw sqlite3_exception( rc );
		}
	}

	void prepared_statement::bind( size_t index ) {}

	column_type prepared_statement::get_column_type( size_t column ) {
		validate( *this, column );

		switch( sqlite3_column_type( m_statement.get( ), static_cast<int>( column ) ) ) {
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

	shared_prepared_statement::shared_prepared_statement( database &db, daw::string_view sql ) {
		assert( sql.size( ) <= std::numeric_limits<int>::max( ) );
		sqlite3_stmt *st = nullptr;
		auto rc = sqlite3_prepare_v2( db.get_handle( ),
		                              sql.data( ),
		                              static_cast<int>( sql.size( ) ),
		                              &st,
		                              nullptr );
		if( rc != SQLITE_OK ) {
			throw sqlite3_exception( rc );
		}
		m_statement.reset( st, ps_impl::sqlite3_stmt_deleter{ } );
	}

	shared_prepared_statement::shared_prepared_statement( prepared_statement statement )
	  : m_statement( statement.m_statement.release( ), ps_impl::sqlite3_stmt_deleter{ } ) {}

	sqlite3_stmt *shared_prepared_statement::get( ) {
		return m_statement.get( );
	}

	size_t shared_prepared_statement::get_column_count( ) {
		auto const count = sqlite3_column_count( get( ) );
		assert( 0 <= count );
		return static_cast<size_t>( count );
	}

	namespace {
		inline void validate( shared_prepared_statement &statement, size_t column ) {
			if( not statement.is_good( ) ) {
				throw sqlite3_exception( "Attempt to use an invalid statement" );
			} else if( statement.get_column_count( ) <= column ) {
				throw sqlite3_exception( "Column specified is out of range" );
			}
		}
	} // namespace

	daw::string_view shared_prepared_statement::get_column_name( size_t column ) {
		validate( *this, column );
		return daw::string_view( sqlite3_column_name( get( ), static_cast<int>( column ) ) );
	}

	types::real_t shared_prepared_statement::get_column_float( size_t column ) {
		validate( *this, column );
		return sqlite3_column_double( get( ), static_cast<int>( column ) );
	}

	types::integer_t shared_prepared_statement::get_column_integer( size_t column ) {
		validate( *this, column );
		return sqlite3_column_int64( get( ), static_cast<int>( column ) );
	}

	types::text_t shared_prepared_statement::get_column_text( size_t column ) {
		validate( *this, column );
		auto first =
		  reinterpret_cast<char const *>( sqlite3_column_text( get( ), static_cast<int>( column ) ) );
		return daw::string_view( first, sqlite3_column_bytes( get( ), static_cast<int>( column ) ) );
	}

	bool shared_prepared_statement::is_column_null( size_t column ) {
		return column_type::Null == get_column_type( column );
	}

	types::blob_t shared_prepared_statement::get_column_blob( size_t column ) {
		validate( *this, column );
		auto first = reinterpret_cast<std::byte const *>(
		  sqlite3_column_blob( get( ), static_cast<int>( column ) ) );
		return types::blob_t(
		  first,
		  static_cast<std::size_t>( sqlite3_column_bytes( get( ), static_cast<int>( column ) ) ) );
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

	void shared_prepared_statement::bind( size_t index, cell_value const &value ) {
		assert( index <= std::numeric_limits<int>::max( ) );
		auto rc = SQLITE_ERROR;
		switch( value.get_type( ) ) {
		case column_type::Float:
			rc = sqlite3_bind_double( m_statement.get( ), static_cast<int>( index ), value.get_float( ) );
			break;
		case column_type::Integer:
			rc =
			  sqlite3_bind_int64( m_statement.get( ), static_cast<int>( index ), value.get_integer( ) );
			break;
		case column_type::Text: {
			auto const &val = value.get_text( );
			auto *arry = new char[val.size( )]; // deleted by sqlite3_bind_text
			                                    // 5th parameter function
			std::copy( std::data( val ), daw::data_end( val ), arry );
			rc = sqlite3_bind_text( m_statement.get( ),
			                        static_cast<int>( index ),
			                        arry,
			                        val.size( ),
			                        delete_text_or_blob );
		} break;
		case column_type::Blob: {
			auto const &val = value.get_blob( );
			auto *arry = new std::byte[val.size( )]; // deleted by sqlite3_bind_blob 5th
			                                         // parameter function
			std::copy( val.begin( ), val.end( ), arry );
			rc = sqlite3_bind_blob( m_statement.get( ),
			                        static_cast<int>( index ),
			                        arry,
			                        val.size( ),
			                        delete_text_or_blob );
		} break;
		case column_type::Null:
			rc = sqlite3_bind_null( m_statement.get( ), static_cast<int>( index ) );
			break;
		default:
			std::cerr << "Unknown sqlite3 column type returned" << std::endl;
			std::terminate( );
		}
		if( rc != SQLITE_OK ) {
			throw sqlite3_exception( rc );
		}
	}

	void shared_prepared_statement::bind( size_t index ) {}

	column_type shared_prepared_statement::get_column_type( size_t column ) {
		validate( *this, column );

		switch( sqlite3_column_type( m_statement.get( ), static_cast<int>( column ) ) ) {
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
