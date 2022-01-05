cmake_minimum_required (VERSION 3.18.0)
project(Tello LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

if(WIN32)
	# target Windows 10
	add_compile_options(-D _WIN32_WINNT=0x0A00)
	# warning level 3, warnings as errors
	add_compile_options(/W3 /WX)
	# /Zi tells the compiler to put debug info into the OBJ file
	add_compile_options($<$<NOT:$<CONFIG:Debug>>:/Zi>)
	# /DEBUG tells the linker that we want to create a PDB file.
	add_link_options($<$<NOT:$<CONFIG:Debug>>:/DEBUG>)
	# /OPT:REF tells the linker to only put the functions that are referenced into the output binary.
	add_link_options($<$<NOT:$<CONFIG:Debug>>:/OPT:REF>)
	# /OPT:ICF tells the linker to look for functions that have identical assembly language code and only generate one of them
	add_link_options($<$<NOT:$<CONFIG:Debug>>:/OPT:ICF>)
endif(WIN32)

if(WIN32)
	find_package(Boost REQUIRED)
else()
	find_package(Boost COMPONENTS log log_setup REQUIRED)
endif(WIN32)

find_package(glfw3 3.3 REQUIRED)

include_directories(
	${Boost_INCLUDE_DIRS}
	${CMAKE_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/../)

link_directories(${Boost_LIBRARY_DIRS})

add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)

add_executable(Repl Repl.cpp)

target_link_libraries (Repl glfw ${Boost_LIBRARIES})
