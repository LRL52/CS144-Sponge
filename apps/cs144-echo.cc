#include "address.hh"
#include "tcp_sponge_socket.hh"
#include "util.hh"
#include <cstdlib>
#include <iostream>
#include <string>

using namespace std;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <ip_address> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    FullStackSocket socket;
    
    socket.connect(Address(argv[1], stoi(argv[2])));

    string input;
    while (getline(cin, input)) {
        input += '\n';
        socket.write(input);
        cout << "[REPLY] " << socket.read();
    }

    socket.wait_until_closed();
    return 0;
}