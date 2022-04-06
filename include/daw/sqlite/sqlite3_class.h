// The MIT License (MIT)
//
// Copyright (c) 2014-2017 Darrell Wright
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files( the "Software" ), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and / or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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
		query_iterator exec( std::string const &sql );

		template<exec_callback Callback>
		void exec( prepared_statement statement, Callback cb ) {
			assert( m_db );
			for( auto &&row : exec( DAW_MOVE( statement ) ) ) {
				cb( DAW_FWD( row ) );
			}
		}

		template<exec_callback Callback>
		void exec( daw::string_view sql, Callback cb ) {
			assert( m_db );
			exec( prepared_statement( *this, sql ), DAW_MOVE( cb ) );
		}
	}; // class database
} // namespace daw::sqlite
