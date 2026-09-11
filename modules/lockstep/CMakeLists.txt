add_library("leaf-lockstep" STATIC
    "session.cpp"
    "host.cpp"
    "remote.cpp"
)
add_library("leaf::lockstep" ALIAS "leaf-lockstep")
target_include_directories("leaf-lockstep" PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)
target_link_libraries("leaf-lockstep" PUBLIC "leaf::core" "leaf::network")
