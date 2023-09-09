#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _capacity(capacity), _written_cnt(0),
                                                _read_cnt(0) {}

size_t ByteStream::write(const string &data) {
    auto ret = min(data.size(), remaining_capacity());
    _buffer.push_back(data.substr(0, ret));
    _written_cnt += ret;
    return ret;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string ret;
    auto n = min(len, buffer_size());
    for (const auto &buffer : _buffer) {
        if (n >= buffer.size()) {
            n -= buffer.size();
            ret += buffer.str();
        } else {
            ret += buffer.str().substr(0, n);
            break;
        }
    }
    return ret;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    auto n = min(len, buffer_size());
    _read_cnt += n;
    while (n > 0) {
        if (n >= _buffer.front().size()) {
            n -= _buffer.front().size();
            _buffer.pop_front();
        } else {
            _buffer.front().remove_prefix(n);
            break;
        }
    }
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

size_t ByteStream::buffer_size() const { return _written_cnt - _read_cnt; }

bool ByteStream::buffer_empty() const { return buffer_size() == 0; }

bool ByteStream::eof() const { return _input_ended_flag && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _written_cnt; }

size_t ByteStream::bytes_read() const { return _read_cnt; }

size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }
