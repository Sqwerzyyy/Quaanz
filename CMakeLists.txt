cmake_minimum_required(VERSION 3.16)
project(bloomberg-terminal VERSION 7.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

option(OPTIMIZE_NATIVE "Enable -march=native optimization" ON)
if(OPTIMIZE_NATIVE)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native")
endif()

include(FetchContent)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
  GIT_TAG        v5.0.0
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(ftxui)

find_package(Threads REQUIRED)

add_executable(bloomberg-terminal
    src/main.cpp
    src/data.cpp
    src/HighResChart.cpp
)

target_include_directories(bloomberg-terminal PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(bloomberg-terminal PRIVATE
    ftxui::screen
    ftxui::dom
    ftxui::component
    Threads::Threads
    m
)

install(TARGETS bloomberg-terminal RUNTIME DESTINATION bin)
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")