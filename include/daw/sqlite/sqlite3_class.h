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
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <sqlite3.h>
#include <utility>

namespace daw::sqlite {
	namespace types {
		using real_t = double;
		using integer_t = int64_t;
		using text_t = daw::string_view;
		using blob_t = daw::contiguous_view<std::byte const>;
		std::string to_string( blob_t const & );
	} // namespace types

	class sqlite3_exception : public std::exception {
		int m_error = -1;
		std::string m_message;

	public:
		explicit sqlite3_exception( int err_no );
		explicit sqlite3_exception( std::string message );

		char const *what( ) const noexcept override;
		int error( ) const;
	}; // struct sqlite3_exception

	enum class column_type { Float, Integer, Text, Blob, Null };

	class database;
	class cell_value;

	class prepared_statement {
		sqlite3_stmt *m_statement;

	public:
		prepared_statement( database &db, daw::string_view sql );

		template<typename Param, typename... Params>
		requires( convertible_to<Param, cell_value> and ( convertible_to<Params, cell_value> and ... ) )
		  prepared_statement( database &db, daw::string_view sql, Param &&param, Params &&...params )
		  : prepared_statement( db, sql ) {
			[&]<std::size_t... Is>( std::index_sequence<Is...> ) {
				bind( 1, cell_value( DAW_FWD( param ) ) );
				(void)( ( bind( Is + 2, DAW_FWD( params ) ), 1 ) + ... + 0 );
			}
			( std::make_index_sequence<sizeof...( Params )>{ } );
		}

		~prepared_statement( );

		sqlite3_stmt *get( );

		size_t get_column_count( );

		column_type get_column_type( size_t column );

		types::text_t get_column_name( size_t column );

		types::real_t get_column_float( size_t column );

		types::integer_t get_column_integer( size_t column );

		types::text_t get_column_text( size_t column );

		bool is_column_null( size_t column );

		types::blob_t get_column_blob( size_t column );

		bool is_good( ) const;

		void reset( );

		void bind( size_t index, cell_value const &value );
		void bind( size_t index ); // bind a null to that value
	};                           // class prepared_statement

	struct cell_value {
		struct null_cell_t {};
		using value_t =
		  std::variant<types::real_t, types::integer_t, types::text_t, types::blob_t, null_cell_t>;

	private:
		value_t m_value = null_cell_t{ };

	public:
		cell_value( prepared_statement &statement, size_t column );

		explicit DAW_CONSTEVAL cell_value( ) = default;

		constexpr cell_value( types::real_t value )
		  : m_value{ std::in_place_type<types::real_t>, std::move( value ) } {}

		constexpr cell_value( types::integer_t value )
		  : m_value{ std::in_place_type<types::integer_t>, std::move( value ) } {}

		constexpr cell_value( types::text_t value )
		  : m_value{ std::in_place_type<types::text_t>, std::move( value ) } {}

		constexpr cell_value( types::blob_t value )
		  : m_value{ std::in_place_type<types::blob_t>, std::move( value ) } {}

		DAW_CONSTEVAL cell_value( std::nullptr_t ) {}

		constexpr types::real_t const &get_float( ) const {
			auto *ptr = std::get_if<types::real_t>( &m_value );
			if( not ptr ) {
				throw sqlite3_exception( "Cell Value is not of type Real" );
			}
			return *ptr;
		}

		constexpr types::integer_t const &get_integer( ) const {
			auto *ptr = std::get_if<types::integer_t>( &m_value );
			if( not ptr ) {
				throw sqlite3_exception( "Cell Value is not of type Integer" );
			}
			return *ptr;
		}

		constexpr types::text_t const &get_text( ) const {
			auto *ptr = std::get_if<types::text_t>( &m_value );
			if( not ptr ) {
				throw sqlite3_exception( "Cell Value is not of type Text" );
			}
			return *ptr;
		}

		constexpr types::blob_t const &get_blob( ) const {
			auto *ptr = std::get_if<types::blob_t>( &m_value );
			if( not ptr ) {
				throw sqlite3_exception( "Cell Value is not of type Blob" );
			}
			return *ptr;
		}

		constexpr bool is_null( ) const {
			return std::holds_alternative<null_cell_t>( m_value );
		}

		constexpr column_type get_type( ) const {
			// 	enum class column_type { Float, Integer, Text, Blob, Null };
			// std::variant<types::real_t, types::integer_t, types::text_t, types::blob_t,
			// std::nullptr_t>;
			switch( m_value.index( ) ) {
			case 0:
				return column_type::Float;
			case 1:
				return column_type::Integer;
			case 2:
				return column_type::Text;
			case 3:
				return column_type::Blob;
			case 4:
				return column_type::Null;
			default:
				std::cerr << "Unexpected column type stored.\n";
				std::terminate( );
			}
		}
	}; // class cell_value

	std::string to_string( cell_value const &value );

	namespace sqlite_impl {
		struct sqlite_deleter {
			inline void operator( )( sqlite3 *ptr ) const noexcept {
				sqlite3_close( ptr );
			}
		};
	} // namespace sqlite_impl

	using result_cell_t = std::pair<daw::string_view, cell_value>;
	using result_row_t = daw::vector<result_cell_t>;
	template<typename T>
	concept exec_callback = std::is_invocable_v<T, result_row_t>;

	class database {
		std::unique_ptr<sqlite3, sqlite_impl::sqlite_deleter> m_db;
		daw::take_t<bool> m_is_open;
		daw::take_t<std::mutex> m_exec_lock;

	public:
		explicit database( ) = default;
		explicit database( std::filesystem::path filename );

		void open( std::filesystem::path filename );
		void close( );
		sqlite3 const *get_handle( ) const;
		sqlite3 *get_handle( );
		daw::vector<std::string> tables( );
		bool has_table( daw::string_view table_name );

		std::optional<result_row_t> exec( prepared_statement statement,
		                                  bool ignore_other_rows = false );
		std::optional<result_row_t> exec( std::string const &sql, bool ignore_other_rows = false );

		template<exec_callback Callback>
		void exec( prepared_statement statement, Callback cb ) {
			assert( m_db );

			int rc = SQLITE_ERROR;

			while( SQLITE_ROW == ( rc = sqlite3_step( statement.get( ) ) ) ) {
				auto const column_count = statement.get_column_count( );
				cb( result_row_t( daw::do_resize_and_overwrite,
				                  column_count,
				                  [&]( auto *ptr, std::size_t sz ) {
					                  for( size_t column = 0; column != column_count; ++column ) {
						                  std::construct_at(
						                    ptr + column,
						                    std::make_pair( statement.get_column_name( column ),
						                                    cell_value( statement, column ) ) );
					                  }
					                  return sz;
				                  } ) );
			}
			if( SQLITE_DONE != rc ) {
				throw sqlite3_exception( rc );
			}
		}

		template<exec_callback Callback>
		void exec( daw::string_view sql, Callback cb ) {
			assert( m_db );
			exec( prepared_statement( *this, sql ), DAW_MOVE( cb ) );
		}
	}; // class database
} // namespace daw::sqlite
