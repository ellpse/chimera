#include <iostream>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " [options] <filepath>\n";
        return 1;
    }

    bool recursive = false;
    bool force = false;
    std::vector<fs::path> targets;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-r" || arg == "--recursive") {
            recursive = true;
        } else if (arg == "-f" || arg == "--force") {
            force = true;
        } else if (arg == "-rf" || arg == "-fr") {
            recursive = true;
            force = true;
        } else if (arg[0] == '-' && arg.size() > 1) {
            std::cerr << "bad usage: " << arg << '\n';
            return 1;
        } else {
            targets.push_back(arg);
        }
    }

    if (targets.empty()) {
        std::cerr << "no file or directory specified.\n";
        return 1;
    }
    for (const auto& filepath : targets) {
        std::error_code ec;

        if (!fs::exists(filepath, ec)) {
            if (!force) {
                std::cerr << "rm: cannot remove '" << filepath.string() << "': No such file or directory\n";
            }
            continue;
        }

        if (fs::is_directory(filepath) && !recursive) {
            std::cerr << "rm: cannot remove '" << filepath.string() << "': is a directory\n";
            continue;
        }

        if (recursive) {
            std::uintmax_t removed = fs::remove_all(filepath, ec);
            if (ec && !force) {
                std::cerr << "rm: failed to remove '" << filepath.string() << "': " << ec.message() << '\n';
            } else if (!ec && removed > 0 && !force) {
                std::cout << "removed (-r) " << filepath.string() << " (" << removed << " items)\n";
            }
        } else {
            fs::remove(filepath, ec);
            if (ec && !force) {
                std::cerr << "rm: failed to remove '" << filepath.string() << "': " << ec.message() << '\n';
            } else if (!ec && !force) {
                std::cout << "Removed " << filepath.string() << '\n';
            }
        }
    }

    return 0;
}
