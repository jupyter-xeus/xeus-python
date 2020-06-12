############################################################################
# Copyright (c) 2016, Martin Renou, Johan Mabille, Sylvain Corlay, and     #
# Wolf Vollprecht                                                          #
# Copyright (c) 2016, QuantStack                                           #
#                                                                          #
# Distributed under the terms of the BSD 3-Clause License.                 #
#                                                                          #
# The full license is in the file LICENSE, distributed with this software. #
############################################################################

cmake_minimum_required(VERSION 3.4.3)
project(xeus-python)

set(XEUS_PYTHON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Versionning
# ===========

file(STRINGS "${XEUS_PYTHON_INCLUDE_DIR}/xeus-python/xeus_python_config.hpp" xpyt_version_defines
     REGEX "#define XPYT_VERSION_(MAJOR|MINOR|PATCH)")
foreach (ver ${xpyt_version_defines})
    if (ver MATCHES "#define XPYT_VERSION_(MAJOR|MINOR|PATCH) +([^ ]+)$")
        set(XPYT_VERSION_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" CACHE INTERNAL "")
    endif ()
endforeach ()
set(${PROJECT_NAME}_VERSION
    ${XPYT_VERSION_MAJOR}.${XPYT_VERSION_MINOR}.${XPYT_VERSION_PATCH})
message(STATUS "Building xeus-python v${${PROJECT_NAME}_VERSION}")

# Configuration
# =============

include(GNUInstallDirs)

if (NOT DEFINED XPYTHON_KERNELSPEC_PATH)
    set(XPYTHON_KERNELSPEC_PATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/")
endif ()

configure_file (
    "${CMAKE_CURRENT_SOURCE_DIR}/share/jupyter/kernels/xpython/kernel.json.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/share/jupyter/kernels/xpython/kernel.json"
)

# Build options
# =============

# Compilation options
OPTION(XPYT_DISABLE_ARCH_NATIVE "disable -march=native flag" OFF)
OPTION(XPYT_DISABLE_TUNE_GENERIC "disable -mtune=generic flag" OFF)
OPTION(XPYT_ENABLE_PYPI_WARNING "Enable warning on PyPI wheels" OFF)

option(XPYT_BUILD_STATIC "Build xeus-python static library" ON)
OPTION(XPYT_BUILD_SHARED "Split xpython build into executable and library" ON)
OPTION(XPYT_BUILD_XPYTHON_EXECUTABLE "Build the xpython executable" ON)
OPTION(XPYT_BUILD_XPYTHON_EXTENSION "Build the xpython extension module" OFF)

OPTION(XPYT_USE_SHARED_XEUS "Link xpython or xpython_extension with the xeus shared library (instead of the static library)" ON)
OPTION(XPYT_USE_SHARED_XEUS_PYTHON "Link xpython and xpython_extension with the xeus-python shared library (instead of the static library)" ON)

# Test options
OPTION(XPYT_BUILD_TESTS "xeus-python test suite" OFF)
OPTION(XPYT_DOWNLOAD_GTEST "build gtest from downloaded sources" OFF)

# Dependencies
# ============

set(xeus_REQUIRED_VERSION 0.23.12)
set(pybind11_REQUIRED_VERSION 2.2.4)
set(pybind11_json_REQUIRED_VERSION 0.2.2)

if (NOT TARGET xeus AND NOT TARGET xeus-static)
    find_package(xeus ${xeus_REQUIRED_VERSION} REQUIRED)
endif ()
if (NOT TARGET pybind11)
    find_package(pybind11 ${pybind11_REQUIRED_VERSION} REQUIRED)
endif ()
if (NOT TARGET pybind11_json)
    find_package(pybind11_json ${pybind11_json_REQUIRED_VERSION} REQUIRED)
endif ()

# Flags
# =====

include(CheckCXXCompilerFlag)

set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)

if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4251 /wd4141")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4018 /wd4267 /wd4715 /wd4146 /wd4129")
endif ()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Intel")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wunused-parameter -Wextra -Wreorder")
    if (XPYT_DISABLE_ARCH_NATIVE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mtune=generic")
    elseif (XPYT_DISABLE_TUNE_GENERIC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else ()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
    endif ()

    CHECK_CXX_COMPILER_FLAG("-std=c++14" HAS_CPP14_FLAG)

    if (HAS_CPP14_FLAG)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
    else ()
        message(FATAL_ERROR "Unsupported compiler -- xeus requires C++14 support!")
    endif ()
endif ()

# Source files
# ============

set(XEUS_PYTHON_SRC
    src/xcomm.cpp
    src/xcomm.hpp
    src/xdebugger.cpp
    src/xdisplay.cpp
    src/xdisplay.hpp
    src/xinput.cpp
    src/xinput.hpp
    src/xinspect.cpp
    src/xinspect.hpp
    src/xinteractiveshell.cpp
    src/xinteractiveshell.hpp
    src/xinterpreter.cpp
    src/xis_complete.cpp
    src/xis_complete.hpp
    src/xlinecache.cpp
    src/xlinecache.hpp
    src/xptvsd_client.cpp
    src/xptvsd_client.hpp
    src/xstream.cpp
    src/xstream.hpp
    src/xtraceback.cpp
    src/xtraceback.hpp
    src/xutils.cpp
    src/xutils.hpp
)

set(XEUS_PYTHON_HEADERS
    include/xeus-python/xdebugger.hpp
    include/xeus-python/xeus_python_config.hpp
    include/xeus-python/xinterpreter.hpp
)

set(XPYTHON_SRC
    src/main.cpp
    src/xpaths.hpp
    src/xpaths.cpp
    src/xpythonhome.hpp
    src/xpythonhome.cpp
)

set(XPYTHON_EXTENSION_SRC
    src/xpython_extension.cpp
)

# Targets and link - Macros
# =========================

include(CheckCXXCompilerFlag)

string(TOUPPER "${CMAKE_BUILD_TYPE}" U_CMAKE_BUILD_TYPE)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib; ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

macro(xpyt_set_common_options target_name)
    if (MSVC)
        target_compile_options(${target_name} PUBLIC /wd4251 /wd4141)
        target_compile_options(${target_name} PUBLIC /wd4018 /wd4267 /wd4715 /wd4146 /wd4129)
    endif ()

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
        CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR
        CMAKE_CXX_COMPILER_ID MATCHES "Intel")

        target_compile_options(${target_name} PUBLIC -Wunused-parameter -Wextra -Wreorder)
    
        # Mtune generic/native
        if (XPYT_DISABLE_ARCH_NATIVE AND NOT XPYT_DISABLE_TUNE_GENERIC)
            target_compile_options(${target_name} PUBLIC -mtune=generic)
        elseif (XPYT_DISABLE_TUNE_GENERIC)
        else ()
            target_compile_options(${target_name} PUBLIC -march=native)
        endif ()

        # C++14 flag
        CHECK_CXX_COMPILER_FLAG("-std=c++14" HAS_CPP14_FLAG)
        if (HAS_CPP14_FLAG)
            target_compile_features(${target_name} PRIVATE cxx_std_14)
        else ()
            message(FATAL_ERROR "Unsupported compiler -- xeus-python requires C++14 support!")
        endif ()

        # Enable link time optimization and set the default symbol
        # visibility to hidden (very important to obtain small binaries)
        if (NOT ${U_CMAKE_BUILD_TYPE} MATCHES DEBUG)
            # Check for Link Time Optimization support
            # (GCC/Clang)
            CHECK_CXX_COMPILER_FLAG("-flto" HAS_LTO_FLAG)
            if (HAS_LTO_FLAG)
                target_compile_options(${target_name} PUBLIC -flto)
            endif ()

            # Avoids removing symbols from the static library
            CHECK_CXX_COMPILER_FLAG("-ffat-lto-objects" HAS_FATLTO_FLAG)
            if (${linkage_upper} MATCHES "STATIC" AND  HAS_FATLTO_FLAG)
                message(STATUS "ENABLE FAT LTO OBJECTS")
                target_compile_options(${target_name} PUBLIC -ffat-lto-objects)
            endif ()

            # Intel equivalent to LTO is called IPO
            if (CMAKE_CXX_COMPILER_ID MATCHES "Intel")
                CHECK_CXX_COMPILER_FLAG("-ipo" HAS_IPO_FLAG)
                if (HAS_IPO_FLAG)
                    target_compile_options(${target_name} PUBLIC -ipo)
                endif ()
            endif ()
        endif ()

    endif ()

    if (APPLE)
        set_target_properties(${target_name} PROPERTIES
            MACOSX_RPATH ON
        )
    else ()
        set_target_properties(${target_name} PROPERTIES
            BUILD_WITH_INSTALL_RPATH 1
            SKIP_BUILD_RPATH FALSE
        )
    endif ()

    set_target_properties(${target_name} PROPERTIES
        INSTALL_RPATH_USE_LINK_PATH TRUE
    )
endmacro()

# Common macro kernels (xpython and xpython_extension)
macro(xpyt_set_kernel_options target_name)
    if(XPYT_ENABLE_PYPI_WARNING)
        message(STATUS "Enabling PyPI warning for target: " ${target_name})
        target_compile_definitions(${target_name} PRIVATE XEUS_PYTHON_PYPI_WARNING)
    endif()

    if (XPYT_USE_SHARED_XEUS_PYTHON)
        target_link_libraries(${target_name} PRIVATE xeus-python)

        if(CMAKE_DL_LIBS)
            target_link_libraries(${target_name} PRIVATE ${CMAKE_DL_LIBS} util)
        endif()
    else ()
        target_link_libraries(${target_name} PRIVATE xeus-python-static)
    endif()

    find_package(Threads) # TODO: add Threads as a dependence of xeus or xeus-static?
    target_link_libraries(${target_name} PRIVATE ${CMAKE_THREAD_LIBS_INIT})
endmacro()

# Common macro for shared and static library xeus-python
macro(xpyt_create_target target_name linkage output_name)
    string(TOUPPER "${linkage}" linkage_upper)

    if (NOT ${linkage_upper} MATCHES "^(SHARED|STATIC)$")
        message(FATAL_ERROR "Invalid library linkage: ${linkage}")
    endif ()

    add_library(${target_name} ${linkage_upper} ${XEUS_PYTHON_SRC} ${XEUS_PYTHON_HEADERS})
    xpyt_set_common_options(${target_name})

    set_target_properties(${target_name} PROPERTIES
                          PUBLIC_HEADER "${XEUS_PYTHON_HEADERS}"
                          PREFIX ""
                          VERSION ${${PROJECT_NAME}_VERSION}
                          SOVERSION ${XPYT_VERSION_MAJOR}
                          OUTPUT_NAME "lib${output_name}")

    target_compile_definitions(${target_name} PUBLIC "XEUS_PYTHON_EXPORTS")

    target_include_directories(${target_name}
                               PUBLIC
                               ${PYTHON_INCLUDE_DIRS}
                               $<BUILD_INTERFACE:${XEUS_PYTHON_INCLUDE_DIR}>
                               $<INSTALL_INTERFACE:include>)

    if (XPYT_USE_SHARED_XEUS)
        set(XPYT_XEUS_TARGET xeus)
    else ()
        set(XPYT_XEUS_TARGET xeus-static)
    endif ()

    target_link_libraries(${target_name} PUBLIC ${XPYT_XEUS_TARGET} PRIVATE pybind11::pybind11 pybind11_json)
    if (WIN32 OR CYGWIN)
        target_link_libraries(${target_name} PRIVATE ${PYTHON_LIBRARIES})
    elseif (APPLE)
        target_link_libraries(${target_name} PRIVATE "-undefined dynamic_lookup")
    endif ()

    find_package(Threads) # TODO: add Threads as a dependence of xeus-static?
    target_link_libraries(${target_name} PRIVATE ${CMAKE_THREAD_LIBS_INIT})
endmacro()

# xeus-python
# ===========

set(XEUS_PYTHON_TARGETS "")

if (XPYT_BUILD_SHARED)
    # Build libraries
    xpyt_create_target(xeus-python SHARED xeus-python)
    list(APPEND XEUS_PYTHON_TARGETS xeus-python)
endif ()

if (XPYT_BUILD_STATIC)
    # On Windows, a static library should use a different output name
    # to avoid the conflict with the import library of a shared one.
    if (CMAKE_HOST_WIN32)
        xpyt_create_target(xeus-python-static STATIC xeus-python-static)
    else ()
        xpyt_create_target(xeus-python-static STATIC xeus-python)
    endif ()
    list(APPEND XEUS_PYTHON_TARGETS xeus-python-static)
endif ()

# xpython
# =======

if (XPYT_BUILD_XPYTHON_EXECUTABLE)
    add_executable(xpython ${XPYTHON_SRC})
    target_link_libraries(xpython PRIVATE pybind11::embed)

    xpyt_set_common_options(xpython)
    xpyt_set_kernel_options(xpython)

    if (XEUS_PYTHONHOME_RELPATH)
        target_compile_definitions(xpython PRIVATE XEUS_PYTHONHOME_RELPATH=${XEUS_PYTHONHOME_RELPATH})
    endif()
endif()

# xpython_extension
# =================

if (XPYT_BUILD_XPYTHON_EXTENSION)
    pybind11_add_module(xpython_extension ${XPYTHON_EXTENSION_SRC})

    xpyt_set_common_options(xpython_extension)
    xpyt_set_kernel_options(xpython_extension)
endif()

# Tests
# =====

if(XPYT_DOWNLOAD_GTEST OR XPYT_GTEST_SRC_DIR)
    set(XPYT_BUILD_TESTS ON)
endif()

if(XPYT_BUILD_TESTS)
    add_subdirectory(test)
endif()

# Installation
# ============

include(CMakePackageConfigHelpers)

set(XEUS_PYTHON_CMAKECONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}" CACHE STRING "install path for xeus-pythonConfig.cmake")

# Install xeus-python and xeus-python-static
if (XPYT_BUILD_SHARED)
    install(TARGETS ${XEUS_PYTHON_TARGETS}
            EXPORT ${PROJECT_NAME}-targets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/xeus-python)

    # Makes the project importable from the build directory
    export(EXPORT ${PROJECT_NAME}-targets
           FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake")
endif ()

# Install xpython
if (XPYT_BUILD_XPYTHON_EXECUTABLE)
    install(TARGETS xpython
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    # Configuration and data directories for jupyter and xeus-python
    set(XJUPYTER_DATA_DIR "share/jupyter"    CACHE STRING "Jupyter data directory")

    # Install xpython Jupyter kernelspec
    set(XPYT_KERNELSPEC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/share/jupyter/kernels)
    install(DIRECTORY ${XPYT_KERNELSPEC_DIR}
            DESTINATION ${XJUPYTER_DATA_DIR}
            PATTERN "*.in" EXCLUDE)

    # Extra path for installing Jupyter kernelspec
    if (XEXTRA_JUPYTER_DATA_DIR)
        install(DIRECTORY ${XPYT_KERNELSPEC_DIR}
                DESTINATION ${XEXTRA_JUPYTER_DATA_DIR}
                PATTERN "*.in" EXCLUDE)
    endif ()
endif ()

# Configure 'xeus-pythonConfig.cmake' for a build tree
set(XEUS_PYTHON_CONFIG_CODE "####### Expanded from \@XEUS_PYTHON_CONFIG_CODE\@ #######\n")
set(XEUS_PYTHON_CONFIG_CODE "${XEUS_PYTHON_CONFIG_CODE}set(CMAKE_MODULE_PATH \"${CMAKE_CURRENT_SOURCE_DIR}/cmake;\${CMAKE_MODULE_PATH}\")\n")
set(XEUS_PYTHON_CONFIG_CODE "${XEUS_PYTHON_CONFIG_CODE}##################################################")
configure_package_config_file(${PROJECT_NAME}Config.cmake.in
                              "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
                              INSTALL_DESTINATION ${PROJECT_BINARY_DIR})

# Configure 'xeus-pythonConfig.cmake' for an install tree
set(XEUS_PYTHON_CONFIG_CODE "")
configure_package_config_file(${PROJECT_NAME}Config.cmake.in
                              "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${PROJECT_NAME}Config.cmake"
                              INSTALL_DESTINATION ${XEUS_PYTHON_CMAKECONFIG_INSTALL_DIR})

write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
                                 VERSION ${${PROJECT_NAME}_VERSION}
                                 COMPATIBILITY AnyNewerVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${PROJECT_NAME}Config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
              DESTINATION ${XEUS_PYTHON_CMAKECONFIG_INSTALL_DIR})

if (XPYT_BUILD_SHARED)
    install(EXPORT ${PROJECT_NAME}-targets
            FILE ${PROJECT_NAME}Targets.cmake
            DESTINATION ${XEUS_PYTHON_CMAKECONFIG_INSTALL_DIR})
endif ()
