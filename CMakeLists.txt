cmake_minimum_required(VERSION 4.2)
project(_Bloomberg_terminal_)

set(CMAKE_CXX_STANDARD 14)

include_directories(bloomberg-terminal/include)

add_executable(_Bloomberg_terminal_
        bloomberg-terminal/include/terminal.hpp
        bloomberg-terminal/src/data.cpp
        bloomberg-terminal/src/main.cpp)
