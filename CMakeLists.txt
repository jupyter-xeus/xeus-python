############################################################################
# Copyright (c) 2016, Martin Renou, Johan Mabille, Sylvain Corlay and      #
# Wolf Vollprecht                                                          #
#                                                                          #
# Distributed under the terms of the BSD 3-Clause License.                 #
#                                                                          #
# The full license is in the file LICENSE, distributed with this software. #
############################################################################

cmake_minimum_required(VERSION 3.4.3)
project(xeus-python)

set(XPYT_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Versionning
# ===========

file(STRINGS "${XPYT_SRC_DIR}/xpyt_config.hpp" xpyt_version_defines
     REGEX "#define XPYT_VERSION_(MAJOR|MINOR|PATCH)")
foreach(ver ${xpyt_version_defines})
    if(ver MATCHES "#define XPYT_VERSION_(MAJOR|MINOR|PATCH) +([^ ]+)$")
        set(XPYT_VERSION_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" CACHE INTERNAL "")
    endif()
endforeach()
set(${PROJECT_NAME}_VERSION
    ${XPYT_VERSION_MAJOR}.${XPYT_VERSION_MINOR}.${XPYT_VERSION_PATCH})
message(STATUS "Building xeus-python v${${PROJECT_NAME}_VERSION}")

# Configuration
# =============

include(GNUInstallDirs)

configure_file (
    "${CMAKE_CURRENT_SOURCE_DIR}/share/jupyter/kernels/xeus-python/kernel.json.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/share/jupyter/kernels/xeus-python/kernel.json"
)

#######################
# Rely on llvm-config #
#######################

set(CONFIG_OUTPUT)
find_program(LLVM_CONFIG "llvm-config")
if(LLVM_CONFIG)
    message(STATUS "Found LLVM_CONFIG as ${LLVM_CONFIG}")
    set(CONFIG_COMMAND ${LLVM_CONFIG}
        "--assertion-mode"
        "--bindir"
        "--libdir"
        "--includedir"
        "--prefix"
        "--src-root")
    execute_process(COMMAND ${CONFIG_COMMAND}
                    RESULT_VARIABLE HAD_ERROR
                    OUTPUT_VARIABLE CONFIG_OUTPUT)
    if(NOT HAD_ERROR)
        string(REGEX REPLACE
               "[ \t]*[\r\n]+[ \t]*" ";"
               CONFIG_OUTPUT ${CONFIG_OUTPUT})
    else()
        string(REPLACE ";" " " CONFIG_COMMAND_STR "${CONFIG_COMMAND}")
        message(STATUS "${CONFIG_COMMAND_STR}")
        message(FATAL_ERROR "llvm-config failed with status ${HAD_ERROR}")
    endif()
else()
    message(FATAL_ERROR "llvm-config not found -- ${LLVM_CONFIG}")
endif()

list(GET CONFIG_OUTPUT 0 ENABLE_ASSERTIONS)
list(GET CONFIG_OUTPUT 1 TOOLS_BINARY_DIR)
list(GET CONFIG_OUTPUT 2 LIBRARY_DIR)
list(GET CONFIG_OUTPUT 3 INCLUDE_DIR)
list(GET CONFIG_OUTPUT 4 LLVM_OBJ_ROOT)
list(GET CONFIG_OUTPUT 5 MAIN_SRC_DIR)

if(NOT MSVC_IDE)
    set(LLVM_ENABLE_ASSERTIONS ${ENABLE_ASSERTIONS} CACHE BOOL "Enable assertions")
    mark_as_advanced(LLVM_ENABLE_ASSERTIONS)
endif()

set(LLVM_TOOLS_BINARY_DIR ${TOOLS_BINARY_DIR} CACHE PATH "Path to llvm/bin")
set(LLVM_LIBRARY_DIR ${LIBRARY_DIR} CACHE PATH "Path to llvm/lib")
set(LLVM_MAIN_INCLUDE_DIR ${INCLUDE_DIR} CACHE PATH "Path to llvm/include")
set(LLVM_BINARY_DIR ${LLVM_OBJ_ROOT} CACHE PATH "Path to LLVM build tree")
set(LLVM_MAIN_SRC_DIR ${MAIN_SRC_DIR} CACHE PATH "Path to LLVM source tree")

link_directories(${LLVM_LIBRARY_DIR})
add_definitions(-DLLVM_DIR="${LLVM_BINARY_DIR}")

################
# Dependencies #
################

find_package(xeus 0.14.1 REQUIRED)
find_package(cppzmq 4.3.0 REQUIRED)
find_package(pugixml REQUIRED)

find_package(Clang REQUIRED)
find_package(PythonLibs REQUIRED)
find_package(pybind11 REQUIRED)
find_package(cxxopts REQUIRED)

#########
# flags #
#########

include(CheckCXXCompilerFlag)

set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)

if(MSVC)
    add_definitions(/DNOMINMAX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4251 /wd4141")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4018 /wd4267 /wd4715 /wd4146 /wd4129")
endif()

OPTION(DISABLE_ARCH_NATIVE "disable -march=native flag" OFF)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Intel")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wunused-parameter -Wextra -Wreorder")
    if (DISABLE_ARCH_NATIVE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mtune=generic")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
    endif()

    CHECK_CXX_COMPILER_FLAG("-std=c++14" HAS_CPP14_FLAG)

    if (HAS_CPP14_FLAG)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
    else()
        message(FATAL_ERROR "Unsupported compiler -- xeus requires C++14 support!")
    endif()
endif()

###################
# Target and link #
###################

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib; ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

set(XEUSCLING_SRC
    src/main.cpp
    src/xpython_interpreter.cpp
    src/xpython_logger.cpp
    src/xpyt_config.hpp
)

set(LIBS ${PYTHON_LIBRARIES} clingMetaProcessor clingUtils xeus pugixml cxxopts::cxxopts)
set(XEUSPYTHON_TARGET xeus-python)
add_executable(${XEUSPYTHON_TARGET} ${XEUSCLING_SRC})
set_target_properties(${XEUSPYTHON_TARGET} PROPERTIES ENABLE_EXPORTS 1)

if (APPLE)
    set_target_properties(${XEUSPYTHON_TARGET} PROPERTIES
        MACOSX_RPATH ON
    )
else()
    set_target_properties(${XEUSPYTHON_TARGET} PROPERTIES
        BUILD_WITH_INSTALL_RPATH 1
        SKIP_BUILD_RPATH FALSE
    )
endif()

set_target_properties(${XEUSPYTHON_TARGET} PROPERTIES
    INSTALL_RPATH_USE_LINK_PATH TRUE
)

include_directories(${PYTHON_INCLUDE_DIRS})
target_link_libraries(${XEUSPYTHON_TARGET} ${LIBS})

################
# Installation #
################

# Install xeus-cling
install(TARGETS ${XEUSPYTHON_TARGET}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Configuration and data directories for jupyter and xeus-cling
set(XJUPYTER_DATA_DIR "share/jupyter"    CACHE STRING "Jupyter data directory")

# Install Jupyter kernelspecs
set(XPYT_KERNELSPEC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/share/jupyter/kernels)
install(DIRECTORY ${XPYT_KERNELSPEC_DIR}
        DESTINATION ${XJUPYTER_DATA_DIR}
        PATTERN "*.in" EXCLUDE)

# Extra path for installing Jupyter kernelspec
if (XEXTRA_JUPYTER_DATA_DIR)
    install(DIRECTORY ${XPYT_KERNELSPEC_DIR}
            DESTINATION ${XEXTRA_JUPYTER_DATA_DIR}
            PATTERN "*.in" EXCLUDE)
endif(XEXTRA_JUPYTER_DATA_DIR)
