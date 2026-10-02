#include <iostream>
#include <string>
#include <cstring>
#include <cerrno>
#include <sys/mount.h>
#include <unistd.h>

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [-f | -l] <target>\n"
    << "Options:\n"
    << "  -f  Force unmount (useful for unreachable network/stuck file systems)\n"
    << "  -l  Lazy unmount (detach the file system now, clean up references later)\n";
}

int main(int argc, char* argv[]) {
    int flags = 0;
    bool use_umount2 = false;
    int opt;

    while ((opt = getopt(argc, argv, "flh")) != -1) {
        switch (opt) {
            case 'f':
                flags |= MNT_FORCE;
                use_umount2 = true;
                break;
            case 'l':
                flags |= MNT_DETACH;
                use_umount2 = true;
                break;
            case 'h':
            default:
                print_usage(argv[0]);
                return 0;
        }
    }

    if (argc - optind != 1) {
        print_usage(argv[0]);
        return 1;
    }

    const char* target = argv[optind];
    int result = 0;

    std::cout << "umounting " << target << "...\n";

    if (use_umount2) {
        result = umount2(target, flags);
    } else {
        result = umount(target);
    }

    if (result == 0) {
        std::cout << "unmount successful\n";
        return 0;
    } else {
        std::cerr << "umount failed: " << std::strerror(errno) << " (errno: " << errno << ")\n";
        return 1;
    }
}
