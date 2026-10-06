find_package("glm" CONFIG REQUIRED)
find_package("libzip" CONFIG REQUIRED)
find_package("yaml-cpp" CONFIG REQUIRED)

add_library("leaf-core" STATIC
    "dynamic_object.cpp"
    "error.cpp"
    "exception.cpp"
    "filesystem.cpp"
    "fixed.cpp"
    "logging.cpp"
	"package.cpp"
    "messages.cpp"
	"profiler.cpp"
	"progress.cpp"
    "random.cpp"
    "string.cpp"
    "time.cpp"
	"zip.cpp"
)
add_library("leaf::core" ALIAS "leaf-core")
target_include_directories("leaf-core" PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)
target_link_libraries("leaf-core"
    PUBLIC
        "glm::glm"
        "Rutile::rutile"
    PRIVATE
		"libzip::zip"
        "yaml-cpp::yaml-cpp"
)
