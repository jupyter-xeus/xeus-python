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

if (NOT DEFINED XPYTHON_KERNELSPEC_PATH)
    set(XPYTHON_KERNELSPEC_PATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/")
endif()

configure_file (
    "${CMAKE_CURRENT_SOURCE_DIR}/share/jupyter/kernels/xpython/kernel.json.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/share/jupyter/kernels/xpython/kernel.json"
)

################
# Dependencies #
################

set(xeus_REQUIRED_VERSION 0.23.2)
set(cppzmq_REQUIRED_VERSION 4.3.0)
set(pybind11_REQUIRED_VERSION 2.2.4)
set(pybind11_json_REQUIRED_VERSION 0.2.2)

if(NOT TARGET xeus)
find_package(xeus ${xeus_REQUIRED_VERSION} REQUIRED)
endif()
if(NOT TARGET cppzmq)
find_package(cppzmq ${cppzmq_REQUIRED_VERSION} REQUIRED)
endif()
if(NOT TARGET pybind11)
find_package(pybind11 ${pybind11_REQUIRED_VERSION} REQUIRED)
endif()
if(NOT TARGET pybind11_json)
find_package(pybind11_json ${pybind11_json_REQUIRED_VERSION} REQUIRED)
endif()

#########
# flags #
#########

include(CheckCXXCompilerFlag)

set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)

if(MSVC)
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
    src/xpythonhome.hpp
    src/xpythonhome.cpp
    src/xpaths.hpp
    src/xpaths.cpp
)

# xeus-python is the target for the library
add_library(xeus-python SHARED ${XEUS_PYTHON_SRC} ${XEUS_PYTHON_HEADERS})

# xpython is the target for the kernel executable
add_executable(xpython ${XPYTHON_SRC})
set_target_properties(xpython PROPERTIES ENABLE_EXPORTS 1)
target_link_libraries(xpython PRIVATE pybind11::embed xeus-python)

if(CMAKE_DL_LIBS)
    target_link_libraries(xpython PRIVATE ${CMAKE_DL_LIBS} util)
endif()

if (APPLE)
    set_target_properties(xpython xeus-python PROPERTIES
        MACOSX_RPATH ON
    )
else()
    set_target_properties(xpython xeus-python PROPERTIES
        BUILD_WITH_INSTALL_RPATH 1
        SKIP_BUILD_RPATH FALSE
    )
endif()

set_target_properties(xpython xeus-python PROPERTIES
    INSTALL_RPATH_USE_LINK_PATH TRUE
)

target_include_directories(xeus-python
                           PUBLIC
                           ${PYTHON_INCLUDE_DIRS}
                           $<BUILD_INTERFACE:${XEUS_PYTHON_INCLUDE_DIR}>
                           $<INSTALL_INTERFACE:include>)
target_link_libraries(xeus-python PUBLIC xeus PRIVATE pybind11::pybind11 pybind11_json)

if(WIN32 OR CYGWIN)
  target_link_libraries(xeus-python PRIVATE ${PYTHON_LIBRARIES})
elseif(APPLE)
  target_link_libraries(xeus-python PRIVATE "-undefined dynamic_lookup")
endif()

set_target_properties(xeus-python PROPERTIES
                      PUBLIC_HEADER "${XEUS_PYTHON_HEADERS}"
                      PREFIX ""
                      VERSION ${${PROJECT_NAME}_VERSION}
                      SOVERSION ${XPYT_VERSION_MAJOR}
                      OUTPUT_NAME "libxeus-python")

target_compile_definitions(xeus-python PUBLIC "XEUS_PYTHON_EXPORTS")

if (XEUS_PYTHONHOME_RELPATH)
    target_compile_definitions(xpython PRIVATE XEUS_PYTHONHOME_RELPATH=${XEUS_PYTHONHOME_RELPATH})
endif()

#########
# Tests #
#########

OPTION(BUILD_TESTS "xeus-python test suite" OFF)
OPTION(DOWNLOAD_GTEST "build gtest from downloaded sources" OFF)

if(DOWNLOAD_GTEST OR GTEST_SRC_DIR)
    set(BUILD_TESTS ON)
endif()

if(BUILD_TESTS)
    add_subdirectory(test)
endif()

################
# Installation #
################

include(CMakePackageConfigHelpers)

set(XEUS_PYTHON_CMAKECONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}" CACHE STRING "install path for xeus-pythonConfig.cmake")

# Install xeus-python and xpython
install(TARGETS xeus-python
        EXPORT ${PROJECT_NAME}-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/xeus-python)
install(TARGETS xpython
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Configuration and data directories for jupyter and xeus-python
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

# Makes the project importable from the build directory
export(EXPORT ${PROJECT_NAME}-targets
       FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake")

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
install(EXPORT ${PROJECT_NAME}-targets
        FILE ${PROJECT_NAME}Targets.cmake
        DESTINATION ${XEUS_PYTHON_CMAKECONFIG_INSTALL_DIR})
