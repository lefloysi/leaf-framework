find_package("ZLIB" REQUIRED)
find_package("Freetype" CONFIG REQUIRED)
find_package("RmlUi" CONFIG REQUIRED)
find_path("STB_INCLUDE_DIR" "stb_image.h" REQUIRED)

add_library("leaf-application" STATIC
    "rml/backend.cpp"
    "rml/file_interface.cpp"
    "rml/render_interface.cpp"
    "rml/system_interface.cpp"
    "scene.cpp"
	"window.cpp"
)
add_library("leaf::application" ALIAS "leaf-application")

if(CMAKE_CONFIGURATION_TYPES)
    set(LEAF_APPLICATION_RESOURCE_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/leaf/application")
else()
    set(LEAF_APPLICATION_RESOURCE_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/leaf/application")
endif()
add_custom_target("leaf-application-assets"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${LEAF_APPLICATION_RESOURCE_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/window.rcss"
        "${LEAF_APPLICATION_RESOURCE_DIR}/window.rcss"
    VERBATIM
)
add_dependencies("leaf-application" "leaf-application-assets")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/window.rcss" DESTINATION "leaf/application")

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
        "leaf::vendor"
        "RmlUi::RmlUi"
)

add_subdirectory("shaders")
