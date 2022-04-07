// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/sqlite_helper
//

#pragma once

#include <daw/daw_string_view.h>

#include <sstream>
#include <string>

namespace daw {
	namespace db {

		struct kv_store {
			kv_store( daw::string_view filename );
			virtual ~kv_store( );
			std::string operator( )( size_t hash );

			template<typename Key, typename std::enable_if_t<!std::is_same<size_t, Key>::value>>
			std::string operator( )( Key key ) {
				static std::hash<Key> const hash;
				return get( hash( key ) );
			}

			template<typename Key,
			         typename Value,
			         typename std::enable_if_t<!std::is_same<std::string, Value>::value>>
			Value operator( )( Key key ) const {
				std::stringstream ss;
				auto tmp = et( hash( key ) );
				ss << tmp;
				Value result;
				ss >> result;
				return result;
			}
		};

	} // namespace db
} // namespace daw
