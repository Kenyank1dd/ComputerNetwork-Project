#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  auto new_route = std::make_tuple(route_prefix, prefix_length, interface_num, next_hop);

  auto it = RouterTable.begin();
  while (it != RouterTable.end()) {
      if (prefix_length >= std::get<1>(*it)) {
          break; 
      }
      ++it;
  }
  
  RouterTable.insert(it, new_route);
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  for (auto& interface : _interfaces) {
    auto& rcv_dgrams = interface->datagrams_received();
    while (!rcv_dgrams.empty()) {
      auto& dgram = rcv_dgrams.front();
      const auto& dgram_dst = dgram.header.dst;
      std::tuple<uint32_t, uint8_t, size_t, std::optional<Address>>* route = nullptr;

      for (auto& item : RouterTable) {
        uint32_t prefix = std::get<0>(item);
        uint8_t prefix_length = std::get<1>(item);
        uint32_t mask = ~(UINT32_MAX >> (prefix_length));
        if ((dgram_dst & mask) == (prefix & mask)) {
          route = &item;
          break;
        }
      }

      if (route == nullptr || dgram.header.ttl <= 1) {
        rcv_dgrams.pop();
        continue; 
      }

      dgram.header.ttl--;
      dgram.header.compute_checksum();
      size_t interface_num = std::get<2>(*route);
      std::optional<Address> next_hop = std::get<3>(*route);

      auto& target_interface = _interfaces[interface_num];
      if (next_hop.has_value()) {
        target_interface->send_datagram(dgram, *next_hop);
      } else {
        target_interface->send_datagram(dgram, Address::from_ipv4_numeric(dgram_dst));
      }
      rcv_dgrams.pop();
    }
  }
}
