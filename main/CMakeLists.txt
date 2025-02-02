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
        tools/sync_object.cpp
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






