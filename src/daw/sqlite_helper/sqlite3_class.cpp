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

#include "daw/sqlite_helper/sqlite3_class.h"

#include <daw/daw_contiguous_view.h>
#include <daw/daw_string_view.h>

#include <iostream>
#include <sqlite3.h>
#include <sstream>
#include <string>

namespace daw {
	namespace db {
		namespace types {
			std::string to_string( blob_t const &blob ) {
				std::ostringstream ss;
				for( auto const &current_byte : blob ) {
					ss << std::hex << current_byte;
				}
				return ss.str( );
			}
		} // namespace types

		Sqlite3DbException::Sqlite3DbException( std::string msg )
		  : message( std::move( msg ) ) {}

		Sqlite3DbException::Sqlite3DbException( int err_no )
		  : message( std::string( sqlite3_errstr( err_no ) ) ) {}

		Sqlite3Db::Sqlite3Db( )
		  : m_db( nullptr )
		  , m_is_open( false ) {}

		Sqlite3Db::~Sqlite3Db( ) {
			close( );
		}

		Sqlite3Db::Sqlite3Db( Sqlite3Db &&other )
		  : m_db( std::move( other.m_db ) )
		  , m_is_open( other.m_is_open ) {
			other.m_db = nullptr;
			other.m_is_open = false;
		}

		Sqlite3Db &Sqlite3Db::operator=( Sqlite3Db &&rhs ) {
			if( this != &rhs ) {
				m_db = std::move( rhs.m_db );
				m_is_open = std::move( rhs.m_is_open );
				rhs.m_db = nullptr;
				rhs.m_is_open = false;
			}
			return *this;
		}

		void Sqlite3Db::open( daw::string_view filename ) {
			auto result = sqlite3_open( filename.data( ), &m_db );
			if( result ) {
				throw Sqlite3DbException( "Could not open database " +
				                          static_cast<std::string>( filename ) + ": " +
				                          sqlite3_errmsg( m_db ) );
			}
			m_is_open = true;
		}

		void Sqlite3Db::close( ) {
			if( m_db && m_is_open ) {
				sqlite3_close( m_db );
				m_db = nullptr;
				m_is_open = false;
			}
		}

		sqlite3 const *Sqlite3Db::get_handle( ) const {
			return m_db;
		}

		sqlite3 *Sqlite3Db::get_handle( ) {
			return m_db;
		}

		void Sqlite3Db::exec( daw::range::utf_range sql, Sqlite3Db::callback_t callback ) {
			Sqlite3DbPreparedStatement statement( *this, sql );
			int rc = SQLITE_ERROR;

			while( SQLITE_ROW == ( rc = sqlite3_step( statement.get( ) ) ) ) {
				if( callback ) {
					std::vector<std::pair<daw::range::utf_range, Sqlite3DbCellValue>> row;
					auto const column_count = statement.get_column_count( );
					for( size_t column = 0; column < column_count; ++column ) {
						row.emplace_back( std::make_pair( statement.get_column_name( column ),
						                                  Sqlite3DbCellValue( statement, column ) ) );
					}
					callback( row );
				}
			}
			if( SQLITE_DONE != rc ) {
				throw Sqlite3DbException( rc );
			}
		}

		Sqlite3DbPreparedStatement::Sqlite3DbPreparedStatement( Sqlite3Db &db,
		                                                        daw::range::utf_range sql )
		  : m_statement( nullptr ) {
			assert( sql.raw_size( ) <= std::numeric_limits<int>::max( ) );
			auto rc = sqlite3_prepare_v2( db.get_handle( ),
			                              sql.raw_begin( ),
			                              static_cast<int>( sql.raw_size( ) ),
			                              &m_statement,
			                              nullptr );
			if( SQLITE_OK != rc ) {
				m_statement = nullptr;
				throw Sqlite3DbException( rc );
			}
		}

		Sqlite3DbPreparedStatement::~Sqlite3DbPreparedStatement( ) {
			sqlite3_finalize( m_statement );
		}

		sqlite3_stmt *Sqlite3DbPreparedStatement::get( ) {
			return m_statement;
		}

		size_t Sqlite3DbPreparedStatement::get_column_count( ) {
			auto const count = sqlite3_column_count( get( ) );
			assert( 0 <= count );
			return static_cast<size_t>( count );
		}

