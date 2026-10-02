#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

class RawTerminal {
    struct termios orig_termios;
public:
    RawTerminal() {
        tcgetattr(STDIN_FILENO, &orig_termios);
        struct termios raw = orig_termios;
        
        raw.c_lflag &= ~(ICANON | ECHO);
        
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    }

    ~RawTerminal() {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    }
};
void get_terminal_size(int& rows, int& cols) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        rows = w.ws_row;
        cols = w.ws_col;
    } else {
        rows = 24;
        cols = 80;
    }
}

void clear_prompt() {
    std::cout << "\r\033[K" << std::flush;
}

void run_more(std::istream& input) {
    int screen_rows = 24, screen_cols = 80;
    get_terminal_size(screen_rows, screen_cols);
    int max_display_rows = screen_rows - 1; 
    
    std::string line;
    int current_printed_rows = 0;

    while (std::getline(input, line)) {
        int visual_rows = (line.length() == 0) ? 1 : (line.length() + screen_cols - 1) / screen_cols;
        
        std::cout << line << "\n";
        current_printed_rows += visual_rows;

        if (current_printed_rows >= max_display_rows) {
            std::cout << "\033[7m-- More --\033[0m" << std::flush;

            char ch;
            {
                RawTerminal raw_mode;
                std::cin.get(ch);
            }

            clear_prompt();

            if (ch == 'q' || ch == 'Q') {
                break;
            } 
            else if (ch == '\n' || ch == '\r') {
                current_printed_rows = max_display_rows - 1; 
            } 
            else if (ch == ' ') {
                get_terminal_size(screen_rows, screen_cols);
                max_display_rows = screen_rows - 1;
                current_printed_rows = 0; 
            }
            else {
                current_printed_rows = 0;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        if (isatty(STDIN_FILENO) && isatty(STDOUT_FILENO)) {
            std::cerr << "usage : " << argv[0] << " <filename>  or  cat <file> | " << argv[0] << "\n";
            return 1;
        }
        run_more(std::cin);
    } 
    else {
        std::ifstream file(argv[1]);
        if (!file.is_open()) {
            std::cerr << "cannot open file " << argv[1] << "\n";
            return 1;
        }
        run_more(file);
    }

    return 0;
}

