find_path("STB_INCLUDE_DIR" "stb_image.h" REQUIRED)
find_package("sol2" CONFIG REQUIRED)
find_package("unofficial-lua" CONFIG REQUIRED)
find_package("yaml-cpp" CONFIG REQUIRED)
find_package("magic_enum" CONFIG REQUIRED)

add_library("leaf-script" STATIC
    "localization.cpp"
    "mod_enabled.cpp"
    "mod_info.cpp"
    "mod_loader.cpp"
    "cursor.cpp"
    "sound.cpp"
    "state.cpp"
    "interfaces.cpp"
    "shader.cpp"
    "texture.cpp"
)
add_library("leaf::script" ALIAS "leaf-script")
target_include_directories("leaf-script" PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)
target_include_directories("leaf-script" PRIVATE "${STB_INCLUDE_DIR}")
target_link_libraries("leaf-script"
    PUBLIC
        "leaf::core"
        "leaf::manager"
        "leaf::resource"
        "sol2::sol2"
    PRIVATE
        "lua"
        "magic_enum::magic_enum"
)
