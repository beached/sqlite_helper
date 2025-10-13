// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/sqlite_helper
//

#pragma once

#include "daw/sqlite/cell_value.h"
#include "daw/sqlite/sqlite3_exception.h"

#include <daw/daw_string_view.h>

#include <memory>

typedef struct sqlite3_stmt sqlite3_stmt;

namespace daw::sqlite {
	class database;

	namespace ps_impl {
		struct sqlite3_stmt_deleter {
			void operator( )( sqlite3_stmt *ptr ) const;
		};
	} // namespace ps_impl

	class shared_prepared_statement;

	template<typename... Ts>
	concept Parameters = ( convertible_to<Ts, cell_value> and ... );

	class prepared_statement {
		std::unique_ptr<sqlite3_stmt, ps_impl::sqlite3_stmt_deleter> m_statement =
			nullptr;

	public:
		using i_am_a_prepared_statement = void;

		explicit prepared_statement( ) = default;
		prepared_statement( database &db, daw::string_view sql );

		friend class ::daw::sqlite::shared_prepared_statement;

		template<typename Param, typename... Params>
			requires( Parameters<Param, Params...> ) //
		prepared_statement( database &db, daw::string_view sql, Param &&param,
		                    Params &&... params )
			: prepared_statement( db, sql ) {
			[&]<std::size_t... Is>( std::index_sequence<Is...> ) {
				bind( 1, cell_value( DAW_FWD( param ) ) );
				(void)( ( bind( Is + 2, DAW_FWD( params ) ), 1 ) + ... + 0 );
			}( std::make_index_sequence<sizeof...( Params )>{} );
		}

		[[nodiscard]] sqlite3_stmt *get( );

		[[nodiscard]] std::size_t get_column_count( );

		[[nodiscard]] column_type get_column_type( std::size_t column );

		[[nodiscard]] types::text_t get_column_name( std::size_t column );

		[[nodiscard]] types::real_t get_column_float( std::size_t column );

		[[nodiscard]] types::integer_t get_column_integer( std::size_t column );

		[[nodiscard]] types::text_t get_column_text( std::size_t column );

		[[nodiscard]] bool is_column_null( std::size_t column );

		[[nodiscard]] types::blob_t get_column_blob( std::size_t column );

		[[nodiscard]] bool is_good( ) const;

		void reset( );
		void reset_to_default_init( );

		void bind( std::size_t index, cell_value const &value );
		void bind( std::size_t index ); // bind a null to that value

		explicit operator bool( ) const {
			return static_cast<bool>(m_statement);
		}

		// clang-format off
		[[nodiscard]] auto operator<=>( prepared_statement const &rhs ) const {
			return m_statement.get( ) <=> rhs.m_statement.get( );
		}
		// clang-format on

		[[nodiscard]] bool operator==( prepared_statement const &rhs ) const {
			return m_statement.get( ) == rhs.m_statement.get( );
		}

		[[nodiscard]] bool operator!=( prepared_statement const & ) const = default;
	};

	class shared_prepared_statement {
		std::shared_ptr<sqlite3_stmt> m_statement = nullptr;

	public:
		using i_am_a_prepared_statement = void;

		explicit shared_prepared_statement( ) = default;
		shared_prepared_statement( database &db, daw::string_view sql );
		shared_prepared_statement( prepared_statement statement );

		template<typename Param, typename... Params>
			requires( convertible_to<Param, cell_value> and
			          ( convertible_to<Params, cell_value> and ... ) )
		shared_prepared_statement( database &db, daw::string_view sql,
		                           Param &&param, Params &&... params )
			: shared_prepared_statement( db, sql ) {
			[&]<std::size_t... Is>( std::index_sequence<Is...> ) {
				bind( 1, cell_value( DAW_FWD( param ) ) );
				(void)( ( bind( Is + 2, DAW_FWD( params ) ), 1 ) + ... + 0 );
			}( std::make_index_sequence<sizeof...( Params )>{} );
		}

		[[nodiscard]] sqlite3_stmt *get( );

		[[nodiscard]] std::size_t get_column_count( );

		[[nodiscard]] column_type get_column_type( std::size_t column );

		[[nodiscard]] types::text_t get_column_name( std::size_t column );

		[[nodiscard]] types::real_t get_column_float( std::size_t column );

		[[nodiscard]] types::integer_t get_column_integer( std::size_t column );

		[[nodiscard]] types::text_t get_column_text( std::size_t column );

		[[nodiscard]] bool is_column_null( std::size_t column );

		[[nodiscard]] types::blob_t get_column_blob( std::size_t column );

		[[nodiscard]] bool is_good( ) const;

		void reset( );
		void reset_to_default_init( );

		void bind( std::size_t index, cell_value const &value );
		void bind( std::size_t index ); // bind a null to that value

		explicit operator bool( ) const {
			return static_cast<bool>(m_statement);
		}

		// clang-format off
		[[nodiscard]] auto operator<=>( shared_prepared_statement const &rhs ) const {
			return m_statement.get( ) <=> rhs.m_statement.get( );
		}
		// clang-format on

		[[nodiscard]] bool operator
		==( shared_prepared_statement const &rhs ) const {
			return m_statement.get( ) == rhs.m_statement.get( );
		}

		[[nodiscard]] bool operator!=( shared_prepared_statement const & ) const
		= default;
	};

	template<typename T>
	concept PreparedStatement = requires
	{
		typename T::i_am_a_prepared_statement;
	};
} // namespace daw::sqlite