cmake_minimum_required(VERSION 3.8)
project(bonefish)

set(CMAKE_CXX_STANDARD 17)

option(shared "build bonefish as a shared library" OFF)
option(stdlib "When building with clang, you can choose libc++ (default) or libstdc++" OFF)
option(examples "build examples" OFF)
option(daemon "build daemon" OFF)
option(test "build tests" OFF)

set(BOOST_COMPONENTS system thread)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if (NOT stdlib)
        set(stdlib "libc++")
    endif()
    if (CMAKE_GENERATOR STREQUAL "Ninja")
        set(CMAKE_CXX_FLAGS "-fcolor-diagnostics ${CMAKE_CXX_FLAGS}")
    endif()
    if (stdlib STREQUAL "libstdc++")
        # libstdc++ defines template<typename> friend class hash, which causes warnings
        set(CMAKE_CXX_FLAGS "-Wno-mismatched-tags ${CMAKE_CXX_FLAGS}")
    endif()
    set(CMAKE_CXX_FLAGS "-stdlib=${stdlib} -Wno-unused-local-typedefs -Wno-unknown-warning-option ${CMAKE_CXX_FLAGS}")
else()
    if(stdlib)
        message(STATUS "stdlib option (set to ${stdlib}) is ignored for ${CMAKE_CXX_COMPILER_ID} compiler")
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.9.0") # i.e. if < 4.9.0
        list(APPEND BOOST_COMPONENTS regex)
    add_definitions(-DUSE_BOOST_REGEX)
    else() # >= 4.9.0
        if (CMAKE_GENERATOR STREQUAL "Ninja")
            set(CMAKE_CXX_FLAGS "-fdiagnostics-color=always ${CMAKE_CXX_FLAGS}")
        else()
            set(CMAKE_CXX_FLAGS "-fdiagnostics-color=auto ${CMAKE_CXX_FLAGS}")
        endif()
    endif()
endif()

if(NOT MSVC)
    set(CMAKE_CXX_FLAGS "-Wall -Werror -Wno-unused-variable -std=c++11 ${CMAKE_CXX_FLAGS}")
else()
    add_definitions(-D_WEBSOCKETPP_CPP11_CHRONO_)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-O0 ${CMAKE_CXX_FLAGS_DEBUG}")
set(CMAKE_C_FLAGS_DEBUG "-O0 ${CMAKE_C_FLAGS_DEBUG}")

SET (CMAKE_CXX_FLAGS "-DMSGPACK_DEFAULT_API_VERSION=1 ${CMAKE_CXX_FLAGS}")

set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")

if(shared)
    set(Boost_USE_STATIC_LIBS OFF)
    set(RT_USE_STATIC_LIBS OFF)
else()
    set(Boost_USE_STATIC_LIBS ON)
    set(RT_USE_STATIC_LIBS ON)
endif()

if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0500)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
endif()

find_package(Boost REQUIRED COMPONENTS ${BOOST_COMPONENTS})
find_package(RT)
find_package(Threads)

include_directories(
    src
    third-party/cppcodec
    third-party/json-msgpack/include
    third-party/msgpack-c/include
    third-party/rapidjson/include
    third-party/websocketpp
    ${Boost_INCLUDE_DIRS}
)

link_directories(${CMAKE_INSTALL_PREFIX}/lib)

if(examples)
    add_subdirectory(examples)
endif()
add_subdirectory(src)
if(daemon)
    add_subdirectory(daemon)
endif()
if(test)
    add_subdirectory(test)
endif()
