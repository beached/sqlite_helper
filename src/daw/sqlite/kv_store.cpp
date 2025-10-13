// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/sqlite_helper
//

#include "daw/sqlite/kv_store.h"
#include "daw/sqlite/sqlite3_class.h"

#include <daw/daw_string_view.h>

namespace daw::db {
	kv_store::kv_store( daw::string_view filename ) {}

	kv_store::~kv_store( ) = default;

	std::string kv_store::operator( )( size_t hash ) {
		return "";
	}
}