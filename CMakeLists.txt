cmake_minimum_required(VERSION 3.5)
project(CiorapBot VERSION 0.4)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

# Check if the operating system is Windows
if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -flto -DNDEBUG -march=native -static")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
endif()

# Engine library sources (excluding Main.cpp so trainer can link them)
set(ENGINE_SOURCES
    engine/Board.cpp
    engine/BoardUtils.cpp
    engine/Evaluate.cpp
    engine/MagicBitboardUtils.cpp
    engine/Search.cpp
    engine/TranspositionTable.cpp
    engine/UCI.cpp
)

add_executable(ciorap-bot-04 engine/Main.cpp ${ENGINE_SOURCES})
target_link_libraries(ciorap-bot-04 PRIVATE Threads::Threads)

add_executable(texel-trainer training/train.cpp ${ENGINE_SOURCES})
target_link_libraries(texel-trainer PRIVATE Threads::Threads)
