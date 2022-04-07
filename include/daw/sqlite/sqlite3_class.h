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
#include "query_iterator.h"
#include "sqlite3_exception.h"

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
#include <sqlite3.h>
#include <utility>

namespace daw::sqlite {
	namespace sqlite_impl {
		struct sqlite_deleter {
			inline void operator( )( sqlite3 *ptr ) const noexcept {
				sqlite3_close( ptr );
			}
		};
	} // namespace sqlite_impl

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

		query_iterator exec( prepared_statement statement );
		query_iterator exec( shared_prepared_statement statement );

		query_iterator exec( daw::string_view sql ) {
			assert( m_db );
			return exec( prepared_statement( *this, sql ) );
		}

		void exec( PreparedStatement auto statement, exec_callback auto callback ) {
			assert( m_db );
			for( auto &&row : exec( DAW_MOVE( statement ) ) ) {
				callback( DAW_FWD( row ) );
			}
		}

		void exec( daw::string_view sql, exec_callback auto callback ) {
			assert( m_db );
			exec( prepared_statement( *this, sql ), DAW_MOVE( callback ) );
		}
	}; // class database
} // namespace daw::sqlite
