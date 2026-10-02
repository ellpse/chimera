#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>
#include <cerrno>
#include <cstdlib>

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr
        << "usage: "
        << argv[0]
        << " <target_uid> <target_gid> <target_file>\n";

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

    uid_t target_uid;
    gid_t target_gid;

    try {
        target_uid = static_cast<uid_t>(
            std::stoul(argv[1])
        );

        target_gid = static_cast<gid_t>(
            std::stoul(argv[2])
        );
    }
    catch (...) {
        std::cerr
        << "error: invalid UID or GID.\n";

        return 1;
    }

    if (chown(argv[3], target_uid, target_gid) != 0) {
        std::cerr
        << "chown failed: "
        << strerror(errno)
        << "\n";

        return 1;
    }

    std::cout
    << "ownership updated successfully\n";

    return 0;
}
