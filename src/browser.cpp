#include <iostream>
#include <string>
#include <cstring>
#include <cstdint>
#include <vector>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

std::vector<std::string> resolve(const std::string& hostname) {
    std::vector<std::string> addresses;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock < 0) {
        return addresses;
    }

    timeval timeout{};

    timeout.tv_sec = 3;
    timeout.tv_usec = 0;

    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    sockaddr_in dns{};

    dns.sin_family = AF_INET;
    dns.sin_port = htons(53);

    inet_pton(AF_INET, "8.8.8.8", &dns.sin_addr);

    uint8_t packet[512]{};

    uint16_t id = static_cast<uint16_t>(getpid() & 0xffff);

    packet[0] = id >> 8;
    packet[1] = id & 0xff;

    packet[2] = 0x01;
    packet[5] = 0x01;

    int offset = 12;

    size_t start = 0;

    while (start < hostname.size()) {
        size_t end = hostname.find('.', start);

        if (end == std::string::npos) {
            end = hostname.size();
        }

        size_t length = end - start;

        packet[offset++] = static_cast<uint8_t>(length);

        for (size_t i = start; i < end; i++) {
            packet[offset++] = hostname[i];
        }

        start = end + 1;
    }

    packet[offset++] = 0;

    packet[offset++] = 0;
    packet[offset++] = 1;

    packet[offset++] = 0;
    packet[offset++] = 1;

    if (sendto(
        sock,
        packet,
        offset,
        0,
        reinterpret_cast<sockaddr*>(&dns),
               sizeof(dns)
    ) < 0) {
        close(sock);
        return addresses;
    }

    uint8_t response[512]{};

    sockaddr_in from{};
    socklen_t fromlen = sizeof(from);

    int received = recvfrom(
        sock,
        response,
        sizeof(response),
                            0,
                            reinterpret_cast<sockaddr*>(&from),
                            &fromlen
    );

    close(sock);

    if (received < 12) {
        return addresses;
    }

    int answers = (response[6] << 8) | response[7];

    int pos = 12;

    while (pos < received && response[pos] != 0) {
        pos += response[pos] + 1;
    }

    pos++;

    if (pos + 4 > received) {
        return addresses;
    }

    pos += 4;

    for (int i = 0; i < answers; i++) {
        if (pos + 12 > received) {
            break;
        }

        if ((response[pos] & 0xc0) == 0xc0) {
            pos += 2;
        } else {
            while (pos < received && response[pos] != 0) {
                pos += response[pos] + 1;
            }

            pos++;
        }

        if (pos + 10 > received) {
            break;
        }

        uint16_t type =
        (response[pos] << 8) |
        response[pos + 1];

        uint16_t length =
        (response[pos + 8] << 8) |
        response[pos + 9];

        pos += 10;

        if (type == 1 && length == 4 && pos + 4 <= received) {
            char address[INET_ADDRSTRLEN]{};

            inet_ntop(
                AF_INET,
                response + pos,
                address,
                sizeof(address)
            );

            addresses.push_back(address);
        }

        pos += length;
    }

    return addresses;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "usage: browser hostname\n";
        return 1;
    }

    std::string hostname = argv[1];

    std::cout << "looking up " << hostname << "\n";

    std::vector<std::string> addresses = resolve(hostname);

    if (addresses.empty()) {
        std::cerr << "dns lookup failed\n";
        return 1;
    }

    std::cout << "found " << addresses.size() << " addresses\n";

    std::string targetip = addresses[0];

    std::cout << "using " << targetip << "\n";

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

    inet_pton(AF_INET, targetip.c_str(), &server.sin_addr);

    std::cout << "connecting\n";

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

    std::string request =
    "GET / HTTP/1.0\r\n"
    "Host: " + hostname + "\r\n"
    "Connection: close\r\n"
    "\r\n";

    if (send(
        sock,
        request.c_str(),
             request.size(),
             0
    ) < 0) {
        std::cerr << "request failed\n";
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

    std::cout << "\n";
    std::cout << headers << "\n\n";
    std::cout << body << "\n";

    return 0;
}
