find_path("STB_INCLUDE_DIR" "stb_image.h" REQUIRED)

add_library("leaf-manager" STATIC
    "texture_atlas.cpp"
    "asset.cpp"
)
add_library("leaf::manager" ALIAS "leaf-manager")
target_include_directories("leaf-manager" PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)
target_include_directories("leaf-manager" PRIVATE
    "${STB_INCLUDE_DIR}"
)
target_link_libraries("leaf-manager"
    PUBLIC
        "leaf::core"
        "leaf::graphics"
)

