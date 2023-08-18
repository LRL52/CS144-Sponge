#ifndef SPONGE_LIBSPONGE_NETWORK_INTERFACE_HH
#define SPONGE_LIBSPONGE_NETWORK_INTERFACE_HH

#include "address.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "tcp_over_ip.hh"
#include "tun.hh"

#include <cstddef>
#include <cstdint>
#include <list>
#include <optional>
#include <queue>
#include <unordered_map>

//! \brief A "network interface" that connects IP (the internet layer, or network layer)
//! with Ethernet (the network access layer, or link layer).

//! This module is the lowest layer of a TCP/IP stack
//! (connecting IP with the lower-layer network protocol,
//! e.g. Ethernet). But the same module is also used repeatedly
//! as part of a router: a router generally has many network
//! interfaces, and the router's job is to route Internet datagrams
//! between the different interfaces.

//! The network interface translates datagrams (coming from the
//! "customer," e.g. a TCP/IP stack or router) into Ethernet
//! frames. To fill in the Ethernet destination address, it looks up
//! the Ethernet address of the next IP hop of each datagram, making
//! requests with the [Address Resolution Protocol](\ref rfc::rfc826).
//! In the opposite direction, the network interface accepts Ethernet
//! frames, checks if they are intended for it, and if so, processes
//! the the payload depending on its type. If it's an IPv4 datagram,
//! the network interface passes it up the stack. If it's an ARP
//! request or reply, the network interface processes the frame
//! and learns or replies as necessary.
class NetworkInterface {
  private:
    //! ARP 条目
    struct ARPEntry {
      EthernetAddress eth_addr;
      size_t ttl;
    };

    //! Ethernet (known as hardware, network-access-layer, or link-layer) address of the interface
    EthernetAddress _ethernet_address;

    //! IP (known as internet-layer or network-layer) address of the interface
    Address _ip_address;

    //! outbound queue of Ethernet frames that the NetworkInterface wants sent
    std::queue<EthernetFrame> _frames_out{};

    //! ARP 表
    std::unordered_map<uint32_t, ARPEntry> _arp_table{};

    //! 正在查询的 ARP 报文。如果发送了 ARP 请求后，在过期时间内没有返回响应，则丢弃等待的 IP 报文
    std::unordered_map<uint32_t, size_t> _waiting_arp_response_ip_addr{};

    //! 等待 ARP 报文返回的待处理 IP 报文，每一个 IP 地址映射到一个 IP 报文等待发送列表
    std::unordered_map<uint32_t, std::list<std::pair<Address, InternetDatagram> > > _waiting_internet_datagrams{};

    //! \brief 发送以太网帧
    void _send(const EthernetAddress &dst, const uint16_t type, BufferList &&payload);

  public:
    //! ARP 条目默认过期时间为 30s
    static constexpr uint32_t ARP_ENTRY_TTL_MS = 30 * 1000;

    //! ARP 请求相应默认等待时间为 5s
    static constexpr uint32_t ARP_RESPONSE_TTL_MS = 5 * 1000; 

    //! \brief Construct a network interface with given Ethernet (network-access-layer) and IP (internet-layer) addresses
    NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address);

    //! \brief Access queue of Ethernet frames awaiting transmission
    std::queue<EthernetFrame> &frames_out() { return _frames_out; }

    //! \brief Sends an IPv4 datagram, encapsulated in an Ethernet frame (if it knows the Ethernet destination address).

    //! Will need to use [ARP](\ref rfc::rfc826) to look up the Ethernet destination address for the next hop
    //! ("Sending" is accomplished by pushing the frame onto the frames_out queue.)
    void send_datagram(const InternetDatagram &dgram, const Address &next_hop);

    //! \brief Receives an Ethernet frame and responds appropriately.

    //! If type is IPv4, returns the datagram.
    //! If type is ARP request, learn a mapping from the "sender" fields, and send an ARP reply.
    //! If type is ARP reply, learn a mapping from the "sender" fields.
    std::optional<InternetDatagram> recv_frame(const EthernetFrame &frame);

    //! \brief Called periodically when time elapses
    void tick(const size_t ms_since_last_tick);
};

#endif  // SPONGE_LIBSPONGE_NETWORK_INTERFACE_HH
