// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/sqlite_helper
//

#pragma once

#include "sqlite3_exception.h"

#include <daw/daw_contiguous_view.h>
#include <daw/daw_string_view.h>
#include <daw/vector.h>

#include <cstdint>
#include <iostream>
#include <string>

namespace daw::sqlite {
	enum class column_type { Float, Integer, Text, Blob, Null };
	namespace types {
		using real_t = double;
		using integer_t = std::int64_t;
		using text_t = daw::string_view;
		using blob_t = daw::contiguous_view<std::byte const>;
		std::string to_string( blob_t const & );
	} // namespace types

	class prepared_statement;
	class shared_prepared_statement;

	struct cell_value {
		struct null_cell_t {};
		using value_t =
		  std::variant<types::real_t, types::integer_t, types::text_t, types::blob_t, null_cell_t>;

	private:
		value_t m_value = null_cell_t{ };

	public:
		cell_value( prepared_statement &statement, size_t column );
		cell_value( shared_prepared_statement &statement, size_t column );

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

	struct result_cell_t {
		daw::string_view name;
		cell_value value;

		result_cell_t( daw::string_view n, cell_value v )
		  : name( DAW_MOVE( n ) )
		  , value( DAW_MOVE( v ) ) {}
	};

	using result_row_t = daw::vector<result_cell_t>;
} // namespace daw::sqlite
