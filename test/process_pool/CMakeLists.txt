cmake_minimum_required(VERSION 2.8.7)
project(child_watch CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include(FindPkgConfig)

pkg_check_modules(GLIB REQUIRED glib-2.0)
include_directories(${GLIB_INCLUDE_DIRS})
link_directories(${GLIB_LIBRARY_DIRS})

find_package(Boost REQUIRED COMPONENTS filesystem system)
include_directories(${Boost_INCLUDE_DIRS})

include_directories( ${CMAKE_SOURCE_DIR}/src/process_pool
                     ${CMAKE_SOURCE_DIR}/src/logger
                     ${CMAKE_SOURCE_DIR}/src/utils
                     ${CMAKE_SOURCE_DIR}/src/dbi
                     ${CMAKE_SOURCE_DIR}/src/registry )

set(LINK_DEPS ${GLIB_LIBRARIES} ${Boost_LIBRARIES} pthread process_pool)

add_executable(glib_child_watch_test glib_child_watch_test.cpp)
target_link_libraries(glib_child_watch_test ${LINK_DEPS})

add_executable(process_test process_test.cpp)
target_link_libraries(process_test ${LINK_DEPS})

add_executable(process_pool_test process_pool_test.cpp)
target_link_libraries(process_pool_test ${LINK_DEPS})
