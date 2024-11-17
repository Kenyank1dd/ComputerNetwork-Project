#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <queue>

class RetransmissionTimer
{
public:
  RetransmissionTimer( uint64_t initial_RTO_ms ) : RTO( initial_RTO_ms ) {}
  bool is_expired() const { return status_active && consumed_time >= RTO; }
  bool is_active() const { return status_active; }
  void start()
  {
    status_active = true;
    reset();
  }
  void stop()
  {
    status_active = false;
    reset();
  }
  void exponential_backoff() { RTO <<= 1; }
  void reset() { consumed_time = 0; }
  RetransmissionTimer& tick( uint64_t ms_since_last_tick )
  {
    consumed_time += status_active ? ms_since_last_tick : 0;
    return *this;
  }

private:
  uint64_t RTO;
  uint64_t consumed_time {};
  bool status_active {};
};

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms ), timer( initial_RTO_ms )
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
  Writer& writer() { return input_.writer(); }
  const Writer& writer() const { return input_.writer(); }

  // Access input stream reader, but const-only (can't read from outside)
  const Reader& reader() const { return input_.reader(); }

private:
  // Variables initialized in constructor
  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;

  RetransmissionTimer timer;
  uint16_t window_size { 1 };
  uint64_t nxt_seqno {};
  uint64_t ack_seqno {};

  std::queue<TCPSenderMessage> msg_queue {};
  uint64_t num_flight {};
  uint64_t num_retrans {};

  bool SYN_sent {}, FIN_sent {};
};
