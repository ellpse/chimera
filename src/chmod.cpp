#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>
#include <cerrno>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr
        << "usage: "
        << argv[0]
        << " <octal_mode> <target_file>\n";

        return 1;
    }

    if (setgid(0) != 0) {
        std::cerr
        << "error gaining group privilege: "
        << strerror(errno)
        << "\n";

        return 1;
    }

    if (setuid(0) != 0) {
        std::cerr
        << "error gaining user privilege: "
        << strerror(errno)
        << "\n";

        return 1;
    }

    mode_t mode;

    try {
        mode = static_cast<mode_t>(
            std::stoul(argv[1], nullptr, 8)
        );
    }
    catch (...) {
        std::cerr
        << "error: invalid mode.\n";

        return 1;
    }

    if (chmod(argv[2], mode) != 0) {
        std::cerr
        << "chmod failed: "
        << strerror(errno)
        << "\n";

        return 1;
    }

    std::cout
    << "permissions updated successfully\n";

    return 0;
}
