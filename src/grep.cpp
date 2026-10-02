#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

std::string to_lower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return str;
}

void search_stream(std::istream& stream, const std::string& pattern, bool case_insensitive, const std::string& stream_name = "") {
    std::string line;
    size_t line_number = 0;
    std::string search_pattern = case_insensitive ? to_lower(pattern) : pattern;

    while (std::getline(stream, line)) {
        line_number++;
        std::string check_line = case_insensitive ? to_lower(line) : line;

        if (check_line.find(search_pattern) != std::string::npos) {
            if (!stream_name.empty()) {
                std::cout << stream_name << ":" << line_number << ":" << line << "\n";
            } else {
                std::cout << line << "\n";
            }
        }
    }
}

void search_file(const fs::path& file_path, const std::string& pattern, bool case_insensitive) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return;
    }
    search_stream(file, pattern, case_insensitive, file_path.string());
}

int main(int argc, char* argv[]) {
    bool recursive = false;
    bool case_insensitive = false;
    std::vector<std::string> positional_args;


    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-r") {
            recursive = true;
        } else if (arg == "-i") {
            case_insensitive = true;
        } else if (arg == "-ri" || arg == "-ir") {
            recursive = true;
            case_insensitive = true;
        } else {
            positional_args.push_back(arg);
        }
    }

    if (positional_args.size() < 1) {
        std::cerr << "usage: " << argv[0] << " [-r] [-i] <pattern> [<path>]\n";
        return 1;
    }

    std::string pattern = positional_args[0];

    if (positional_args.size() == 1) {
        search_stream(std::cin, pattern, case_insensitive);
        return 0;
    }

    fs::path target_path = positional_args[1];

    if (!fs::exists(target_path)) {
        std::cerr << "path does not exist: " << target_path << "\n";
        return 1;
    }

    if (fs::is_directory(target_path)) {
        if (!recursive) {
            std::cerr << argv[0] << ": " << target_path.string() << ": is a directory (use -r to search recursively!!)\n";
            return 1;
        }

        for (const auto& entry : fs::recursive_directory_iterator(target_path, fs::directory_options::skip_permission_denied)) {
            if (fs::is_regular_file(entry.path())) {
                search_file(entry.path(), pattern, case_insensitive);
            }
        }
    } else if (fs::is_regular_file(target_path)) {
        search_file(target_path, pattern, case_insensitive);
    }

    return 0;
}
