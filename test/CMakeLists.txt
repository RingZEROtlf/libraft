find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(test_libraft test_libraft.cc)

target_link_libraries(test_libraft ${GTEST_BOTH_LIBRARIES} pthread)

gtest_discover_tests(test_libraft)