############################################################################
# Copyright (c) 2016, Martin Renou, Johan Mabille, Sylvain Corlay, and     #
# Wolf Vollprecht                                                          #
# Copyright (c) 2016, QuantStack                                           #
#                                                                          #
# Distributed under the terms of the BSD 3-Clause License.                 #
#                                                                          #
# The full license is in the file LICENSE, distributed with this software. #
############################################################################

cmake_minimum_required(VERSION 3.20)
project(xeus-python)

set(XEUS_PYTHON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Versioning
# ==========

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
    set(XPYTHON_KERNELSPEC_PATH "${CMAKE_INSTALL_FULL_BINDIR}/")
endif ()

if(EMSCRIPTEN)
    # the variables PYTHON_VERSION_MAJOR and PYTHON_VERSION_MINOR
    # might be wrong for the EMSCRIPTEN case since it may contains
    # the versions of build-python and not the one of the host.
    FILE(GLOB WASM_PYTHON_LIBRARY  $ENV{PREFIX}/lib/libpython*.a)
    STRING(REGEX MATCH "python[0-9]+[.][0-9]+" PYTHON_VERSION_STRING ${WASM_PYTHON_LIBRARY})
    STRING(REGEX MATCH "[0-9]+" PYTHON_VERSION_MAJOR ${PYTHON_VERSION_STRING})
    STRING(REGEX MATCH "[0-9]+$" PYTHON_VERSION_MINOR ${PYTHON_VERSION_STRING})
endif()

configure_file (
    "${CMAKE_CURRENT_SOURCE_DIR}/share/jupyter/kernels/xpython/kernel.json.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/share/jupyter/kernels/xpython/kernel.json"
)
configure_file (
    "${CMAKE_CURRENT_SOURCE_DIR}/share/jupyter/kernels/xpython-raw/kernel.json.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/share/jupyter/kernels/xpython-raw/kernel.json"
)

# Build options
# =============

# Compilation options

option(XPYT_ENABLE_PYPI_WARNING "Enable warning on PyPI wheels" OFF)

option(XPYT_BUILD_STATIC "Build xeus-python static library" ON)
option(XPYT_BUILD_SHARED "Split xpython build into executable and library" ON)
option(XPYT_BUILD_XPYTHON_EXECUTABLE "Build the xpython executable" ON)
option(XPYT_BUILD_XPYTHON_EXTENSION "Build the xpython extension module" OFF)

option(XPYT_USE_SHARED_XEUS "Link xpython or xpython_extension with the xeus shared library (instead of the static library)" ON)
option(XPYT_USE_SHARED_XEUS_PYTHON "Link xpython and xpython_extension with the xeus-python shared library (instead of the static library)" ON)

option(XPYT_EMSCRIPTEN_WASM_BUILD "Build for wasm with emscripten" OFF)

# Test options
option(XPYT_BUILD_TESTS "xeus-python test suite" OFF)

function(cat IN_FILE OUT_FILE)
  file(READ ${IN_FILE} CONTENTS)
  file(APPEND ${OUT_FILE} "\n${CONTENTS}")
endfunction()


if(EMSCRIPTEN)
    # Prepare a temporary file to "post.js.in" to
    # merge xeus custom post js and the pyjs post js
    file(WRITE post.js.in "")
    cat(wasm_patches/post.js  post.js.in)

    add_compile_definitions(XPYT_EMSCRIPTEN_WASM_BUILD)

    set(XPYT_BUILD_STATIC OFF)
    set(XPYT_BUILD_SHARED OFF)
    set(XPYT_BUILD_XPYTHON_EXECUTABLE OFF)
    set(XPYT_BUILD_XPYTHON_EXTENSION OFF)
    set(XPYT_USE_SHARED_XEUS OFF)
    set(XPYT_USE_SHARED_XEUS_PYTHON OFF)
    set(XPYT_BUILD_TESTS OFF)
endif()

# Dependencies
# ============

set(xeus_zmq_REQUIRED_VERSION 3.0.0)
set(xeus-lite_REQUIRED_VERSION 1.0.1)
set(pybind11_REQUIRED_VERSION 2.6.1)
set(pybind11_json_REQUIRED_VERSION 0.2.8)
set(pyjs_REQUIRED_VERSION 2.0.0)

find_package(Python COMPONENTS Interpreter REQUIRED)

if (NOT TARGET pybind11::headers)
    # Defaults to ON for cmake >= 3.18
    # https://github.com/pybind/pybind11/blob/35ff42b56e9d34d9a944266eb25f2c899dbdfed7/CMakeLists.txt#L96
    set(PYBIND11_FINDPYTHON OFF)
    find_package(pybind11 ${pybind11_REQUIRED_VERSION} REQUIRED)
endif ()
if (NOT TARGET pybind11_json)
    find_package(pybind11_json ${pybind11_json_REQUIRED_VERSION} REQUIRED)
endif ()

if(EMSCRIPTEN)
    if (NOT TARGET xeus-lite)
        find_package(xeus-lite ${xeus-lite_REQUIRED_VERSION} REQUIRED)
    endif ()
    if (NOT TARGET pyjs)
        find_package(pyjs ${pyjs_REQUIRED_VERSION} REQUIRED)
    endif ()
    configure_file(${pyjs_PRE_JS_PATH} ${CMAKE_CURRENT_BINARY_DIR}/pre.js COPYONLY)
    cat(${pyjs_POST_JS_PATH}  post.js.in)
    configure_file( post.js.in  ${CMAKE_CURRENT_BINARY_DIR}/post.js COPYONLY)
else ()
    if (NOT TARGET xeus-zmq AND NOT TARGET xeus-zmq-static)
        find_package(xeus-zmq ${xeus-zmq_REQUIRED_VERSION} REQUIRED)
    endif ()
endif()

# Source files
# ============

set(XEUS_PYTHON_SRC
    src/xcomm.cpp
    src/xcomm.hpp
    src/xdebugger.cpp
    src/xdebugpy_client.hpp
    src/xdebugpy_client.cpp
    src/xdisplay.cpp
    src/xdisplay.hpp
    src/xinput.cpp
    src/xinput.hpp
    src/xinspect.cpp
    src/xinspect.hpp
    src/xinternal_utils.cpp
    src/xinternal_utils.hpp
    src/xinterpreter.cpp
    src/xinterpreter_raw.cpp
    src/xkernel.cpp
    src/xkernel.hpp
    src/xpaths.cpp
    src/xstream.cpp
    src/xstream.hpp
    src/xtraceback.cpp
    src/xutils.cpp
)

set(XEUS_PYTHON_HEADERS
    include/xeus-python/xdebugger.hpp
    include/xeus-python/xeus_python_config.hpp
    include/xeus-python/xpaths.hpp
    include/xeus-python/xinterpreter.hpp
    include/xeus-python/xinterpreter_raw.hpp
    include/xeus-python/xtraceback.hpp
    include/xeus-python/xutils.hpp
)

set(XPYTHON_SRC
    src/main.cpp
)

set(XPYTHON_EXTENSION_SRC
    src/xpython_extension.cpp
)

set(XEUS_PYTHON_WASM_SRC
    src/xcomm.cpp
    src/xcomm.hpp
    src/xdisplay.cpp
    src/xdisplay.hpp
    src/xinput.cpp
    src/xinput.hpp
    src/xinspect.cpp
    src/xinspect.hpp
    src/xinternal_utils.cpp
    src/xinternal_utils.hpp
    src/xinterpreter.cpp
    src/xinterpreter_wasm.cpp
    src/xkernel.cpp
    src/xkernel.hpp
    src/xpaths.cpp
    src/xstream.cpp
    src/xstream.hpp
    src/xtraceback.cpp
    src/xutils.cpp
)

set(XEUS_PYTHON_WASM_HEADERS
    include/xeus-python/xdebugger.hpp
    include/xeus-python/xeus_python_config.hpp
    include/xeus-python/xpaths.hpp
    include/xeus-python/xinterpreter.hpp
    include/xeus-python/xinterpreter_wasm.hpp
    include/xeus-python/xtraceback.hpp
    include/xeus-python/xutils.hpp
)

set(XPYTHON_WASM_SRC
    src/main_wasm.cpp
)

# Targets and link - Macros
# =========================

string(TOUPPER "${CMAKE_BUILD_TYPE}" U_CMAKE_BUILD_TYPE)

macro(xpyt_set_common_options target_name)
    target_compile_features(${target_name} PRIVATE cxx_std_17)
    if (MSVC)
        target_compile_options(${target_name} PUBLIC /wd4251 /wd4141)
        target_compile_options(${target_name} PUBLIC /wd4018 /wd4267 /wd4715 /wd4146 /wd4129)
    endif ()

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
        CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR
        CMAKE_CXX_COMPILER_ID MATCHES "Intel")
        target_compile_options(${target_name} PUBLIC -Wunused-parameter -Wextra -Wreorder)
    endif ()
