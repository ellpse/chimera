#include <iostream>
#include <string>
#include <cstring>
#include <sys/utsname.h>
#include <unistd.h>

void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " [OPTION]...\n"
              << "Print certain system information. With no OPTION, same as -s.\n\n"
              << "  -a, --all        print all information\n"
              << "  -s, --kernel     print the kernel name\n"
              << "  -n, --nodename   print the network node hostname\n"
              << "  -r, --release    print the kernel release\n"
              << "  -v, --version    print the kernel version\n"
              << "  -m, --machine    print the machine hardware architecture\n";
}

int main(int argc, char* argv[]) {
    struct utsname buf;
   
    if (uname(&buf) != 0) {
        std::cerr << "uname error: " << std::strerror(errno) << "\n";
        return 1;
    }

    if (argc == 1) {
        std::cout << buf.sysname << "\n";
        return 0;
    }

    bool flag_all = false;
    bool flag_sysname = false;
    bool flag_nodename = false;
    bool flag_release = false;
    bool flag_version = false;
    bool flag_machine = false;

    int opt;
    while ((opt = getopt(argc, argv, "asnrvm")) != -1) {
        switch (opt) {
            case 'a': flag_all = true; break;
            case 's': flag_sysname = true; break;
            case 'n': flag_nodename = true; break;
            case 'r': flag_release = true; break;
            case 'v': flag_version = true; break;
            case 'm': flag_machine = true; break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    std::string output = "";

    if (flag_all || flag_sysname)  output += std::string(buf.sysname) + " ";
    if (flag_all || flag_nodename) output += std::string(buf.nodename) + " ";
    if (flag_all || flag_release)  output += std::string(buf.release) + " ";
    if (flag_all || flag_version)  output += std::string(buf.version) + " ";
    if (flag_all || flag_machine)  output += std::string(buf.machine) + " ";

    if (!output.empty()) {
        output.pop_back();
        std::cout << output << "\n";
    }

    return 0;
}
