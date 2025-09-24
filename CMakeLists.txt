#   -   -   -   -   -   Global  -   -   -   -   -

cmake_minimum_required(VERSION 3.10)
project(Arachnida)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
  cxxopts
  GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
  GIT_TAG        v3.3.1
)

FetchContent_MakeAvailable(cxxopts)

#   -   -   -   -   -   Spider  -   -   -   -   -

find_package(CURL REQUIRED)

set(SPIDER_SOURCES

    src/spider/main.cpp
    src/spider/Spider.cpp
)

add_executable(spider ${SPIDER_SOURCES})
target_link_libraries(spider PRIVATE cxxopts CURL::libcurl)

# #   -   -   -   -   -   Scorpion    -   -   -   -   -

