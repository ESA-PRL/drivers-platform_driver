project(platform_driver_lib)

cmake_minimum_required(VERSION 2.6)

add_definitions(-Wall)
add_definitions(-DBASE_LOG_NAMESPACE=${PROJECT_NAME})
add_definitions(-DBASE_LOG_INFO)

find_package(Rock)
rock_init(platform_driver 0.1)
rock_standard_layout()
