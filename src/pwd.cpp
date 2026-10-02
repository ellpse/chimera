#include <iostream>
#include <filesystem>
namespace fs = std::filesystem;

int main() {
    try {
        fs::path current_dir = fs::current_path();
        std::cout  << current_dir << std::endl;
    } 
    catch (const fs::filesystem_error& e) {
        std::cerr << "error accessing fs: " << e.what() << std::endl;
    }

    return 0;
}

