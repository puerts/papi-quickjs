cmake_minimum_required(VERSION 3.9)

project(PapiQjs LANGUAGES C CXX)

include(CheckCCompilerFlag)
include(GNUInstallDirs)

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected, default to Release")
    set(CMAKE_BUILD_TYPE "Release")
endif()

message(STATUS "Building in ${CMAKE_BUILD_TYPE} mode")
message(STATUS "Building with ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION} on ${CMAKE_SYSTEM}")

macro(xcheck_add_c_compiler_flag FLAG)
    string(REPLACE "-" "" FLAG_NO_HYPHEN ${FLAG})
    check_c_compiler_flag(${FLAG} COMPILER_SUPPORTS_${FLAG_NO_HYPHEN})
    if(COMPILER_SUPPORTS_${FLAG_NO_HYPHEN})
        add_compile_options(${FLAG})
    endif()
endmacro()

#xcheck_add_c_compiler_flag(-Wall)
#if(NOT MSVC AND NOT IOS)
#    xcheck_add_c_compiler_flag(-Werror)
#    xcheck_add_c_compiler_flag(-Wextra)
#endif()
xcheck_add_c_compiler_flag(-Wno-implicit-fallthrough)
xcheck_add_c_compiler_flag(-Wno-sign-compare)
xcheck_add_c_compiler_flag(-Wno-missing-field-initializers)
xcheck_add_c_compiler_flag(-Wno-unused-parameter)
xcheck_add_c_compiler_flag(-Wno-unused-but-set-variable)
xcheck_add_c_compiler_flag(-Wno-array-bounds)
xcheck_add_c_compiler_flag(-Wno-format-truncation)
xcheck_add_c_compiler_flag(-funsigned-char)

# ClangCL is command line compatible with MSVC, so 'MSVC' is set.
if(MSVC)
    xcheck_add_c_compiler_flag(-Wno-unsafe-buffer-usage)
    xcheck_add_c_compiler_flag(-Wno-sign-conversion)
    xcheck_add_c_compiler_flag(-Wno-nonportable-system-include-path)
    xcheck_add_c_compiler_flag(-Wno-implicit-int-conversion)
    xcheck_add_c_compiler_flag(-Wno-shorten-64-to-32)
    xcheck_add_c_compiler_flag(-Wno-reserved-macro-identifier)
    xcheck_add_c_compiler_flag(-Wno-reserved-identifier)
    xcheck_add_c_compiler_flag(-Wdeprecated-declarations)
    xcheck_add_c_compiler_flag(/experimental:c11atomics)
    xcheck_add_c_compiler_flag(/wd4018) # -Wno-sign-conversion
    xcheck_add_c_compiler_flag(/wd4061) # -Wno-implicit-fallthrough
    xcheck_add_c_compiler_flag(/wd4100) # -Wno-unused-parameter
    xcheck_add_c_compiler_flag(/wd4200) # -Wno-zero-length-array
    xcheck_add_c_compiler_flag(/wd4242) # -Wno-shorten-64-to-32
    xcheck_add_c_compiler_flag(/wd4244) # -Wno-shorten-64-to-32
    xcheck_add_c_compiler_flag(/wd4245) # -Wno-sign-compare
    xcheck_add_c_compiler_flag(/wd4267) # -Wno-shorten-64-to-32
    xcheck_add_c_compiler_flag(/wd4388) # -Wno-sign-compare
    xcheck_add_c_compiler_flag(/wd4389) # -Wno-sign-compare
    xcheck_add_c_compiler_flag(/wd4710) # Function not inlined
    xcheck_add_c_compiler_flag(/wd4711) # Function was inlined
    xcheck_add_c_compiler_flag(/wd4820) # Padding added after construct
    xcheck_add_c_compiler_flag(/wd4996) # -Wdeprecated-declarations
    xcheck_add_c_compiler_flag(/wd5045) # Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified
    xcheck_add_c_compiler_flag(/wd4514)
    xcheck_add_c_compiler_flag(/wd4514)
    xcheck_add_c_compiler_flag(-fchar8_t)
    xcheck_add_c_compiler_flag(/Zc:char8_t)
endif()

xcheck_add_c_compiler_flag(-fno-strict-aliasing)

if ( APPLE )
    xcheck_add_c_compiler_flag(-Wno-shorten-64-to-32)
