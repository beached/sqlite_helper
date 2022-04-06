// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/sqlite_helper
//

#pragma once

#include "cell_value.h"
#include "prepared_statement.h"

#include <daw/daw_consteval.h>
#include <daw/daw_string_view.h>
#include <daw/vector.h>

#include <optional>

namespace daw::sqlite {
	class database;

	struct query_iterator {
		using iterator_type = query_iterator;
		using value_type = result_row_t;
		using difference_type = std::ptrdiff_t;
		using pointer = value_type *;
		using const_pointer = value_type const *;
		using reference = value_type &;
		using const_reference = value_type const &;
		using iterator_category = std::input_iterator_tag;
		friend class ::daw::sqlite::database;
	private:
		shared_prepared_statement m_statement = { };
		std::size_t m_row = static_cast<std::size_t>( -1 );
		std::optional<result_row_t> m_last_value = { };

		explicit query_iterator( prepared_statement statement )
		  : m_statement( DAW_MOVE( statement ) ) {
			operator++( );
		}
	public:
		explicit query_iterator( ) = default;

		const_reference operator*( ) noexcept;

		inline const_pointer operator->( ) noexcept {
			return &( operator*( ) );
		}

		iterator_type &operator++( );

		inline void operator++( int ) & {
			operator++( );
		}

		constexpr bool operator==( iterator_type const &rhs ) const {
			return ( m_statement == rhs.m_statement ) and ( m_row == rhs.m_row );
		}
		constexpr bool operator!=( iterator_type const & ) const = default;

		iterator_type begin( ) {
			return *this;
		}

		iterator_type end( ) {
			return iterator_type{ };
		}

		explicit inline operator bool( ) const {
			return static_cast<bool>( m_statement );
		}
	};
} // namespace daw::sqlite
