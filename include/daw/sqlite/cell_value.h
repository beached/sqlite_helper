// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/sqlite_helper
//

#pragma once

#include "daw/sqlite/sqlite3_exception.h"

#include <daw/daw_contiguous_view.h>
#include <daw/daw_string_view.h>
#include <daw/daw_visit.h>

#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <variant>

namespace daw::sqlite {
	enum class column_type { Float, Integer, Text, Blob, Null };

	namespace types {
		using real_t = double;
		using integer_t = std::int64_t;
		using text_t = daw::string_view;
		using blob_t = daw::contiguous_view<std::byte const>;
		[[nodiscard]] std::string to_string( blob_t const & );
	} // namespace types

	class prepared_statement;
	class shared_prepared_statement;

	struct cell_value {
		struct null_cell_t {};

		using value_t = std::variant<types::real_t, types::integer_t, types::text_t,
		                             types::blob_t, null_cell_t>;

	private:
		value_t m_value = null_cell_t{};

	public:
		explicit cell_value( ) = default;

		explicit cell_value( prepared_statement &statement, size_t column );
		explicit cell_value( shared_prepared_statement &statement, size_t column );


		template<std::same_as<bool> Bool>
		explicit constexpr cell_value( Bool value )
			: m_value{std::in_place_type<types::integer_t>, value} {}

		explicit constexpr cell_value( types::real_t value )
			: m_value{std::in_place_type<types::real_t>, value} {}

		explicit constexpr cell_value( types::integer_t value )
			: m_value{std::in_place_type<types::integer_t>, value} {}

		explicit constexpr cell_value( types::text_t value )
			: m_value{std::in_place_type<types::text_t>, value} {}

		explicit constexpr cell_value( types::blob_t value )
			: m_value{std::in_place_type<types::blob_t>, value} {}

		explicit DAW_CONSTEVAL cell_value( std::nullptr_t ) {}

		explicit constexpr cell_value( null_cell_t ) {}

		[[nodiscard]] constexpr types::real_t const &get_float( ) const {
			auto *ptr = std::get_if<types::real_t>( &m_value );
			if(not ptr) {
				throw sqlite3_exception( "Cell Value is not of type Real" );
			}
			return *ptr;
		}

		[[nodiscard]] constexpr bool get_bool( ) const {
			auto *ptr = std::get_if<types::integer_t>( &m_value );
			if(not ptr) {
				throw sqlite3_exception( "Cell Value is not of type Integer" );
			}
			return static_cast<bool>(*ptr);
		}

		[[nodiscard]] constexpr types::integer_t const &get_integer( ) const {
			auto *ptr = std::get_if<types::integer_t>( &m_value );
			if(not ptr) {
				throw sqlite3_exception( "Cell Value is not of type Integer" );
			}
			return *ptr;
		}

		[[nodiscard]] constexpr types::text_t const &get_text( ) const {
			auto *ptr = std::get_if<types::text_t>( &m_value );
			if(not ptr) {
				throw sqlite3_exception( "Cell Value is not of type Text" );
			}
			return *ptr;
		}

		[[nodiscard]] constexpr types::blob_t const &get_blob( ) const {
			auto *ptr = std::get_if<types::blob_t>( &m_value );
			if(not ptr) {
				throw sqlite3_exception( "Cell Value is not of type Blob" );
			}
			return *ptr;
		}

		[[nodiscard]] constexpr bool is_null( ) const {
			return std::holds_alternative<null_cell_t>( m_value );
		}

		[[nodiscard]] constexpr column_type get_type( ) const {
			// column_type enumerators are in the same order as the alternatives
			return static_cast<column_type>( m_value.index( ) );
		}

		/***
		 * @brief Visit the stored value with an overload set of visitors.  The
		 * alternatives are trivial so the variant is never valueless
		 */
		template<typename... Visitors>
		constexpr decltype( auto ) visit( Visitors &&...visitors ) const {
			return daw::visit_nt( m_value, DAW_FWD( visitors )... );
		}
	}; // class cell_value

	static_assert(
	  std::same_as<std::variant_alternative_t<static_cast<std::size_t>(
	                                            column_type::Float ),
	                                          cell_value::value_t>,
	               types::real_t> and
	  std::same_as<std::variant_alternative_t<static_cast<std::size_t>(
	                                            column_type::Integer ),
	                                          cell_value::value_t>,
	               types::integer_t> and
	  std::same_as<std::variant_alternative_t<static_cast<std::size_t>(
	                                            column_type::Text ),
	                                          cell_value::value_t>,
	               types::text_t> and
	  std::same_as<std::variant_alternative_t<static_cast<std::size_t>(
	                                            column_type::Blob ),
	                                          cell_value::value_t>,
	               types::blob_t> and
	  std::same_as<std::variant_alternative_t<static_cast<std::size_t>(
	                                            column_type::Null ),
	                                          cell_value::value_t>,
	               cell_value::null_cell_t>,
	  "column_type must match the order of cell_value::value_t" );

	[[nodiscard]] std::string to_string( cell_value const &value );

	struct result_cell_t {
		daw::string_view name;
		cell_value value;

		result_cell_t( daw::string_view n, cell_value v )
			: name( n )
			  , value( v ) {}
	};
} // namespace daw::sqlite