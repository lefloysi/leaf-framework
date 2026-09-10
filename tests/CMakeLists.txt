find_package(Catch2 CONFIG REQUIRED)
include(Catch)

add_executable(leaf-procedural-tests EXCLUDE_FROM_ALL procedural/mesh_generator.cpp)
target_link_libraries(leaf-procedural-tests PRIVATE leaf::procedural Catch2::Catch2WithMain)
catch_discover_tests(leaf-procedural-tests TEST_PREFIX "leaf::procedural::")

add_executable("leaf-tests" EXCLUDE_FROM_ALL
    "core/component_container.cpp"
    "core/binary.cpp"
    "core/time.cpp"
    "graphics/vertex_layout.cpp"
    "core/filesystem.cpp"
    "resource_schemas.cpp"
    "script/prototype_lua.cpp"
    "script/mod_loader.cpp"
    "script/state.cpp"
)

target_link_libraries("leaf-tests" PRIVATE
    "Catch2::Catch2WithMain"
    "leaf::core"
    "leaf::resource"
    "leaf::script"
    "leaf::system"
)

catch_discover_tests("leaf-tests"
    TEST_PREFIX "leaf::"
)
