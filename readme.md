# Sqlite Helper

Sqlite Helper is a small library to aid in using sqlite3 from C++. It provides RAII wrappers, prepared statements, and a
query iterator to iterate over each row in a result.

## Installation

Currently it is a FetchContent'able cmake library. But can be installed too.

```cmake
FetchContent_Declare(
        daw_sqlite_helper
        GIT_REPOSITORY https://github.com/beached/sqlite_helper
)
# ....
target_link_libraries(MyTarget daw::daw-sqlite-helper)
```

### SQLite build options

SQLite is built from the amalgamation pinned in `extern/Sqlite3/CMakeLists.txt`. Its
[compile-time options](https://www.sqlite.org/compile.html) are set with `DAW_SQLITE3_DEFINITIONS`, a `;` list that is
empty by default, which keeps SQLite's own defaults:

```cmake
set( DAW_SQLITE3_DEFINITIONS "SQLITE_ENABLE_FTS5;SQLITE_ENABLE_MATH_FUNCTIONS;SQLITE_DQS=0" )
FetchContent_MakeAvailable( daw_sqlite_helper )
```

or `-DDAW_SQLITE3_DEFINITIONS="..."` on the command line. The definitions are public on the SQLite target, so code that
includes `sqlite3.h` sees the same declarations SQLite was built with. Most `SQLITE_OMIT_*` options are not supported
with the amalgamation.

Compiler flags for the amalgamation alone, such as `-march=native`, go in `DAW_SQLITE3_COMPILE_OPTIONS`, also a `;`
list. They are private to the SQLite target.

To link the system's SQLite instead, set `DAW_USE_SYSTEM_SQLITE3=ON`; it is found with `find_package( SQLite3 )`.
`DAW_SQLITE3_DEFINITIONS` and `DAW_SQLITE3_COMPILE_OPTIONS` have no effect then, and setting either gives a warning.



#### Opening a database

```c++
auto db = daw::sqlite::database( "file.sqlite" );
// Optional SQLITE_OPEN_* flags for sqlite3_open_v2, defaults to READWRITE | CREATE
auto ro = daw::sqlite::database( "file.sqlite", SQLITE_OPEN_READONLY );
```

#### Prepared statements

```c++
// Optional flags for sqlite3_prepare_v3, e.g. for long-lived statements
auto stmt = daw::sqlite::shared_prepared_statement(
  db, "INSERT INTO tbl VALUES( ?, ? );", daw::sqlite::prepare_flags::persistent );

// Flags go after the SQL and before any parameters, and can be combined with |
using daw::sqlite::prepare_flags;
auto it = db.exec( "SELECT colA FROM tbl WHERE name=?",
                   prepare_flags::persistent | prepare_flags::no_vtab,
                   "foo" );
```

Available flags mirror `SQLITE_PREPARE_*`: `persistent`, `no_vtab`, `dont_log` (sqlite 3.48+), and
`normalize` (a no-op in current sqlite).

#### Querying a database

```c++
auto it = db.exec( 
      "SELECT colA, colB FROM tbl WHERE name=?",
      "foo" 
    );
```

#### Typed statements

When the result columns are known, put their types in the statement. The column count is checked once when it is
prepared, and rows are read as a `std::tuple` with no per-row validation.

```c++
using daw::sqlite::types::text_t;
auto stmt = daw::sqlite::typed_statement<std::int64_t, text_t, std::optional<std::int64_t>>(
  db, "SELECT id, body, parent_id FROM posts WHERE user_id = ?1", user_id );

for( auto [id, body, parent_id] : stmt.rows( ) ) {
  // body views sqlite's memory and is valid until the next row
}
```

Columns can be any arithmetic type, `text_t`, `std::string`, `blob_t`, `cell_value`, or a `std::optional` of those.
NULL in a column that isn't `std::optional` throws. The range from `rows( )` resets the statement when it is destroyed,
keeping its bindings, so `bind` and call `rows( )` again to rerun it.

#### Using query results

```c++
std::cout << "Found " << it.count( ) << " rows\n";
for( auto const & row: it ) {
  for( auto const & col: row ) {  
    std::cout << col.name << ": " << col.value << '\n';    
  }  
  std::cout << '\n';
}
```

The returned iterator can be reset to the beginning of the row set by calling `reset( )`.
