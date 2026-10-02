#include <iostream>
#include <csignal>
#include <sys/types.h>

int main() {
    if (kill(1, SIGKILL) == 0) {
        std::cout << "shutting down...\n";
    } else {
        perror("failed to kill pid 1");
    }
    return 0;
}
