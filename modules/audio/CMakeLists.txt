include("FetchContent")
FetchContent_Declare("miniaudio"
	URL "https://raw.githubusercontent.com/mackron/miniaudio/0.11.23/miniaudio.h"
	URL_HASH "SHA256=7e4f3f13c8fe66df2080ac3dd12a89193e3c2463cb7f067c798abd7331cd8ee6"
	DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_MakeAvailable("miniaudio")

add_library("leaf-audio" STATIC)
add_library("leaf::audio" ALIAS "leaf-audio")
target_sources("leaf-audio" PRIVATE "audio.cpp" "miniaudio.c" "script.cpp")
target_include_directories("leaf-audio" PUBLIC "${PROJECT_SOURCE_DIR}/include" PRIVATE "${miniaudio_SOURCE_DIR}")
target_link_libraries("leaf-audio" PUBLIC "leaf::core" "leaf::script" PRIVATE "${CMAKE_DL_LIBS}")
if(UNIX)
	find_package("Threads" REQUIRED)
	target_link_libraries("leaf-audio" PRIVATE "Threads::Threads" "m")
endif()
