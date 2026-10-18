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
#include <daw/daw_unique_ptr.h>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace daw::sqlite {
	class database;

	namespace ps_impl {
		struct sqlite3_stmt_deleter {
			void operator( )( sqlite3_stmt *ptr ) const;
		};
	} // namespace ps_impl
	using unique_stmt_ptr =
	  daw::unique_ptr<sqlite3_stmt, ps_impl::sqlite3_stmt_deleter>;

	namespace ps_impl {
		// Unchecked column access.  Callers must have validated the statement and
		// column index.  Text and blob views are valid until the next step/reset

		[[nodiscard]] inline int to_column( std::size_t column ) {
			assert( column <=
			        static_cast<std::size_t>( std::numeric_limits<int>::max( ) ) );
			return static_cast<int>( column );
		}

		[[nodiscard]] inline bool is_null( sqlite3_stmt *stmt, int column ) {
			return sqlite3_column_type( stmt, column ) == SQLITE_NULL;
		}

		[[nodiscard]] inline types::text_t column_name( sqlite3_stmt *stmt,
		                                                int column ) {
			return types::text_t( sqlite3_column_name( stmt, column ) );
		}

		[[nodiscard]] inline types::text_t column_text( sqlite3_stmt *stmt,
		                                                int column ) {
			// sqlite3_column_text must be called before sqlite3_column_bytes
			auto const *first =
			  reinterpret_cast<char const *>( sqlite3_column_text( stmt, column ) );
			return types::text_t(
			  first,
			  static_cast<std::size_t>( sqlite3_column_bytes( stmt, column ) ) );
		}

		[[nodiscard]] inline types::blob_t column_blob( sqlite3_stmt *stmt,
		                                                int column ) {
			auto const *first =
			  static_cast<std::byte const *>( sqlite3_column_blob( stmt, column ) );
			return types::blob_t(
			  first,
			  static_cast<std::size_t>( sqlite3_column_bytes( stmt, column ) ) );
		}

		/***
		 * @brief Call vis with the column value as its sqlite storage type: one of
		 * types::real_t, types::integer_t, types::text_t, types::blob_t or
		 * cell_value::null_cell_t
		 */
		template<typename Visitor>
		std::invoke_result_t<Visitor, types::integer_t>
		visit_column( sqlite3_stmt *stmt, int column, Visitor &&vis ) {
			switch( sqlite3_column_type( stmt, column ) ) {
			case SQLITE_INTEGER:
				return DAW_FWD( vis )(
				  types::integer_t{ sqlite3_column_int64( stmt, column ) } );
			case SQLITE_FLOAT:
				return DAW_FWD( vis )(
				  types::real_t{ sqlite3_column_double( stmt, column ) } );
			case SQLITE_TEXT:
				return DAW_FWD( vis )( column_text( stmt, column ) );
			case SQLITE_BLOB:
				return DAW_FWD( vis )( column_blob( stmt, column ) );
			case SQLITE_NULL:
				return DAW_FWD( vis )( cell_value::null_cell_t{ } );
			default:
				std::terminate( );
			}
		}

		template<typename>
		inline constexpr bool is_optional_v = false;

		template<typename T>
		inline constexpr bool is_optional_v<std::optional<T>> = true;

		template<typename T>
		inline constexpr bool is_column_value_v =
		  std::is_arithmetic_v<T> or std::same_as<T, types::text_t> or
		  std::same_as<T, types::blob_t> or std::same_as<T, std::string>;

		// Read a column, letting sqlite convert between storage types, with
		// std::nullopt for NULL.  sqlite3_column_type can take the connection
		// mutex, so it is only called when the value read is ambiguous: NULL reads
		// as 0, 0.0, or a null pointer
		template<typename T>
		[[nodiscard]] std::optional<T> read_nullable( sqlite3_stmt *stmt,
		                                              int column ) {
			if constexpr( std::integral<T> ) {
				auto const value = sqlite3_column_int64( stmt, column );
				if( value == 0 and is_null( stmt, column ) ) {
					return std::nullopt;
				}
				if constexpr( std::same_as<T, bool> ) {
					return value != 0;
				} else {
					if( not std::in_range<T>( value ) ) {
						throw sqlite3_exception( "Column value out of range for type" );
					}
					return static_cast<T>( value );
				}
			} else if constexpr( std::floating_point<T> ) {
				auto const value = sqlite3_column_double( stmt, column );
				if( value == 0.0 and is_null( stmt, column ) ) {
					return std::nullopt;
				}
				return static_cast<T>( value );
			} else if constexpr( std::same_as<T, types::text_t> or
			                     std::same_as<T, std::string> ) {
				// sqlite3_column_text must be called before sqlite3_column_bytes
				auto const *first =
				  reinterpret_cast<char const *>( sqlite3_column_text( stmt, column ) );
				if( not first ) {
					if( is_null( stmt, column ) ) {
						return std::nullopt;
					}
					throw sqlite3_exception( SQLITE_NOMEM );
				}
				auto const size =
				  static_cast<std::size_t>( sqlite3_column_bytes( stmt, column ) );
				return T( first, size );
			} else {
				static_assert( std::same_as<T, types::blob_t> );
				auto const *first =
				  static_cast<std::byte const *>( sqlite3_column_blob( stmt, column ) );
				if( not first ) {
					// Either NULL or a zero length blob
					if( is_null( stmt, column ) ) {
						return std::nullopt;
					}
					return types::blob_t( );
				}
				return types::blob_t(
				  first,
				  static_cast<std::size_t>( sqlite3_column_bytes( stmt, column ) ) );
			}
		}

		/***
		 * @brief Read a column as T.  T can be an arithmetic type, types::text_t,
		 * std::string, types::blob_t, cell_value, or std::optional of any of those
		 * but cell_value.  NULL maps to std::nullopt/a null cell_value, and throws
		 * for any other T
		 */
		template<typename T>
		[[nodiscard]] T read_column( sqlite3_stmt *stmt, int column ) {
			if constexpr( std::same_as<T, cell_value> ) {
				return visit_column( stmt, column, []( auto v ) {
					return cell_value( v );
				} );
			} else if constexpr( is_optional_v<T> ) {
				static_assert( is_column_value_v<typename T::value_type>,
				               "Unsupported column type" );
				return read_nullable<typename T::value_type>( stmt, column );
			} else {
				static_assert( is_column_value_v<T>, "Unsupported column type" );
				auto result = read_nullable<T>( stmt, column );
				if( not result ) {
					throw sqlite3_exception(
					  "Unexpected NULL in column, use std::optional" );
				}
				return *std::move( result );
			}
		}

		/***
		 * @brief Row level access shared by the prepared statement types.  The
		 * statement is validated once per call rather than once per column
		 */
		template<typename Statement>
		class row_access {
			[[nodiscard]] sqlite3_stmt *checked_handle( ) {
				auto &self = static_cast<Statement &>( *this );
				if( not self.is_good( ) ) {
					throw sqlite3_exception( "Attempt to use an invalid statement" );
				}
				return self.get( );
			}

			[[nodiscard]] sqlite3_stmt *checked_handle( std::size_t column_count ) {
				auto *stmt = checked_handle( );
				if( static_cast<std::size_t>( sqlite3_column_count( stmt ) ) <
				    column_count ) {
					throw sqlite3_exception( "Column specified is out of range" );
				}
				return stmt;
			}

		public:
			/***
			 * @brief Call vis with the value of column as its sqlite storage type
			 */
			template<typename Visitor>
			std::invoke_result_t<Visitor, types::integer_t>
			visit_column( std::size_t column, Visitor &&vis ) {
				auto *stmt = checked_handle( column + 1 );
				return ps_impl::visit_column(
				  stmt, to_column( column ), DAW_FWD( vis ) );
			}

			/***
			 * @brief Call vis( column_index, value ) for each column in the
			 * current row, value is as in visit_column
			 */
			template<typename Visitor>
			void visit_row( Visitor &&vis ) {
				auto *stmt = checked_handle( );
				auto const count = sqlite3_column_count( stmt );
				for( int column = 0; column < count; ++column ) {
					ps_impl::visit_column( stmt, column, [&]( auto value ) {
						vis( static_cast<std::size_t>( column ), value );
					} );
				}
			}

			/***
			 * @brief Read the first sizeof...( Ts ) columns of the current row.  See
			 * ps_impl::read_column for the supported types
			 */
			template<typename... Ts>
			[[nodiscard]] std::tuple<Ts...> get_row( ) {
				auto *stmt = checked_handle( sizeof...( Ts ) );
				return [&]<std::size_t... Is>( std::index_sequence<Is...> ) {
					return std::tuple<Ts...>(
					  read_column<Ts>( stmt, static_cast<int>( Is ) )... );
				}( std::index_sequence_for<Ts...>{ } );
			}
		};
	} // namespace ps_impl

	class shared_prepared_statement;

	/***
	 * @brief Flags passed to sqlite3_prepare_v3.  Values mirror the
	 * SQLITE_PREPARE_* constants
	 */
	enum class prepare_flags : unsigned {
		none = 0x00,
		// Hint that the statement will be retained and reused many times
		persistent = 0x01, // SQLITE_PREPARE_PERSISTENT
		// No-op since sqlite 3.30, kept for completeness
		normalize = 0x02, // SQLITE_PREPARE_NORMALIZE
		// Fail to prepare if the SQL uses virtual tables
		no_vtab = 0x04, // SQLITE_PREPARE_NO_VTAB
		// Suppress errors from this prepare going to the sqlite error log.
		// Requires sqlite 3.48, ignored by older versions
		dont_log = 0x10 // SQLITE_PREPARE_DONT_LOG
	};

	[[nodiscard]] constexpr prepare_flags operator|( prepare_flags lhs,
	                                                 prepare_flags rhs ) {
		return static_cast<prepare_flags>( static_cast<unsigned>( lhs ) |
		                                   static_cast<unsigned>( rhs ) );
	}

	[[nodiscard]] constexpr prepare_flags operator&( prepare_flags lhs,
	                                                 prepare_flags rhs ) {
		return static_cast<prepare_flags>( static_cast<unsigned>( lhs ) &
		                                   static_cast<unsigned>( rhs ) );
	}

	constexpr prepare_flags &operator|=( prepare_flags &lhs, prepare_flags rhs ) {
		return lhs = lhs | rhs;
	}

	template<typename... Ts>
	concept Parameters = ( constructible_from<cell_value, Ts> and ... );

	class prepared_statement : public ps_impl::row_access<prepared_statement> {
		unique_stmt_ptr m_statement = nullptr;

	public:
		using i_am_a_prepared_statement = void;

		explicit prepared_statement( ) = default;
		prepared_statement( database &db, daw::string_view sql,
		                    prepare_flags flags = prepare_flags::none );

		friend class ::daw::sqlite::shared_prepared_statement;

		template<typename Param, typename... Params>
		requires( Parameters<Param, Params...> ) //
		prepared_statement( database &db, daw::string_view sql, Param &&param,
		                    Params &&...params )
		  : prepared_statement( db, sql, prepare_flags::none, DAW_FWD( param ),
		                        DAW_FWD( params )... ) {}

		template<typename Param, typename... Params>
		requires( Parameters<Param, Params...> ) //
		prepared_statement( database &db, daw::string_view sql, prepare_flags flags,
		                    Param &&param, Params &&...params )
		  : prepared_statement( db, sql, flags ) {
			std::size_t index = 1;
			bind( index++, cell_value( DAW_FWD( param ) ) );
			( bind( index++, cell_value( DAW_FWD( params ) ) ), ... );
		}

		[[nodiscard]] sqlite3_stmt *get( ) const {
			return m_statement.get( );
		}

		[[nodiscard]] std::size_t get_column_count( ) {
			auto const count = sqlite3_column_count( get( ) );
			assert( 0 <= count );
			return static_cast<size_t>( count );
		}

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
			return static_cast<bool>( m_statement );
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

	class shared_prepared_statement
	  : public ps_impl::row_access<shared_prepared_statement> {
		std::shared_ptr<sqlite3_stmt> m_statement = nullptr;

	public:
		using i_am_a_prepared_statement = void;

		explicit shared_prepared_statement( ) = default;
		shared_prepared_statement( database &db, daw::string_view sql,
		                           prepare_flags flags = prepare_flags::none );
		shared_prepared_statement( prepared_statement statement );

		template<typename Param, typename... Params>
		requires( Parameters<Param, Params...> ) //
		shared_prepared_statement( database &db, daw::string_view sql,
		                           Param &&param, Params &&...params )
		  : shared_prepared_statement( db, sql, prepare_flags::none,
		                               DAW_FWD( param ), DAW_FWD( params )... ) {}

		template<typename Param, typename... Params>
		requires( Parameters<Param, Params...> ) //
		shared_prepared_statement( database &db, daw::string_view sql,
		                           prepare_flags flags, Param &&param,
		                           Params &&...params )
		  : shared_prepared_statement( db, sql, flags ) {
			std::size_t index = 1;
			bind( index++, cell_value( DAW_FWD( param ) ) );
			( bind( index++, cell_value( DAW_FWD( params ) ) ), ... );
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
			return static_cast<bool>( m_statement );
		}

		// clang-format off
		[[nodiscard]] auto operator<=>( shared_prepared_statement const &rhs ) const {
			return m_statement.get( ) <=> rhs.m_statement.get( );
		}
		// clang-format on

		[[nodiscard]] bool
		operator==( shared_prepared_statement const &rhs ) const {
			return m_statement.get( ) == rhs.m_statement.get( );
		}

		[[nodiscard]] bool
		operator!=( shared_prepared_statement const & ) const = default;
	};

	template<typename T>
	concept PreparedStatement =
	  requires { typename T::i_am_a_prepared_statement; };
} // namespace daw::sqlite