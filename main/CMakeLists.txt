set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
#set(CMAKE_CXX_CLANG_TIDY "clang-tidy")

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CCACHE_PROGRAM}")
    message(STATUS "ccache found and enabled")
endif()

if(CMAKE_CXX_STANDARD GREATER_EQUAL 20)
    message(STATUS "C++20 or newer is supported/enabled")

    # mem pool allocator to prevent memory fragmentation for small and frequently dynamically allocated events/messages
    # note: it can introduce some slow-down but memory allocation pattern is more predictive and stable

    # uncomment to use mem pool allocator instead of regular heap allocation (requires C++ 20 or above)
    add_compile_definitions(USE_MEM_POOL_ALLOCATOR)

    # uncomment to warmup the mem pool with pre-allocated chunks of memory
    #add_compile_definitions(USE_MEM_POOL_ALLOCATOR_WARMUP)
endif()

# Local header files here ONLY
file(GLOB_RECURSE TARGET_H
    *.h
    *.hpp
    *.inl
)

# Local source files here

set(TARGET_CEXCEPTION_SRC
        CException/CException.c
)

set(TARGET_CJSON_SRC
        cJSON/cJSON.c
        cjsonpp/cjsonpp.cpp
)

set(TARGET_UZLIB_SRC
        uzlib/adler32.c
        uzlib/crc32.c
        uzlib/defl_static.c
        uzlib/genlz77.c
        uzlib/tinfgzip.c
        uzlib/tinflate.c
        uzlib/tinfzlib.c
)

set(TARGET_TOOLS_SRC
        tools/gzip_wrapper.cpp
        tools/mem_pool_allocator.cpp
        tools/sync_object.cpp
        tools/timer_scheduler.cpp
)

set(TARGET_SRC
        main.cpp
        "${TARGET_TOOLS_SRC}"
        "${TARGET_CEXCEPTION_SRC}"
        "${TARGET_CJSON_SRC}"
        "${TARGET_UZLIB_SRC}"
)

set(TARGET_INCLUDE_DIR .)

idf_component_register(SRCS "${TARGET_SRC}" "${TARGET_H}" INCLUDE_DIRS "${TARGET_INCLUDE_DIR}")
