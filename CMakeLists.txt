cmake_minimum_required(VERSION 3.10)

project(splice C CXX)

add_executable(splice
	"src/main.c"
	"src/search.c"
	"src/common.c"
	"src/unscrambler.c"
	"src/database.c"
	"src/platform.c"
	"src/drag_drop.c"
)

option(USE_DEBUG "" OFF)
if (USE_DEBUG)
	message(STATUS "Using debug options")
	target_compile_options(splice PRIVATE
		-O0 -g
		
		-Werror -Wall -Wextra -pedantic-errors

		-Wmissing-prototypes
		-Wcast-qual
		-Wincompatible-pointer-types
		-Wuninitialized
		-Wnarrowing
		-Wvla
		-Wunused-variable
		-Wunused-function
		-Wunused-parameter

		-Wno-sign-compare
		-Wno-pointer-sign
	)

	option(USE_SANITIZERS "" OFF)
	if (USE_SANITIZERS)
		message(STATUS "Using sanitizers")
		target_compile_options(splice PRIVATE -fsanitize=address,undefined,leak,pointer-compare,pointer-subtract)
		target_link_options(splice PRIVATE -fsanitize=address,undefined,leak,pointer-compare,pointer-subtract)
	endif ()
endif ()

target_compile_definitions(splice PRIVATE _XOPEN_SOURCE=700 _CRT_SECURE_NO_WARNINGS)
target_include_directories(splice PRIVATE "src")

set_property(TARGET splice PROPERTY C_STANDARD 17)
set_property(TARGET splice PROPERTY C_EXTENSIONS OFF)

set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)
add_subdirectory("lib/cJSON-1.7.19")
target_include_directories(splice PRIVATE "lib/cJSON-1.7.19")
add_subdirectory("lib/glew-2.3.0/build/cmake")
target_include_directories(splice PRIVATE "lib/glew-2.3.0/include")
add_subdirectory("lib/glfw")
add_subdirectory("lib/curl-8.17.0")

add_library(cimgui 
	"lib/cimgui/cimgui.cpp"
    "lib/cimgui/imgui/imgui.cpp"
    "lib/cimgui/imgui/imgui_draw.cpp"
	"lib/cimgui/imgui/imgui_demo.cpp"
    "lib/cimgui/imgui/imgui_tables.cpp"
    "lib/cimgui/imgui/imgui_widgets.cpp"
	"lib/cimgui/imgui/backends/imgui_impl_glfw.cpp"
	"lib/cimgui/imgui/backends/imgui_impl_opengl3.cpp"
)
target_include_directories(cimgui PRIVATE "lib/cimgui/imgui")
target_link_libraries(cimgui PRIVATE glfw)
target_include_directories(splice PRIVATE "lib/cimgui")
if(WIN32)
    target_compile_definitions(cimgui PUBLIC "-DIMGUI_IMPL_API=extern \"C\" __declspec(dllexport)")
else()
    target_compile_definitions(cimgui PUBLIC "-DIMGUI_IMPL_API=extern \"C\"")
endif()

add_library(miniaudio STATIC "lib/miniaudio-0.11.23/miniaudio.c")
target_include_directories(miniaudio INTERFACE "lib/miniaudio-0.11.23/")

target_link_libraries(splice PRIVATE 
	OpenGL::GL
	cimgui
	cjson
	miniaudio
	glew
	glfw
	libcurl
)
