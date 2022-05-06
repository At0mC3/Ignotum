cmake_minimum_required(VERSION 3.20)
set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED True)

project(Ignotum)

if(CMAKE_CXX_BYTE_ORDER STREQUAL "LITTLE_ENDIAN")
    message("[INFO]: System is LITTLE ENDIAN")
    add_compile_definitions(LITTLE_ENDIAN=1)
else()
    message("[INFO]: System is BIG ENDIAN")
endif()

# Register Zydis dependency.
# Disable build of tools and examples.
option(ZYDIS_BUILD_TOOLS "" OFF)
option(ZYDIS_BUILD_EXAMPLES "" OFF)
add_subdirectory("deps/zydis")
include_directories("include")
include_directories("${CMAKE_SOURCE_DIR}/deps/argparse/include")
include_directories("${CMAKE_SOURCE_DIR}/deps/result")

set(SOURCE src/PeFile.cpp 
            src/Assembler.cpp
            src/Translation.cpp 
            src/Virtual.cpp 
            src/MappedMemory.cpp 
            include/Parameter.hpp 
            src/Parameter.cpp 
            deps/result/result.h)

find_package(spdlog REQUIRED)

if(${LibMode} STREQUAL "False")
    add_executable(Ignotum
        src/Main.cpp
        ${SOURCE})
else()
    message("Library mode enabled. Lib.cpp will be used as the entry point")
    add_library(Ignotum SHARED 
        src/Lib.cpp
        ${SOURCE})
endif()

target_link_libraries(Ignotum PRIVATE "Zydis")
target_link_libraries(Ignotum PRIVATE spdlog::spdlog)
