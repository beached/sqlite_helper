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

#pragma once

#include <daw/daw_concepts.h>
#include <daw/daw_contiguous_view.h>
#include <daw/daw_move.h>
#include <daw/daw_string_view.h>
#include <daw/daw_take.h>
#include <daw/vector.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <sqlite3.h>
#include <utility>

namespace daw::db {
	namespace types {
		using real_t = double;
		using integer_t = int64_t;
		using text_t = daw::string_view;
		using blob_t = daw::contiguous_view<std::byte const>;
		std::string to_string( blob_t const & );
	} // namespace types

	struct Sqlite3DbException {
		std::string message;

		Sqlite3DbException( std::string msg );

		Sqlite3DbException( int err_no );
	}; // struct Sqlite3DbException

	enum class Sqlite3DbColumnType { Float, Integer, Text, Blob, Null };

	class Sqlite3Db;
	class Sqlite3DbCellValue;

	class Sqlite3DbPreparedStatement {
		sqlite3_stmt *m_statement;

	public:
		Sqlite3DbPreparedStatement( Sqlite3Db &db, daw::string_view sql );

		template<typename Param, typename... Params>
		requires( convertible_to<Param, Sqlite3DbCellValue> and
		          ( convertible_to<Params, Sqlite3DbCellValue> and ... ) )
		  Sqlite3DbPreparedStatement( Sqlite3Db &db,
		                              daw::string_view sql,
		                              Param &&param,
		                              Params &&...params )
		  : Sqlite3DbPreparedStatement( db, sql ) {
			[&]<std::size_t... Is>( std::index_sequence<Is...> ) {
				bind( 1, Sqlite3DbCellValue( DAW_FWD( param ) ) );
				(void)( ( bind( Is + 2, DAW_FWD( params ) ), 1 ) + ... );
			}
			( std::make_index_sequence<sizeof...( Params )>{ } );
		}

		~Sqlite3DbPreparedStatement( );

		sqlite3_stmt *get( );

		size_t get_column_count( );

		Sqlite3DbColumnType get_column_type( size_t column );

		types::text_t get_column_name( size_t column );

		types::real_t get_column_float( size_t column );

		types::integer_t get_column_integer( size_t column );

		types::text_t get_column_text( size_t column );

		bool is_column_null( size_t column );

		types::blob_t get_column_blob( size_t column );

		bool is_good( ) const;

		void reset( );

		void bind( size_t index, Sqlite3DbCellValue const &value );
		void bind( size_t index ); // bind a null to that value
	};                           // class Sqlite3DbPreparedStatement

	struct Sqlite3DbCellValue {
		struct null_cell_t {};
		using value_t =
		  std::variant<types::real_t, types::integer_t, types::text_t, types::blob_t, null_cell_t>;

	private:
		value_t m_value = null_cell_t{ };

	public:
		Sqlite3DbCellValue( Sqlite3DbPreparedStatement &statement, size_t column );

		explicit DAW_CONSTEVAL Sqlite3DbCellValue( ) = default;

		constexpr Sqlite3DbCellValue( types::real_t value )
		  : m_value{ std::in_place_type<types::real_t>, std::move( value ) } {}

		constexpr Sqlite3DbCellValue( types::integer_t value )
		  : m_value{ std::in_place_type<types::integer_t>, std::move( value ) } {}

		constexpr Sqlite3DbCellValue( types::text_t value )
		  : m_value{ std::in_place_type<types::text_t>, std::move( value ) } {}

		constexpr Sqlite3DbCellValue( types::blob_t value )
		  : m_value{ std::in_place_type<types::blob_t>, std::move( value ) } {}

		DAW_CONSTEVAL Sqlite3DbCellValue( std::nullptr_t ) {}

		constexpr types::real_t const &get_float( ) const {
			auto *ptr = std::get_if<types::real_t>( &m_value );
			if( not ptr ) {
				throw Sqlite3DbException( "Cell Value is not of type Real" );
			}
			return *ptr;
		}

