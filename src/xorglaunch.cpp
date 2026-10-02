#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cstdlib>
#include <filesystem>

void check_file(const std::string& path) {
    if (std::filesystem::exists(path)) {
        std::cout << "[ OK ] Found: " << path << "\n";
    } else {
        std::cerr << "[FAIL] MISSING: " << path << " !!!\n";
    }
}

int main() {
    // 1. Environmental Setup
    setenv("PATH", "/bin:/usr/bin:/usr/local/bin", 1);
    setenv("LD_LIBRARY_PATH", "/lib:/usr/lib", 1);

    // 2. Diagnostic Check
    std::cout << "--- Running X11 Dependency Check ---\n";
    const std::string xinit_path = "/usr/bin/xinit";
    const std::string xterm_path = "/usr/bin/xterm";
    const std::string xorg_path  = "/usr/bin/Xorg";
    const std::string term_path  = "/bin/term";

    check_file(xinit_path);
    check_file(xterm_path);
    check_file(xorg_path);
    check_file(term_path);
    std::cout << "------------------------------------\n";

    if (!std::filesystem::exists(xinit_path) ||
        !std::filesystem::exists(xterm_path) ||
        !std::filesystem::exists(xorg_path)  ||
        !std::filesystem::exists(term_path)) {
        std::cerr << "[ERROR] Boot stopped due to missing files listed above.\n";
    return 1;
        }

        std::cout << "[INFO] Spinning up X11 environment with custom shell...\n";

        // 3. Fork and Execute
        pid_t pid = fork();
        if (pid < 0) {
            std::cerr << "[ERROR] Failed to fork process.\n";
            return 1;
        }
        else if (pid == 0) {
            char* args[] = {
                const_cast<char*>(xinit_path.c_str()),
                const_cast<char*>(xterm_path.c_str()),
                const_cast<char*>("e"),
                const_cast<char*>(term_path.c_str()),
                const_cast<char*>("--"),
                const_cast<char*>(xorg_path.c_str()),
                const_cast<char*>(":0"),
                const_cast<char*>("-sharevts"),
                const_cast<char*>("-keeptty"),
                nullptr
            };
            execv(xinit_path.c_str(), args);
            std::cerr << "[ERROR] execv failed.\n";
            exit(1);
        }
        else {
            int status;
            waitpid(pid, &status, 0);
            std::cout << "[INFO] X11 environment closed cleanly.\n";
        }

        return 0;
}