		namespace {
			inline void validate( Sqlite3DbPreparedStatement &statement, size_t column ) {
				if( !statement.is_good( ) ) {
					throw Sqlite3DbException( "Attempt to use an invalid statement" );
				} else if( statement.get_column_count( ) <= column ) {
					throw Sqlite3DbException( "Column specified is out of range" );
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

		daw::range::utf_range Sqlite3DbPreparedStatement::get_column_name( size_t column ) {
			validate( *this, column );
			return daw::range::create_char_range(
			  sqlite3_column_name( get( ), static_cast<int>( column ) ) );
		}

		types::real_t Sqlite3DbPreparedStatement::get_column_float( size_t column ) {
			validate( *this, column );
			return sqlite3_column_double( m_statement, static_cast<int>( column ) );
		}

		types::integer_t Sqlite3DbPreparedStatement::get_column_integer( size_t column ) {
			validate( *this, column );
			return sqlite3_column_int64( get( ), static_cast<int>( column ) );
		}

		types::text_t Sqlite3DbPreparedStatement::get_column_text( size_t column ) {
			validate( *this, column );
			auto first =
			  reinterpret_cast<char const *>( sqlite3_column_text( get( ), static_cast<int>( column ) ) );
			return daw::range::create_char_range(
			  first,
			  first + sqlite3_column_bytes( get( ), static_cast<int>( column ) ) );
		}

		bool Sqlite3DbPreparedStatement::is_column_null( size_t column ) {
			return Sqlite3DbColumnType::Null == get_column_type( column );
		}

		types::blob_t Sqlite3DbPreparedStatement::get_column_blob( size_t column ) {
			validate( *this, column );
			auto first = reinterpret_cast<uint8_t const *>(
			  sqlite3_column_blob( get( ), static_cast<int>( column ) ) );
			return types::blob_t(
			  first,
			  static_cast<std::size_t>( sqlite3_column_bytes( get( ), static_cast<int>( column ) ) ) );
		}

		bool Sqlite3DbPreparedStatement::is_good( ) const {
			return nullptr != m_statement;
		}

		void Sqlite3DbPreparedStatement::reset( ) {
			auto rc = sqlite3_reset( m_statement );
			if( SQLITE_OK != rc ) {
				throw Sqlite3DbException( rc );
			}
		}

		namespace {
			void delete_text_or_blob( void *val ) {
				auto ptr = static_cast<char const *>( val );
				delete[] ptr;
			}
		} // namespace

		void Sqlite3DbPreparedStatement::bind( size_t index, Sqlite3DbCellValue const &value ) {
			assert( index <= std::numeric_limits<int>::max( ) );
			auto rc = SQLITE_ERROR;
			switch( value.get_type( ) ) {
			case Sqlite3DbColumnType::Float:
				rc = sqlite3_bind_double( m_statement, static_cast<int>( index ), value.get_float( ) );
				break;
			case Sqlite3DbColumnType::Integer:
				rc = sqlite3_bind_int64( m_statement, static_cast<int>( index ), value.get_integer( ) );
				break;
			case Sqlite3DbColumnType::Text: {
				auto const &val = value.get_text( );
				auto arry = new char[val.raw_size( )]; // deleted by sqlite3_bind_text
				                                       // 5th parameter function
				std::copy( val.raw_begin( ), val.raw_end( ), arry );
				rc = sqlite3_bind_text( m_statement,
				                        static_cast<int>( index ),
				                        arry,
				                        val.raw_size( ),
				                        delete_text_or_blob );
			} break;
			case Sqlite3DbColumnType::Blob: {
				auto const &val = value.get_blob( );
				auto arry = new char[val.size( )]; // deleted by sqlite3_bind_blob 5th
				                                   // parameter function
				std::copy( val.begin( ), val.end( ), arry );
				rc = sqlite3_bind_text( m_statement,
				                        static_cast<int>( index ),
				                        arry,
				                        val.size( ),
				                        delete_text_or_blob );
			} break;
			case Sqlite3DbColumnType::Null:
				rc = sqlite3_bind_null( m_statement, static_cast<int>( index ) );
				break;
			default:
				std::cerr << "Unknown sqlite3 column type returned" << std::endl;
				std::terminate( );
			}
			if( SQLITE_OK != rc ) {
				throw Sqlite3DbException( rc );
			}
		}

		void Sqlite3DbPreparedStatement::bind( size_t index ) {}

		Sqlite3DbColumnType Sqlite3DbPreparedStatement::get_column_type( size_t column ) {
			validate( *this, column );

			switch( sqlite3_column_type( m_statement, static_cast<int>( column ) ) ) {
			case SQLITE_INTEGER:
				return Sqlite3DbColumnType::Integer;
			case SQLITE_FLOAT:
				return Sqlite3DbColumnType::Float;
			case SQLITE_TEXT:
				return Sqlite3DbColumnType::Text;
			case SQLITE_BLOB:
				return Sqlite3DbColumnType::Blob;
			case SQLITE_NULL:
				return Sqlite3DbColumnType::Null;
			default:
				std::cerr << "Unknown sqlite3 column type returned" << std::endl;
				std::terminate( );
			}
		}

		Sqlite3DbCellValue::Sqlite3DbCellValue( )
		  : m_value_type( Sqlite3DbColumnType::Null )
		  , m_value{ nullptr } {}

		namespace {
			Sqlite3DbCellValue::value_t get_column( Sqlite3DbPreparedStatement &statement,
			                                        size_t column ) {
				switch( statement.get_column_type( column ) ) {
				case Sqlite3DbColumnType::Float:
					return statement.get_column_float( column );
				case Sqlite3DbColumnType::Integer:
					return statement.get_column_integer( column );
				case Sqlite3DbColumnType::Text:
					return statement.get_column_text( column );
				case Sqlite3DbColumnType::Blob:
					return statement.get_column_blob( column );
				case Sqlite3DbColumnType::Null:
					return nullptr;
				default:
					std::cerr << "Unknown sqlite3 column type returned" << std::endl;
					std::terminate( );
				}
			}
		} // namespace

		Sqlite3DbCellValue::Sqlite3DbCellValue( Sqlite3DbPreparedStatement &statement, size_t column )
		  : m_value_type( statement.get_column_type( column ) )
		  , m_value{ get_column( statement, column ) } {}

		Sqlite3DbCellValue::Sqlite3DbCellValue( types::real_t value )
		  : m_value_type( Sqlite3DbColumnType::Float )
		  , m_value{ std::move( value ) } {}

		Sqlite3DbCellValue::Sqlite3DbCellValue( types::integer_t value )
		  : m_value_type( Sqlite3DbColumnType::Integer )
		  , m_value{ std::move( value ) } {}

		Sqlite3DbCellValue::Sqlite3DbCellValue( daw::range::utf_range value )
		  : m_value_type( Sqlite3DbColumnType::Text )
		  , m_value{ std::move( value ) } {}

		Sqlite3DbCellValue::Sqlite3DbCellValue( daw::contiguous_view<uint8_t const> value )
		  : m_value_type( Sqlite3DbColumnType::Blob )
		  , m_value{ std::move( value ) } {}

		double const &Sqlite3DbCellValue::get_float( ) const {
			if( Sqlite3DbColumnType::Float != m_value_type ) {
				throw Sqlite3DbException( "Cell Value is not of type Real" );
			}
			return std::get<types::real_t>( m_value );
		}

		int64_t const &Sqlite3DbCellValue::get_integer( ) const {
			if( Sqlite3DbColumnType::Integer != m_value_type ) {
				throw Sqlite3DbException( "Cell Value is not of type Integer" );
			}
			return std::get<types::integer_t>( m_value );
		}

		daw::range::utf_range const &Sqlite3DbCellValue::get_text( ) const {
			if( Sqlite3DbColumnType::Text != m_value_type ) {
				throw Sqlite3DbException( "Cell Value is not of type Text" );
			}
			return std::get<types::text_t>( m_value );
		}

		bool Sqlite3DbCellValue::is_null( ) const {
			return Sqlite3DbColumnType::Null == m_value_type;
		}

		daw::contiguous_view<uint8_t const> const &Sqlite3DbCellValue::get_blob( ) const {
			if( Sqlite3DbColumnType::Blob != m_value_type ) {
				throw Sqlite3DbException( "Cell Value is not of type Text" );
			}
			return std::get<types::blob_t>( m_value );
		}

		Sqlite3DbColumnType Sqlite3DbCellValue::get_type( ) const {
			return m_value_type;
		}

		std::string to_string( Sqlite3DbCellValue const &value ) {
			switch( value.get_type( ) ) {
			case Sqlite3DbColumnType::Float:
				return std::to_string( value.get_float( ) );
			case Sqlite3DbColumnType::Integer:
				return std::to_string( value.get_integer( ) );
			case Sqlite3DbColumnType::Text:
				return to_string( value.get_text( ) );
			case Sqlite3DbColumnType::Blob:
				return types::to_string( value.get_blob( ) );
			case Sqlite3DbColumnType::Null:
				return "{Null}";
			default:
				std::cerr << "Unknown sqlite3 column type returned" << std::endl;
				std::terminate( );
			}
		}

	} // namespace db
} // namespace daw
