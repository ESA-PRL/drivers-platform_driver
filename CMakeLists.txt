project(platform_driver)
cmake_minimum_required(VERSION 2.6)

add_definitions(-Wall)
add_definitions(-DBASE_LOG_NAMESPACE=${PROJECT_NAME}_lib)
add_definitions(-DBASE_LOG_INFO)

find_package(Rock)
rock_init(platform_driver 0.1)
rock_standard_layout()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
