# Local header files here ONLY
file(GLOB_RECURSE TARGET_H
    *.h
    *.hpp
    *.inl
)

# Local source files here

set(TARGET_TOOLS_SRC
        tools/sync_object.cpp
)

set(TARGET_SRC
        main.cpp
        "${TARGET_TOOLS_SRC}"
)

idf_component_register(SRCS "${TARGET_SRC}" "${TARGET_H}" INCLUDE_DIRS "${PROJECT_SOURCE_DIR}")






