cmake_minimum_required(VERSION 3.10)

project(Scraper
    DESCRIPTION "A simple service to support web query"
    LANGUAGES CXX)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

include_directories(src)

set(SRC_DIR src)
set(BUILD build)
#set(TEST_DIR test)

add_executable(Scraper
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/scraper.cpp
    ${SRC_DIR}/server.cpp
)

target_link_libraries(Scraper PRIVATE Threads::Threads)
