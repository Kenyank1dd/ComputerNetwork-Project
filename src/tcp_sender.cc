#include "tcp_sender.hh"
#include "iostream"
#include "tcp_config.hh"

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return num_flight;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return num_retrans;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  if ( FIN_sent )
    return;
  const size_t wnd_size = window_size == 0 ? 1 : window_size;
  while ( wnd_size > num_flight ) {
    TCPSenderMessage msg = make_empty_message();
    if ( !SYN_sent ) {
      msg.SYN = true;
      SYN_sent = true;
    }
    while ( reader().bytes_buffered() > 0 && msg.sequence_length() < wnd_size - num_flight
            && msg.payload.size() < TCPConfig::MAX_PAYLOAD_SIZE ) {
      string_view bytes_view = reader().peek();
      const uint64_t available_size = std::min( TCPConfig::MAX_PAYLOAD_SIZE - msg.payload.size(),
                                                wnd_size - ( num_flight + msg.sequence_length() ) );
      if ( bytes_view.size() > available_size )
        bytes_view = bytes_view.substr( 0, available_size );
      msg.payload.append( bytes_view );
      input_.reader().pop( bytes_view.size() );
    }

    if ( !FIN_sent && reader().is_finished() && msg.sequence_length() < wnd_size - num_flight ) {
      msg.FIN = true;
      FIN_sent = true;
    }

    if ( msg.sequence_length() == 0 )
      break;

    nxt_seqno += msg.sequence_length();
    num_flight += msg.sequence_length();
    transmit( msg );
    msg_queue.emplace( std::move( msg ) );

    if ( !timer.is_active() )
      timer.start();
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  return TCPSenderMessage { Wrap32::wrap( nxt_seqno, isn_ ), false, {}, false, input_.has_error() };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( msg.RST ) {
    input_.set_error();
    return;
  }
  window_size = msg.window_size;
  if ( !msg.ackno.has_value() )
    return;
  const uint64_t expect_seqno = msg.ackno->unwrap( isn_, nxt_seqno );

  if ( expect_seqno > nxt_seqno )
    return;
  bool ack_flag = false;
  while ( !msg_queue.empty() ) {
    TCPSenderMessage curmsg = msg_queue.front();
    if ( expect_seqno <= ack_seqno || expect_seqno < ack_seqno + curmsg.sequence_length() )
      break;

    ack_flag = true;
    num_flight -= curmsg.sequence_length();
    ack_seqno += curmsg.sequence_length();
    msg_queue.pop();
  }

  if ( ack_flag ) {
    num_retrans = 0;
    timer = RetransmissionTimer( initial_RTO_ms_ );
    if ( !msg_queue.empty() )
      timer.start();
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if ( timer.tick( ms_since_last_tick ).is_expired() ) {
    transmit( msg_queue.front() );
    if ( window_size != 0 ) {
      num_retrans++;
      timer.exponential_backoff();
    }
    timer.reset();
  }
}
