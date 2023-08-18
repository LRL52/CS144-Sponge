#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

void NetworkInterface::_send(const EthernetAddress &dst, const uint16_t type, BufferList &&payload) {
    EthernetFrame eth_frame;
    eth_frame.header().src = _ethernet_address;
    eth_frame.header().dst = dst;
    eth_frame.header().type = type;
    eth_frame.payload() = std::move(payload);
    _frames_out.push(std::move(eth_frame));
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    auto it = _arp_table.find(next_hop_ip);
    if (it != _arp_table.end()) {
        // ARP 表命中，则直接发送 IP 数据报
        _send(it->second.eth_addr, EthernetHeader::TYPE_IPv4, dgram.serialize());
    } else {
        // ARP 表不命中，且最近没有对该 IP 发送过 ARP 查询数据报，则发送查询报文
        if (_waiting_arp_response_ip_addr.find(next_hop_ip) == _waiting_arp_response_ip_addr.end()) {
            ARPMessage arp_msg;
            arp_msg.opcode = ARPMessage::OPCODE_REQUEST;
            arp_msg.sender_ethernet_address = _ethernet_address;
            arp_msg.target_ethernet_address = {};
            arp_msg.sender_ip_address = _ip_address.ipv4_numeric();
            arp_msg.target_ip_address = next_hop_ip;

            _send(ETHERNET_BROADCAST, EthernetHeader::TYPE_ARP, arp_msg.serialize());

            // 加入等待回复 ARP 报文的记录表中
            _waiting_arp_response_ip_addr[next_hop_ip] = ARP_RESPONSE_TTL_MS;
        }
        // 加入 IP 报文等待发送列表
        _waiting_internet_datagrams[next_hop_ip].emplace_back(next_hop, dgram);
    }

}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    // 如果不是广播帧或者 MAC 地址不是本机，则直接过滤
    if (frame.header().dst != ETHERNET_BROADCAST && frame.header().dst != _ethernet_address) {
        return nullopt;
    }
    // 如果是 IPv4 数据报，则把解析后把数据返回
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram ret;
        // 解析成功则返回
        if (ret.parse(frame.payload()) == ParseResult::NoError) return ret;
        return nullopt;
    }
    // 如果是 ARP 数据报，则尝试从中学习到 IP-MAC 映射关系（我们可以同时从 ARP 请求和响应包中获取到新的 ARP 表项）
    if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp_msg;
        if (arp_msg.parse(frame.payload()) != ParseResult::NoError) return nullopt;

        const uint32_t my_ip = _ip_address.ipv4_numeric();
        const uint32_t src_ip = arp_msg.sender_ip_address;
        // 如果是发给本机的 ARP 请求
        if (arp_msg.opcode == ARPMessage::OPCODE_REQUEST && arp_msg.target_ip_address == my_ip) {
            ARPMessage arp_reply;

            arp_reply.opcode = ARPMessage::OPCODE_REPLY;
            arp_reply.sender_ethernet_address = _ethernet_address;
            arp_reply.target_ethernet_address = arp_msg.sender_ethernet_address;
            arp_reply.sender_ip_address = my_ip;
            arp_reply.target_ip_address = src_ip;

            _send(arp_msg.sender_ethernet_address, EthernetHeader::TYPE_ARP, arp_reply.serialize());
        }
        // 从 ARP 报文中学习新的 ARP 表项（即使不是发给我的也可以学，比如广播但目标 IP 不是本机）
        _arp_table[src_ip] = {arp_msg.sender_ethernet_address,  ARP_ENTRY_TTL_MS};
        
        // 如果该 IP 地址有等待发送的数据报，则全部发送出去
        auto it = _waiting_internet_datagrams.find(src_ip);
        if (it != _waiting_internet_datagrams.end()) {
            for (const auto &[next_hop, dgram] : it->second) {
                // send_datagram(dgram, next_hop);
                _send(arp_msg.sender_ethernet_address, EthernetHeader::TYPE_IPv4, dgram.serialize());
            }
            _waiting_internet_datagrams.erase(it);
        }
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    // 删除 ARP 表中过期条目
    for (auto it = _arp_table.begin(); it != _arp_table.end(); ) {
        if (it->second.ttl <= ms_since_last_tick) {
            it = _arp_table.erase(it);
        } else {
            it->second.ttl -= ms_since_last_tick;
            it = std::next(it);
        }
    }
    // 删除等待 ARP 报文回复超时的记录，不作重发处理
    for (auto it = _waiting_arp_response_ip_addr.begin(); it != _waiting_arp_response_ip_addr.end(); ) {
        if (it->second <= ms_since_last_tick) {
            // 如果还有等待发往该未回复的 IP 地址的数据报，则直接丢弃
            auto it2 = _waiting_internet_datagrams.find(it->first);
            if (it2 != _waiting_internet_datagrams.end())
                _waiting_internet_datagrams.erase(it2);

            it = _waiting_arp_response_ip_addr.erase(it);
        } else {
            it->second -= ms_since_last_tick;
            it = std::next(it);
        }
    }
}
