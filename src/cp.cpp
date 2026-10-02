#include <iostream>
#include <filesystem>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "usage: " << argv[0] << " <source_path> <destination_path>\n";
        return 1;
    }

    std::filesystem::path source = argv[1];
    std::filesystem::path destination = argv[2];

    try {
        auto options = std::filesystem::copy_options::overwrite_existing;

        std::filesystem::copy_file(source, destination, options);

        std::cout << "copied " << source << " to " << destination << "\n";
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
