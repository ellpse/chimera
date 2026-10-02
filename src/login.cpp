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
#include <sys/stat.h>
#include <grp.h>
#include <cerrno>
#include <cstdlib>

class SHA256 {
private:
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t data[64];
    size_t datalen;

    static const uint32_t k[64];

    static uint32_t rotr(uint32_t x, uint32_t n) {
        return (x >> n) | (x << (32 - n));
    }

    static uint32_t ch(uint32_t x, uint32_t y, uint32_t z) {
        return (x & y) ^ (~x & z);
    }

    static uint32_t maj(uint32_t x, uint32_t y, uint32_t z) {
        return (x & y) ^ (x & z) ^ (y & z);
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
            h +
            S1 +
            ch(e, f, g) +
            k[i] +
            m[i];

            uint32_t S0 =
            rotr(a, 2) ^
            rotr(a, 13) ^
            rotr(a, 22);

            uint32_t temp2 =
            S0 +
            maj(a, b, c);

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
    SHA256()
    : bitlen(0), datalen(0) {
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
            data[datalen] = input[i];
            ++datalen;

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
            while (i < 56) {
                data[i++] = 0;
            }
        } else {
            while (i < 64) {
                data[i++] = 0;
            }

            transform();

            std::memset(data, 0, 56);
        }

        bitlen += static_cast<uint64_t>(datalen) * 8;

        data[56] = static_cast<uint8_t>(bitlen >> 56);
        data[57] = static_cast<uint8_t>(bitlen >> 48);
        data[58] = static_cast<uint8_t>(bitlen >> 40);
        data[59] = static_cast<uint8_t>(bitlen >> 32);
        data[60] = static_cast<uint8_t>(bitlen >> 24);
        data[61] = static_cast<uint8_t>(bitlen >> 16);
        data[62] = static_cast<uint8_t>(bitlen >> 8);
        data[63] = static_cast<uint8_t>(bitlen);

        transform();

        std::ostringstream output;

        for (int j = 0; j < 8; ++j) {
            output
            << std::hex
            << std::setfill('0')
            << std::setw(8)
            << state[j];
        }

        return output.str();
    }
};

const uint32_t SHA256::k[64] = {
    0x428a2f98, 0x71374491,
    0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01,
    0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe,
    0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa,
    0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d,
    0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138,
    0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb,
    0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624,
    0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08,
    0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f,
    0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb,
    0xbef9a3f7, 0xc67178f2
};

std::string sha256(const std::string& input) {
    SHA256 hash;

    hash.update(
        reinterpret_cast<const uint8_t*>(input.data()),
                input.size()
    );

    return hash.final();
}

struct UserConfig {
    uid_t uid;
    gid_t gid;
    std::string home;
    std::string shell;
};

std::string remove_cr(const std::string& input) {
    if (!input.empty() && input[input.size() - 1] == '\r') {
        return input.substr(0, input.size() - 1);
    }

    return input;
}

bool set_echo(bool enabled) {
    struct termios tty;

    if (tcgetattr(STDIN_FILENO, &tty) != 0) {
        return false;
    }

    if (enabled) {
        tty.c_lflag |= ECHO;
    } else {
        tty.c_lflag &= ~ECHO;
    }

    return tcsetattr(STDIN_FILENO, TCSANOW, &tty) == 0;
}

bool authenticate_user(
    const std::string& username,
    const std::string& password
) {
    std::ifstream file("/etc/users");

    if (!file.is_open()) {
        std::cerr << "login: cannot open /etc/users\n";
        return false;
    }

    const std::string password_hash = sha256(password);

    std::string line;

    while (std::getline(file, line)) {
        line = remove_cr(line);

        size_t colon = line.find(':');

        if (colon == std::string::npos) {
            continue;
        }

        std::string stored_user = line.substr(0, colon);
        std::string stored_hash = line.substr(colon + 1);

        if (stored_user == username &&
            stored_hash == password_hash) {
            return true;
            }
    }

    return false;
}



