#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timer(retx_timeout) {}

size_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    uint16_t window_size = max(_window_size, static_cast<uint16_t>(1));
    while (_bytes_in_flight < window_size) {
        TCPSegment seg;
        // 首先发 SYN 包，不含 payload（因为初始时 window_size 为 1）
        if (!_set_syn_flag) {
            seg.header().syn = true;
            _set_syn_flag = true;
        }

        // MAX_PAYLOAD_SIZE 只限制字符串长度并不包括 SYN 和 FIN，但是 window_size 包括 SYN 和 FIN
        auto payload_size = min(TCPConfig::MAX_PAYLOAD_SIZE, \
                            min(window_size - _bytes_in_flight - seg.header().syn, _stream.buffer_size()));
        auto payload = _stream.read(payload_size);
        seg.payload() = Buffer(move(payload));

        // 如果读到 EOF 了且 window_size 还有空位
        if (!_set_fin_flag && _stream.eof() && _bytes_in_flight + seg.length_in_sequence_space() < window_size) {
            seg.header().fin = true;
            _set_fin_flag = true;
        }

        // 空数据报就不发送了
        uint64_t length;
        if ((length = seg.length_in_sequence_space()) == 0) break;

        // 发送
        seg.header().seqno = next_seqno(); // next_seqno() 是 TCP seqno
        _segments_out.push(seg);

        // 如果定时器关闭，则启动定时器
        if (!_timer.is_running()) _timer.restart();

        // 保存备份，重发时可能会用
        _outstanding_seg.emplace(_next_seqno, move(seg));
        
        // 更新序列号和发出但未 ACK 的字节数
        _next_seqno += length; // _next_seqno 是 absolute seqno
        _bytes_in_flight += length;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    auto abs_ackno = unwrap(ackno, _isn, next_seqno_absolute());
    if (abs_ackno > next_seqno_absolute()) return; // 传入的 ACK 是不可靠的，直接丢弃
    int is_successful = 0;

    // 处理已经收到的包（序列号空间要小于 ACK）
    while (!_outstanding_seg.empty()) {
        auto &[abs_seq, seg] = _outstanding_seg.front();
        if (abs_seq + seg.length_in_sequence_space() - 1 < abs_ackno) {
            is_successful = 1;
            _bytes_in_flight -= seg.length_in_sequence_space();
            _outstanding_seg.pop();
        } else {
            break;
        } 
    }

    // 有成功 ACK 的包，则重置定时器，清零连续重传次数
    if (is_successful) {
        _consecutive_retransmissions_count = 0;
        _timer.set_time_out(_initial_retransmission_timeout);
        _timer.restart();
    }

    // 没有等待 ACK 的包了，则关闭定时器
    if (_bytes_in_flight == 0) {
        _timer.stop();
    }

    // 更新 window_size，并尝试填满窗口
    _window_size = window_size;
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer.tick(ms_since_last_tick);

    // 定时器超时（已经确保定时器已经打开），如果定时器关闭不会超时检查不会返回 true
    // 理论上不用检测 _outstanding_seg 非空，但为了鲁棒性就检测下吧
    if (_timer.check_time_out() && !_outstanding_seg.empty()) {
        // 重传最早的报文
        _segments_out.push(_outstanding_seg.front().second);

        // window_size 非 0 对应的操作
        if (_window_size > 0) {
            ++_consecutive_retransmissions_count;
            _timer.set_time_out(_timer.get_time_out() * 2);
        }
        
        // 重启定时器
        _timer.restart();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions_count; }

void TCPSender::send_empty_segment() {
    // 发送空数据报，可以用于仅仅 ACK
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.emplace(move(seg));
}
