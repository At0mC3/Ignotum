cmake_minimum_required(VERSION 3.16)
set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED True)

project(Ignotum)

# Register Zydis dependency.
# Disable build of tools and examples.
option(ZYDIS_BUILD_TOOLS "" OFF)
option(ZYDIS_BUILD_EXAMPLES "" OFF)
add_subdirectory("deps/zydis")
include_directories("include")
include_directories("${CMAKE_SOURCE_DIR}/deps/argparse/include")

add_executable(Ignotum 
    src/Main.cpp
    src/PeFile.cpp
    src/Translation.cpp
    src/Virtual.cpp
        include/Parameter.hpp src/Parameter.cpp include/utl/Result.hpp include/utl/nResult.hpp)

target_link_libraries(Ignotum PRIVATE "Zydis")
