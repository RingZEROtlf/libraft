cmake_minimum_required(VERSION 3.16)

# set the project name and version
project(libraft VERSION 0.0.1)

# specify the cxx standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

include_directories(src src/protocol)

# unittests
find_package(GTest REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Boost REQUIRED
  COMPONENTS log)

add_executable(unittest 
  test/unittest.cc
  test/raft_leader_election_unittest.cc
  src/raft_leader_election.cc
  src/protocol/raft.pb.cc)
target_link_libraries(unittest
  ${GTEST_BOTH_LIBRARIES}
  ${Protobuf_LIBRARIES}
  ${Boost_LIBRARIES}
  pthread)