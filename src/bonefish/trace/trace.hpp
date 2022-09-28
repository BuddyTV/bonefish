/**
 *  Copyright (C) 2015 Topology LP
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef BONEFISH_WAMP_TRACE_HPP
#define BONEFISH_WAMP_TRACE_HPP

#include <boost/format.hpp>
#include <boost/preprocessor/comparison/greater.hpp>
#include <boost/preprocessor/control/iif.hpp>
#include <boost/preprocessor/variadic/size.hpp>
#include <iostream>
#include <functional>
#include <string.h>

namespace bonefish {
namespace trace {

extern bool _enabled;
extern std::function<void(const std::string&)> trace;
extern std::function<void(const std::string&)> error;

inline bool is_enabled()
{
    return _enabled;
}

inline void set_enabled(bool is_enabled)
{
    _enabled = is_enabled;
}

template<typename Callback>
inline void set_trace_callback(Callback callback)
{
    trace = callback;
}

template<typename Callback>
inline void set_error_callback(Callback callback)
{
    error = callback;
}

inline const char* base_file_name(const char* file_path)
{
    const char* file_name = strrchr(file_path, '/');
    return file_name == nullptr ? file_path : file_name + 1;
}

} // namespace trace
} // namespace bonefish

// Macro for facilitating debug trace logging.
#define BONEFISH_TRACE(...) \
	BOOST_PP_IIF(BOOST_PP_GREATER(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), 1), BONEFISH_TRACE_ARGS(__VA_ARGS__), BONEFISH_TRACE_NOARGS(BOOST_PP_VARIADIC_ELEM(0, __VA_ARGS__)))

#define BONEFISH_TRACE_NOARGS(fmt) \
    if (bonefish::trace::is_enabled()) { \
        std::ostringstream oss; \
        oss << "[" << bonefish::trace::base_file_name(__FILE__) << ":" << __LINE__ << "][" << __FUNCTION__ << "] " \
                << boost::format(fmt); \
        bonefish::trace::trace(oss.str()); \
    }

#define BONEFISH_TRACE_ARGS(fmt, ...) \
    if (bonefish::trace::is_enabled()) { \
        std::ostringstream oss; \
        oss << "[" << bonefish::trace::base_file_name(__FILE__) << ":" << __LINE__ << "][" << __FUNCTION__ << "] " \
                << (boost::format(fmt) % __VA_ARGS__); \
        bonefish::trace::trace(oss.str()); \
    }

// Macro for facilitating error logging.
#define BONEFISH_ERROR(...) \
    BOOST_PP_IIF(BOOST_PP_GREATER(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), 1), BONEFISH_ERROR_ARGS(__VA_ARGS__), BONEFISH_ERROR_NOARGS(BOOST_PP_VARIADIC_ELEM(0, __VA_ARGS__)))

#define BONEFISH_ERROR_NOARGS(fmt) \
    if (bonefish::trace::is_enabled()) { \
        std::ostringstream oss; \
        oss << "[" << bonefish::trace::base_file_name(__FILE__) << ":" << __LINE__ << "][" << __FUNCTION__ << "] " \
                << boost::format(fmt); \
        bonefish::trace::error(oss.str()); \
    }

#define BONEFISH_ERROR_ARGS(fmt, ...) \
    if (bonefish::trace::is_enabled()) { \
        std::ostringstream oss; \
        oss << "[" << bonefish::trace::base_file_name(__FILE__) << ":" << __LINE__ << "][" << __FUNCTION__ << "] " \
                << (boost::format(fmt) % __VA_ARGS__); \
        bonefish::trace::error(oss.str()); \
    }

#endif // BONEFISH_WAMP_TRACE_HPP
