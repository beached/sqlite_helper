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

#include <daw/daw_contiguous_view.h>
#include <daw/daw_string_view.h>
#include <daw/daw_take.h>
#include <daw/utf_range/daw_utf_range.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <sqlite3.h>
#include <utility>
#include <vector>

namespace daw {
	namespace db {
		namespace types {
			using real_t = double;
			using integer_t = int64_t;
			using text_t = daw::range::utf_range;
			using blob_t = daw::contiguous_view<std::uint8_t const>;
			std::string to_string( blob_t const & );
		} // namespace types

		struct Sqlite3DbException {
			std::string message;

			Sqlite3DbException( std::string msg );

			Sqlite3DbException( int err_no );
		}; // struct Sqlite3DbException

		enum class Sqlite3DbColumnType { Float, Integer, Text, Blob, Null };

		class Sqlite3Db;
		class Sqlite3DbCellValue;

		class Sqlite3DbPreparedStatement {
			sqlite3_stmt *m_statement;

		public:
			Sqlite3DbPreparedStatement( Sqlite3Db &db, daw::range::utf_range sql );

			~Sqlite3DbPreparedStatement( );

			sqlite3_stmt *get( );

			size_t get_column_count( );

			Sqlite3DbColumnType get_column_type( size_t column );

			types::text_t get_column_name( size_t column );

			types::real_t get_column_float( size_t column );

			types::integer_t get_column_integer( size_t column );

			daw::range::utf_range get_column_text( size_t column );

			bool is_column_null( size_t column );

			types::blob_t get_column_blob( size_t column );

			bool is_good( ) const;

			void reset( );

			void bind( size_t index, Sqlite3DbCellValue const &value );
			void bind( size_t index ); // bind a null to that value
		};                           // class Sqlite3DbPreparedStatement

		struct Sqlite3DbCellValue {
			using value_t =
			  std::variant<types::real_t, types::integer_t, types::text_t, types::blob_t, std::nullptr_t>;

		private:
			Sqlite3DbColumnType m_value_type;
			value_t m_value;

		public:
			Sqlite3DbCellValue( Sqlite3DbPreparedStatement &statement, size_t column );

			Sqlite3DbCellValue( );                              // null
			Sqlite3DbCellValue( types::real_t value );          // float
			Sqlite3DbCellValue( types::integer_t value );       // integer
			explicit Sqlite3DbCellValue( types::text_t value ); // utf-8 string
			explicit Sqlite3DbCellValue( types::blob_t value ); // blob

			types::real_t const &get_float( ) const;

			types::integer_t const &get_integer( ) const;

			types::text_t const &get_text( ) const;

			bool is_null( ) const;

			types::blob_t const &get_blob( ) const;

			Sqlite3DbColumnType get_type( ) const;
		}; // class Sqlite3DbCellValue

		std::string to_string( Sqlite3DbCellValue const &value );

		struct sqlite_deleter {
			inline void operator( )( sqlite3 *ptr ) const noexcept {
				sqlite3_close( ptr );
			}
		};

		class Sqlite3Db {
			std::unique_ptr<sqlite3, sqlite_deleter> m_db;
			daw::take_t<bool> m_is_open;
			daw::take_t<std::mutex> m_exec_lock;

		public:
			using callback_t = std::function<void(
			  std::vector<std::pair<daw::range::utf_range, Sqlite3DbCellValue>> result )>;

			Sqlite3Db( ) = default;

			void open( daw::string_view filename );
			void close( );
			sqlite3 const *get_handle( ) const;
			sqlite3 *get_handle( );
			void exec( daw::range::utf_range sql, callback_t callback = nullptr );
		}; // class Sqlite3Db
	}    // namespace db
} // namespace daw
