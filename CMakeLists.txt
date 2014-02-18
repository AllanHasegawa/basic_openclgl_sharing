cmake_minimum_required (VERSION 2.8)
project(oglcl)

if(UNIX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11")
endif()

find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)
include_directories(${GLFW_INCLUDE_DIRS})

pkg_search_module(SDL2 REQUIRED sdl2)
include_directories(${SDL2_INCLUDE_DIRS})

find_package(OpenGL REQUIRED)
include_directories(${OpenGL_INCLUDE_DIRS})
link_directories(${OpenGL_LIBRARY_DIRS})
add_definitions(${OpenGL_DEFINITIONS})
if(NOT OPENGL_FOUND)
	message(ERROR " OPENGL not found!")
endif(NOT OPENGL_FOUND)

set(SRCS_GLFW3
	main_glfw3.cpp
)

add_executable(oglcl_glfw3 ${SRCS_GLFW3})
target_link_libraries(oglcl_glfw3 ${GLFW_LIBRARIES} ${OPENGL_LIBRARIES} GLEW OpenCL)

set(SRCS_SDL2
	main_sdl2.cpp
)

add_executable(oglcl_sdl2 ${SRCS_SDL2})
target_link_libraries(oglcl_sdl2 ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} GLEW OpenCL)
