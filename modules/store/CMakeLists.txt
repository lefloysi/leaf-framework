add_library("leaf-store" INTERFACE)
add_library("leaf::store" ALIAS "leaf-store")
target_include_directories("leaf-store" INTERFACE
    "${PROJECT_SOURCE_DIR}/include"
)
target_link_libraries("leaf-store" INTERFACE "leaf::core")

add_subdirectory("none")

if(WIN32 OR (UNIX AND NOT APPLE))
    if(EXISTS "$ENV{STEAMWORKS_SDK_DIR}/public/steam/steam_api.h")
        message(STATUS "Leaf Steam store target enabled")
        add_subdirectory("steam")
    else()
        message(STATUS "Leaf Steam store target disabled: STEAMWORKS_SDK_DIR is missing or invalid")
    endif()
endif()
