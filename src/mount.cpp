#include <iostream>
#include <string>
#include <cstring>
#include <cerrno>
#include <sys/mount.h>
#include <unistd.h>

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [-t fstype] [-o options] <source> <target>\n"
    << "Options:\n"
    << "  -t fstype   Specify the filesystem type (e.g., vfat, ext4, proc)\n"
    << "  -o options  Comma-separated mount options (e.g., ro, rw, iocharset=utf8)\n";
}

int main(int argc, char* argv[]) {
    std::string fstype = "";
    std::string options_str = "";
    unsigned long mount_flags = 0;

    int opt;
    while ((opt = getopt(argc, argv, "t:o:h")) != -1) {
        switch (opt) {
            case 't':
                fstype = optarg;
                break;
            case 'o':
                options_str = optarg;
                break;
            case 'h':
            default:
                print_usage(argv[0]);
                return 0;
        }
    }

    if (argc - optind != 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char* source = argv[optind];
    const char* target = argv[optind + 1];

    if (!options_str.empty()) {
        if (options_str.find("ro") != std::string::npos) {
            mount_flags |= MS_RDONLY;
        }
    }
    const void* data = options_str.empty() ? nullptr : options_str.c_str();
    const char* fs = fstype.empty() ? nullptr : fstype.c_str();

    std::cout << "mounting " << source << " to " << target;
    if (fs) std::cout << " as " << fs;
    std::cout << "...\n";

    if (mount(source, target, fs, mount_flags, data) == 0) {
        std::cout << "mount successful.\n";
        return 0;
    } else {
        std::cerr << "mount failed: " << std::strerror(errno) << " (errno: " << errno << ")\n";
        return 1;
    }
}
