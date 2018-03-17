include_directories(${CMAKE_SOURCE_DIR}/src/logger)
include_directories(${CMAKE_SOURCE_DIR}/src/registry)
include_directories(${CMAKE_SOURCE_DIR}/src/dbi)
add_executable(test_registry test_registry.cpp)
target_link_libraries(test_registry ums_registry)

add_executable(test_config_import test_config_import.cpp)
target_link_libraries(test_config_import ums_registry)
