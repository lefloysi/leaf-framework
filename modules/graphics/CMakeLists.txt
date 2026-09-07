add_library("leaf-graphics" STATIC
	"buffer.cpp"
	"command_buffer.cpp"
    "graphics.cpp"
	"graphics_program.cpp"
	"queue.cpp"
    "resource.cpp"
    "rutile.cpp"
	"sampler.cpp"
	"swapchain.cpp"
	"timepoint.cpp"
	"texture.cpp"
	"texture_view.cpp"
)
add_library("leaf::graphics" ALIAS "leaf-graphics")
target_include_directories("leaf-graphics" PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)
target_link_libraries("leaf-graphics"
    PUBLIC
        "leaf::core"
        "leaf::platform"
        "Rutile::rutile"
)
