cmake_minimum_required(VERSION 3.12...3.21)

# Flags / Options
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(AsciiVideoPlayer VERSION 1.2 LANGUAGES CXX)
add_executable(AsciiVideoPlayer ./src/main.cpp)

find_package(OpenCV REQUIRED)
find_package(Threads REQUIRED)

# Headers
target_include_directories(AsciiVideoPlayer PRIVATE ${OpenCV_INCLUDE_DIRS})

# Libraries
set(THREADS_PREFER_PTHREAD_FLAG ON)
target_link_libraries(AsciiVideoPlayer PRIVATE Threads::Threads)

target_link_libraries(AsciiVideoPlayer PRIVATE ${OpenCV_LIBS})

# Compilation

target_compile_features(AsciiVideoPlayer PRIVATE cxx_std_20)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_compile_options(AsciiVideoPlayer PRIVATE /W4)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_compile_options(AsciiVideoPlayer PRIVATE -Wall -Wextra -Wpedantic -Wno-unknown-pragmas)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	target_compile_options(AsciiVideoPlayer PRIVATE)
endif()
