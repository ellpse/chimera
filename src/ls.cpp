#include <iostream>
#include <filesystem>
#include <fmt/core.h>
#include <string>
#include <vector>
#include <chrono>
#include <cmath>

namespace fs = std::filesystem;

const std::string COLOR_RESET = "\033[0m";
const std::string COLOR_BLUE = "\033[34m";
const std::string COLOR_GREEN = "\033[32m";
const std::string COLOR_GRAY = "\033[90m";

std::string format_size(uint64_t bytes) {
    const char* suffixes[] = {"B", "K", "M", "G", "T", "P", "E"};
    int suffix_index = 0;
    double double_bytes = static_cast<double>(bytes);

    while (double_bytes >= 1024 && suffix_index < 6) {
        double_bytes /= 1024;
        suffix_index++;
    }

    if (suffix_index == 0) {
        return std::format("{}{}", bytes, suffixes[suffix_index]);
    } else {
        return std::format("{:.1f}{}", double_bytes, suffixes[suffix_index]);
    }
}

std::string get_permissions_string(fs::perms p) {
    std::string s;
    s += ((p & fs::perms::owner_read) != fs::perms::none ? "r" : "-");
    s += ((p & fs::perms::owner_write) != fs::perms::none ? "w" : "-");
    s += ((p & fs::perms::owner_exec) != fs::perms::none ? "x" : "-");
    s += ((p & fs::perms::group_read) != fs::perms::none ? "r" : "-");
    s += ((p & fs::perms::group_write) != fs::perms::none ? "w" : "-");
    s += ((p & fs::perms::group_exec) != fs::perms::none ? "x" : "-");
    s += ((p & fs::perms::others_read) != fs::perms::none ? "r" : "-");
    s += ((p & fs::perms::others_write) != fs::perms::none ? "w" : "-");
    s += ((p & fs::perms::others_exec) != fs::perms::none ? "x" : "-"); //yes this looks hacky. yes its the only way.
    return s;
}

int main(int argc, char* argv[]) {
    bool show_hidden = false;
    bool long_format = false;
    bool human_readable = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (!arg.empty() && arg[0] == '-') {
            for (size_t j = 1; j < arg.length(); ++j) {
                switch (arg[j]) {
                    case 'l':
                        long_format = true;
                        break;
                    case 'a':
                        show_hidden = true;
                        break;
                    case 'h':
                        human_readable = true;
                        break;
                    default:
                        std::cerr << "bad usage: -" << arg[j] << "\n";
                        return 1;
                }
            }
        }
    }

    fs::path current_dir = ".";
    if (!fs::exists(current_dir) || !fs::is_directory(current_dir)) {
        std::cerr << "error accessing file\n";
        return 1;
    }

    for (const auto& entry : fs::directory_iterator(current_dir)) {
        std::string filename = entry.path().filename().string();
        bool is_hidden = (!filename.empty() && filename[0] == '.');

        if (is_hidden && !show_hidden) {
            continue;
        }

        char type_char = '-';
        std::string color = COLOR_GREEN;

        if (fs::is_directory(entry.status())) {
            type_char = 'd';
            color = COLOR_BLUE;
        }

        if (long_format) {
            std::string size_str = "0";
            if (type_char == '-') {
                try {
                    uint64_t bytes = fs::file_size(entry.path());
                    if (human_readable) {
                        size_str = format_size(bytes);
                    } else {
                        size_str = std::to_string(bytes);
                    }
                } catch (...) {
                    size_str = "?";
                }
            } else {
                size_str = human_readable ? "4.0K" : "4096";
            }

            std::string perms_str = get_permissions_string(entry.status().permissions());
            std::cout << std::format("{}{}{} {:>8} ", COLOR_GRAY, type_char, perms_str, size_str);
        }

        std::cout << std::format("{}{}{}", color, filename, COLOR_RESET);

        if (long_format) {
            std::cout << "\n";
        } else {
            std::cout << " ";
        }
    }

    if (!long_format) {
        std::cout << "\n";
    }

    return 0;
}
