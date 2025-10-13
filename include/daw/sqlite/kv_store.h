// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/sqlite_helper
//

#pragma once

#include <daw/daw_string_view.h>

#include <concepts>
#include <sstream>
#include <string>

namespace daw::db {
	struct kv_store {
		explicit kv_store( daw::string_view filename );
		virtual ~kv_store( );
		[[nodiscard]] std::string operator( )( size_t hash );

		template<typename Key>
			requires( not std::same_as<std::size_t, Key> )
		[[nodiscard]] std::string operator( )( Key key ) {
			static std::hash<Key> const hash{};
			return get( hash( key ) );
		}

		template<typename Key, typename Value>
			requires( not std::same_as<std::string, Value> )
		[[nodiscard]] Value operator( )( Key key ) const {
			std::stringstream ss;
			auto tmp = get( hash( key ) );
			ss << tmp;
			Value result;
			ss >> result;
			return result;
		}
	};
}