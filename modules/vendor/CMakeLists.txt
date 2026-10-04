add_library("leaf-vendor" STATIC
    "stb_image.c"
)
add_library("leaf::vendor" ALIAS "leaf-vendor")
target_include_directories("leaf-vendor" PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

# Pixel decoding is third-party compute code, including in Debug builds.
if(MSVC)
    string(REGEX REPLACE "/RTC[1su]+|/Od|/Ob0" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    target_compile_options("leaf-vendor" PRIVATE /O2 /Ob2)
else()
    target_compile_options("leaf-vendor" PRIVATE -O2)
endif()
