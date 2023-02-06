/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XPYT_CONFIG_HPP
#define XPYT_CONFIG_HPP

// Project version
#define XPYT_VERSION_MAJOR 0
#define XPYT_VERSION_MINOR 15
#define XPYT_VERSION_PATCH 3

// Composing the version string from major, minor and patch
#define XPYT_CONCATENATE(A, B) XPYT_CONCATENATE_IMPL(A, B)
#define XPYT_CONCATENATE_IMPL(A, B) A##B
#define XPYT_STRINGIFY(a) XPYT_STRINGIFY_IMPL(a)
#define XPYT_STRINGIFY_IMPL(a) #a

#define XPYT_VERSION XPYT_STRINGIFY(XPYT_CONCATENATE(XPYT_VERSION_MAJOR,   \
                 XPYT_CONCATENATE(.,XPYT_CONCATENATE(XPYT_VERSION_MINOR,   \
                                  XPYT_CONCATENATE(.,XPYT_VERSION_PATCH)))))

#ifdef _WIN32
    #ifdef XEUS_PYTHON_STATIC_LIB
        #define XEUS_PYTHON_API
    #else
        #ifdef XEUS_PYTHON_EXPORTS
            #define XEUS_PYTHON_API __declspec(dllexport)
        #else
            #define XEUS_PYTHON_API __declspec(dllimport)
        #endif
    #endif
#else
    #define XEUS_PYTHON_API
#endif

#ifdef _MSC_VER
    #define XPYT_FORCE_PYBIND11_EXPORT
#else
    #define XPYT_FORCE_PYBIND11_EXPORT __attribute__ ((visibility ("default")))
#endif
#endif
