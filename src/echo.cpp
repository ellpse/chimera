#include <iostream>
#include <fstream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        args.push_back(argv[i]);
    }

    if (args.empty()) {
        std::cout << '\n';
        return 0;
    }

    bool newline = true;
    std::string filename = "";
    bool append_mode = false;
    std::string text_to_print = "";

    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-n") {
            newline = false;
        }
        else if (args[i] == ">") {
            if (i + 1 < args.size()) {
                filename = args[i + 1];
                append_mode = false;
                break;
            } else {
                std::cerr << "expected file path after '>'\n";
                return 1;
            }
        }
        else if (args[i] == ">>") {

            if (i + 1 < args.size()) {
                filename = args[i + 1];
                append_mode = true;
                break;
            } else {
                std::cerr << "expected file path after '>>'\n";
                return 1;
            }
        }
        else {
            if (!text_to_print.empty()) {
                text_to_print += " ";
            }
            text_to_print += args[i];
        }
    }

    if (!filename.empty()) {
        std::ios_base::openmode mode = std::ios::out;
        if (append_mode) {
            mode |= std::ios::app;
        }

        std::ofstream outfile(filename, mode);
        if (!outfile) {
            std::cerr << "could not open/create'" << filename << "'\n";
            return 1;
        }

        outfile << text_to_print;
        if (newline) {
            outfile << '\n';
        }
        outfile.close();
    } else {
        std::cout << text_to_print; //default behaviour
        if (newline) {
            std::cout << '\n';
        }
    }

    return 0;
}
