add_library("leaf-store" STATIC)
target_sources("leaf-store" PRIVATE "progression.cpp")
add_library("leaf::store" ALIAS "leaf-store")
target_include_directories("leaf-store" PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)
target_link_libraries("leaf-store" PUBLIC "leaf::core" "leaf::progression")

add_subdirectory("none")

if(WIN32 OR (UNIX AND NOT APPLE))
    if(EXISTS "$ENV{STEAMWORKS_SDK_DIR}/public/steam/steam_api.h")
        message(STATUS "Leaf Steam store target enabled")
        add_subdirectory("steam")
    else()
        message(STATUS "Leaf Steam store target disabled: STEAMWORKS_SDK_DIR is missing or invalid")
    endif()
endif()
