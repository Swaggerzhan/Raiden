cmake_minimum_required(VERSION 3.16)
set(NAME Raiden)
project(${NAME})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

include_directories(${CMAKE_SOURCE_DIR})

aux_source_directory(net NET_SOURCE_FILES)
aux_source_directory(util UTIL_SOURCE_FILES)

set(CMAKE_CXX_STANDARD 14)

add_executable(${NAME}
        ${NET_SOURCE_FILES}
        ${UTIL_SOURCE_FILES}
        main.cc)
