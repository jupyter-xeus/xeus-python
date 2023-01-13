############################################################################
# Copyright (c) 2016, Martin Renou, Johan Mabille, Sylvain Corlay, and     #
# Wolf Vollprecht                                                          #
# Copyright (c) 2016, QuantStack                                           #
#                                                                          #
# Distributed under the terms of the BSD 3-Clause License.                 #
#                                                                          #
# The full license is in the file LICENSE, distributed with this software. #
############################################################################

# Unit tests
# ==========

cmake_minimum_required(VERSION 3.1)

if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    project(xeus-python-test)

    enable_testing()

    find_package(xeus-python REQUIRED CONFIG)
endif ()

message(STATUS "Forcing tests build type to Release")
set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)

include(CheckCXXCompilerFlag)

string(TOUPPER "${CMAKE_BUILD_TYPE}" U_CMAKE_BUILD_TYPE)

if(CMAKE_CXX_COMPILER_ID MATCHES Clang OR CMAKE_CXX_COMPILER_ID MATCHES GNU OR CMAKE_CXX_COMPILER_ID MATCHES Intel)
    add_compile_options(-Wunused-parameter -Wextra -Wreorder -Wconversion -Wsign-conversion)

    CHECK_CXX_COMPILER_FLAG(-march=native HAS_MARCH_NATIVE)
    if (HAS_MARCH_NATIVE)
        add_compile_options(-march=native)
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES MSVC)
    add_compile_options(/EHsc /MP /bigobj)
    set(CMAKE_EXE_LINKER_FLAGS /MANIFEST:NO)
endif()

find_package(Threads)
find_package(doctest)

include_directories(${GTEST_INCLUDE_DIRS} SYSTEM)

set(XEUS_PYTHON_TESTS
    main.cpp
    ../src/xutils.cpp
    test_debugger.cpp
    xeus_client.hpp
    xeus_client.cpp
)

add_executable(test_xeus_python ${XEUS_PYTHON_TESTS})

if (APPLE)
    set_target_properties(test_xeus_python PROPERTIES
        MACOSX_RPATH ON
    )
endif()

set_target_properties(test_xeus_python PROPERTIES
    INSTALL_RPATH_USE_LINK_PATH TRUE
)

include_directories(${PYTHON_INCLUDE_DIRS})
target_link_libraries(test_xeus_python ${PYTHON_LIBRARIES} xeus-zmq doctest::doctest ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(test_xeus_python PRIVATE ${XEUS_PYTHON_INCLUDE_DIR})

add_custom_target(xtest COMMAND test_xeus_python DEPENDS test_xeus_python)

