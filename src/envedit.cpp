#include <iostream>
#include <cstdlib>
#include <string>

void print_usage(const std::string& exe_name) {
    std::cout << "usage\n"
    << "  set variable: " << exe_name << " -s <VARIABLE> <VALUE>\n"
    << "  get variable: " << exe_name << " -g <VARIABLE>\n";
}

int main(int argc, char* argv[]) {
    std::string exe_name = (argc > 0) ? argv[0] : "./poop";
    if (argc < 2) {
        print_usage(exe_name);
        return 1;
    }

    std::string flag = argv[1];

    if (flag == "-g") {
        if (argc < 3) {
            std::cerr << "missing variable name for -g\n";
            print_usage(exe_name);
            return 1;
        }

        std::string var_name = argv[2];
        const char* val = std::getenv(var_name.c_str());

        if (val != nullptr) {
            std::cout << var_name << "=" << val << "\n";
        } else {
            std::cout << var_name << " is not set\n";
        }
    }
    else if (flag == "-s") {
        if (argc < 4) {
            std::cerr << "missing var name for -s\n";
            print_usage(exe_name);
            return 1;
        }

        std::string var_name = argv[2];
        std::string var_value = argv[3];

        if (setenv(var_name.c_str(), var_value.c_str(), 1) == 0) {
            std::cout << "set " << var_name << " to " << var_value << "\n";

            const char* check_val = std::getenv(var_name.c_str());
            std::cout << "verifying... " << var_name << "=" << (check_val ? check_val : "") << "\n";
        } else {
            std::cerr << "setenv failed...\n";
            return 1;
        }
    }
    else {
        std::cerr << "bad usage: " << flag << "\n";
        print_usage(exe_name);
        return 1;
    }

    return 0;
}