bool get_user_config(
    const std::string& username,
    UserConfig& config
) {
    std::ifstream file("/etc/passwd");

    if (!file.is_open()) {
        std::cerr << "login: cannot open /etc/passwd\n";
        return false;
    }

    std::string line;

    while (std::getline(file, line)) {
        line = remove_cr(line);

        std::vector<std::string> fields;
        std::stringstream ss(line);
        std::string field;

        while (std::getline(ss, field, ':')) {
            fields.push_back(field);
        }

        if (fields.size() < 7) {
            continue;
        }

        if (fields[0] != username) {
            continue;
        }

        char* uid_end = NULL;
        char* gid_end = NULL;

        errno = 0;

        unsigned long uid_value =
        std::strtoul(
            fields[2].c_str(),
                     &uid_end,
                     10
        );

        if (errno != 0 ||
            uid_end == fields[2].c_str() ||
            *uid_end != '\0') {
            std::cerr << "login: invalid UID\n";
        return false;
            }

            errno = 0;

            unsigned long gid_value =
            std::strtoul(
                fields[3].c_str(),
                         &gid_end,
                         10
            );

            if (errno != 0 ||
                gid_end == fields[3].c_str() ||
                *gid_end != '\0') {
                std::cerr << "login: invalid GID\n";
            return false;
                }

                config.uid = static_cast<uid_t>(uid_value);
                config.gid = static_cast<gid_t>(gid_value);
                config.home = fields[5];
                config.shell = fields[6];

                return true;
    }

    return false;
}

bool start_user_session(
    const std::string& username,
    const UserConfig& config
) {
    if (config.shell.empty()) {
        std::cerr << "login: no shell configured\n";
        return false;
    }

    if (config.shell[0] != '/') {
        std::cerr << "login: shell path must be absolute\n";
        return false;
    }

    if (access(config.shell.c_str(), X_OK) != 0) {
        std::cerr
        << "login: cannot execute "
        << config.shell
        << ": "
        << std::strerror(errno)
        << "\n";

        return false;
    }

    if (initgroups(username.c_str(), config.gid) != 0) {
        std::cerr
        << "login: initgroups failed: "
        << std::strerror(errno)
        << "\n";

        return false;
    }

    if (setgid(config.gid) != 0) {
        std::cerr
        << "login: setgid failed: "
        << std::strerror(errno)
        << "\n";

        return false;
    }

    if (setuid(config.uid) != 0) {
        std::cerr
        << "login: setuid failed: "
        << std::strerror(errno)
        << "\n";

        return false;
    }

    setenv("USER", username.c_str(), 1);
    setenv("LOGNAME", username.c_str(), 1);
    setenv("HOME", config.home.c_str(), 1);
    setenv("SHELL", config.shell.c_str(), 1);

    if (chdir(config.home.c_str()) != 0) {
        std::cerr
            << "login: could not enter "
            << config.home
            << ": "
            << std::strerror(errno)
            << "\n";

        if (chdir("/") != 0) {
            return false;
        }
    }

    size_t slash =
        config.shell.find_last_of('/');

    std::string shell_name;

    if (slash == std::string::npos) {
        shell_name = config.shell;
    } else {
        shell_name =
        config.shell.substr(slash + 1);
    }

    std::string login_name =
    "-" + shell_name;

    char* argv[2];

    argv[0] =
    const_cast<char*>(login_name.c_str());

    argv[1] = NULL;

    execv(
        config.shell.c_str(),
          argv
    );

    std::cerr
    << "login: execv failed for "
    << config.shell
    << ": "
    << std::strerror(errno)
    << "\n";

    return false;
}

int main() {

    while (true) {
        std::cout << "\nWelcome!\n";

        std::string username;
        std::string password;

        bool authenticated = false;

        while (!authenticated) {
            std::cout
            << "user > "
            << std::flush;

            if (!std::getline(std::cin, username)) {
                return 0;
            }

            username = remove_cr(username);

            if (username.empty()) {
                continue;
            }

            std::cout
            << "password > "
            << std::flush;

            if (!set_echo(false)) {
                std::cerr
                << "login: could not disable echo\n";
            }

            if (!std::getline(std::cin, password)) {
                set_echo(true);
                std::cout << "\n";
                return 0;
            }

            set_echo(true);
            std::cout << "\n";

            password = remove_cr(password);

            if (authenticate_user(username, password)) {
                authenticated = true;

                std::cout
                << "login successful!\n";
            } else {
                std::cout
                << "invalid credentials. try again.\n\n";
            }

            password.clear();
        }

        UserConfig config;

        if (!get_user_config(username, config)) {
            std::cerr
            << "login: no matching entry in /etc/passwd\n";

            continue;
        }

        std::cout
        << "starting "
        << config.shell
        << "...\n";

        pid_t pid = fork();

        if (pid < 0) {
            std::cerr
            << "login: fork failed: "
            << std::strerror(errno)
            << "\n";

            continue;
        }

        if (pid == 0) {
            bool success =
            start_user_session(
                username,
                config
            );

            _exit(success ? 0 : 1);
        }

        int status = 0;

        while (waitpid(pid, &status, 0) < 0) {
            if (errno == EINTR) {
                continue;
            }

            std::cerr
            << "login: waitpid failed: "
            << std::strerror(errno)
            << "\n";

            break;
        }

        std::cout
        << "\033[2J\033[1;1H";

        std::cout
        << "logged out.\n";
    }

    return 0;
}
