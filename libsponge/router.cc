#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    // Your code here.
    _route_table.emplace_back(route_prefix, prefix_length, next_hop, interface_num);
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    // Your code here.
    // 取出 IP 字段，在路由表中进行最长前缀匹配
    auto ip = dgram.header().dst;
    auto best_match_it = _route_table.end();
    for (auto it = _route_table.begin(); it != _route_table.end(); it = std::next(it)) {
        // 前缀匹配成功
        //! NOTE: 根据 CSAPP，在 32 位整数下，右移量只取低 5 位（即 mod 32），因此右移 32 位等价于右移 0 位，因此这里需要特判右移 32 位的情况 
        // cerr << "DEBUG route table: " << ((it->route_prefix ^ ip) >> (32 - it->prefix_length)) << '\n';
        if (it->prefix_length == 0 ||  (it->route_prefix ^ ip) >> (32 - it->prefix_length) == 0) {
            if (best_match_it == _route_table.end() || it->prefix_length > best_match_it->prefix_length) {
                best_match_it = it;
            }
        }
    }
    // 匹配到路由规则并且 TTL 大于 1
    if (best_match_it != _route_table.end() && dgram.header().ttl > 1) {
        --dgram.header().ttl;
        auto &next_interface = interface(best_match_it->interface_num);
        // 如果路由器直接连接到相关网络，则下一跳就是目的 IP 地址，否则为下一跳路由器的 IP 地址
        if (best_match_it->next_hop.has_value()) {
            next_interface.send_datagram(dgram, best_match_it->next_hop.value());
        } else {
            next_interface.send_datagram(dgram, Address::from_ipv4_numeric(ip));
        }
    }
    // 其余情况数据报则直接丢弃，也不作 ICMP 回复
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
