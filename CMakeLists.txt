cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

project(yagbe VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

set(DEBUG_CPU "enable cpu debug output" CACHE BOOL OFF)
set(SWITCHING_SHIT "enable less readable switch based codepath" CACHE BOOL ON)

find_package(SDL2 REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set( CMAKE_CXX_EXTENSIONS ON )

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3")
set(CMAKE_CXX_FLAGS_ASAN
    "-fsanitize=address -fno-optimize-sibling-calls -fsanitize-address-use-after-scope -fno-omit-frame-pointer -g -O3"
    CACHE STRING "Flags used by the C++ compiler during AddressSanitizer builds."
    FORCE)

set(CMAKE_CXX_FLAGS_ASAN1
    "-fsanitize=address -fno-optimize-sibling-calls -fsanitize-address-use-after-scope -fno-omit-frame-pointer -ggdb -O1"
    CACHE STRING "Flags used by the C++ compiler during AddressSanitizer builds."
    FORCE)

set(CMAKE_CXX_FLAGS_LSAN
    "-fsanitize=leak -fno-omit-frame-pointer -g -O2"
    CACHE STRING "Flags used by the C++ compiler during LeakSanitizer builds."
    FORCE)

set(CMAKE_CXX_FLAGS_UBSAN
	"-fsanitize=undefined"
    CACHE STRING "Flags used by the C compiler during UndefinedBehaviourSanitizer builds."
    FORCE)

set(SOURCE_FILES
  src/main.cc)

add_executable(yagbe
  ${SOURCE_FILES})

if (DEBUG_CPU)
  target_compile_definitions(yagbe PRIVATE -DDEBUG_CPU)
endif()

if (SWITCHING_SHIT)
	target_compile_definitions( yagbe PRIVATE -DDO_SWITCHING_SHIT)
endif()

target_include_directories(yagbe SYSTEM
  PRIVATE ${SDL2_INCLUDE_DIRS})

target_link_libraries(yagbe
  PRIVATE  ${SDL2_LIBRARIES})
