#include "raft_leader_election.hh"

#include <boost/log/trivial.hpp>

#include <random>

namespace raft {

raft_leader_election::raft_leader_election(
    uint64_t id,
    size_t cluster_size,
    size_t election_timeout,
    size_t heartbeat_timeout)
  : id_(id),
    current_term_(0),
    cluster_size_(cluster_size),
    election_timeout_(election_timeout),
    heartbeat_timeout_(heartbeat_timeout) {
  std::unique_lock<std::mutex> lock(mutex_);
  convert_to_follower(current_term_, INVALID_UINT64);
}

raft_leader_election::~raft_leader_election() {
}

raft_debug_state_t
raft_leader_election::get_debug_state() const {
  std::unique_lock<std::mutex> lock(mutex_);
  return raft_debug_state_t(current_term_, state_);
}

void
raft_leader_election::tick() {
  switch (state_) {
    case raft_state_t::FOLLOWER:
      ++election_elapsed_;
      if (election_elapsed_ >= randomized_election_timeout_) {
        convert_to_candidate();
      }      
      break;
    case raft_state_t::CANDIDATE:
      ++election_elapsed_;
      if (election_elapsed_ >= randomized_election_timeout_) {
        start_new_election();
      }
      break;
    case raft_state_t::LEADER:
      ++heartbeat_elapsed_;
      if (heartbeat_elapsed_ >= heartbeat_timeout_) {
        broadcast_heartbeat();
      }
      break;
  }
}

size_t random_intn(size_t n) {
  static std::mt19937 generator(std::random_device{}());
  return generator() % n;
}

void
raft_leader_election::reset_randomized_election_timeout() {
  randomized_election_timeout_ = election_timeout_ + random_intn(election_timeout_);
}

void
raft_leader_election::start_new_election() {
  election_elapsed_ = 0;
  reset_randomized_election_timeout();
  broadcast_request_vote();
}

void
raft_leader_election::convert_to_follower(
    uint64_t term,
    uint64_t vote_for) {
  current_term_ = term;
  voted_for_ = vote_for;
  election_elapsed_ = 0;
  reset_randomized_election_timeout();
  state_ = raft_state_t::FOLLOWER;
#if DEBUG
  BOOST_LOG_TRIVIAL(debug) << id_ << " became follower at term " << current_term_
                                  << ", with election timeout: " << randomized_election_timeout_;
#endif
}

void
raft_leader_election::convert_to_candidate() {
  ++current_term_;
  voted_for_ = id_;
  vote_collected_ = 1;
  state_ = raft_state_t::CANDIDATE;
#if DEBUG
  BOOST_LOG_TRIVIAL(debug) << id_ << " became candidate at term " << current_term_;
#endif
  start_new_election();
}

void
raft_leader_election::convert_to_leader() {
  state_ = raft_state_t::LEADER;
#if DEBUG
  BOOST_LOG_TRIVIAL(debug) << id_ << " became leader at term " << current_term_;
#endif
  broadcast_heartbeat();
}

void
raft_leader_election::broadcast_heartbeat() {
  heartbeat_elapsed_ = 0;
  protocol::Heartbeat_Request request;
  request.set_term(current_term_);
  request.set_leader_id(id_);

  heartbeat_messages_.push(request);
#if DEBUG
  BOOST_LOG_TRIVIAL(debug) << id_ << " bcast heartbeat at term " << current_term_;
#endif
}

void
raft_leader_election::handle_heartbeat(
    const protocol::Heartbeat_Request &request,
    protocol::Heartbeat_Response &response) {
  response.set_term(current_term_);
  response.set_success(true);
  if (current_term_ <= request.term()) {
    convert_to_follower(request.term(), INVALID_UINT64);
  } else if (current_term_ > request.term()) {
    response.set_success(false);
  }
#if DEBUG
  BOOST_LOG_TRIVIAL(debug) << id_ << " response heartbeat with term: " 
                                  << response.term()
                                  << ", and success: "
                                  << response.success();
#endif
}

void
raft_leader_election::heartbeat_callback(
    const protocol::Heartbeat_Response &response) {
  if (current_term_ < response.term()) {
    convert_to_follower(response.term(), INVALID_UINT64);
  }
}

void
raft_leader_election::broadcast_request_vote() {
  protocol::RequestVote_Request request;
  request.set_term(current_term_);
  request.set_candidate_id(id_);

  request_vote_messages_.push(request);
#if DEBUG
  BOOST_LOG_TRIVIAL(debug) << id_ << " bcast request vote at term " << current_term_;
#endif
}

void
raft_leader_election::handle_request_vote(
    const protocol::RequestVote_Request &request,
    protocol::RequestVote_Response &response) {
  response.set_term(current_term_);
  response.set_vote_granted(false);
  if (current_term_ < request.term()) {
    convert_to_follower(request.term(), request.candidate_id());
    response.set_term(request.term());
    response.set_vote_granted(true);
  } else if (current_term_ == request.term()) {
      if (state_ == raft_state_t::FOLLOWER && voted_for_ == INVALID_UINT64) {
        voted_for_ == request.candidate_id();
        response.set_vote_granted(true);
      }
  }
#if DEBUG
  BOOST_LOG_TRIVIAL(debug) << id_ << " response request vote with term: " 
                                  << response.term()
                                  << ", and granted: "
                                  << response.vote_granted();
#endif
}

void
raft_leader_election::request_vote_callback(
    const protocol::RequestVote_Response &response) {
  if (current_term_ < response.term()) {
    convert_to_follower(response.term(), INVALID_UINT64);
  } else if (current_term_ == response.term()) {
    if (state_ == raft_state_t::CANDIDATE && response.vote_granted()) {
      ++vote_collected_;
#if DEBUG
      BOOST_LOG_TRIVIAL(debug) << id_ << " receive vote granted, collected: "
                                      << vote_collected_;
#endif
      if (vote_collected_ > cluster_size_ / 2) {
        convert_to_leader();
      }
    }
  }
}

void
raft_leader_election::step_heartbeat_request(
    const protocol::Heartbeat_Request &request,
    protocol::Heartbeat_Response &response) {
  std::unique_lock<std::mutex> lock(mutex_);
  handle_heartbeat(request, response);
}

void
raft_leader_election::step_heartbeat_response(
    const protocol::Heartbeat_Response &response) {
  std::unique_lock<std::mutex> lock(mutex_);
  heartbeat_callback(response);
}

void
raft_leader_election::step_request_vote_request(
    const protocol::RequestVote_Request &request,
    protocol::RequestVote_Response &response) {
  std::unique_lock<std::mutex> lock(mutex_);
  handle_request_vote(request, response);
}

void
raft_leader_election::step_request_vote_response(
    const protocol::RequestVote_Response &response) {
  std::unique_lock<std::mutex> lock(mutex_);
  request_vote_callback(response);
}

raft_ready_t
raft_leader_election::ready() {
  std::unique_lock<std::mutex> lock(mutex_);
  raft_ready_t ready;
  ready.heartbeat_messages = std::move(heartbeat_messages_);
  ready.request_vote_messages = std::move(request_vote_messages_);
  return ready;
}

} // namespace raft