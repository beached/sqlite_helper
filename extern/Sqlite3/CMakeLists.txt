include(FetchContent)
include(GNUInstallDirs)

FetchContent_Declare(
    sqlite3
    URL https://www.sqlite.org/2026/sqlite-amalgamation-3540000.zip
    URL_HASH SHA256=68e913b0fe8ec6b4e5f391c594a28872884e80d6cd042a1e50c957be46b5aa4d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(sqlite3)

add_library(sqlite3 STATIC
    ${sqlite3_SOURCE_DIR}/sqlite3.c
)

# SQLite compile-time options, e.g. "SQLITE_ENABLE_FTS5;SQLITE_DQS=0".  Empty
# leaves SQLite's defaults.  PUBLIC because sqlite3.h changes what it declares
# for some options, such as SQLITE_OMIT_DEPRECATED and SQLITE_ENABLE_SESSION.
# A parent project can set this as a normal variable before adding this one
set(DAW_SQLITE3_DEFINITIONS "" CACHE STRING
    "Compile definitions for the bundled SQLite amalgamation (a ; list)")
target_compile_definitions(sqlite3 PUBLIC ${DAW_SQLITE3_DEFINITIONS})

target_include_directories(sqlite3 PUBLIC
    $<BUILD_INTERFACE:${sqlite3_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

set_target_properties(sqlite3 PROPERTIES
    EXPORT_NAME SQLite3
    POSITION_INDEPENDENT_CODE ON
)

add_library(SQLite::SQLite3 ALIAS sqlite3)

install(TARGETS sqlite3
    EXPORT SQLite3Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES
    ${sqlite3_SOURCE_DIR}/sqlite3.h
    ${sqlite3_SOURCE_DIR}/sqlite3ext.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT SQLite3Targets
    FILE SQLite3Targets.cmake
    NAMESPACE SQLite::
    DESTINATION lib/cmake/${PROJECT_NAME}
)
