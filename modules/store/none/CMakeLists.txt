add_library("leaf-store-standalone" STATIC
    "lifecycle.cpp"
    "network.cpp"
    "user.cpp"
    "progression.cpp"
)
add_library("leaf::store-standalone" ALIAS "leaf-store-standalone")
target_include_directories("leaf-store-standalone" PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)
target_link_libraries("leaf-store-standalone" PUBLIC "leaf::store")
