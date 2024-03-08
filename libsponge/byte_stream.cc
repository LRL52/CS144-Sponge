#include "byte_stream.hh"
#include <vector>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : buffer(capacity+1), _capacity(capacity) {}

size_t ByteStream::write(const string &data) {
    // 如果关闭了管道，那就不能够再次写入
    /* 这里注意：之前写代码出现一个bug就是因为不符合判断条件时候返回-1，这是不可以的，
    因为这个函数的返回值是指写入了几个字节，返回-1是错误，不符合条件就是没有写入，应该返回0*/
    if (_input_end) {
        return 0;
        // return -1;
    }
    if ((nwrite + 1) % (_capacity+1) == nread) {
        // 说明缓冲区已满，这时候应该要sleep该进程/线程，这里我们就简单的返回就行
        return 0;
        // return -1;
    }
    auto remain_buffer = remaining_capacity();
    unsigned int n = min(remain_buffer, data.length());
    for (unsigned int a = 0; a < n; a++) {
        buffer[nwrite] = data[a];
        nwrite = (nwrite + 1) % (_capacity+1);
    }
    total_write += n;
    return n;
}

//! \param[in] len bytes will be copied from the output side of the buffer
std::string ByteStream::peek_output(const size_t len) const {
    string output = "";
    auto numReaded = buffer_size();
    unsigned int n = min(len, numReaded);
    for (unsigned int a = 0; a < n; a++) {
        output += buffer[(nread + a) % (_capacity+1)];
    }
    return output;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    auto numReaded = buffer_size();
    unsigned int n = min(len, numReaded);
    nread = (nread + n) % (_capacity+1);
    total_read += n;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string output = peek_output(len);
    pop_output(len);
    return output;
}

void ByteStream::end_input() { _input_end = true; }

bool ByteStream::input_ended() const { return _input_end == true; }

size_t ByteStream::buffer_size() const { return total_write - total_read; }

bool ByteStream::buffer_empty() const { return nwrite == nread; }

bool ByteStream::eof() const { return _input_end == true && nread == nwrite ? true : false; }

size_t ByteStream::bytes_written() const { return total_write; }

size_t ByteStream::bytes_read() const { return total_read; }

// Returns the number of additional bytes that the stream has space for
size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }
