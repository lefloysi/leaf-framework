if(APPLE)
	find_package("glfw3" "3.4" CONFIG REQUIRED)
else()
	find_package("glfw3" CONFIG REQUIRED)
endif()

add_library("leaf-platform" STATIC
    "desktop_glfw.cpp"
)
add_library("leaf::platform" ALIAS "leaf-platform")
target_include_directories("leaf-platform" PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)
target_compile_definitions("leaf-platform" PRIVATE LEAF_WINDOW_BACKEND_GLFW)

if(APPLE)
	find_path(LEAF_VULKAN_INCLUDE_DIRECTORY "vulkan/vulkan_core.h"
		HINTS "$ENV{VULKAN_SDK}/include" REQUIRED)
	target_include_directories("leaf-platform" PRIVATE "${LEAF_VULKAN_INCLUDE_DIRECTORY}")
endif()
target_link_libraries("leaf-platform"
    PUBLIC
        "leaf::core"
        "leaf::system"
        "glfw"
)
