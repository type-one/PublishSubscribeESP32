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
endif()

# mem pool allocator to prevent memory fragmentation for small and frequently dynamically allocated events/messages
# note: it can introduce some slow-down but memory allocation pattern is more predictive and stable
option(ENABLE_MEM_POOL_ALLOCATOR "Enable custom mem pool allocator" ON)
option(ENABLE_MEM_POOL_ALLOCATOR_WARMUP "Warm up mem pool allocator with pre-allocated chunks" OFF)

set(TARGET_COMPILE_DEFINITIONS)

if(ENABLE_MEM_POOL_ALLOCATOR)
    list(APPEND TARGET_COMPILE_DEFINITIONS USE_MEM_POOL_ALLOCATOR)
endif()

if(ENABLE_MEM_POOL_ALLOCATOR_WARMUP)
    list(APPEND TARGET_COMPILE_DEFINITIONS USE_MEM_POOL_ALLOCATOR_WARMUP)
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

file(GLOB TARGET_EXAMPLES_SRC
        examples/*.cpp
)

set(TARGET_SRC
        main.cpp
        "${TARGET_EXAMPLES_SRC}"
        "${TARGET_TOOLS_SRC}"
        "${TARGET_CEXCEPTION_SRC}"
        "${TARGET_CJSON_SRC}"
        "${TARGET_UZLIB_SRC}"
)

set(TARGET_INCLUDE_DIR .)

idf_component_register(SRCS "${TARGET_SRC}" "${TARGET_H}" INCLUDE_DIRS "${TARGET_INCLUDE_DIR}")

if(NOT CMAKE_BUILD_EARLY_EXPANSION AND TARGET_COMPILE_DEFINITIONS)
        target_compile_definitions(${COMPONENT_LIB} PRIVATE ${TARGET_COMPILE_DEFINITIONS})
endif()
