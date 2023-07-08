#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _buffer(capacity + 1), _capacity(capacity), _written_cnt(0),
                                                _read_cnt(0), _head(0), _tail(_capacity) {}

size_t ByteStream::write(const string &data) {
    auto ret = min(data.size(), remaining_capacity());
    for (size_t i = 0; i < ret; ++i) {
        _tail = (_tail + 1) % (_capacity + 1);
        _buffer[_tail] = data[i];
    }
    _written_cnt += ret;
    return ret;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string ret;
    auto n = min(len, buffer_size());
    for (size_t i = 0; i < n; ++i) {
        ret.push_back(_buffer[(_head + i) % (_capacity + 1)]);
    }
    return ret;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    auto n = min(len, buffer_size());
    _head = (_head + n) % (_capacity + 1);
    _read_cnt += n;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
string ByteStream::read(const size_t len) {
    auto ret = peek_output(len);
    pop_output(len);
    return ret;
}

void ByteStream::end_input() { _input_ended_flag = true; }

bool ByteStream::input_ended() const { return _input_ended_flag; }

size_t ByteStream::buffer_size() const { return (_tail - _head + 1 + _capacity + 1) % (_capacity + 1) ; }

bool ByteStream::buffer_empty() const { return buffer_size() == 0; }

bool ByteStream::eof() const { return _input_ended_flag && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _written_cnt; }

size_t ByteStream::bytes_read() const { return _read_cnt; }

size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }
