find_package("ZLIB" REQUIRED)
find_package("Freetype" CONFIG REQUIRED)
find_package("RmlUi" CONFIG REQUIRED)
find_path("STB_INCLUDE_DIR" "stb_image.h" REQUIRED)

add_library("leaf-application" STATIC
    "rml/backend.cpp"
    "rml/cast_shadow.cpp"
    "rml/context.cpp"
    "rml/file_interface.cpp"
    "rml/render_interface.cpp"
    "rml/system_interface.cpp"
    "scene.cpp"
	"window.cpp"
)
add_library("leaf::application" ALIAS "leaf-application")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/window.rcss")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/assets/window.rcss" LEAF_WINDOW_STYLESHEET)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file("rml/resources.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/generated/resources.cpp" @ONLY)
target_sources("leaf-application" PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/resources.cpp")

target_include_directories("leaf-application" PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)
target_include_directories("leaf-application" PRIVATE
    "${STB_INCLUDE_DIR}"
)
target_link_libraries("leaf-application"
    PUBLIC
        "leaf::core"
        "leaf::system"
        "leaf::platform"
        "leaf::graphics"
        "leaf::network"
        "leaf::script"
        "leaf::audio"
        "leaf::vendor"
        "RmlUi::RmlUi"
)

add_subdirectory("shaders")
