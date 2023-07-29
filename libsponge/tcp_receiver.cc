#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const auto &header = seg.header();
    if (!_isn.has_value()) {
        if (!header.syn) return; // SYN 之前的包都丢弃
        _isn = header.seqno; // 建立连接的 seqno 即是 ISN，也是 SYN 对应的 seqno
    }
    uint64_t checkpoint = _reassembler.stream_out().bytes_written();
    uint64_t abs_seq = unwrap(header.seqno, _isn.value(), checkpoint);
    uint64_t stream_index = abs_seq - 1 + (header.syn ? 1 : 0);
    _reassembler.push_substring(seg.payload().copy(), stream_index, header.fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_isn.has_value()) return nullopt;
    uint64_t abs_seq = _reassembler.stream_out().bytes_written() + 1 + \
                    (_reassembler.stream_out().input_ended() ? 1 : 0);
    return wrap(abs_seq, _isn.value());
}

size_t TCPReceiver::window_size() const {
    return _capacity - _reassembler.stream_out().buffer_size();
}
