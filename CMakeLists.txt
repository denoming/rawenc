cmake_minimum_required(VERSION 3.18)

project(rawenc
    VERSION 0.0.1
    DESCRIPTION "Encoding RAW video by H.26x encoder family"
)

include(cmake/ProjectOptions.cmake)
include(cmake/ProjectConfigs.cmake)

add_subdirectory(src)
