cmake_minimum_required( VERSION 4.2.0 )
project( daw-sqlite-helper
				 VERSION "0.4.1"
				 DESCRIPTION "C++ bindings for sqlite3"
				 HOMEPAGE_URL "https://github.com/beached/sqlite_helper"
				 LANGUAGES C CXX
				 )

option( DAW_USE_SYSTEM_SQLITE3 "Use the system Sqlite3 install" OFF )
if( DAW_USE_SYSTEM_SQLITE3 )
	find_package( SQLite3 REQUIRED )
	# These only apply when SQLite is built from the bundled amalgamation
	if( DAW_SQLITE3_DEFINITIONS OR DAW_SQLITE3_COMPILE_OPTIONS )
		message( WARNING "DAW_USE_SYSTEM_SQLITE3 is ON, so DAW_SQLITE3_DEFINITIONS and "
		         "DAW_SQLITE3_COMPILE_OPTIONS are ignored" )
	endif()
endif( )

add_subdirectory( extern )

include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/include" )

add_library( ${PROJECT_NAME}
						 src/daw/sqlite/sqlite3_class.cpp
						 src/daw/sqlite/query_iterator.cpp
						 src/daw/sqlite/prepared_statement.cpp
						 )
target_link_libraries( ${PROJECT_NAME}
											 PUBLIC
											 daw::daw-header-libraries
											 daw::daw-utf-range
											 SQLite::SQLite3
											 )
add_library( daw::${PROJECT_NAME} ALIAS ${PROJECT_NAME} )
target_compile_features( ${PROJECT_NAME} PUBLIC cxx_std_20 )

# Clang defines several feature macros for previous C++ standards then warns if you use them :(
option( DAW_SQLITE3_SUPPRESS_CLANG_EXTENSION_WARNINGS
		"Disable Clang's C++23 and C++26 extension warnings" ON )
if( DAW_SQLITE3_SUPPRESS_CLANG_EXTENSION_WARNINGS )
	if( ${CMAKE_CXX_COMPILER_ID} STREQUAL "AppleClang" )
		if( CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 16 )
			target_compile_options( ${PROJECT_NAME} PRIVATE
					-Wno-c++23-extensions
					-Wno-c++26-extensions
					)
		endif()
	elseif( ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" )
		if( CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 17 )
			target_compile_options( ${PROJECT_NAME} PRIVATE
					-Wno-c++23-extensions
					-Wno-c++26-extensions
					)
		endif()
	endif()
endif()


#Install
include( GNUInstallDirs )

option( BUILD_SHARED_LIBS "Build shared library" ON )

target_include_directories( ${PROJECT_NAME} INTERFACE
														$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
														$<INSTALL_INTERFACE:include>
														PRIVATE src )

install( TARGETS ${PROJECT_NAME}
				 EXPORT ${PROJECT_NAME}_Targets
				 ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
				 LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
				 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} )

install( DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} )

include( CMakePackageConfigHelpers )

set_target_properties( ${PROJECT_NAME} PROPERTIES
											 VERSION ${PROJECT_VERSION}
											 SOVERSION 1 )

set( lib${PROJECT_NAME}_INSTALL_CMAKEDIR "share/Lib${PROJECT_NAME}/cmake"
		 CACHE STRING "Install location for generated CMake files." )

include( CMakePackageConfigHelpers )

configure_package_config_file(
		"${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in"
		"${PROJECT_BINARY_DIR}/Lib${PROJECT_NAME}Config.cmake"
		INSTALL_DESTINATION lib/cmake/Lib${PROJECT_NAME}
		)

write_basic_package_version_file( "${PROJECT_NAME}ConfigVersion.cmake"
																	VERSION ${PROJECT_VERSION}
																	COMPATIBILITY SameMajorVersion )

install( EXPORT ${PROJECT_NAME}_Targets
				 FILE ${PROJECT_NAME}Targets.cmake
				 NAMESPACE daw::
				 DESTINATION lib/cmake/${PROJECT_NAME}
				 )

# Testing
option( DAW_ENABLE_TESTING "Build unit tests" OFF )
if( DAW_ENABLE_TESTING )
	enable_testing()
	add_subdirectory( tests )
endif()