endmacro()

# Common macro kernels (xpython and xpython_extension)
macro(xpyt_set_kernel_options target_name)
    if(XPYT_ENABLE_PYPI_WARNING)
        message(STATUS "Enabling PyPI warning for target: " ${target_name})
        target_compile_definitions(${target_name} PRIVATE XEUS_PYTHON_PYPI_WARNING)
    endif()
endmacro()

# Common macro for linking targets
macro(xpyt_target_link_libraries target_name)
    if (XPYT_USE_SHARED_XEUS_PYTHON)
        target_link_libraries(${target_name} PRIVATE xeus-python)

        if(CMAKE_DL_LIBS)
            target_link_libraries(${target_name} PRIVATE ${CMAKE_DL_LIBS} util)
        endif()
    else ()
        target_compile_definitions(${target_name} PUBLIC "XEUS_PYTHON_STATIC_LIB")
        target_link_libraries(${target_name} PRIVATE xeus-python-static)
    endif()

    find_package(Threads) # TODO: add Threads as a dependence of xeus or xeus-static?
    target_link_libraries(${target_name} PRIVATE ${CMAKE_THREAD_LIBS_INIT})
    set_target_properties(${target_name} PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
endmacro()

# Common macro for shared and static library xeus-python
macro(xpyt_create_target target_name src headers linkage output_name)
    string(TOUPPER "${linkage}" linkage_upper)

    if (NOT ${linkage_upper} MATCHES "^(SHARED|STATIC)$")
        message(FATAL_ERROR "Invalid library linkage: ${linkage}")
    endif ()

    add_library(${target_name} ${linkage_upper} ${src} ${headers})
    xpyt_set_common_options(${target_name})

    set_target_properties(${target_name} PROPERTIES
                          PUBLIC_HEADER "${headers}"
                          PREFIX ""
                          VERSION ${${PROJECT_NAME}_VERSION}
                          SOVERSION ${XPYT_VERSION_MAJOR}
                          OUTPUT_NAME "lib${output_name}")

    if (${linkage_upper} STREQUAL "STATIC")
        target_compile_definitions(${target_name} PUBLIC "XEUS_PYTHON_STATIC_LIB")
    else ()
        target_compile_definitions(${target_name} PUBLIC "XEUS_PYTHON_EXPORTS")
    endif ()

    target_include_directories(${target_name}
                               PUBLIC
                               ${PYTHON_INCLUDE_DIRS}
                               $<BUILD_INTERFACE:${XEUS_PYTHON_INCLUDE_DIR}>
                               $<INSTALL_INTERFACE:include>)

    if (EMSCRIPTEN)
        set(XPY_XEUS_TARGET xeus)
    elseif (XPYT_USE_SHARED_XEUS)
        set(XPYT_XEUS_TARGET xeus-zmq)
    else ()
        set(XPYT_XEUS_TARGET xeus-zmq-static)
    endif ()

    target_link_libraries(${target_name} PUBLIC ${XPYT_XEUS_TARGET} PRIVATE pybind11::pybind11 pybind11_json)
    if (WIN32 OR CYGWIN)
        target_link_libraries(${target_name} PRIVATE ${PYTHON_LIBRARIES})
    elseif (APPLE)
        target_link_libraries(${target_name} PRIVATE "-undefined dynamic_lookup")
    endif ()

    find_package(Threads) # TODO: add Threads as a dependence of xeus-static?
    target_link_libraries(${target_name} PRIVATE ${CMAKE_THREAD_LIBS_INIT})

    if (XEUS_PYTHONHOME_RELPATH)
        target_compile_definitions(${target_name} PRIVATE XEUS_PYTHONHOME_RELPATH=${XEUS_PYTHONHOME_RELPATH})
    elseif (XEUS_PYTHONHOME_ABSPATH)
        target_compile_definitions(${target_name} PRIVATE XEUS_PYTHONHOME_ABSPATH=${XEUS_PYTHONHOME_ABSPATH})
    endif ()
endmacro ()

# xeus-python
# ===========

set(XEUS_PYTHON_TARGETS "")

if (XPYT_BUILD_SHARED)
    # Build libraries
    xpyt_create_target(xeus-python "${XEUS_PYTHON_SRC}" "${XEUS_PYTHON_HEADERS}" SHARED xeus-python)
    list(APPEND XEUS_PYTHON_TARGETS xeus-python)
endif ()

if (XPYT_BUILD_STATIC)
    # On Windows, a static library should use a different output name
    # to avoid the conflict with the import library of a shared one.
    if (CMAKE_HOST_WIN32)
        xpyt_create_target(xeus-python-static "${XEUS_PYTHON_SRC}" "${XEUS_PYTHON_HEADERS}" STATIC xeus-python-static)
    else ()
        xpyt_create_target(xeus-python-static "${XEUS_PYTHON_SRC}" "${XEUS_PYTHON_HEADERS}" STATIC xeus-python)
    endif ()

    list(APPEND XEUS_PYTHON_TARGETS xeus-python-static)
endif ()

if (EMSCRIPTEN)
    xpyt_create_target(xeus-python-wasm "${XEUS_PYTHON_WASM_SRC}" "${XEUS_PYTHON_WASM_HEADERS}" STATIC xeus-python-wasm)
    target_compile_options(xeus-python-wasm PRIVATE -fPIC)
    list(APPEND XEUS_PYTHON_TARGETS xeus-python-wasm)

    target_link_libraries(xeus-python-wasm PRIVATE pyjs)

endif ()

# xpython
# =======

if (XPYT_BUILD_XPYTHON_EXECUTABLE)
    add_executable(xpython ${XPYTHON_SRC})
    target_link_libraries(xpython PRIVATE pybind11::embed)

    xpyt_set_common_options(xpython)
    xpyt_set_kernel_options(xpython)
    xpyt_target_link_libraries(xpython)
endif()

# xpython_extension
# =================

if (XPYT_BUILD_XPYTHON_EXTENSION)
    pybind11_add_module(xpython_extension ${XPYTHON_EXTENSION_SRC})

    xpyt_set_common_options(xpython_extension)
    xpyt_set_kernel_options(xpython_extension)
    xpyt_target_link_libraries(xpython_extension)
endif()

# xpython
# ============

if (EMSCRIPTEN)
    # TODO MAKE BETTER
    SET(PYTHON_UTIL_LIBS
        ${CMAKE_INSTALL_PREFIX}/lib/libbz2.a
        ${CMAKE_INSTALL_PREFIX}/lib/libz.a
        ${CMAKE_INSTALL_PREFIX}/lib/libsqlite3.a
        ${CMAKE_INSTALL_PREFIX}/lib/libffi.a
        ${CMAKE_INSTALL_PREFIX}/lib/libzstd.a
        ${CMAKE_INSTALL_PREFIX}/lib/libssl.a
        ${CMAKE_INSTALL_PREFIX}/lib/libcrypto.a
        ${CMAKE_INSTALL_PREFIX}/lib/liblzma.a
    )

    add_executable(xpython ${XPYTHON_WASM_SRC})
    target_link_libraries(xpython PRIVATE ${PYTHON_UTIL_LIBS} pybind11::embed)

    xpyt_set_common_options(xpython)
    target_compile_options(xpython PRIVATE -fPIC)

    xpyt_set_kernel_options(xpython)
    target_link_libraries(xpython PRIVATE xeus-python-wasm xeus-lite)

    include(WasmBuildOptions)
    xeus_wasm_compile_options(xpython)
    xeus_wasm_link_options(xpython "web,worker")
    target_link_options(xpython
        PUBLIC "SHELL: -s LZ4=1"
        PUBLIC "SHELL: --post-js  ${CMAKE_CURRENT_BINARY_DIR}/post.js"
        PUBLIC "SHELL: --pre-js pre.js"
        PUBLIC "SHELL: -s MAIN_MODULE=1"
        PUBLIC "SHELL: -s WASM_BIGINT"
        PUBLIC "-s DEFAULT_LIBRARY_FUNCS_TO_INCLUDE=\"['\$Browser', '\$ERRNO_CODES']\" "
    )
endif()

# Tests
# =====

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
endif()

if(XPYT_BUILD_XPYTHON_EXECUTABLE OR EMSCRIPTEN)
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

if (EMSCRIPTEN)

    install(FILES
            "$<TARGET_FILE_DIR:xpython>/xpython.js"
            "$<TARGET_FILE_DIR:xpython>/xpython.wasm"
            DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

endif ()
