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

# The ESP-IDF toolchain's bundled libstdc++ does not define _GLIBCXX_HAVE_ATOMIC_LOCK_POLICY
# in its c++config.h, so std::shared_ptr falls back to mutex-based (_S_mutex) refcounting even
# though ESP32/ESP32-S3/ESP32-C5 all support lock-free atomic CAS (__GCC_ATOMIC_INT_LOCK_FREE == 2).
# _S_mutex means every shared_ptr control block calls pthread_mutex_init() -> xQueueCreateMutex(),
# allocating heap memory per shared_ptr; under heap pressure this allocation can silently fail,
# leaving a garbage mutex handle that crashes on destruction. Force _S_atomic to avoid this.
# idf_build_set_property is only defined when this file is processed by the real ESP-IDF build
# system (idf.py); guard against tooling that parses this component CMakeLists.txt standalone.
if(COMMAND idf_build_set_property AND NOT IDF_TARGET STREQUAL "linux")
    idf_build_set_property(COMPILE_OPTIONS "-D_GLIBCXX_HAVE_ATOMIC_LOCK_POLICY" APPEND)
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

set(TARGET_PORTABLE_CONCURRENCY_SRC
        portable_concurrency/bits/portable_concurrency_runtime.cpp
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
        "${TARGET_PORTABLE_CONCURRENCY_SRC}"
        "${TARGET_UZLIB_SRC}"
)

set(TARGET_INCLUDE_DIR .)

idf_component_register(SRCS "${TARGET_SRC}" "${TARGET_H}" INCLUDE_DIRS "${TARGET_INCLUDE_DIR}")

if(NOT CMAKE_BUILD_EARLY_EXPANSION AND TARGET_COMPILE_DEFINITIONS)
        target_compile_definitions(${COMPONENT_LIB} PRIVATE ${TARGET_COMPILE_DEFINITIONS})
endif()
