#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cerrno>
#include <cstdlib>
#include <pwd.h>

class SHA256 {
private:
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t data[64];
    size_t datalen;

    static constexpr uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
        0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
        0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
        0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
        0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };


    static uint32_t rotr(uint32_t x, uint32_t n) {
        return (x >> n) | (x << (32 - n));
    }

    static uint32_t choose(uint32_t e, uint32_t f, uint32_t g) {
        return (e & f) ^ (~e & g);
    }

    static uint32_t majority(uint32_t a, uint32_t b, uint32_t c) {
        return (a & b) ^ (a & c) ^ (b & c);
    }

    void transform() {
        uint32_t m[64];

        for (int i = 0; i < 16; ++i) {
            m[i] =
            (static_cast<uint32_t>(data[i * 4]) << 24) |
            (static_cast<uint32_t>(data[i * 4 + 1]) << 16) |
            (static_cast<uint32_t>(data[i * 4 + 2]) << 8) |
            static_cast<uint32_t>(data[i * 4 + 3]);
        }

        for (int i = 16; i < 64; ++i) {
            uint32_t s0 =
            rotr(m[i - 15], 7) ^
            rotr(m[i - 15], 18) ^
            (m[i - 15] >> 3);

            uint32_t s1 =
            rotr(m[i - 2], 17) ^
            rotr(m[i - 2], 19) ^
            (m[i - 2] >> 10);

            m[i] = m[i - 16] + s0 + m[i - 7] + s1;
        }

        uint32_t a = state[0];
        uint32_t b = state[1];
        uint32_t c = state[2];
        uint32_t d = state[3];
        uint32_t e = state[4];
        uint32_t f = state[5];
        uint32_t g = state[6];
        uint32_t h = state[7];

        for (int i = 0; i < 64; ++i) {
            uint32_t S1 =
            rotr(e, 6) ^
            rotr(e, 11) ^
            rotr(e, 25);

            uint32_t temp1 =
            h + S1 + choose(e, f, g) + k[i] + m[i];

            uint32_t S0 =
            rotr(a, 2) ^
            rotr(a, 13) ^
            rotr(a, 22);

            uint32_t temp2 =
            S0 + majority(a, b, c);

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }

public:
    SHA256() : bitlen(0), datalen(0) {
        state[0] = 0x6a09e667;
        state[1] = 0xbb67ae85;
        state[2] = 0x3c6ef372;
        state[3] = 0xa54ff53a;
        state[4] = 0x510e527f;
        state[5] = 0x9b05688c;
        state[6] = 0x1f83d9ab;
        state[7] = 0x5be0cd19;
    }

    void update(const uint8_t* input, size_t length) {
        for (size_t i = 0; i < length; ++i) {
            data[datalen++] = input[i];

            if (datalen == 64) {
                transform();
                bitlen += 512;
                datalen = 0;
            }
        }
    }

    std::string final() {
        size_t i = datalen;

        data[i++] = 0x80;

        if (datalen < 56) {
            while (i < 56)
                data[i++] = 0x00;
        } else {
            while (i < 64)
                data[i++] = 0x00;

            transform();
            std::memset(data, 0, 56);
        }

        bitlen += datalen * 8;

        data[63] = static_cast<uint8_t>(bitlen);
        data[62] = static_cast<uint8_t>(bitlen >> 8);
        data[61] = static_cast<uint8_t>(bitlen >> 16);
        data[60] = static_cast<uint8_t>(bitlen >> 24);
        data[59] = static_cast<uint8_t>(bitlen >> 32);
        data[58] = static_cast<uint8_t>(bitlen >> 40);
        data[57] = static_cast<uint8_t>(bitlen >> 48);
        data[56] = static_cast<uint8_t>(bitlen >> 56);

        transform();

        std::ostringstream ss;

        for (int i = 0; i < 8; ++i) {
            ss << std::hex
            << std::setfill('0')
            << std::setw(8)
            << state[i];
        }

        return ss.str();
    }
};

std::string sha256(const std::string& input) {
    SHA256 ctx;

    ctx.update(
        reinterpret_cast<const uint8_t*>(input.data()),
               input.size()
    );

    return ctx.final();
}

void set_echo(bool enable) {
    struct termios tty;

    if (tcgetattr(STDIN_FILENO, &tty) != 0)
        return;

    if (enable)
        tty.c_lflag |= ECHO;
    else
        tty.c_lflag &= ~ECHO;

    tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

bool authenticate_user(
    const std::string& input_user,
    const std::string& input_passwd
) {
    std::ifstream file("/etc/users");

    if (!file.is_open()) {
        std::cerr << "could not open /etc/users\n";
        return false;
    }

    const std::string hashed_passwd = sha256(input_passwd);

    std::string line;

    while (std::getline(file, line)) {
        const size_t colon_pos = line.find(':');

        if (colon_pos == std::string::npos)
            continue;

        const std::string db_user =
        line.substr(0, colon_pos);

        const std::string db_passwd =
        line.substr(colon_pos + 1);

        if (db_user == input_user &&
            db_passwd == hashed_passwd) {
            return true;
            }
    }

    return false;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr
        << "usage: "
        << argv[0]
        << " <binary_name> [args...]\n";

        return 1;
    }

    struct passwd* pw = getpwuid(getuid());

    if (pw == nullptr) {
        std::cerr << "could not determine current user\n";
        return 1;
    }

    const std::string user = pw->pw_name;
    std::string passwd;

    std::cout
    << "[sudo] password for "
    << user
    << ": "
    << std::flush;

    set_echo(false);

    if (!(std::cin >> passwd)) {
        set_echo(true);
        std::cout << '\n';
        return 1;
    }

    set_echo(true);
    std::cout << '\n';

    if (!authenticate_user(user, passwd)) {
        std::cerr << "incorrect credentials\n";
        return 1;
    }

    if (setgid(0) != 0) {
        std::cerr
        << "failed to obtain root group privileges: "
        << strerror(errno)
        << '\n';

        return 1;
    }

    if (setuid(0) != 0) {
        std::cerr
        << "failed to obtain root privileges: "
        << strerror(errno)
        << '\n';

        return 1;
    }

    if (geteuid() != 0) {
        std::cerr
        << "privilege elevation failed\n";
        return 1;
    }

    std::cout
    << "authentication successful\n";
    std::string full_path =
    "/bin/" + std::string(argv[1]);

    std::vector<char*> exec_args;

    exec_args.push_back(
        const_cast<char*>(full_path.c_str())
    );

    for (int i = 2; i < argc; ++i)
        exec_args.push_back(argv[i]);

    exec_args.push_back(nullptr);

    execv(exec_args[0], exec_args.data());

    std::cerr
    << "execution failed: "
    << full_path
    << " ("
    << strerror(errno)
    << ")\n";

    return 1;
}
