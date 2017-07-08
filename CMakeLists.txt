cmake_minimum_required( VERSION 3.2 )

project( sqlite_helper_prj )

include( ExternalProject )

find_package( Boost 1.59.0 COMPONENTS system filesystem regex unit_test_framework REQUIRED )
find_package( OpenSSL REQUIRED )

if( ${CMAKE_CXX_COMPILER_ID} STREQUAL 'MSVC' )
	add_compile_options( -D_WIN32_WINNT=0x0601 /std:c++latest ) 
else( )
	if( ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "AppleClang" )
		add_compile_options(-march=native -pthread -Weverything -Wno-c++98-compat -Wno-covered-switch-default -Wno-padded -Wno-exit-time-destructors -Wno-c++98-compat-pedantic -Wno-unused-parameter -Wno-missing-noreturn -Wno-missing-prototypes -Wno-disabled-macro-expansion)
		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++14 -g")
		set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++14 -O3")
	elseif( ${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" )
		add_compile_options(-std=c++14 -march=native -pthread -Wall -Wno-deprecated-declarations)
		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++14 -g")
		set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++14 -O3")
	endif( )
endif( )

externalproject_add(
    headlibsprj
    GIT_REPOSITORY "https://github.com/beached/header_libraries.git"
    GIT_TAG "master"
    SOURCE_DIR "${CMAKE_BINARY_DIR}/deps/headlibs"
	INSTALL_DIR "${CMAKE_BINARY_DIR}/inst"
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/inst
)

externalproject_add(
    chrrngprj
    GIT_REPOSITORY "https://github.com/beached/char_range.git"
    GIT_TAG "master"
    SOURCE_DIR "${CMAKE_BINARY_DIR}/deps/chrrng"
	INSTALL_DIR "${CMAKE_BINARY_DIR}/inst"
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/inst
)

set( HEADER_FOLDER "include" )
set( SOURCE_FOLDER "src" )

set( HEADER_FILES
	${HEADER_FOLDER}/sqlite3_class.h
	${HEADER_FOLDER}/kv_store.h
)	

set( SOURCE_FILES
	${SOURCE_FOLDER}/sqlite3_class.cpp
	${SOURCE_FOLDER}/kv_store.cpp
)

add_subdirectory( ./sqlite3 )
include_directories( ./sqlite3/src )

include_directories( SYSTEM "${CMAKE_BINARY_DIR}/inst/include" )
include_directories( SYSTEM ${Boost_INCLUDE_DIRS} )
link_directories( "${CMAKE_BINARY_DIR}/inst/lib" )
include_directories( "./include" )


include_directories( SYSTEM ${Boost_INCLUDE_DIRS} )

add_library( sqlite_helper STATIC ${HEADER_FILES} ${SOURCE_FILES} )
add_dependencies( sqlite_helper headlibsprj chrrngprj )
target_link_libraries( sqlite_helper sqlite3_static char_range lib_nodepp ${Boost_LIBRARIES} )

install( TARGETS sqlite_helper DESTINATION lib )
install( FILES ${CMAKE_BINARY_DIR}/sqlite3/libsqlite3.so DESTINATION lib )
install( FILES ${CMAKE_BINARY_DIR}/sqlite3/libsqlite3_static.a DESTINATION lib )
install( DIRECTORY ${HEADER_FOLDER}/ DESTINATION include/daw/sqlite_helper )

