file(GLOB MY_HEADERS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.h)
file(GLOB MY_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp)

rock_library(platform_driver
    SOURCES ${MY_SOURCES}
    HEADERS ${MY_HEADERS}
    DEPS_PKGCONFIG soem base-lib)
