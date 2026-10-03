#include <iostream>
#include <string>
#include <cstring>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

int main(int argc, char* argv[]) {
    std::string targetip = (argc > 1) ? argv[1] : "173.194.219.100";

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0) {
        std::cerr << "socket creation failed\n";
        return 1;
    }

    timeval timeout{};

    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        std::cerr << "failed to set timeout\n";
        close(sock);
        return 1;
    }

    sockaddr_in server{};

    server.sin_family = AF_INET;
    server.sin_port = htons(80);

    if (inet_pton(AF_INET, targetip.c_str(), &server.sin_addr) <= 0) {
        std::cerr << "invalid ip address\n";
        close(sock);
        return 1;
    }

    std::cout << "connecting to " << targetip << ":80\n";

    if (connect(
        sock,
        reinterpret_cast<sockaddr*>(&server),
                sizeof(server)
    ) < 0) {
        std::cerr << "connection failed: "
        << strerror(errno) << "\n";
        close(sock);
        return 1;
    }

    std::cout << "connected\n";

    close(sock);

    return 0;
}
