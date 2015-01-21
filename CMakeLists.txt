# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(lua_circular_buffer C)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Lua circular_buffer Module")
set(CPACK_PACKAGE_VERSION_MAJOR 0)
set(CPACK_PACKAGE_VERSION_MINOR 1)
set(CPACK_PACKAGE_VERSION_PATCH 0)
set(CPACK_PACKAGE_VENDOR        "Mozilla Services")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")
include(CPack)
include(CTest)

if(MSVC)
    # Predefined Macros: http://msdn.microsoft.com/en-us/library/b0084kay.aspx
    # Compiler options: http://msdn.microsoft.com/en-us/library/fwkeyyhe.aspx

    # set a high warning level and treat them as errors
    set(CMAKE_C_FLAGS           "/W3 /WX")

    # debug multi threaded dll runtime, complete debugging info, runtime error checking
    set(CMAKE_C_FLAGS_DEBUG     "/MDd /Zi /RTC1")

    # multi threaded dll runtime, optimize for speed, auto inlining
    set(CMAKE_C_FLAGS_RELEASE   "/MD /O2 /Ob2 /DNDEBUG")

    set(CPACK_GENERATOR         "NSIS")
else()
    # Predefined Macros: clang|gcc -dM -E -x c /dev/null
    # Compiler options: http://gcc.gnu.org/onlinedocs/gcc/Invoking-GCC.html#Invoking-GCC
    set(CMAKE_C_FLAGS   "-std=gnu99 -pedantic -Werror -Wall -Wextra")
    if (NOT WIN32)
        set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -fPIC")
    endif()
    set(CMAKE_C_FLAGS_DEBUG     "-g")

    set(CMAKE_C_FLAGS_RELEASE   "-O2 -DNDEBUG")

    set(CPACK_GENERATOR         "TGZ")

    set(CMAKE_SKIP_BUILD_RPATH              FALSE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH      FALSE)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH   FALSE)
endif()

if (LUA_SANDBOX_INCLUDE)
    add_definitions(-DLUA_SANDBOX)
    include_directories(${EP_BASE}/include)
    include_directories(${LUA_SANDBOX_INCLUDE})
    set(LIB_PATH ${EP_BASE}/lib)
    find_library(LUA_LIBRARIES lua PATHS ${LIB_PATH} NO_DEFAULT_PATH)
    find_library(LUA_SANDBOX_LIBRARIES luasandbox PATHS ${EP_BASE}/../src NO_DEFAULT_PATH)
else()
    find_package(Lua51 REQUIRED)
endif()

set(LUA_CIRCULAR_BUFFER_SRC
lua_circular_buffer.c
cephes.c
)

add_library(circular_buffer SHARED ${LUA_CIRCULAR_BUFFER_SRC})

# remove the prefix to work with the default Lua cpath
set_target_properties(circular_buffer PROPERTIES PREFIX "")
if(LUA_SANDBOX_INCLUDE)
    target_link_libraries(circular_buffer ${LUA_LIBRARIES} ${LUA_SANDBOX_LIBRARIES})
    install(TARGETS circular_buffer DESTINATION modules)
else()
    target_link_libraries(circular_buffer ${LUA_LIBRARIES})
    install(TARGETS circular_buffer DESTINATION lib)
endif()

if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
elseif(MINGW)
    add_definitions(-D_MINGW)
    set_target_properties(circular_buffer PROPERTIES LINK_FLAGS -s)
    if (ADDRESS_MODEL EQUAL 32)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
        set_target_properties(circular_buffer PROPERTIES LINK_FLAGS "-s -m32")
    endif()
endif()

add_test(test lua ${CMAKE_SOURCE_DIR}/test.lua)
