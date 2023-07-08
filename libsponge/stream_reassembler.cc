#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity), 
                                                              _stream(), _cur_index(0),
                                                              _eof_index(std::numeric_limits<size_t>::max()), 
                                                              _unassembled_bytes_cnt(0) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    auto st = max(index, _cur_index);
    auto ed = min(index + data.size(), min(_cur_index + _capacity - _output.buffer_size(), _eof_index));
    if (eof) _eof_index = min(_eof_index, index + data.size());
    for (size_t i = st, j = st - index; i < ed; ++i, ++j) {
        auto &t = _stream[i];
        if (t.second == true) {
            if (t.first != data[j])
                throw runtime_error("StreamReassembler::push_substring: Inconsistent substrings!");
        } else {
            t = make_pair(data[j], true);
            ++_unassembled_bytes_cnt;
        }
    }
    string str;
    auto old = _cur_index;
    pair<char, bool> t;
    while (_cur_index < _eof_index && (t = _stream[_cur_index]).second == true) {
        str.push_back(t.first);
        --_unassembled_bytes_cnt, ++_cur_index;
    }
    _output.write(str);
    _stream.erase(_stream.find(old), _stream.find(_cur_index));
    if (_cur_index == _eof_index) _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes_cnt; }

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }
