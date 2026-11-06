cmake_minimum_required(VERSION 3.0)
project(CiorapBot VERSION 0.4)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Check if the operating system is Windows
if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -flto -DNDEBUG -march=native -static")
endif()

# Add your source files here
file(GLOB SOURCE_FILES "engine/*.cpp")

add_executable(ciorap-bot-04 ${SOURCE_FILES})
