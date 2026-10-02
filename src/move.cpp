#include <iostream>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

void expand_wildcards(const std::string& pattern, std::vector<fs::path>& expanded_paths) {
    if (!pattern.empty() && pattern.back() == '*') {
        fs::path base_dir = pattern;

        base_dir = base_dir.parent_path();

        if (base_dir.empty()) {
            base_dir = ".";
        }

        if (fs::is_directory(base_dir)) {
            for (const auto& entry : fs::directory_iterator(base_dir)) {
                expanded_paths.push_back(entry.path());
            }
            return;
        }
    }

    expanded_paths.push_back(pattern);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "usage: " << argv[0] << " [-i|-n|-v] <source...> <destination>\n";
        return 1;
    }

    bool interactive = false;
    bool no_clobber = false;
    bool verbose = false;
    std::vector<std::string> raw_paths;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg[0] == '-' && arg.length() > 1) {
            for (size_t j = 1; j < arg.length(); ++j) {
                if (arg[j] == 'i') interactive = true;
                else if (arg[j] == 'n') no_clobber = true;
                else if (arg[j] == 'v') verbose = true;
                else {
                    std::cerr << "bad usage -" << arg[j] << "\n";
                    return 1;
                }
            }
        } else {
            raw_paths.push_back(arg);
        }
    }

    if (raw_paths.size() < 2) {
        std::cerr << "missing source or destination path.\n";
        return 1;
    }

    fs::path dest = raw_paths.back();
    raw_paths.pop_back();

    std::vector<fs::path> sources;
    for (const auto& raw_src : raw_paths) {
        expand_wildcards(raw_src, sources);
    }

    if (sources.size() > 1 && !fs::is_directory(dest)) {
        std::cerr << "error: '" << dest.string() << "' is not a directory.\n";
        return 1;
    }

    for (const auto& source : sources) {
        if (!fs::exists(source)) {
            std::cerr << "error: '" << source.string() << "' does not exist.\n";
            continue;
        }

        fs::path final_dest = dest;
        if (fs::is_directory(dest)) {
            final_dest = dest / source.filename();
        }

        if (fs::exists(final_dest)) {
            if (no_clobber) {
                continue;
            }
            if (interactive) {
                std::cout << argv[0] << ": overwrite '" << final_dest.string() << "'? (y/n): ";
                char answer;
                std::cin >> answer;
                if (answer != 'y' && answer != 'Y') {
                    continue;
                }
            }
        }

        try {
            fs::rename(source, final_dest);
            if (verbose) {
                std::cout << "renamed '" << source.string() << "' > '" << final_dest.string() << "'\n";
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "error moving '" << source.string() << "': " << e.what() << "\n";
        }
    }

    return 0;
}
