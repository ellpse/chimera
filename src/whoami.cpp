#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <cstring>
#include <cerrno>

int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::cerr << "usage: " << argv[0] << "\n";
        return 1;
    }


    uid_t uid = getuid();

    struct passwd* pw = getpwuid(uid);

    if (pw == nullptr) {
        std::cerr << argv[0] << ": cannot find name for user ID " << uid 
                  << ": " << std::strerror(errno) << "\n";
        return 1;
    }
    std::cout << pw->pw_name << "\n";

    return 0;
}
