cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

project(yagbe VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

set(DEBUG_CPU "enable cpu debug output" CACHE BOOL OFF)

find_package(SDL2 REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

set(SOURCE_FILES
  src/main.cc)

add_executable(yagbe
  ${SOURCE_FILES})

if (DEBUG_CPU)
  target_compile_definitions(yagbe PRIVATE -DDEBUG_CPU)
endif()

target_include_directories(yagbe SYSTEM
  PRIVATE ${SDL2_INCLUDE_DIRS})

target_link_libraries(yagbe
  PRIVATE  ${SDL2_LIBRARIES})
