// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/sqlite_helper
//

#pragma once

#include "daw/sqlite/cell_value.h"

#include <daw/daw_string_view.h>
#include <daw/daw_ensure.h>
#include <daw/daw_move.h>

#include <cstdint>
#include <ranges>

namespace daw::sqlite {
	class result_row_t {
		daw::vector<result_cell_t> m_columns{};

	public:
		explicit result_row_t( ) = default;

		explicit constexpr result_row_t( daw::vector<result_cell_t> columns )
			: m_columns( std::move( columns ) ) {}

		explicit constexpr result_row_t( daw::do_resize_and_overwrite_t,
		                                 std::size_t sz, auto &&operation )
			: m_columns( daw::do_resize_and_overwrite, sz, DAW_FWD( operation ) ) {}

		[[nodiscard]] constexpr result_cell_t const &operator[](
			std::size_t idx ) const {
			return m_columns[idx];
		}

		[[nodiscard]] constexpr cell_value const &operator[](
			daw::string_view name ) const {
			auto const idx = get_index_of( name );
			daw_ensure( idx.has_value( ) );
			return m_columns[*idx].value;
		}

		[[nodiscard]] constexpr std::optional<std::size_t> get_index_of(
			daw::string_view name ) const noexcept {
			auto const pos = std::ranges::find( m_columns,
			                                    name,
			                                    &result_cell_t::name );
			if(pos == m_columns.cend( )) {
				return std::nullopt;
			}
			return static_cast<std::size_t>(std::ranges::distance(
				m_columns.cbegin( ),
				pos ));
		}

		[[nodiscard]] auto const &front( ) const {
			return m_columns.front( );
		}

		[[nodiscard]] auto const &back( ) const {
			return m_columns.back( );
		}

		[[nodiscard]] auto begin( ) const {
			return m_columns.begin( );
		}

		[[nodiscard]] auto end( ) const {
			return m_columns.end( );
		}

		[[nodiscard]] auto cbegin( ) const {
			return m_columns.cbegin( );
		}

		[[nodiscard]] auto cend( ) const {
			return m_columns.cend( );
		}

		[[nodiscard]] auto *data( ) const {
			return m_columns.data( );
		}

		[[nodiscard]] std::size_t size( ) const {
			return m_columns.size( );
		}
	};
}