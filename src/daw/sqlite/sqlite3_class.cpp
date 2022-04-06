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

		auto statement = prepared_statement(
		  *this,
		  R"sql(SELECT name FROM sqlite_schema WHERE type='table' ORDER BY name;)sql" );
		int rc = SQLITE_ERROR;

		auto result = daw::vector<std::string>{ };
		while( SQLITE_ROW == ( rc = sqlite3_step( statement.get( ) ) ) ) {
			auto const column_count = statement.get_column_count( );
			result.push_back( static_cast<std::string>( cell_value( statement, 0 ).get_text( ) ) );
		}
		if( SQLITE_DONE != rc ) {
			throw sqlite3_exception( rc );
		}
		return result;
	}

	bool database::has_table( daw::string_view table_name ) {
		auto statement = prepared_statement(
		  *this,
		  R"sql(SELECT name FROM sqlite_schema WHERE type='table' and name=?;)sql" );
		statement.bind( 1, cell_value( table_name ) );
		int rc = SQLITE_ERROR;

		if( SQLITE_ROW == ( rc = sqlite3_step( statement.get( ) ) ) ) {
			return true;
		}
		if( SQLITE_DONE != rc ) {
			throw sqlite3_exception( rc );
		}
		return false;
	}

	result_row_t database::exec( prepared_statement statement, bool ignore_other_rows ) {
		assert( m_db );

		int rc = SQLITE_ERROR;
		result_row_t result;
		if( SQLITE_ROW == ( rc = sqlite3_step( statement.get( ) ) ) ) {
			auto const column_count = statement.get_column_count( );
			result =
			  result_row_t( daw::do_resize_and_overwrite, column_count, [&]( auto *ptr, std::size_t sz ) {
				  for( size_t column = 0; column != column_count; ++column ) {
					  std::construct_at( ptr + column,
					                     std::make_pair( statement.get_column_name( column ),
					                                     cell_value( statement, column ) ) );
				  }
				  return sz;
			  } );
		}
		if( SQLITE_DONE != rc or not ignore_other_rows ) {
			throw sqlite3_exception( rc );
		}
		return result;
	}

	result_row_t database::exec( std::string const &sql ) {
		assert( m_db );
		return exec( prepared_statement( *this, sql ) );
	}

	database::database( std::filesystem::path filename ) {
		open( filename );
	}

	prepared_statement::prepared_statement( database &db, daw::string_view sql )
	  : m_statement( nullptr ) {
		assert( sql.size( ) <= std::numeric_limits<int>::max( ) );
		auto rc = sqlite3_prepare_v2( db.get_handle( ),
		                              sql.data( ),
		                              static_cast<int>( sql.size( ) ),
		                              &m_statement,
		                              nullptr );
		if( SQLITE_OK != rc ) {
			m_statement = nullptr;
			throw sqlite3_exception( rc );
		}
	}

	prepared_statement::~prepared_statement( ) {
		sqlite3_finalize( m_statement );
	}

	sqlite3_stmt *prepared_statement::get( ) {
		return m_statement;
	}

	size_t prepared_statement::get_column_count( ) {
		auto const count = sqlite3_column_count( get( ) );
		assert( 0 <= count );
		return static_cast<size_t>( count );
	}

	namespace {
		inline void validate( prepared_statement &statement, size_t column ) {
			if( !statement.is_good( ) ) {
				throw sqlite3_exception( "Attempt to use an invalid statement" );
			} else if( statement.get_column_count( ) <= column ) {
				throw sqlite3_exception( "Column specified is out of range" );
			}
		}

		template<typename T>
		inline void delete_not_null( T **ptr ) {
			assert( nullptr != ptr );
			if( nullptr != *ptr ) {
				delete *ptr;
				*ptr = nullptr;
			}
		}
	} // namespace

	daw::string_view prepared_statement::get_column_name( size_t column ) {
		validate( *this, column );
		return daw::string_view( sqlite3_column_name( get( ), static_cast<int>( column ) ) );
	}

	types::real_t prepared_statement::get_column_float( size_t column ) {
		validate( *this, column );
		return sqlite3_column_double( m_statement, static_cast<int>( column ) );
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
		auto rc = sqlite3_reset( m_statement );
		if( SQLITE_OK != rc ) {
			throw sqlite3_exception( rc );
		}
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
			rc = sqlite3_bind_double( m_statement, static_cast<int>( index ), value.get_float( ) );
			break;
		case column_type::Integer:
			rc = sqlite3_bind_int64( m_statement, static_cast<int>( index ), value.get_integer( ) );
			break;
		case column_type::Text: {
			auto const &val = value.get_text( );
			auto *arry = new char[val.size( )]; // deleted by sqlite3_bind_text
			                                    // 5th parameter function
			std::copy( std::data( val ), daw::data_end( val ), arry );
			rc = sqlite3_bind_text( m_statement,
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
			rc = sqlite3_bind_blob( m_statement,
			                        static_cast<int>( index ),
			                        arry,
			                        val.size( ),
			                        delete_text_or_blob );
		} break;
		case column_type::Null:
			rc = sqlite3_bind_null( m_statement, static_cast<int>( index ) );
			break;
		default:
			std::cerr << "Unknown sqlite3 column type returned" << std::endl;
			std::terminate( );
		}
		if( SQLITE_OK != rc ) {
			throw sqlite3_exception( rc );
		}
	}

	void prepared_statement::bind( size_t index ) {}

	column_type prepared_statement::get_column_type( size_t column ) {
		validate( *this, column );

		switch( sqlite3_column_type( m_statement, static_cast<int>( column ) ) ) {
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

	namespace {
		cell_value::value_t get_column( prepared_statement &statement, size_t column ) {
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
