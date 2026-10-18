// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/sqlite_helper
//

#pragma once

#include <daw/daw_unique_ptr.h>

#include <exception>
#include <sqlite3.h>
#include <string>

namespace daw::sqlite {
	class sqlite3_exception : public std::exception {
		int m_error = -1;
		std::string m_message;

	public:
		explicit sqlite3_exception( int err_no );
		sqlite3_exception( int err_no, std::string message );
		explicit sqlite3_exception( std::string message );

		[[nodiscard]] char const *what( ) const noexcept override;
		[[nodiscard]] int error( ) const;
	};

	namespace se_impl {
		struct sqlite_char_ptr_deleter {
			DAW_CPP23_STATIC_CALL_OP inline void
			operator( )( char *p ) DAW_CPP23_STATIC_CALL_OP_CONST {
				sqlite3_free( p );
			}
		};
	} // namespace se_impl
	using unique_sqlite_char_ptr =
	  daw::unique_ptr<char, se_impl::sqlite_char_ptr_deleter>;
} // namespace daw::sqlite