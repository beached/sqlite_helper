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

		explicit query_iterator( shared_prepared_statement statement )
		  : m_statement( DAW_MOVE( statement ) ) {
			operator++( );
		}

		const_reference operator*( ) noexcept;

		inline const_pointer operator->( ) noexcept {
			return &( operator*( ) );
		}

		iterator_type &operator++( );

		inline void operator++( int ) & {
			operator++( );
		}

		inline bool operator==( iterator_type const &rhs ) const {
			if( ( m_statement == rhs.m_statement ) and ( m_row == rhs.m_row ) ) {
				return true;
			}
			if( m_row != rhs.m_row ) {
				return false;
			}
			return not m_statement or not rhs.m_statement;
		}
		inline bool operator!=( iterator_type const & ) const = default;

		inline iterator_type begin( ) {
			return *this;
		}

		inline iterator_type end( ) {
			return iterator_type{ };
		}

		inline std::size_t row( ) const {
			return m_row;
		}

		inline void reset( ) {
			if( m_statement ) {
				m_statement.reset( );
				m_row = static_cast<std::size_t>( -1 );
				operator++( );
			}
		}

		inline std::size_t count( ) {
			auto f = *this;
			auto result = static_cast<std::size_t>( -1 );
			try {
				result = static_cast<std::size_t>( std::distance( f, f.end( ) ) );
			} catch( ... ) {
				reset( );
				throw;
			}
			reset( );
			return result;
		}

		explicit inline operator bool( ) const {
			return static_cast<bool>( m_statement );
		}
	};
} // namespace daw::sqlite
