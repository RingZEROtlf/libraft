cmake_minimum_required(VERSION 3.17.0)
project(libraft VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)

# gtest
set(GTestDir ./googletest)
add_subdirectory(${GTestDir})
include_directories(${GTestDir}/googletest/include)
include_directories(${GTestDir}/googlemock/include)

add_executable(test_libraft main.cc)

# link with gtest
target_link_libraries(test_libraft gtest gtest_main)
target_link_libraries(test_libraft gmock gmock_main)