// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/sqlite_helper
//

#pragma once

#include "daw/sqlite/cell_value.h"
#include "daw/sqlite/prepared_statement.h"
#include "daw/sqlite/sqlite3_exception.h"

#include <daw/daw_concepts.h>
#include <daw/daw_string_view.h>

#include <cstddef>
#include <iterator>
#include <sqlite3.h>
#include <string>
#include <tuple>
#include <utility>

namespace daw::sqlite {
	/***
	 * @brief A type that a typed_statement column can be read as: an arithmetic
	 * type, types::text_t, std::string, types::blob_t, cell_value, or
	 * std::optional of any of those but cell_value
	 */
	template<typename T>
	concept ColumnType =
	  std::same_as<T, cell_value> or ps_impl::is_column_value_v<T> or
	  ( ps_impl::is_optional_v<T> and
	    ps_impl::is_column_value_v<typename T::value_type> );

	/***
	 * @brief A prepared statement whose result columns have types known at
	 * compile time.  The column count is checked once when it is prepared, so
	 * rows are read without any per-row validation.  Non-optional columns throw
	 * on NULL.  Text and blob views are valid until the next step or reset
	 */
	template<ColumnType... Columns>
	class typed_statement {
		prepared_statement m_statement;

		void validate_columns( ) {
			auto const count = m_statement.get_column_count( );
			if( count != sizeof...( Columns ) ) {
				throw sqlite3_exception(
				  "typed_statement declares " + std::to_string( sizeof...( Columns ) ) +
				  " columns but the statement returns " + std::to_string( count ) );
			}
		}

	public:
		using row_type = std::tuple<Columns...>;
		static constexpr std::size_t column_count = sizeof...( Columns );

		typed_statement( database &db, daw::string_view sql,
		                 prepare_flags flags = prepare_flags::none )
		  : m_statement( db, sql, flags ) {
			validate_columns( );
		}

		template<typename Param, typename... Params>
		requires( Parameters<Param, Params...> ) //
		typed_statement( database &db, daw::string_view sql, Param &&param,
		                 Params &&...params )
		  : typed_statement( db, sql, prepare_flags::none, DAW_FWD( param ),
		                     DAW_FWD( params )... ) {}

		template<typename Param, typename... Params>
		requires( Parameters<Param, Params...> ) //
		typed_statement( database &db, daw::string_view sql, prepare_flags flags,
		                 Param &&param, Params &&...params )
		  : m_statement( db, sql, flags, DAW_FWD( param ), DAW_FWD( params )... ) {
			validate_columns( );
		}

		[[nodiscard]] sqlite3_stmt *get( ) const {
			return m_statement.get( );
		}

		void bind( std::size_t index, cell_value const &value ) {
			m_statement.bind( index, value );
		}

		// bind a null to that value
		void bind( std::size_t index ) {
			m_statement.bind( index );
		}

		void reset( ) {
			m_statement.reset( );
		}

		/***
		 * @brief Step to the next row
		 * @return true when a row is ready, false when the statement is done
		 */
		bool step( ) {
			auto const rc = sqlite3_step( get( ) );
			if( rc == SQLITE_ROW ) {
				return true;
			}
			if( rc == SQLITE_DONE ) {
				return false;
			}
			throw sqlite3_exception( rc,
			                         sqlite3_errmsg( sqlite3_db_handle( get( ) ) ) );
		}

		/***
		 * @brief Read the current row.  Only valid after step( ) returned true
		 */
		[[nodiscard]] row_type row( ) const {
			auto *stmt = get( );
			return [&]<std::size_t... Is>( std::index_sequence<Is...> ) {
				return row_type(
				  ps_impl::read_column<Columns>( stmt, static_cast<int>( Is ) )... );
			}( std::index_sequence_for<Columns...>{ } );
		}

		[[nodiscard]] decltype( auto )
		visit( daw::invocable<Columns...> auto &&func ) const {
			auto *stmt = get( );
			return [&]<std::size_t... Is>(
			         std::index_sequence<Is...> ) -> decltype( auto ) {
				return DAW_FWD( func )(
				  ps_impl::read_column<Columns>( stmt, static_cast<int>( Is ) )... );
			}( std::index_sequence_for<Columns...>{ } );
		}

		class row_iterator {
			typed_statement *m_statement = nullptr;
			bool m_done = true;

			friend class typed_statement;

			explicit row_iterator( typed_statement &statement )
			  : m_statement( &statement )
			  , m_done( not statement.step( ) ) {}

		public:
			using value_type = row_type;
			using difference_type = std::ptrdiff_t;
			using iterator_concept = std::input_iterator_tag;

			row_iterator( ) = default;

			[[nodiscard]] value_type operator*( ) const {
				return m_statement->row( );
			}

			row_iterator &operator++( ) {
				m_done = not m_statement->step( );
				return *this;
			}

			void operator++( int ) {
				operator++( );
			}

			[[nodiscard]] friend bool operator==( row_iterator const &it,
			                                      std::default_sentinel_t ) {
				return it.m_done;
			}
		};

		/***
		 * @brief The rows of one execution.  Resets the statement on destruction,
		 * releasing its read transaction and keeping its bindings
		 */
		class row_range {
			typed_statement *m_statement;

			friend class typed_statement;

			explicit row_range( typed_statement &statement )
			  : m_statement( &statement ) {}

		public:
			row_range( row_range const & ) = delete;
			row_range &operator=( row_range const & ) = delete;

			~row_range( ) {
				sqlite3_reset( m_statement->get( ) );
			}

			[[nodiscard]] row_iterator begin( ) {
				return row_iterator( *m_statement );
			}

			[[nodiscard]] static std::default_sentinel_t end( ) {
				return { };
			}
		};

		/***
		 * @brief Execute the statement and iterate its rows as row_type.  The
		 * statement is reset first, so bindings made since the last run are used
		 */
		[[nodiscard]] row_range rows( ) {
			// Not reset( ): sqlite3_reset returns the previous run's error, which
			// step( ) already threw
			sqlite3_reset( get( ) );
			return row_range( *this );
		}
	};
} // namespace daw::sqlite
