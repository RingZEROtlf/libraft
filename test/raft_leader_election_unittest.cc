#include "raft_leader_election.hh"
#include "utils/thread_safe_queue.hpp"

#include <gtest/gtest.h>
#include <boost/log/trivial.hpp>

#include <thread>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace {

class RaftLeaderElectionTest;

std::unordered_map<uint64_t, RaftLeaderElectionTest*> cluster;

class RaftLeaderElectionTest {
 public:
  static constexpr auto duration = std::chrono::milliseconds(100);

 public:
  explicit RaftLeaderElectionTest(
      uint64_t id,
      size_t cluster_size)
    : this_node_(id, cluster_size, 30, 10),
      exiting_(false),
      id_(id),
      ticker_thread_(),
      step_thread_(),
      ready_thread_() {
    
  }

  ~RaftLeaderElectionTest() {
    if (!exiting_) {
      stop();
    }
    if (ticker_thread_.joinable()) {
      ticker_thread_.join();
    }
    if (step_thread_.joinable()) {
      step_thread_.join();
    }
    if (ready_thread_.joinable()) {
      ready_thread_.join();
    }
  }

  void run() {
    ticker_thread_ = std::thread(&RaftLeaderElectionTest::ticker_thread_impl, this);
    step_thread_ = std::thread(&RaftLeaderElectionTest::step_thread_impl, this);
    ready_thread_ = std::thread(&RaftLeaderElectionTest::ready_thread_impl, this);
  }

  void stop() {
    exiting_ = true;
  }

 private:
  void ticker_thread_impl() {
    auto till = std::chrono::steady_clock::now() + duration;
    while (!exiting_) {
      auto curr = std::chrono::steady_clock::now();
      if (curr >= till) {
        this_node_.tick();
        till = curr + duration;
      }
    }
  }

  void step_thread_impl() {
    while (!exiting_) {
      std::unique_lock<std::mutex> lock(mutex_);

      while (!heartbeat_requests_.empty()) {
        auto message = heartbeat_requests_.pop();
        auto source_id = message.first;
        auto request = message.second;
        raft::protocol::Heartbeat_Response response;

        this_node_.step_heartbeat_request(request, response);
        cluster[source_id]->heartbeat_responses_.push(response);
      }

      while (!heartbeat_responses_.empty()) {
        auto response = heartbeat_responses_.pop();
        this_node_.step_heartbeat_response(response);
      }

      while (!request_vote_requests_.empty()) {
        auto message = request_vote_requests_.pop();
        auto source_id = message.first;
        auto request = message.second;
        raft::protocol::RequestVote_Response response;

        this_node_.step_request_vote_request(request, response);
        cluster[source_id]->request_vote_responses_.push(response);
      }

      while (!request_vote_responses_.empty()) {
        auto response = request_vote_responses_.pop();
        this_node_.step_request_vote_response(response);
      }
    }
  }

  void ready_thread_impl() {
    while (!exiting_) {
      std::unique_lock<std::mutex> lock(mutex_);

      auto ready = this_node_.ready();
      auto &heartbeat_messages = ready.heartbeat_messages;
      while (!heartbeat_messages.empty()) {
        auto heartbeat = heartbeat_messages.front();
        heartbeat_messages.pop();
        for (auto &[target_id, target_node]: cluster) {
          if (target_id != id_) {
            target_node->heartbeat_requests_.push(std::make_pair(id_, heartbeat));
          }
        }
      }
      auto &request_vote_messages = ready.request_vote_messages;
      while (!request_vote_messages.empty()) {
        auto request_vote = request_vote_messages.front();
        request_vote_messages.pop();
        for (auto &[target_id, target_node]: cluster) {
          if (target_id != id_) {
            target_node->request_vote_requests_.push(std::make_pair(id_, request_vote));
          }
        }
      }
    }
  }

 private:
  std::mutex mutex_;

  raft::raft_leader_election this_node_;
  std::unordered_set<uint64_t> cluster_;

  bool exiting_;
  uint64_t id_;
  utils::thread_safe_queue<std::pair<uint64_t, raft::protocol::Heartbeat_Request>>
      heartbeat_requests_;
  utils::thread_safe_queue<raft::protocol::Heartbeat_Response>
      heartbeat_responses_;
  utils::thread_safe_queue<std::pair<uint64_t, raft::protocol::RequestVote_Request>>
      request_vote_requests_;
  utils::thread_safe_queue<raft::protocol::RequestVote_Response>
      request_vote_responses_;

  std::thread ticker_thread_;
  std::thread step_thread_;
  std::thread ready_thread_;
};

TEST(RaftLeaderElection, Run) {
  for (int i = 0; i < 3; ++i) {
    cluster[i] = new RaftLeaderElectionTest(i, 3);
  }
  for (int i = 0; i < 3; ++i) {
    cluster[i]->run();
  }
  sleep(30);
  for (int i = 0; i < 3; ++i) {
    cluster[i]->stop();
  }
  sleep(1);
  for (int i = 0; i < 3; ++i) {
    delete cluster[i];
  }
}

} // namespace