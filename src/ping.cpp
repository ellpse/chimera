#include <iostream>
#include <cstring>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

struct ICMPHeader {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
};

uint16_t computeChecksum(uint16_t* addr, int count) {
    uint32_t sum = 0;
    while (count > 1) {
        sum += *addr++;
        count -= 2;
    }
    if (count > 0) {
        sum += *(uint8_t*)addr;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return static_cast<uint16_t>(~sum);
}

int main(int argc, char* argv[]) {
    std::string targetIP = (argc > 1) ? argv[1] : "10.0.2.2";

    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        std::cerr << "socket creation failed, got root?\n";
        return 1;
    }

    struct timeval tv{};
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    sockaddr_in destAddr{};
    destAddr.sin_family = AF_INET;
    if (inet_pton(AF_INET, targetIP.c_str(), &destAddr.sin_addr) <= 0) {
        std::cerr << "invalid target ip format: " << targetIP << "\n";
        close(sock);
        return 1;
    }

    ICMPHeader packet{};
    packet.type = 8;
    packet.code = 0;
    packet.id = htons(getpid() & 0xFFFF);// ooooh evil hexadecimal!!!
    packet.seq = htons(1);
    packet.checksum = computeChecksum(reinterpret_cast<uint16_t*>(&packet), sizeof(packet));

    std::cout << "ping " << targetIP << " with 8 bytes...\n";

    if (sendto(sock, &packet, sizeof(packet), 0, reinterpret_cast<sockaddr*>(&destAddr), sizeof(destAddr)) < 0) {
        std::cerr << "failed to send packet\n";
        close(sock);
        return 1;
    }

    uint8_t buffer[1024];
    sockaddr_in fromAddr{};
    socklen_t fromLen = sizeof(fromAddr);

    if (recvfrom(sock, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&fromAddr), &fromLen) < 0) {
        std::cout << "timed out - " << targetIP << ").\n";
    } else {
        char replyIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &fromAddr.sin_addr, replyIP, sizeof(replyIP));
        std::cout << "reply from " << replyIP << "!\n";
    }

    close(sock);
    return 0;
}
