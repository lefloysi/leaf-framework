set(STEAMWORKS_SDK_DIR "$ENV{STEAMWORKS_SDK_DIR}")

if(NOT EXISTS "${STEAMWORKS_SDK_DIR}/public/steam/steam_api.h")
    message(FATAL_ERROR "STEAMWORKS_SDK_DIR is missing or invalid: ${STEAMWORKS_SDK_DIR}")
endif()

add_library("leaf-store-steam" STATIC)
target_sources("leaf-store-steam" PRIVATE
    "lifecycle.cpp"
    "network.cpp"
    "user.cpp"
    "progression.cpp"
    "workshop.cpp"
)
add_library("leaf::store-steam" ALIAS "leaf-store-steam")
target_include_directories("leaf-store-steam"
    PUBLIC "${PROJECT_SOURCE_DIR}/include"
        PRIVATE "${STEAMWORKS_SDK_DIR}/public"
)
target_link_libraries("leaf-store-steam" PUBLIC "leaf::store" "leaf::network")

if(WIN32)
    target_link_libraries("leaf-store-steam" PRIVATE
        "${STEAMWORKS_SDK_DIR}/redistributable_bin/win64/steam_api64.lib"
    )
elseif(UNIX AND NOT APPLE)
    target_link_libraries("leaf-store-steam" PRIVATE
        "${STEAMWORKS_SDK_DIR}/redistributable_bin/linux64/libsteam_api.so"
    )
endif()
