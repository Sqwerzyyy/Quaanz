cmake_minimum_required(VERSION 3.16)
project(bloomberg-terminal VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG   "-g -Wall -Wextra")

find_package(Curses REQUIRED)
find_package(Threads REQUIRED)

add_executable(bloomberg-terminal
    src/main.cpp
    src/data.cpp
)

target_include_directories(bloomberg-terminal PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CURSES_INCLUDE_DIRS}
)

target_link_libraries(bloomberg-terminal PRIVATE
    ${CURSES_LIBRARIES}
    Threads::Threads
    m
)

install(TARGETS bloomberg-terminal RUNTIME DESTINATION bin)

message(STATUS "Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "ncurses    : ${CURSES_LIBRARIES}")
