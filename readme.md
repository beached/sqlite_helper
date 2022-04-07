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
target_link_libraries(MyTarget daw::daw-sql-helper)
```

## Usage

#### Opening a database

```c++
auto db = daw::sqlite::database( "file.sqlite" );
```

#### Querying a database

```c++
auto it = db.exec( 
      db, 
      "SELECT colA, colB FROM tbl WHERE name=?",
      "foo" 
    );
```

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