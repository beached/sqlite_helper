// The MIT License (MIT)
//
// Copyright (c) 2014-2015 Darrell Wright
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files( the "Software" ), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <boost/utility/string_ref.hpp>
#include <string>
#include <sstream>

namespace daw {
	namespace db {

		struct kv_store {
			kv_store( boost::string_ref filename );
			virtual ~kv_store( );
			std::string operator( )( size_t hash );

			template<typename Key, typename std::enable_if_t<!std::is_same<size_t, Key>::value > >
			std::string operator( )( Key key ) {
				static std::hash<Key> const hash;
				return get( hash( key ) );
			}

			template<typename Key, typename Value, typename std::enable_if_t<!std::is_same<std::string, Value>::value > >
			Value operator( )( Key key ) const {
				std::stringstream ss;
				auto tmp = et( hash( key ) );
				ss << tmp;
				Value result;
				ss >> result;
				return result;
			}

		};

	}    // namespace db
}    // namespace daw
