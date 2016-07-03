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
#include <memory>
#include <sqlite3.h>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>
#include <cstdint>
#include <daw/char_range/daw_char_range.h>
#include <daw/daw_range.h>

namespace daw {
	namespace db {
		struct Sqlite3DbException {
			std::string message;

			Sqlite3DbException( std::string msg );

			Sqlite3DbException( int err_no );

			Sqlite3DbException( ) = delete;

			~Sqlite3DbException( ) = default;

			Sqlite3DbException( Sqlite3DbException const & ) = default;

			Sqlite3DbException( Sqlite3DbException && ) = default;

			Sqlite3DbException & operator=( Sqlite3DbException const & ) = default;

			Sqlite3DbException & operator=( Sqlite3DbException && ) = default;
		};    // struct Sqlite3DbException

		enum class Sqlite3DbColumnType { Float, Integer, Text, Blob, Null };

		class Sqlite3Db;
		class Sqlite3DbCellValue;

		class Sqlite3DbPreparedStatement {
			sqlite3_stmt * m_statement;
		public:
			Sqlite3DbPreparedStatement( Sqlite3Db & db, daw::range::CharRange sql );

			~Sqlite3DbPreparedStatement( );

			sqlite3_stmt * get( );

			size_t get_column_count( );

			Sqlite3DbColumnType get_column_type( size_t column );

			daw::range::CharRange get_column_name( size_t column );

			double get_column_float( size_t column );

			int64_t get_column_integer( size_t column );

			daw::range::CharRange get_column_text( size_t column );

			bool is_column_null( size_t column );

			daw::range::Range<uint8_t const *> get_column_blob( size_t column );

			bool is_good( ) const;

			void reset( );

			void bind( size_t index, Sqlite3DbCellValue const & value );
			void bind( size_t index );	// bind a null to that value
		};  // class Sqlite3DbPreparedStatement

		class Sqlite3DbCellValue {
			union value_t {
				double float_value;
				int64_t integer_value;
				std::shared_ptr<daw::range::CharRange const> * text_value;
				std::shared_ptr<daw::range::Range<uint8_t const *>> * blob_value;
			} m_value;
			Sqlite3DbColumnType m_value_type;
		public:
			Sqlite3DbCellValue( Sqlite3DbPreparedStatement & statement, size_t column );
			Sqlite3DbCellValue( Sqlite3DbCellValue const & other );
			Sqlite3DbCellValue & operator=( Sqlite3DbCellValue const & rhs );
			Sqlite3DbCellValue( Sqlite3DbCellValue && other );
			Sqlite3DbCellValue & operator=( Sqlite3DbCellValue && rhs );

			~Sqlite3DbCellValue( );

			Sqlite3DbCellValue( );	// null
			Sqlite3DbCellValue( double value );	// float
			Sqlite3DbCellValue( int64_t value );	// integer
			explicit Sqlite3DbCellValue( daw::range::CharRange value );	// utf-8 string
			explicit Sqlite3DbCellValue( daw::range::Range<uint8_t const *> value );	// blob

			double const & get_float( ) const;

			int64_t const & get_integer( ) const;

			daw::range::CharRange const & get_text( ) const;

			bool is_null( ) const;

			daw::range::Range<uint8_t const *> const & get_blob( ) const;

			Sqlite3DbColumnType get_type( ) const;
		};  // class Sqlite3DbCellValue

		std::string to_string( Sqlite3DbCellValue const & value );

		class Sqlite3Db {
			sqlite3 * m_db;
			bool m_is_open;
			std::mutex m_exec_lock;
		public:
			using callback_t = std::function<void( std::vector<std::pair<daw::range::CharRange, Sqlite3DbCellValue>> result )>;

			Sqlite3Db( Sqlite3Db const & ) = delete;

			Sqlite3Db & operator=( Sqlite3Db const & ) = delete;

			Sqlite3Db( );

			~Sqlite3Db( );

			Sqlite3Db( Sqlite3Db && other );

			Sqlite3Db & operator=( Sqlite3Db && rhs );

			void open( boost::string_ref filename );

			void close( );

			sqlite3 const * get_handle( ) const;

			sqlite3 * get_handle( );

			void exec( daw::range::CharRange sql, callback_t callback = nullptr );
		};    // class Sqlite3Db



	}    // namespace db
}    // namespace daw
