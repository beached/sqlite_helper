// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/sqlite_helper
//

#pragma once

#include <exception>
#include <string>

namespace daw::sqlite {
	class sqlite3_exception : public std::exception {
		int m_error = -1;
		std::string m_message;

	public:
		explicit sqlite3_exception( int err_no );
		explicit sqlite3_exception( std::string message );

		[[nodiscard]] char const *what( ) const noexcept override;
		[[nodiscard]] int error( ) const;
	};
}