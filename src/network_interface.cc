#include <iostream>

#include "arp_message.hh"
#include "exception.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  uint32_t tgt_ip = next_hop.ipv4_numeric();
  auto place = mapping_cache.find( tgt_ip );
  if ( place == mapping_cache.end() ) {
    waiting_arp_list[tgt_ip].emplace_back( dgram );
    if ( arp_request_list.count( tgt_ip ) == 0 ) {
      arp_request_list[tgt_ip] = 0;
      EthernetHeader header = { ETHERNET_BROADCAST, ethernet_address_, EthernetHeader::TYPE_ARP };
      ARPMessage arp_msg = { .opcode = ARPMessage::OPCODE_REQUEST,
                             .sender_ethernet_address = ethernet_address_,
                             .sender_ip_address = ip_address_.ipv4_numeric(),
                             .target_ip_address = tgt_ip };
      vector<string> payload = serialize( arp_msg );
      EthernetFrame frame = { header, payload };
      transmit( frame );
    }
  } else {
    EthernetHeader header = { place->second.first, ethernet_address_, EthernetHeader::TYPE_IPv4 };
    vector<string> payload = serialize( dgram );
    EthernetFrame frame = { header, payload };
    transmit( frame );
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if ( frame.header.dst != ETHERNET_BROADCAST && frame.header.dst != ethernet_address_ )
    return;

  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram ip_dgram;
    if ( !parse( ip_dgram, frame.payload ) )
      return;

    datagrams_received_.emplace( move( ip_dgram ) );
  } else if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage arp_msg;
    if ( !parse( arp_msg, frame.payload ) )
      return;

    mapping_cache[arp_msg.sender_ip_address] = make_pair( arp_msg.sender_ethernet_address, 0 );
    if ( arp_msg.opcode == ARPMessage::OPCODE_REQUEST ) {
      if ( arp_msg.target_ip_address == ip_address_.ipv4_numeric() ) {
        EthernetHeader header = { arp_msg.sender_ethernet_address, ethernet_address_, EthernetHeader::TYPE_ARP };
        ARPMessage temp_arp_msg = { .opcode = ARPMessage::OPCODE_REPLY,
                                    .sender_ethernet_address = ethernet_address_,
                                    .sender_ip_address = ip_address_.ipv4_numeric(),
                                    .target_ethernet_address = arp_msg.sender_ethernet_address,
                                    .target_ip_address = arp_msg.sender_ip_address };
        vector<string> payload = serialize( temp_arp_msg );
        EthernetFrame tempframe = { header, payload };
        transmit( tempframe );
      }
    } else if ( arp_msg.opcode == ARPMessage::OPCODE_REPLY ) {
      vector<InternetDatagram> waiting_list = waiting_arp_list[arp_msg.sender_ip_address];
      for ( auto datagram : waiting_list ) {
        EthernetHeader header = { arp_msg.sender_ethernet_address, ethernet_address_, EthernetHeader::TYPE_IPv4 };
        vector<string> payload = serialize( datagram );
        EthernetFrame tempframe = { header, payload };
        transmit( tempframe );
      }
      waiting_arp_list.erase( arp_msg.sender_ip_address );
    } else
      return;
  } else
    return;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  for ( auto iter = mapping_cache.begin(); iter != mapping_cache.end(); ) {
    iter->second.second += ms_since_last_tick;
    if ( iter->second.second > 30000 ) {
      iter = mapping_cache.erase( iter );
    } else
      iter++;
  }

  for ( auto iter = arp_request_list.begin(); iter != arp_request_list.end(); ) {
    iter->second += ms_since_last_tick;
    if ( iter->second > 5000 ) {
      iter = arp_request_list.erase( iter );
    } else
      iter++;
  }
}
