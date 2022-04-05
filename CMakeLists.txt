cmake_minimum_required(VERSION 3.16)
project(daw-sqlite-helper
        VERSION "0.1.0"
        DESCRIPTION "C++ bindings for sqlite3"
        HOMEPAGE_URL "https://github.com/beached/sqlite_helper"
        LANGUAGES C CXX
        )

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(extern)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(${PROJECT_NAME}
        src/daw/sqlite_helper/sqlite3_class.cpp
        src/daw/sqlite_helper/kv_store.cpp
        )
target_link_libraries(${PROJECT_NAME}
        daw::daw-header-libraries
        daw::daw-utf-range
        sqlite3
        )
add_library(daw::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_20)

#Install
include(GNUInstallDirs)

option(BUILD_SHARED_LIBS "Build shared library" ON)

target_include_directories(${PROJECT_NAME} INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE src)
target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_20)

install(TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}_Targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

include(CMakePackageConfigHelpers)

set_target_properties(${PROJECT_NAME} PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 1)

set(lib${PROJECT_NAME}_INSTALL_CMAKEDIR "share/Lib${PROJECT_NAME}/cmake"
        CACHE STRING "Install location for generated CMake files.")

include(CMakePackageConfigHelpers)

configure_package_config_file(
        "${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in"
        "${PROJECT_BINARY_DIR}/Lib${PROJECT_NAME}Config.cmake"
        INSTALL_DESTINATION lib/cmake/Lib${PROJECT_NAME}
)

write_basic_package_version_file("${PROJECT_NAME}ConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)

install(EXPORT ${PROJECT_NAME}_Targets
        FILE ${PROJECT_NAME}Targets.cmake
        NAMESPACE daw::
        DESTINATION lib/cmake/${PROJECT_NAME}
        )

# Testing
#option(DAW_ENABLE_TESTING "Build unit tests" OFF)
#if (DAW_ENABLE_TESTING)
#    enable_testing()
#    add_subdirectory(tests)
#endif ()