		constexpr types::integer_t const &get_integer( ) const {
			auto *ptr = std::get_if<types::integer_t>( &m_value );
			if( not ptr ) {
				throw Sqlite3DbException( "Cell Value is not of type Integer" );
			}
			return *ptr;
		}

		constexpr types::text_t const &get_text( ) const {
			auto *ptr = std::get_if<types::text_t>( &m_value );
			if( not ptr ) {
				throw Sqlite3DbException( "Cell Value is not of type Text" );
			}
			return *ptr;
		}

		constexpr types::blob_t const &get_blob( ) const {
			auto *ptr = std::get_if<types::blob_t>( &m_value );
			if( not ptr ) {
				throw Sqlite3DbException( "Cell Value is not of type Blob" );
			}
			return *ptr;
		}

		constexpr bool is_null( ) const {
			return std::holds_alternative<null_cell_t>( m_value );
		}

		constexpr Sqlite3DbColumnType get_type( ) const {
			// 	enum class Sqlite3DbColumnType { Float, Integer, Text, Blob, Null };
			// std::variant<types::real_t, types::integer_t, types::text_t, types::blob_t,
			// std::nullptr_t>;
			switch( m_value.index( ) ) {
			case 0:
				return Sqlite3DbColumnType::Float;
			case 2:
				return Sqlite3DbColumnType::Integer;
			case 3:
				return Sqlite3DbColumnType::Text;
			case 4:
				return Sqlite3DbColumnType::Blob;
			case 5:
				return Sqlite3DbColumnType::Null;
			default:
				std::cerr << "Unexpected column type stored.\n";
				std::terminate( );
			}
		}
	}; // class Sqlite3DbCellValue

	std::string to_string( Sqlite3DbCellValue const &value );

	struct sqlite_deleter {
		inline void operator( )( sqlite3 *ptr ) const noexcept {
			sqlite3_close( ptr );
		}
	};

	template<typename T>
	concept exec_callback =
	  std::is_invocable_v<T, daw::vector<std::pair<daw::string_view, Sqlite3DbCellValue>>>;

	class Sqlite3Db {
		std::unique_ptr<sqlite3, sqlite_deleter> m_db;
		daw::take_t<bool> m_is_open;
		daw::take_t<std::mutex> m_exec_lock;

	public:
		Sqlite3Db( ) = default;

		void open( daw::string_view filename );
		void close( );
		sqlite3 const *get_handle( ) const;
		sqlite3 *get_handle( );
		daw::vector<std::string> tables( );
		bool has_table( daw::string_view table_name );

		void exec( Sqlite3DbPreparedStatement statement );
		void exec( std::string const &sql );

		template<exec_callback Callback>
		void exec( Sqlite3DbPreparedStatement statement, Callback cb ) {
			assert( m_db );

			int rc = SQLITE_ERROR;

			while( SQLITE_ROW == ( rc = sqlite3_step( statement.get( ) ) ) ) {
				using cell_t = std::pair<daw::string_view, Sqlite3DbCellValue>;
				auto const column_count = statement.get_column_count( );
				cb( daw::vector<cell_t>( daw::do_resize_and_overwrite,
				                         column_count,
				                         [&]( auto *ptr, std::size_t sz ) {
					                         for( size_t column = 0; column != column_count; ++column ) {
						                         std::construct_at(
						                           ptr + column,
						                           std::make_pair( statement.get_column_name( column ),
						                                           Sqlite3DbCellValue( statement, column ) ) );
					                         }
					                         return sz;
				                         } ) );
			}
			if( SQLITE_DONE != rc ) {
				throw Sqlite3DbException( rc );
			}
		}

		template<exec_callback Callback>
		void exec( daw::string_view sql, Callback cb ) {
			assert( m_db );
			exec( Sqlite3DbPreparedStatement( *this, sql ), DAW_MOVE( cb ) );
		}
	}; // class Sqlite3Db
} // namespace daw::db
