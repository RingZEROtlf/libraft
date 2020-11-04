#pragma once

#include "protocol/raft.pb.h"

#include <cstdint>
#include <limits>
#include <queue>
#include <mutex>

namespace raft {

constexpr uint64_t INVALID_UINT64 = std::numeric_limits<uint64_t>::max();

enum class raft_state_t {
  FOLLOWER,
  LEADER,
  CANDIDATE,
};

struct raft_ready_t {
  std::queue<protocol::Heartbeat_Request> heartbeat_messages;
  std::queue<protocol::RequestVote_Request> request_vote_messages;
};

struct raft_debug_state_t {
  uint64_t current_term;
  bool is_leader;

  raft_debug_state_t(uint64_t current_term, raft_state_t state)
    : current_term(current_term),
      is_leader(state == raft_state_t::LEADER) {
  }
};

class raft_leader_election {
 public:
  explicit raft_leader_election(
      uint64_t id,
      size_t cluster_size,
      size_t election_timeout, 
      size_t heartbeat_timeout);
  ~raft_leader_election();

 private:
  raft_leader_election(raft_leader_election &other) = delete;
  raft_leader_election& operator=(raft_leader_election &other) = delete;

 public:
  raft_debug_state_t get_debug_state() const;

 public:
  void tick();

 private:
  void reset_randomized_election_timeout();
  void start_new_election();

  void convert_to_follower(
      uint64_t term,
      uint64_t vote_for);
  void convert_to_candidate();
  void convert_to_leader();

  void broadcast_heartbeat();
  void handle_heartbeat(
      const protocol::Heartbeat_Request &request,
      protocol::Heartbeat_Response &response);
  void heartbeat_callback(
      const protocol::Heartbeat_Response &response);

  void broadcast_request_vote();
  void handle_request_vote(
      const protocol::RequestVote_Request &request,
      protocol::RequestVote_Response &response);
  void request_vote_callback(
      const protocol::RequestVote_Response &response);

 public:
  void step_heartbeat_request(
      const protocol::Heartbeat_Request &request,
      protocol::Heartbeat_Response &response);
  void step_heartbeat_response(
      const protocol::Heartbeat_Response &response);

  void step_request_vote_request(
      const protocol::RequestVote_Request &request,
      protocol::RequestVote_Response &response);
  void step_request_vote_response(
      const protocol::RequestVote_Response &response);

  raft_ready_t ready();

 private:
  mutable std::mutex mutex_;

  uint64_t id_;

  uint64_t current_term_;
  uint64_t voted_for_;
  uint64_t vote_collected_;

  raft_state_t state_;

  size_t cluster_size_;

  size_t election_timeout_;
  size_t randomized_election_timeout_;
  size_t election_elapsed_;

  size_t heartbeat_timeout_;
  size_t heartbeat_elapsed_;

  std::queue<protocol::Heartbeat_Request> heartbeat_messages_;
  std::queue<protocol::RequestVote_Request> request_vote_messages_;
};

} // namespace raft