endif()

xcheck_add_c_compiler_flag(-Wno-unused-variable)

# MacOS and GCC 11 or later need -Wno-maybe-uninitialized
# https://github.com/quickjs-ng/quickjs/issues/453
if(APPLE AND CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 11)
    xcheck_add_c_compiler_flag(-Wno-maybe-uninitialized)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "WASI")
    add_compile_definitions(
        _WASI_EMULATED_PROCESS_CLOCKS
        _WASI_EMULATED_SIGNAL
    )
    add_link_options(
        -lwasi-emulated-process-clocks
        -lwasi-emulated-signal
    )
endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    add_compile_options(-O0)
    xcheck_add_c_compiler_flag(-ggdb)
    xcheck_add_c_compiler_flag(-fno-omit-frame-pointer)
endif()

set(qjs_sources
    quickjs/cutils.c
    quickjs/libbf.c
    quickjs/libregexp.c
    quickjs/libunicode.c
    quickjs/quickjs.c
)

set(papi_sources
    source/PapiQuickjsImpl.cpp
    source/CppObjectMapper.cpp
)

add_definitions(-D_CHAR16T)
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-D_SCL_SECURE_NO_WARNINGS)
add_definitions(-DEASTL_OPENSOURCE=1)

include_directories(include quickjs)
include_directories(EASTL/include)
include_directories(papi-register/include)

file(GLOB eastl_sources "EASTL/source/*.cpp")
file(GLOB_RECURSE eastl_headers "EASTL/include/EASTL/**.h")

list(APPEND qjs_defines _GNU_SOURCE)
if(WIN32)
    list(APPEND qjs_defines WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0602)
endif()
list(APPEND qjs_libs ${CMAKE_DL_LIBS})
find_package(Threads)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "WASI")
    list(APPEND qjs_libs ${CMAKE_THREAD_LIBS_INIT})
endif()
if(NOT MSVC)
    list(APPEND qjs_libs m)
endif()

set(papi_register_sources
    papi-register/source/JSClassRegister.cpp
)

if ( APPLE AND IOS )
    add_library(PapiQjs STATIC ${qjs_sources} ${eastl_sources} ${papi_sources})
    add_library(PapiRegister STATIC ${papi_register_sources})
else ()
    # 在Windows上需要显式生成导入库
    add_library(PapiQjs SHARED ${qjs_sources} ${eastl_sources} ${papi_sources}) # if add eastl_headers will undefined symbol: __udivti3 __udivmodti4 
    # 设置导出符号
    set_target_properties(PapiQjs PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS TRUE
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN TRUE
    )
    # 确保生成导入库路径正确
    set_target_properties(PapiQjs PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    add_library(PapiRegister SHARED ${papi_register_sources})
endif()
target_compile_definitions(PapiQjs PRIVATE ${qjs_defines})
target_include_directories(PapiQjs PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(PapiQjs PUBLIC ${qjs_libs})
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -nostdlib")

target_compile_definitions(PapiQjs PUBLIC EA_DEPRECATIONS_FOR_2024_APRIL=EA_DISABLED)
target_compile_definitions(PapiQjs PUBLIC EA_DEPRECATIONS_FOR_2024_SEPT=EA_DISABLED)
target_compile_definitions(PapiQjs PUBLIC EA_DEPRECATIONS_FOR_2025_APRIL=EA_DISABLED)
#target_compile_definitions(PapiQjs PUBLIC EASTL_DLL)
target_compile_definitions(PapiQjs PUBLIC USING_REGISTER_API_SHARED)
target_compile_definitions(PapiRegister PUBLIC BUILD_REGISTER_API_SHARED)

if ( APPLE AND NOT IOS OR MSVC OR LINUX )

# 配置GTest
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(BUILD_GTEST ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.8.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)


add_executable(cpp_object_mapper_test 
    test/CppObjectMapperTest.cpp
)

target_link_libraries(cpp_object_mapper_test
    PRIVATE
    PapiQjs
    gtest
    gtest_main
)

target_include_directories(cpp_object_mapper_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${CMAKE_CURRENT_SOURCE_DIR}/quickjs
    ${googletest_SOURCE_DIR}/googletest/include
)

gtest_discover_tests(cpp_object_mapper_test)

endif()
