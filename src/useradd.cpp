#include <iostream>
#include <fstream>
#include <filesystem>
#include <sha256.h>
#include <string>

int main() {
    std::string username;
    std::string passwd;
    std::string home;
    std::string shell;
    int gid;

    std::cout << "username? (exit to exit) > ";
    std::cin >> username;

    if (username == "exit") {
        return 0;
    }

    std::cout << "password? > ";
    std::cin >> passwd;

    std::cout << "GID? > ";
    std::cin >> gid;

    std::cout << "home directory? > ";
    std::cin >> home;

    std::cout << "shell path? > ";
    std::cin >> shell;

    std::string hash = sha256(passwd);

    std::ofstream users("/etc/users", std::ios::app);
    if (!users) {
        std::cerr << "could not open /etc/users\n";
        return 1;
    }

    users << username << ":" << hash << "\n";
    users.close();

    try {
        std::filesystem::create_directories(home);
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "error creating home directory: "
        << e.what() << "\n";
        return 1;
    }

    std::ofstream passwdFile("/etc/passwd", std::ios::app);
    if (!passwdFile) {
        std::cerr << "could not open /etc/passwd\n";
        return 1;
    }

    passwdFile << username << ":x:"
    << gid << ":"
    << gid << ":"
    << username << ":"
    << home << ":"
    << shell << "\n";

    passwdFile.close();

    std::cout << "\nuser created successfully.\n";
    std::cout << "user = " << username << "\n";
    std::cout << "gid = " << gid << "\n";
    std::cout << "home = " << home << "\n";
    std::cout << "shell = " << shell << "\n";

    return 0;
}
