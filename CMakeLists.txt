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

set(XEUSPYTHON_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(XEUSPYTHON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Versionning
# ===========

file(STRINGS "${XEUSPYTHON_INCLUDE_DIR}/xeus-python/xeus_python_config.hpp" xpyt_version_defines
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
    "${CMAKE_CURRENT_SOURCE_DIR}/share/jupyter/kernels/xpython/kernel.json.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/share/jupyter/kernels/xpython/kernel.json"
)

################
# Dependencies #
################

set(xeus_REQUIRED_VERSION 0.18.1)
set(cppzmq_REQUIRED_VERSION 4.3.0)
set(pybind11_REQUIRED_VERSION 2.2.4)

find_package(xeus ${xeus_REQUIRED_VERSION} REQUIRED)
find_package(cppzmq ${cppzmq_REQUIRED_VERSION} REQUIRED)
find_package(pybind11 ${pybind11_REQUIRED_VERSION} REQUIRED)

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

set(XEUSPYTHON_SRC
    src/xcomm.cpp
    src/xcomm.hpp
    src/xinput.cpp
    src/xinput.hpp
    src/xinspect.cpp
    src/xinspect.hpp
    src/xinterpreter.cpp
    src/xis_complete.cpp
    src/xis_complete.hpp
    src/xstream.cpp
    src/xstream.hpp
    src/xdisplay.cpp
    src/xdisplay.hpp
    src/xtraceback.cpp
    src/xtraceback.hpp
    src/xutils.cpp
    src/xutils.hpp
)

set(XEUSPYTHON_HEADERS
    include/xeus-python/xinterpreter.hpp
    include/xeus-python/xeus_python_config.hpp
    include/xeus-python/xpythonhome.hpp
)

# xeus-python is the target for the library
add_library(xeus-python SHARED ${XEUSPYTHON_SRC} ${XEUSPYTHON_HEADERS})

# xpython is the target for the kernel executable
add_executable(xpython src/main.cpp)
set_target_properties(xpython PROPERTIES ENABLE_EXPORTS 1)
target_link_libraries(xpython PRIVATE pybind11::pybind11 xeus-python)
target_compile_definitions(xpython PRIVATE XEUS_PYTHON_HOME="${PYTHON_PREFIX}")

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
                           $<BUILD_INTERFACE:${XEUSPYTHON_INCLUDE_DIR}>
                           $<INSTALL_INTERFACE:include>)
target_link_libraries(xeus-python PUBLIC pybind11::embed xeus)

set_target_properties(xeus-python PROPERTIES
                      PUBLIC_HEADER "${XEUSPYTHON_HEADERS}"
                      COMPILE_DEFINITIONS "XEUS_PYTHON_EXPORTS"
                      PREFIX ""
                      VERSION ${${PROJECT_NAME}_VERSION}
                      SOVERSION ${XPYT_VERSION_MAJOR}
                      OUTPUT_NAME "libxeus-python")

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

set(XEUSPYTHON_CMAKECONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}" CACHE STRING "install path for xeus-pythonConfig.cmake")

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
set(XEUSPYTHON_CONFIG_CODE "####### Expanded from \@XEUSPYTHON_CONFIG_CODE\@ #######\n")
set(XEUSPYTHON_CONFIG_CODE "${XEUSPYTHON_CONFIG_CODE}set(CMAKE_MODULE_PATH \"${CMAKE_CURRENT_SOURCE_DIR}/cmake;\${CMAKE_MODULE_PATH}\")\n")
set(XEUSPYTHON_CONFIG_CODE "${XEUSPYTHON_CONFIG_CODE}##################################################")
configure_package_config_file(${PROJECT_NAME}Config.cmake.in
                              "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
                              INSTALL_DESTINATION ${PROJECT_BINARY_DIR})

# Configure 'xeus-pythonConfig.cmake' for an install tree
set(XEUSPYTHON_CONFIG_CODE "")
configure_package_config_file(${PROJECT_NAME}Config.cmake.in
                              "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${PROJECT_NAME}Config.cmake"
                              INSTALL_DESTINATION ${XEUSPYTHON_CMAKECONFIG_INSTALL_DIR})


write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
                                 VERSION ${${PROJECT_NAME}_VERSION}
                                 COMPATIBILITY AnyNewerVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${PROJECT_NAME}Config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
              DESTINATION ${XEUSPYTHON_CMAKECONFIG_INSTALL_DIR})
install(EXPORT ${PROJECT_NAME}-targets
        FILE ${PROJECT_NAME}Targets.cmake
        DESTINATION ${XEUSPYTHON_CMAKECONFIG_INSTALL_DIR})
