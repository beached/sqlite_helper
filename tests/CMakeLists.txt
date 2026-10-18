cmake_policy( SET CMP0065 NEW )

set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS OFF )

set( Boost_NO_WARN_NEW_VERSIONS ON )

include( CheckCXXCompilerFlag )

check_cxx_compiler_flag(
		"-Wc++23-extensions"
		HAS_WNO_CXX23_EXTENSIONS
		)

if( HAS_WNO_CXX23_EXTENSIONS )
	add_compile_options(
			"-Wno-c++23-extensions"
			)
endif()
check_cxx_compiler_flag(
		"-Wc++2c-extensions"
		HAS_WNO_CXX26_EXTENSIONS
		)

if( HAS_WNO_CXX26_EXTENSIONS )
	add_compile_options(
			"-Wno-c++2c-extensions"
			)
endif()
add_custom_target( full )


add_executable( sqlite_helper_test
								src/sqlite_helper_test.cpp
								)
target_link_libraries( sqlite_helper_test PRIVATE ${PROJECT_NAME} daw::daw-sqlite-helper )
add_dependencies( full sqlite_helper_test )
add_test( NAME sqlite_helper_test COMMAND sqlite_helper_test )
