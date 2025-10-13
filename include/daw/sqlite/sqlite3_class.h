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
#include "daw/sqlite/query_iterator.h"

#include <daw/daw_string_view.h>
#include <daw/daw_take.h>
#include <daw/vector.h>

#include <exception>
#include <filesystem>
#include <memory>
#include <sqlite3.h>

namespace daw::sqlite {
	namespace sqlite_impl {
		struct sqlite_deleter {
			DAW_CPP23_STATIC_CALL_OP void operator( )(
				sqlite3 *ptr ) DAW_CPP23_STATIC_CALL_OP_CONST noexcept {
				sqlite3_close( ptr );
			}
		};
	} // namespace sqlite_impl

	class database {
		std::unique_ptr<sqlite3, sqlite_impl::sqlite_deleter> m_db{};
		daw::take_t<bool> m_is_open{};

	public:
		explicit database( ) = default;

		/***
		 * @brief Open an existing or creat a new sqlite database at the path
		 * specified
		 */
		explicit database( std::filesystem::path filename );

		/***
		 * @brief Give ownership of an existing sqlite db
		 */
		explicit database( sqlite3 *db );

		void open( std::filesystem::path filename );
		void close( );
		[[nodiscard]] sqlite3 const *get_handle( ) const;
		[[nodiscard]] sqlite3 *get_handle( );
		[[nodiscard]] sqlite3 *release( );
		[[nodiscard]] daw::vector<std::string> tables( );
		[[nodiscard]] bool has_table( daw::string_view table_name );

		query_iterator exec( prepared_statement statement );
		query_iterator exec( shared_prepared_statement statement );

		template<typename... Params>
			requires( Parameters<Params...> ) //
		query_iterator exec( daw::string_view sql,
		                     Params &&... params ) {
			assert( m_db );
			return exec(
				shared_prepared_statement( *this, sql, DAW_FWD( params )... ) );
		}
	}; // class database
}    // namespace daw::sqlite