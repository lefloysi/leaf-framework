add_library("leaf-framework" STATIC
    "leaf.cpp"
)
add_library("leaf::framework" ALIAS "leaf-framework")
target_include_directories("leaf-framework" PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)
target_link_libraries("leaf-framework" PUBLIC
    "leaf::application"
    "leaf::lockstep"
    "leaf::store"
    "leaf::progression"
)
