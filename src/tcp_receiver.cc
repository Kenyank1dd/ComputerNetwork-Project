#include "tcp_receiver.hh"
#include<iostream>
#include<algorithm>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if(message.RST) reassembler_.reader().set_error();
  if((!message.SYN && !ISN.has_value()) || (ISN.has_value() && ISN == message.seqno)){
    return;
  }
  if(!ISN.has_value()) {
    ISN = message.seqno;
  }
  
  uint64_t checkpoint = reassembler_.writer().bytes_pushed() + 1;
  uint64_t abs_seqno = message.seqno.unwrap(*ISN, checkpoint);
  // cout << abs_seqno << endl;
  uint64_t first_idnex = abs_seqno == 0 ? abs_seqno : abs_seqno - 1;
  // cout << first_idnex << message.payload << endl;
  reassembler_.insert(first_idnex, message.payload, message.FIN);

}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage msg{};
  msg.window_size = min((uint64_t)UINT16_MAX, reassembler_.writer().available_capacity());

  uint64_t abs_seqno = reassembler_.writer().bytes_pushed() + ISN.has_value() + reassembler_.writer().is_closed();
  // cout << abs_seqno << endl;
  if(ISN.has_value()) {
    msg.ackno = Wrap32::wrap(abs_seqno, *ISN);
  }
  msg.RST = reassembler_.writer().has_error();

  return msg;
}
