#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    // 计时清零
    _time_since_last_segment_received = 0;

    const auto& header =  seg.header();

    // 接收到 RST 包
    if (header.rst) {
        _set_rst_state(false);
        return;
    }

    // 将包交给 TCPReceiver，由于代码足够鲁棒，可以不经过任何过滤
    _receiver.segment_received(seg);

    // 是否需要发送一个不占序列空间的空 ack 包，因为收到任何占序列空间的 TCP 段都需要 ack，或者 keep-alive 也需要空 ack 包
    bool need_empty_ack = seg.length_in_sequence_space() > 0;

    // 如果设置了 ack，交给 TCPSender 处理 ack
    if (header.ack) {
        // 实际上在 ack_received 的时候就已经 fill_window() 了 
        _sender.ack_received(header.ackno, header.win);
        // 发送了新的数据包，可以顺带 ack，那么可以不必再发空 ack 包了
        if (need_empty_ack && !_segments_out.empty())
            need_empty_ack = false;
    }

    // LISTEN 时收到 SYN，进入 FSM 的 SYN RECEIVED 状态
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::CLOSED) {
        connect();
        return;
    }

    // 判断是否为 Passive CLOSE，并进入 FSM 的 CLOSE WAIT 状态
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED) {
        _linger_after_streams_finish = false;
    }

    // Passive CLOSE，判断是否进入 FSM 的 CLOSED 状态
    if (!_linger_after_streams_finish && 
        TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED) {
        _is_active = false;
        return;
    }
    
    // Keep-alive 判断
    if (_receiver.ackno().has_value() && (seg.length_in_sequence_space() == 0)
        && seg.header().seqno == _receiver.ackno().value() - 1) {
        need_empty_ack = true;
    }

    // 发送 empty ack
    if (need_empty_ack) {
        _sender.send_empty_segment();
    }

    // 将待发送的包添加上 ackno 和 window_size 发送出去
    _add_ackno_and_window_to_send();
}

bool TCPConnection::active() const { return _is_active; }

size_t TCPConnection::write(const string &data) {
    auto ret = _sender.stream_in().write(data);
    _sender.fill_window();
    _add_ackno_and_window_to_send();
    return ret;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_since_last_segment_received += ms_since_last_tick;

    // 调用 _sender 的 tick
    _sender.tick(ms_since_last_tick);

    // 连续重传次数超过阈值，发送 RST 包
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        // 清除本应该重发的包
        while (!_sender.segments_out().empty()) _sender.segments_out().pop();
        // 发送 RST 包
        _set_rst_state(true);
        return;
    }

    // 调用 _sender.tick 可能导致有新数据包需要发送
    _add_ackno_and_window_to_send();

    // Active CLOSE，判断是否等待时间完成进入 CLOSED 状态
    if (_linger_after_streams_finish && 
        TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED &&
        _time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
        _is_active = false;
        _linger_after_streams_finish = false;
    }
}

void TCPConnection::end_input_stream() { 
    _sender.stream_in().end_input();
    // 流结束后可能需要发送 FIN
    _sender.fill_window();
    _add_ackno_and_window_to_send();
}

void TCPConnection::connect() {
    // 第一次调用 fill_window() 会发送一个 SYN 数据包
    _sender.fill_window();
    _add_ackno_and_window_to_send();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _set_rst_state(true);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::_set_rst_state(const bool send_rst) {
    if (send_rst) {
        TCPSegment seg;
        seg.header().seqno = _sender.next_seqno();
        seg.header().rst = true;
        // 直接把包送到 _segments_out，不需要再调用 _add_ackno_and_window_to_send 发送出去了
        _segments_out.emplace(std::move(seg));
    }
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _linger_after_streams_finish = false;
    _is_active = false;
}

void TCPConnection::_add_ackno_and_window_to_send() {
    while (!_sender.segments_out().empty()) {
        auto seg = std::move(_sender.segments_out().front());
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
        }
        seg.header().win = min(static_cast<size_t>(numeric_limits<uint16_t>::max()), _receiver.window_size());
        _segments_out.emplace(std::move(seg));
    }
}