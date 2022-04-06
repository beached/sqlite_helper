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

#include <cstddef>
#include <iostream>
#include <sqlite3.h>
#include <sstream>
#include <string>

namespace daw::db {
	namespace types {
		std::string to_string( blob_t const &blob ) {
			std::ostringstream ss;
			for( auto const &current_byte : blob ) {
				ss << std::hex << static_cast<unsigned char>( current_byte );
			}
			return ss.str( );
		}
	} // namespace types

	Sqlite3DbException::Sqlite3DbException( std::string msg )
	  : message( std::move( msg ) ) {}

	Sqlite3DbException::Sqlite3DbException( int err_no )
	  : message( std::string( sqlite3_errstr( err_no ) ) ) {}

	void Sqlite3Db::open( daw::string_view filename ) {
		sqlite3 *ptr = nullptr;
		auto result = sqlite3_open( filename.data( ), &ptr );
		if( result ) {
			throw Sqlite3DbException( "Could not open database " + static_cast<std::string>( filename ) +
			                          ": " + sqlite3_errmsg( ptr ) );
		}
		m_db.reset( ptr );
		m_is_open = true;
	}

	void Sqlite3Db::close( ) {
		m_db.reset( );
		m_is_open.reset( );
	}

	sqlite3 const *Sqlite3Db::get_handle( ) const {
		assert( m_db );
		return m_db.get( );
	}

	sqlite3 *Sqlite3Db::get_handle( ) {
		assert( m_db );
		return m_db.get( );
	}
	daw::vector<std::string> Sqlite3Db::tables( ) {

		auto statement = Sqlite3DbPreparedStatement(
		  *this,
		  R"sql(SELECT name FROM sqlite_schema WHERE type='table' ORDER BY name;)sql" );
		int rc = SQLITE_ERROR;

		auto result = daw::vector<std::string>{ };
		while( SQLITE_ROW == ( rc = sqlite3_step( statement.get( ) ) ) ) {
			auto const column_count = statement.get_column_count( );
			result.push_back(
			  static_cast<std::string>( Sqlite3DbCellValue( statement, 0 ).get_text( ) ) );
		}
		if( SQLITE_DONE != rc ) {
			throw Sqlite3DbException( rc );
		}
		return result;
	}

	bool Sqlite3Db::has_table( daw::string_view table_name ) {
		auto statement = Sqlite3DbPreparedStatement(
		  *this,
		  R"sql(SELECT name FROM sqlite_schema WHERE type='table' and name=?;)sql" );
		statement.bind( 1, Sqlite3DbCellValue( table_name ) );
		int rc = SQLITE_ERROR;

		if( SQLITE_ROW == ( rc = sqlite3_step( statement.get( ) ) ) ) {
			return true;
		}
		if( SQLITE_DONE != rc ) {
			throw Sqlite3DbException( rc );
		}
		return false;
	}
	void Sqlite3Db::exec( Sqlite3DbPreparedStatement statement ) {
		assert( m_db );

		int rc = SQLITE_ERROR;
		while( SQLITE_ROW == ( rc = sqlite3_step( statement.get( ) ) ) ) {}
		if( SQLITE_DONE != rc ) {
			throw Sqlite3DbException( rc );
		}
	}

	void Sqlite3Db::exec( std::string const &sql ) {
		assert( m_db );
		int rc = sqlite3_exec( m_db.get( ), sql.c_str( ), nullptr, nullptr, nullptr );
		if( SQLITE_OK != rc ) {
			throw Sqlite3DbException( rc );
		}
	}

	Sqlite3DbPreparedStatement::Sqlite3DbPreparedStatement( Sqlite3Db &db, daw::string_view sql )
	  : m_statement( nullptr ) {
		assert( sql.size( ) <= std::numeric_limits<int>::max( ) );
		auto rc = sqlite3_prepare_v2( db.get_handle( ),
		                              sql.data( ),
		                              static_cast<int>( sql.size( ) ),
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

	daw::string_view Sqlite3DbPreparedStatement::get_column_name( size_t column ) {
		validate( *this, column );
		return daw::string_view( sqlite3_column_name( get( ), static_cast<int>( column ) ) );
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
		return daw::string_view( first, sqlite3_column_bytes( get( ), static_cast<int>( column ) ) );
	}

	bool Sqlite3DbPreparedStatement::is_column_null( size_t column ) {
		return Sqlite3DbColumnType::Null == get_column_type( column );
	}

	types::blob_t Sqlite3DbPreparedStatement::get_column_blob( size_t column ) {
		validate( *this, column );
		auto first = reinterpret_cast<std::byte const *>(
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
			auto *arry = new char[val.size( )]; // deleted by sqlite3_bind_text
			                                    // 5th parameter function
			std::copy( std::data( val ), daw::data_end( val ), arry );
			rc = sqlite3_bind_text( m_statement,
			                        static_cast<int>( index ),
			                        arry,
			                        val.size( ),
			                        delete_text_or_blob );
		} break;
		case Sqlite3DbColumnType::Blob: {
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

	namespace {
		Sqlite3DbCellValue::value_t get_column( Sqlite3DbPreparedStatement &statement, size_t column ) {
			switch( statement.get_column_type( column ) ) {
			case Sqlite3DbColumnType::Float:
				return Sqlite3DbCellValue::value_t( std::in_place_type<types::real_t>,
				                                    statement.get_column_float( column ) );
			case Sqlite3DbColumnType::Integer:
				return Sqlite3DbCellValue::value_t( std::in_place_type<types::integer_t>,
				                                    statement.get_column_integer( column ) );
			case Sqlite3DbColumnType::Text:
				return Sqlite3DbCellValue::value_t( std::in_place_type<types::text_t>,
				                                    statement.get_column_text( column ) );
			case Sqlite3DbColumnType::Blob:
				return Sqlite3DbCellValue::value_t( std::in_place_type<types::blob_t>,
				                                    statement.get_column_blob( column ) );
			case Sqlite3DbColumnType::Null:
				return Sqlite3DbCellValue::value_t( std::in_place_type<Sqlite3DbCellValue::null_cell_t>,
				                                    Sqlite3DbCellValue::null_cell_t{ } );
			default:
				std::cerr << "Unknown sqlite3 column type returned" << std::endl;
				std::terminate( );
			}
		}
	} // namespace
	Sqlite3DbCellValue::Sqlite3DbCellValue( Sqlite3DbPreparedStatement &statement, size_t column )
	  : m_value{ get_column( statement, column ) } {}

	std::string to_string( Sqlite3DbCellValue const &value ) {
		switch( value.get_type( ) ) {
		case Sqlite3DbColumnType::Float:
			return std::to_string( value.get_float( ) );
		case Sqlite3DbColumnType::Integer:
			return std::to_string( value.get_integer( ) );
		case Sqlite3DbColumnType::Text:
			return static_cast<std::string>( value.get_text( ) );
		case Sqlite3DbColumnType::Blob:
			return types::to_string( value.get_blob( ) );
		case Sqlite3DbColumnType::Null:
			return "{Null}";
		default:
			std::cerr << "Unknown sqlite3 column type returned" << std::endl;
			std::terminate( );
		}
	}
} // namespace daw::db
