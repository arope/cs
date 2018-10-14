#ifndef INCLUDE_ROUTER_ROUTER_HPP
#define INCLUDE_ROUTER_ROUTER_HPP

#include <vector>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <router/ARPHeader.hpp>
#include <string>
#include <sys/types.h>

namespace router {
class Router {
  public:
    Router() = default;
    ~Router() = default;

    ARPHeader build_arp_reply(
      struct ether_header *eh,
      struct ether_arp *arp_frame,
      uint8_t destination_mac);
    ARPHeader build_arp_request(
      struct ether_header *eh,
      struct ether_arp *arp_frame,
      uint8_t hop_ip);
    uint8_t get_dest_mac(
      struct ifaddrs *ifaddr,
      struct ifaddrs *tmp,
      uint8_t *arp_tpa,
      int socket);
    uint8_t get_src_mac(
      struct ifaddrs *ifaddr,
      struct ifaddrs *tmp,
      uint8_t if_ip,
      int socket,
      uint8_t destination_mac);

    int Start(std::string routing_table);
};
} // namespace router

#endif