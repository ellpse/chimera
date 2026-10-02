#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>

int main() {
    std::string username;

    std::cout << "username? (exit to exit) > ";
    std::cin >> username;

    if (username == "exit") {
        return 0;
    }

    std::ifstream passwdFile("/etc/passwd");
    if (!passwdFile) {
        std::cerr << "could not open /etc/passwd\n";
        return 1;
    }

    std::vector<std::string> passwdLines;
    std::string line;
    std::string home;

    while (std::getline(passwdFile, line)) {
        std::size_t colon = line.find(':');

        if (colon != std::string::npos &&
            line.substr(0, colon) == username) {

            std::size_t homeStart = line.find(':', colon + 1);
        homeStart = line.find(':', homeStart + 1);
        homeStart = line.find(':', homeStart + 1);
        homeStart = line.find(':', homeStart + 1);

        if (homeStart != std::string::npos) {
            std::size_t homeEnd = line.find(':', homeStart + 1);

            if (homeEnd != std::string::npos) {
                home = line.substr(
                    homeStart + 1,
                    homeEnd - homeStart - 1
                );
            }
        }

        continue;
            }

            passwdLines.push_back(line);
    }

    passwdFile.close();

    if (home.empty()) {
        std::cerr << "user not found in /etc/passwd\n";
        return 1;
    }

    std::ofstream passwdOut("/etc/passwd", std::ios::trunc);
    if (!passwdOut) {
        std::cerr << "could not write /etc/passwd\n";
        return 1;
    }

    for (const auto& passwdLine : passwdLines) {
        passwdOut << passwdLine << "\n";
    }

    passwdOut.close();

    std::ifstream usersFile("/etc/users");
    if (!usersFile) {
        std::cerr << "could not open /etc/users\n";
        return 1;
    }

    std::vector<std::string> userLines;

    while (std::getline(usersFile, line)) {
        std::size_t colon = line.find(':');

        if (colon != std::string::npos &&
            line.substr(0, colon) == username) {
            continue;
            }

            userLines.push_back(line);
    }

    usersFile.close();

    std::ofstream usersOut("/etc/users", std::ios::trunc);
    if (!usersOut) {
        std::cerr << "could not write /etc/users\n";
        return 1;
    }

    for (const auto& userLine : userLines) {
        usersOut << userLine << "\n";
    }

    usersOut.close();

    std::string answer;

    std::cout << "delete home directory '" << home << "'? [y/N] > ";
    std::cin >> answer;

    if (answer == "y" || answer == "Y") {
        try {
            std::filesystem::remove_all(home);
            std::cout << "home directory removed.\n";
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "error removing home directory: "
            << e.what() << "\n";
            return 1;
        }
    }

    std::cout << "\nuser deleted successfully.\n";
    std::cout << "user = " << username << "\n";

    return 0;
}
