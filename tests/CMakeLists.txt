cmake_policy(SET CMP0065 NEW)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(Boost_NO_WARN_NEW_VERSIONS ON)

add_custom_target(full)

add_executable(sqlite_helper_test
        src/sqlite_helper_test.cpp
        )
target_link_libraries(sqlite_helper_test PRIVATE ${PROJECT_NAME} daw::daw-sqlite-helper)
add_dependencies(full sqlite_helper_test)
