#include <iostream>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <cstdlib>

int main(int argc, char* argv[]) {
    std::string prog_name = argv[0];
    mode_t mode = 0666;
    std::vector<std::string> args;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            std::cerr << "Usage: " << prog_name << " [OPTION]... NAME TYPE [MAJOR MINOR]\n";
            return 0;
        } else if (arg == "-m" || arg.rfind("--mode=", 0) == 0) {
            std::string mode_str;
            if (arg == "-m") {
                if (i + 1 < argc) {
                    mode_str = argv[++i];
                } else {
                    std::cerr << prog_name << ": option requires an argument -- 'm'\n";
                    return 1;
                }
            } else {
                mode_str = arg.substr(7);
            }
            try {
                mode = std::stoul(mode_str, nullptr, 8);
            } catch (...) {
                std::cerr << prog_name << ": invalid mode '" << mode_str << "'\n";
                return 1;
            }
        } else {
            args.push_back(arg);
        }
    }

    if (args.empty()) {
        std::cerr << prog_name << ": missing operand\n";
        return 1;
    }
    if (args.size() == 1) {
        std::cerr << prog_name << ": missing operand after '" << args[0] << "'\n";
        return 1;
    }

    std::string path = args[0];
    char type = args[1][0];
    mode_t type_flag = 0;
    dev_t dev = 0;

    if (type == 'p') {
        type_flag = S_IFIFO;
        if (args.size() > 2) {
            std::cerr << prog_name << ": extra operand '" << args[2] << "'\n";
            return 1;
        }
    } else if (type == 'b' || type == 'c' || type == 'u') {
        type_flag = (type == 'b') ? S_IFBLK : S_IFCHR;
        if (args.size() < 4) {
            std::cerr << prog_name << ": missing major or minor device number for type '" << type << "'\n";
            return 1;
        }

        try {
            unsigned int major = std::stoul(args[2]);
            unsigned int minor = std::stoul(args[3]);
            dev = makedev(major, minor);
        } catch (...) {
            std::cerr << prog_name << ": invalid major or minor device number\n";
            return 1;
        }
    } else {
        std::cerr << prog_name << ": invalid device type '" << args[1] << "'\n";
        return 1;
    }

    if (mknod(path.c_str(), type_flag | mode, dev) < 0) {
        perror(prog_name.c_str());
        return 1;
    }

    return 0;
}
