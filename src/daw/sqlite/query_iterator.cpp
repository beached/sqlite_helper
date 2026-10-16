// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/sqlite_helper
//

#include "daw/sqlite/query_iterator.h"
#include "daw/sqlite/prepared_statement.h"
#include "daw/sqlite/result_row.h"
#include "daw/sqlite/sqlite3_exception.h"

#include <sqlite3.h>

namespace daw::sqlite {
	query_iterator::const_reference query_iterator::front( ) const {
		if( not m_last_value ) {
			if( not m_statement.is_good( ) ) {
				throw sqlite3_exception( "Attempt to use an invalid statement" );
			}
			// Validated once for the row, so the columns can be read unchecked
			auto *stmt = m_statement.get( );
			auto const column_count = m_statement.get_column_count( );
			m_last_value = result_row_t(
			  daw::do_resize_and_overwrite,
			  column_count,
			  [&]( auto *ptr, std::size_t sz ) {
				  for( int column = 0; column != static_cast<int>( column_count );
				       ++column ) {
					  std::construct_at( ptr + column,
					                     ps_impl::column_name( stmt, column ),
					                     ps_impl::read_column<cell_value>( stmt, column ) );
				  }
				  return sz;
			  } );
		}
		return *m_last_value;
	}

	query_iterator::iterator_type &query_iterator::operator++( ) {
		m_last_value.reset( );
		int rc = sqlite3_step( m_statement.get( ) );
		if( rc == SQLITE_DONE ) {
			m_row = static_cast<std::size_t>( -1 );
		} else if( rc != SQLITE_DONE and rc != SQLITE_ROW ) {
			throw sqlite3_exception( rc );
		} else {
			++m_row;
		}
		return *this;
	}
} // namespace daw::sqlite