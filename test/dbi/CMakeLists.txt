include_directories(${CMAKE_SOURCE_DIR}/src/dbi)
add_executable(test_dbi test_dbi.cpp)
target_link_libraries(test_dbi ums_dbi)
