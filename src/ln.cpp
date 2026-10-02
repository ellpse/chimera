#include <iostream>
#include <filesystem>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

void print_usage(std::string_view program_name) {
    std::cerr << "usage: " << program_name << " [-s] [-f] <target>... <destination_directory>\n"
    << "       " << program_name << " [-s] [-f] <target> <link_name>\n"
    << "options:\n"
    << "  -s    create symbolic link\n"
    << "  -f    force\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    bool is_symbolic = false;
    bool force = false;
    std::vector<fs::path> positional_args;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);

        if (arg == "-s") {
            is_symbolic = true;
        } else if (arg == "-f") {
            force = true;
        } else if (arg.starts_with("-")) {
            std::cerr << "error: bad usage " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        } else {
            positional_args.push_back(arg);
        }
    }

    if (positional_args.size() < 2) {
        std::cerr << "error: Target and destination must be specified.\n";
        print_usage(argv[0]);
        return 1;
    }

    fs::path destination = positional_args.back();
    positional_args.pop_back();

    bool dest_is_dir = fs::is_directory(destination);
    if (positional_args.size() > 1 && !dest_is_dir) {
        std::cerr << "error: target '" << destination.string() << "' is not a directory\n";
        return 1;
    }

    try {
        for (const auto& target : positional_args) {
            fs::path final_link = dest_is_dir ? destination / target.filename() : destination;

            if (force && fs::exists(final_link)) {
                fs::remove(final_link);
            }

            if (is_symbolic) {
                fs::create_symlink(target, final_link);
            } else {
                fs::create_hard_link(target, final_link);
            }
        }
        std::cout << "successfully created " << positional_args.size() << " links.\n";
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
