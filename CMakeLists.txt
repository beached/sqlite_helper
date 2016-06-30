cmake_minimum_required( VERSION 3.2 )

project( sqlite_helper_prj )

include( ExternalProject )

find_package( Boost 1.59.0 COMPONENTS system filesystem regex unit_test_framework REQUIRED )
find_package( OpenSSL REQUIRED )

if( ${CMAKE_CXX_COMPILER_ID} STREQUAL 'MSVC' )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_WIN32_WINNT=0x0601" )
else( )
	find_package( Threads REQUIRED )
	if( THREADS_HAVE_PTHREAD_ARG )
		set( COMPILER_SPECifIC_LIBS, "-lpthread" )
	endif( )

	if( ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" )
		set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -stdlib=libc++ -Weverything -Wno-c++98-compat -Wfatal-errors -Wno-covered-switch-default -Wno-padded -Wno-exit-time-destructors -Wno-c++98-compat-pedantic -Wno-unused-parameter -Wno-missing-noreturn -Wno-missing-prototypes -Wno-disabled-macro-expansion" )
	elseif( ${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" )
		set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Wno-deprecated-declarations" )
	endif( )
endif( )

externalproject_add(
    lib_nodepp_prj
    GIT_REPOSITORY "https://github.com/beached/lib_nodepp.git"
    GIT_TAG "master"
    SOURCE_DIR "${CMAKE_BINARY_DIR}/deps/nodepp"
	INSTALL_DIR "${CMAKE_BINARY_DIR}/inst"
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/inst
    PATCH_COMMAND ""
    TEST_COMMAND ""
)

externalproject_add(
    header_libraries_prj
    GIT_REPOSITORY "https://github.com/beached/header_libraries.git"
    GIT_TAG "master"
    SOURCE_DIR "${CMAKE_BINARY_DIR}/deps/headlibs"
	INSTALL_DIR "${CMAKE_BINARY_DIR}/inst"
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/inst
    PATCH_COMMAND ""
    TEST_COMMAND ""
)

externalproject_add(
    char_range_prj
    GIT_REPOSITORY "https://github.com/beached/char_range.git"
    GIT_TAG "master"
    SOURCE_DIR "${CMAKE_BINARY_DIR}/deps/chrrng"
	INSTALL_DIR "${CMAKE_BINARY_DIR}/inst"
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/inst
    PATCH_COMMAND ""
    TEST_COMMAND ""
)

set( HEADER_FOLDER "include" )

set( HEADER_FILES
	${HEADER_FOLDER}/sqlite3_class.h
	${HEADER_FOLDER}/kv_store.h
)	

set( SOURCE_FILES
	sqlite3_class.cpp
	kv_store.cpp
)

add_subdirectory( ./sqlite3 )
include_directories( ./sqlite3/src )

include_directories( SYSTEM "${CMAKE_BINARY_DIR}/inst/include" )
include_directories( SYSTEM ${Boost_INCLUDE_DIRS} )
link_directories( "${CMAKE_BINARY_DIR}/inst/lib" )
include_directories( "./include" )


include_directories( SYSTEM ${Boost_INCLUDE_DIRS} )

add_library( sqlite_helper STATIC ${HEADER_FILES} ${SOURCE_FILES} )
add_dependencies( sqlite_helper lib_nodepp_prj header_libraries_prj char_range_prj )
target_link_libraries( sqlite_helper sqlite3_static char_range lib_nodepp ${Boost_LIBRARIES} )

install( TARGETS sqlite_helper DESTINATION lib )
install( FILES ${CMAKE_BINARY_DIR}/sqlite3/libsqlite3.so DESTINATION lib )
install( FILES ${CMAKE_BINARY_DIR}/sqlite3/libsqlite3_static.a DESTINATION lib )
install( DIRECTORY ${HEADER_FOLDER}/ DESTINATION include/daw/sqlite_helper )

