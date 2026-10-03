#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>

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

    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

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

    const char request[] =
    "GET / HTTP/1.0\r\n"
    "Host: google.com\r\n"
    "Connection: close\r\n"
    "\r\n";

    if (send(sock, request, strlen(request), 0) < 0) {
        std::cerr << "failed to send request: "
        << strerror(errno) << "\n";
        close(sock);
        return 1;
    }

    std::string response;

    char buffer[4096];

    while (true) {
        ssize_t received = recv(
            sock,
            buffer,
            sizeof(buffer),
                                0
        );

        if (received < 0) {
            std::cerr << "receive failed: "
            << strerror(errno) << "\n";
            close(sock);
            return 1;
        }

        if (received == 0) {
            break;
        }

        response.append(buffer, received);
    }

    close(sock);

    size_t header_end = response.find("\r\n\r\n");

    if (header_end == std::string::npos) {
        std::cerr << "invalid http response\n";
        return 1;
    }

    std::string headers = response.substr(0, header_end);
    std::string body = response.substr(header_end + 4);

    size_t first_line_end = headers.find("\r\n");

    if (first_line_end == std::string::npos) {
        std::cerr << "invalid http status line\n";
        return 1;
    }

    std::string status = headers.substr(0, first_line_end);

    std::cout << "\nstatus\n";
    std::cout << status << "\n";

    std::cout << "\nheaders\n";
    std::cout << headers.substr(first_line_end + 2) << "\n";

    std::cout << "\nbody\n";
    std::cout << body << "\n";

    return 0;
}
