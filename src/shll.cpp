#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <map>
#include <set>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <termios.h>
#include <dirent.h>
#include <algorithm>
#include <cctype>
#include <limits.h>
#include <cerrno>
#include <cstring>
#include <glob.h>
#include <cstdlib>
#include <fcntl.h>
#include <signal.h>

extern char **environ;

const std::string color_reset = "\033[0m";
const std::string color_blue = "\033[34m";
const std::string color_green = "\033[32m";

const std::string VERSION = "chimera v1.7";

std::string path = "/";
std::string previous_path = "/";
std::vector<std::string> commands;
std::map<std::string, std::string> aliases;
bool should_exit_shell = false;

struct parsed_command {
    std::vector<std::string> args;
    std::string input_file;
    std::string output_file;
    bool append_output;

    parsed_command() : append_output(false) {}
};

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");

    if (first == std::string::npos) {
        return "";
    }

    size_t last = str.find_last_not_of(" \t\r\n");

    return str.substr(first, last - first + 1);
}

std::string lowercase(const std::string& value) {
    std::string result = value;

    for (size_t i = 0; i < result.length(); ++i) {
        result[i] = static_cast<char>(
            std::tolower(
                static_cast<unsigned char>(result[i])
            )
        );
    }

    return result;
}

bool directory_exists(const std::string& value) {
    struct stat st;

    return stat(value.c_str(), &st) == 0 &&
    S_ISDIR(st.st_mode);
}

std::string join_path(
    const std::string& base,
    const std::string& name
) {
    if (name.empty()) {
        return base;
    }

    if (name[0] == '/') {
        return name;
    }

    if (base.empty()) {
        return name;
    }

    if (base[base.length() - 1] == '/') {
        return base + name;
    }

    return base + "/" + name;
}

std::string normalize_path(const std::string& input) {
    std::vector<std::string> parts;
    std::stringstream ss(input);
    std::string part;

    while (std::getline(ss, part, '/')) {
        if (part.empty() || part == ".") {
            continue;
        }

        if (part == "..") {
            if (!parts.empty()) {
                parts.pop_back();
            }
        } else {
            parts.push_back(part);
        }
    }

    std::string result = "/";

    for (size_t i = 0; i < parts.size(); ++i) {
        result += parts[i];

        if (i + 1 < parts.size()) {
            result += "/";
        }
    }

    return result;
}

std::string load_hostname() {
    std::ifstream file("/etc/hostname");
    std::string hostname;

    if (file.is_open() && std::getline(file, hostname)) {
        hostname = trim(hostname);

        if (!hostname.empty()) {
            return hostname;
        }
    }

    const char* env = std::getenv("HOSTNAME");

    if (env && *env) {
        return env;
    }

    return "localhost";
}

std::string get_prompt_path() {
    char cwd[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        return path;
    }

    std::string current(cwd);
    const char* home_env = std::getenv("HOME");

    if (!home_env) {
        return current;
    }

    std::string home(home_env);

    if (current == home) {
        return "~";
    }

    if (
        home != "/" &&
        current.rfind(home + "/", 0) == 0
    ) {
        return "~" + current.substr(home.length());
    }

    return current;
}

std::string expand_vars(
    std::string line,
    const std::map<std::string, std::string>& variables
) {
    for (
        std::map<std::string, std::string>::const_iterator it =
        variables.begin();
    it != variables.end();
    ++it
    ) {
        std::string placeholder = "$" + it->first;
        size_t pos = 0;

        while (
            (pos = line.find(placeholder, pos)) !=
            std::string::npos
        ) {
            line.replace(
                pos,
                placeholder.length(),
                         it->second
            );

            pos += it->second.length();
        }
    }

    return line;
}

std::string expand_environment_variables(std::string line) {
    std::string result;
    result.reserve(line.length());

    for (size_t i = 0; i < line.length();) {
        if (line[i] != '$') {
            result += line[i++];
            continue;
        }

        if (i + 1 >= line.length()) {
            result += '$';
            ++i;
            continue;
        }

        if (line[i + 1] == '{') {
            size_t end = line.find('}', i + 2);

            if (end == std::string::npos) {
                result += '$';
                ++i;
                continue;
            }

            std::string name = line.substr(
                i + 2,
                end - i - 2
            );

            const char* value = std::getenv(name.c_str());

            if (value) {
                result += value;
            }

            i = end + 1;
            continue;
        }

        if (
            std::isalpha(
                static_cast<unsigned char>(line[i + 1])
            ) ||
            line[i + 1] == '_'
        ) {
            size_t end = i + 1;

            while (
                end < line.length() &&
                (
                    std::isalnum(
                        static_cast<unsigned char>(line[end])
                    ) ||
                    line[end] == '_'
                )
            ) {
                ++end;
            }

            std::string name = line.substr(
                i + 1,
                end - i - 1
            );

            const char* value = std::getenv(name.c_str());

            if (value) {
                result += value;
            }

            i = end;
            continue;
        }

        result += '$';
        ++i;
    }

    return result;
}

std::vector<std::string> tokenize_command(
    const std::string& line
) {
    std::vector<std::string> tokens;
    std::string current;

    bool in_quotes = false;
    char quote_char = '\0';
    bool escaped = false;

    for (size_t i = 0; i < line.length(); ++i) {
        char c = line[i];

        if (escaped) {
            current += c;
            escaped = false;
            continue;
        }

        if (c == '\\' && !in_quotes) {
            escaped = true;
            continue;
        }

        if (in_quotes) {
            if (c == quote_char) {
                in_quotes = false;
                quote_char = '\0';
            } else {
                current += c;
            }

            continue;
        }

        if (c == '\'' || c == '"') {
            in_quotes = true;
            quote_char = c;
            continue;
        }

        if (c == '|' || c == '<' || c == '>') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }

            if (
                c == '>' &&
                i + 1 < line.length() &&
                line[i + 1] == '>'
            ) {
                tokens.push_back(">>");
                ++i;
            } else {
                tokens.push_back(std::string(1, c));
            }

            continue;
        }

        if (
            std::isspace(
                static_cast<unsigned char>(c)
            )
        ) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }

            continue;
        }

        current += c;
    }

    if (escaped) {
        current += '\\';
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }

    return tokens;
}

bool has_wildcard(const std::string& token) {
    return
    token.find('*') != std::string::npos ||
    token.find('?') != std::string::npos ||
    token.find('[') != std::string::npos;
}

std::vector<std::string> expand_wildcards(
    const std::vector<std::string>& tokens
) {
    std::vector<std::string> result;

    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& token = tokens[i];

        if (!has_wildcard(token)) {
            result.push_back(token);
            continue;
        }

        glob_t matches;
        std::memset(&matches, 0, sizeof(matches));

        int ret = glob(
            token.c_str(),
                       0,
                       NULL,
                       &matches
        );

        if (ret == 0) {
            for (size_t j = 0; j < matches.gl_pathc; ++j) {
                result.push_back(matches.gl_pathv[j]);
            }
        } else {
            result.push_back(token);
        }

        globfree(&matches);
    }

    return result;
}

std::vector<std::string> get_search_paths() {
    std::vector<std::string> paths;

    const char* env_path = std::getenv("PATH");

    if (env_path) {
        std::stringstream ss(env_path);
        std::string item;

        while (std::getline(ss, item, ':')) {
            if (item.empty()) {
                item = ".";
            }

            paths.push_back(item);
        }
    }

    if (paths.empty()) {
        paths.push_back("/bin");
        paths.push_back("/usr/bin");
    }

    return paths;
}

std::string resolve_executable(
    const std::string& command
) {
    if (command.empty()) {
        return "";
    }

    if (
        command[0] == '/' ||
        command.find('/') != std::string::npos
    ) {
        if (access(command.c_str(), X_OK) == 0) {
            return command;
        }

        return "";
    }

    std::vector<std::string> paths = get_search_paths();

    for (size_t i = 0; i < paths.size(); ++i) {
        std::string candidate = join_path(
            paths[i],
            command
        );

        if (access(candidate.c_str(), X_OK) == 0) {
            return candidate;
        }
    }

    return "";
}

void load_aliases() {
    aliases.clear();

    const char* home = std::getenv("HOME");

    if (!home) {
        return;
    }

    std::ifstream file(
        (std::string(home) + "/.shllaliases").c_str()
    );

    if (!file.is_open()) {
        return;
    }

    std::string line;

    while (std::getline(file, line)) {
        line = trim(line);

        if (line.empty() || line[0] == '#') {
            continue;
        }

        size_t equals = line.find('=');

        if (equals == std::string::npos) {
            continue;
        }

        std::string name = lowercase(
            trim(line.substr(0, equals))
        );

        std::string value = trim(
            line.substr(equals + 1)
        );

        if (name.empty() || value.empty()) {
            continue;
        }

        if (
            value.length() >= 2 &&
            (
                (
                    value[0] == '"' &&
                    value[value.length() - 1] == '"'
                ) ||
                (
                    value[0] == '\'' &&
                    value[value.length() - 1] == '\''
                )
            )
        ) {
            value = value.substr(
                1,
                value.length() - 2
            );
        }

        aliases[name] = value;
    }
}

std::vector<std::string> expand_alias(
    const std::vector<std::string>& tokens
) {
    if (tokens.empty()) {
        return tokens;
    }

    std::vector<std::string> result = tokens;

    for (int depth = 0; depth < 20; ++depth) {
        if (result.empty()) {
            break;
        }

        std::string name = lowercase(result[0]);

        std::map<std::string, std::string>::iterator it =
        aliases.find(name);

        if (it == aliases.end()) {
            break;
        }

        std::vector<std::string> alias_tokens =
        tokenize_command(it->second);

        if (alias_tokens.empty()) {
            break;
        }

        if (
            lowercase(alias_tokens[0]) ==
            lowercase(result[0])
        ) {
            break;
        }

        std::vector<std::string> next = alias_tokens;

        for (size_t i = 1; i < result.size(); ++i) {
            next.push_back(result[i]);
        }

        result = next;
    }

    return result;
}

void load_commands() {
    commands.clear();

    const char* builtins[] = {
        "cd",
        "exit",
        "export",
        "refresh",
        "set",
        "shll",
        "chroot"
    };

    for (size_t i = 0; i < 7; ++i) {
        commands.push_back(builtins[i]);
    }

    for (
        std::map<std::string, std::string>::const_iterator it =
        aliases.begin();
    it != aliases.end();
    ++it
    ) {
        commands.push_back(it->first);
    }

    std::vector<std::string> paths = get_search_paths();

    for (size_t p = 0; p < paths.size(); ++p) {
        DIR* directory = opendir(paths[p].c_str());

        if (!directory) {
            continue;
        }

        struct dirent* entry;

        while ((entry = readdir(directory)) != NULL) {
            std::string name = entry->d_name;

            if (name == "." || name == "..") {
                continue;
            }

            std::string executable = join_path(
                paths[p],
                name
            );

            if (access(executable.c_str(), X_OK) == 0) {
                commands.push_back(lowercase(name));
            }
        }

        closedir(directory);
    }

    std::sort(commands.begin(), commands.end());

    commands.erase(
        std::unique(
            commands.begin(),
                    commands.end()
        ),
        commands.end()
    );
}

void set_raw_mode(bool enable) {
    static struct termios old_termios;
    static bool saved = false;

    if (enable) {
        if (
            tcgetattr(
                STDIN_FILENO,
                &old_termios
            ) == 0
        ) {
            struct termios termios = old_termios;

            termios.c_lflag &= ~(ICANON | ECHO);
            termios.c_iflag &= ~(IXON | ICRNL);
            termios.c_oflag |= OPOST;
            termios.c_cc[VMIN] = 1;
            termios.c_cc[VTIME] = 0;

            if (
                tcsetattr(
                    STDIN_FILENO,
                    TCSANOW,
                    &termios
                ) == 0
            ) {
                saved = true;
            }
        }
    } else if (saved) {
        tcsetattr(
            STDIN_FILENO,
            TCSANOW,
            &old_termios
        );
        saved = false;
    }
}

void redraw_line(
    const std::string& prompt,
    const std::string& input,
    size_t cursor
) {
    std::cout
    << "\r"
    << "\033[2K"
    << prompt
    << input
    << "\r"
    << prompt;

    if (cursor > 0) {
        std::cout
        << "\033["
        << cursor
        << "C";
    }

    std::cout << std::flush;
}

std::string longest_common_prefix(
    const std::vector<std::string>& values
) {
    if (values.empty()) {
        return "";
    }

    std::string prefix = values[0];

    for (size_t i = 1; i < values.size(); ++i) {
        size_t j = 0;

        while (
            j < prefix.length() &&
            j < values[i].length() &&
            prefix[j] == values[i][j]
        ) {
            ++j;
        }

        prefix.resize(j);

        if (prefix.empty()) {
            break;
        }
    }

    return prefix;
}

std::vector<std::string> get_file_matches(
    const std::string& prefix,
    std::string& replacement_base
) {
    std::vector<std::string> matches;

    size_t slash = prefix.find_last_of('/');

    std::string directory;
    std::string partial;

    if (slash == std::string::npos) {
        directory = path;
        partial = prefix;
        replacement_base = "";
    } else {
        partial = prefix.substr(slash + 1);

        std::string directory_part =
        prefix.substr(0, slash + 1);

        if (
            !directory_part.empty() &&
            directory_part[0] == '/'
        ) {
            directory = directory_part;
        } else {
            directory = join_path(
                path,
                directory_part
            );
        }

        replacement_base = directory_part;
    }

    DIR* dir = opendir(directory.c_str());

    if (!dir) {
        return matches;
    }

    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;

        if (name == "." || name == "..") {
            continue;
        }

        if (name.rfind(partial, 0) != 0) {
            continue;
        }

        std::string full = join_path(
            directory,
            name
        );

        if (directory_exists(full)) {
            name += "/";
        }

        matches.push_back(name);
    }

    closedir(dir);

    std::sort(matches.begin(), matches.end());

    return matches;
}

void print_completion_matches(
    const std::vector<std::string>& matches
) {
    if (matches.empty()) {
        return;
    }

    std::cout << "\n";

    size_t width = 0;

    for (size_t i = 0; i < matches.size(); ++i) {
        width = std::max(
            width,
            matches[i].length()
        );
    }

    size_t columns = 1;

    if (width < 80) {
        columns = 80 / (width + 2);

        if (columns == 0) {
            columns = 1;
        }
    }

    for (size_t i = 0; i < matches.size(); ++i) {
        std::cout << matches[i];

        if (
            (i + 1) % columns == 0 ||
            i + 1 == matches.size()
        ) {
            std::cout << "\n";
        } else {
            size_t spaces =
            width -
            matches[i].length() +
            2;

            for (size_t j = 0; j < spaces; ++j) {
                std::cout << " ";
            }
        }
    }
}

std::string read_line(
    const std::string& prompt,
    std::vector<std::string>& history
) {
    if (!isatty(STDIN_FILENO)) {
        std::string line;

        if (!std::getline(std::cin, line)) {
            return "";
        }

        return line;
    }

    std::string input;
    size_t cursor = 0;
    size_t history_index = history.size();
    std::string saved_input;
    std::vector<std::string> completion_matches;
    std::string completion_prefix;

    set_raw_mode(true);

    std::cout << prompt << std::flush;

    while (true) {
        char c;

        ssize_t n = read(
            STDIN_FILENO,
            &c,
            1
        );

        if (n == 0) {
            set_raw_mode(false);
            std::cout << "\n";
            return input;
        }

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }

            set_raw_mode(false);
            std::cout << "\n";
            return input;
        }

        if (c == '\n' || c == '\r') {
            std::cout << "\n";

            if (
                !input.empty() &&
                (
                    history.empty() ||
                    history.back() != input
                )
            ) {
                history.push_back(input);
            }

            set_raw_mode(false);
            return input;
        }

        if (c == 3) {
            std::cout << "^C\n";
            set_raw_mode(false);
            return "";
        }

        if (c == 4) {
            if (input.empty()) {
                std::cout << "\n";
                set_raw_mode(false);
                return "";
            }

            continue;
        }

        if (c == 1) {
            cursor = 0;
            redraw_line(prompt, input, cursor);
            continue;
        }

        if (c == 5) {
            cursor = input.length();
            redraw_line(prompt, input, cursor);
            continue;
        }

        if (c == 21) {
            input.erase(0, cursor);
            cursor = 0;
            redraw_line(prompt, input, cursor);
            continue;
        }

        if (c == 11) {
            input.erase(cursor);
            redraw_line(prompt, input, cursor);
            continue;
        }

        if (c == 23) {
            if (cursor > 0) {
                size_t start = cursor;

                while (
                    start > 0 &&
                    std::isspace(
                        static_cast<unsigned char>(
                            input[start - 1]
                        )
                    )
                ) {
                    --start;
                }

                while (
                    start > 0 &&
                    !std::isspace(
                        static_cast<unsigned char>(
                            input[start - 1]
                        )
                    )
                ) {
                    --start;
                }

                input.erase(
                    start,
                    cursor - start
                );

                cursor = start;
                redraw_line(prompt, input, cursor);
            }

            continue;
        }

        if (c == 127 || c == 8) {
            if (cursor > 0) {
                input.erase(cursor - 1, 1);
                --cursor;
                redraw_line(prompt, input, cursor);
            }

            completion_matches.clear();
            continue;
        }

        if (c == '\t') {
            bool completing_command =
            input.find_last_of(" \t", cursor) ==
            std::string::npos;

            size_t word_start = 0;

            if (!completing_command && cursor > 0) {
                word_start = input.find_last_of(
                    " \t",
                    cursor - 1
                );

                if (word_start == std::string::npos) {
                    word_start = 0;
                } else {
                    ++word_start;
                }
            }

            std::string prefix = input.substr(
                word_start,
                cursor - word_start
            );

            if (
                completion_matches.empty() ||
                completion_prefix != prefix
            ) {
                completion_matches.clear();
                completion_prefix = prefix;

                if (completing_command) {
                    std::string lower_prefix =
                    lowercase(prefix);

                    for (size_t i = 0; i < commands.size(); ++i) {
                        if (
                            commands[i].rfind(
                                lower_prefix,
                                0
                            ) == 0
                        ) {
                            completion_matches.push_back(
                                commands[i]
                            );
                        }
                    }
                } else {
                    std::string base;

                    completion_matches =
                    get_file_matches(
                        prefix,
                        base
                    );
                }
            } else if (completion_matches.size() > 1) {
                std::string selected =
                completion_matches[0];

                std::string current = input.substr(
                    word_start,
                    cursor - word_start
                );

                size_t selected_index = 0;

                for (
                    size_t i = 0;
                i < completion_matches.size();
                ++i
                ) {
                    if (
                        completion_matches[i] ==
                        current
                    ) {
                        selected_index = i + 1;
                        break;
                    }
                }

                if (
                    selected_index <
                    completion_matches.size()
                ) {
                    selected =
                    completion_matches[selected_index];
                }

                std::string replacement = selected;

                if (!completing_command) {
                    std::string base;

                    get_file_matches(
                        prefix,
                        base
                    );

                    replacement =
                    base + selected;
                }

                input.replace(
                    word_start,
                    cursor - word_start,
                    replacement
                );

                cursor =
                word_start +
                replacement.length();

                redraw_line(
                    prompt,
                    input,
                    cursor
                );

                completion_prefix = replacement;
                continue;
            }

            if (completion_matches.empty()) {
                continue;
            }

            std::string common =
            longest_common_prefix(
                completion_matches
            );

            if (common.length() > prefix.length()) {
                std::string replacement = common;

                if (!completing_command) {
                    std::string base;

                    get_file_matches(
                        prefix,
                        base
                    );

                    replacement =
                    base + common;
                }

                input.replace(
                    word_start,
                    cursor - word_start,
                    replacement
                );

                cursor =
                word_start +
                replacement.length();

                redraw_line(
                    prompt,
                    input,
                    cursor
                );

                completion_prefix = replacement;
            } else if (completion_matches.size() > 1) {
                print_completion_matches(
                    completion_matches
                );

                redraw_line(
                    prompt,
                    input,
                    cursor
                );
            }

            continue;
        }

        if (c == 27) {
            char sequence[2];

            if (
                read(
                    STDIN_FILENO,
                     &sequence[0],
                     1
                ) != 1
            ) {
                continue;
            }

            if (
                read(
                    STDIN_FILENO,
                     &sequence[1],
                     1
                ) != 1
            ) {
                continue;
            }

            if (
                sequence[0] == '[' &&
                sequence[1] == 'A'
            ) {
                if (history.empty()) {
                    continue;
                }

                if (history_index == history.size()) {
                    saved_input = input;
                }

                if (history_index > 0) {
                    --history_index;

                    input =
                    history[history_index];

                    cursor = input.length();

                    redraw_line(
                        prompt,
                        input,
                        cursor
                    );
                }

                completion_matches.clear();
                continue;
            }

            if (
                sequence[0] == '[' &&
                sequence[1] == 'B'
            ) {
                if (history_index >= history.size()) {
                    continue;
                }

                ++history_index;

                if (history_index == history.size()) {
                    input = saved_input;
                } else {
                    input =
                    history[history_index];
                }

                cursor = input.length();

                redraw_line(
                    prompt,
                    input,
                    cursor
                );

                completion_matches.clear();
                continue;
            }

            if (
                sequence[0] == '[' &&
                sequence[1] == 'C'
            ) {
                if (cursor < input.length()) {
                    ++cursor;

                    redraw_line(
                        prompt,
                        input,
                        cursor
                    );
                }

                continue;
            }

            if (
                sequence[0] == '[' &&
                sequence[1] == 'D'
            ) {
                if (cursor > 0) {
                    --cursor;

                    redraw_line(
                        prompt,
                        input,
                        cursor
                    );
                }

                continue;
            }

            continue;
        }

        if (
            std::isprint(
                static_cast<unsigned char>(c)
            )
        ) {
            input.insert(
                cursor,
                1,
                c
            );

            ++cursor;

            redraw_line(
                prompt,
                input,
                cursor
            );

            history_index = history.size();
            completion_matches.clear();
        }
    }
}


bool change_root(const std::string& new_root) {
    if (new_root.empty()) {
        std::cerr
        << "chroot: missing directory\n";
        return false;
    }

    char resolved[PATH_MAX];

    if (
        realpath(
            new_root.c_str(),
                 resolved
        ) == NULL
    ) {
        std::cerr
        << "chroot: "
        << std::strerror(errno)
        << "\n";

        return false;
    }

    struct stat st;

    if (stat(resolved, &st) != 0) {
        std::cerr
        << "chroot: "
        << std::strerror(errno)
        << "\n";

        return false;
    }

    if (!S_ISDIR(st.st_mode)) {
        std::cerr
        << "chroot: not a directory\n";
        return false;
    }

    if (chroot(resolved) != 0) {
        std::cerr
        << "chroot: "
        << std::strerror(errno)
        << "\n";

        return false;
    }

    if (chdir("/") != 0) {
        std::cerr
        << "chroot: could not change directory\n";
        return false;
    }

    path = "/";
    previous_path = "/";

    return true;
}

bool parse_redirection(
    const std::vector<std::string>& tokens,
    parsed_command& command
) {
    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& token = tokens[i];

        if (token == "<") {
            if (i + 1 >= tokens.size()) {
                std::cerr
                << "syntax error: missing input file\n";
                return false;
            }

            command.input_file = tokens[++i];
            continue;
        }

        if (token == ">" || token == ">>") {
            if (i + 1 >= tokens.size()) {
                std::cerr
                << "syntax error: missing output file\n";
                return false;
            }

            command.output_file = tokens[++i];
            command.append_output = token == ">>";
            continue;
        }

        command.args.push_back(token);
    }

    return true;
}

void wait_for_process(pid_t pid) {
    int status = 0;

    while (true) {
        pid_t result = waitpid(pid, &status, 0);

        if (result == pid) {
            return;
        }

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }

            return;
        }
    }
}

void reset_child_signals() {
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
}

void execute_external(
    const std::vector<std::string>& tokens,
    bool wait_for_child,
    const std::string& input_file,
    const std::string& output_file,
    bool append_output
) {
    if (tokens.empty()) {
        return;
    }

    std::vector<std::string> expanded =
    expand_wildcards(tokens);

    if (expanded.empty()) {
        return;
    }

    std::vector<char*> argv;

    for (size_t i = 0; i < expanded.size(); ++i) {
        argv.push_back(
            const_cast<char*>(
                expanded[i].c_str()
            )
        );
    }

    argv.push_back(NULL);

    pid_t pid = fork();

    if (pid < 0) {
        std::cerr
        << "fork: "
        << std::strerror(errno)
        << "\n";
        return;
    }

    if (pid == 0) {
        reset_child_signals();

        if (!input_file.empty()) {
            int fd = open(
                input_file.c_str(),
                          O_RDONLY
            );

            if (fd < 0) {
                std::cerr
                << input_file
                << ": "
                << std::strerror(errno)
                << "\n";
                _exit(1);
            }

            if (dup2(fd, STDIN_FILENO) < 0) {
                close(fd);
                _exit(1);
            }

            close(fd);
        }

        if (!output_file.empty()) {
            int flags = O_WRONLY | O_CREAT;

            if (append_output) {
                flags |= O_APPEND;
            } else {
                flags |= O_TRUNC;
            }

            int fd = open(
                output_file.c_str(),
                          flags,
                          0666
            );

            if (fd < 0) {
                std::cerr
                << output_file
                << ": "
                << std::strerror(errno)
                << "\n";
                _exit(1);
            }

            if (dup2(fd, STDOUT_FILENO) < 0) {
                close(fd);
                _exit(1);
            }

            close(fd);
        }

        execvp(
            argv[0],
            argv.data()
        );

        std::cerr
        << expanded[0]
        << ": "
        << std::strerror(errno)
        << "\n";

        _exit(127);
    }

    if (wait_for_child) {
        wait_for_process(pid);
    }
}

void parse_shll(
    const std::string& file,
    std::map<std::string, std::string>& local_vars
);

void execute_single_command(
    const std::vector<std::string>& tokens,
    bool& should_exit,
    std::map<std::string, std::string>& local_vars,
    const std::string& input_file = "",
    const std::string& output_file = "",
    bool append_output = false
);

void execute_pipeline(
    const std::vector<std::string>& tokens,
    bool& should_exit,
    std::map<std::string, std::string>& local_vars
) {
    std::vector<std::vector<std::string> > pipeline;
    std::vector<std::string> current;

    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i] == "|") {
            if (current.empty()) {
                std::cerr
                << "syntax error near '|'\n";
                return;
            }

            pipeline.push_back(current);
            current.clear();
        } else {
            current.push_back(tokens[i]);
        }
    }

    if (!current.empty()) {
        pipeline.push_back(current);
    } else if (
        !tokens.empty() &&
        tokens.back() == "|"
    ) {
        std::cerr
        << "syntax error: pipe at end\n";
        return;
    }

    if (pipeline.empty()) {
        return;
    }

    for (size_t i = 0; i < pipeline.size(); ++i) {
        pipeline[i] = expand_alias(pipeline[i]);
    }

    if (pipeline.size() == 1) {
        parsed_command parsed;

        if (!parse_redirection(pipeline[0], parsed)) {
            return;
        }

        if (parsed.args.empty()) {
            return;
        }

        execute_single_command(
            parsed.args,
            should_exit,
            local_vars,
            parsed.input_file,
            parsed.output_file,
            parsed.append_output
        );

        return;
    }

    for (size_t i = 0; i < pipeline.size(); ++i) {
        if (pipeline[i].empty()) {
            continue;
        }

        std::string command =
        lowercase(pipeline[i][0]);

        if (
            command == "cd" ||
            command == "set" ||
            command == "export" ||
            command == "refresh" ||
            command == "chroot" ||
            command == "exit"
        ) {
            std::cerr
            << command
            << ": cannot be used in a pipeline\n";
            return;
        }
    }

    size_t count = pipeline.size();

    std::vector<int> pipefds;

    if (count > 1) {
        pipefds.resize(2 * (count - 1), -1);
    }

    for (size_t i = 0; i + 1 < count; ++i) {
        if (
            pipe2(
                &pipefds[i * 2],
                O_CLOEXEC
            ) < 0
        ) {
            std::cerr
            << "pipe: "
            << std::strerror(errno)
            << "\n";

            for (size_t j = 0; j < pipefds.size(); ++j) {
                if (pipefds[j] >= 0) {
                    close(pipefds[j]);
                }
            }

            return;
        }
    }

    std::vector<pid_t> pids;

    for (size_t i = 0; i < count; ++i) {
        parsed_command parsed;

        if (!parse_redirection(pipeline[i], parsed)) {
            for (size_t j = 0; j < pipefds.size(); ++j) {
                if (pipefds[j] >= 0) {
                    close(pipefds[j]);
                    pipefds[j] = -1;
                }
            }

            for (size_t j = 0; j < pids.size(); ++j) {
                wait_for_process(pids[j]);
            }

            return;
        }

        if (parsed.args.empty()) {
            continue;
        }

        pid_t pid = fork();

        if (pid < 0) {
            std::cerr
            << "fork: "
            << std::strerror(errno)
            << "\n";
            continue;
        }

        if (pid == 0) {
            reset_child_signals();

            if (i > 0) {
                if (
                    dup2(
                        pipefds[(i - 1) * 2],
                         STDIN_FILENO
                    ) < 0
                ) {
                    _exit(1);
                }
            }

            if (i + 1 < count) {
                if (
                    dup2(
                        pipefds[i * 2 + 1],
                         STDOUT_FILENO
                    ) < 0
                ) {
                    _exit(1);
                }
            }

            if (!parsed.input_file.empty()) {
                int fd = open(
                    parsed.input_file.c_str(),
                              O_RDONLY
                );

                if (fd < 0) {
                    std::cerr
                    << parsed.input_file
                    << ": "
                    << std::strerror(errno)
                    << "\n";
                    _exit(1);
                }

                if (dup2(fd, STDIN_FILENO) < 0) {
                    close(fd);
                    _exit(1);
                }

                close(fd);
            }

            if (!parsed.output_file.empty()) {
                int flags = O_WRONLY | O_CREAT;

                if (parsed.append_output) {
                    flags |= O_APPEND;
                } else {
                    flags |= O_TRUNC;
                }

                int fd = open(
                    parsed.output_file.c_str(),
                              flags,
                              0666
                );

                if (fd < 0) {
                    std::cerr
                    << parsed.output_file
                    << ": "
                    << std::strerror(errno)
                    << "\n";
                    _exit(1);
                }

                if (dup2(fd, STDOUT_FILENO) < 0) {
                    close(fd);
                    _exit(1);
                }

                close(fd);
            }

            for (size_t j = 0; j < pipefds.size(); ++j) {
                if (pipefds[j] >= 0) {
                    close(pipefds[j]);
                }
            }

            std::vector<std::string> expanded =
            expand_wildcards(parsed.args);

            if (expanded.empty()) {
                _exit(1);
            }

            std::vector<char*> argv;

            for (size_t j = 0; j < expanded.size(); ++j) {
                argv.push_back(
                    const_cast<char*>(
                        expanded[j].c_str()
                    )
                );
            }

            argv.push_back(NULL);

            execvp(
                argv[0],
                argv.data()
            );

            std::cerr
            << expanded[0]
            << ": "
            << std::strerror(errno)
            << "\n";

            _exit(127);
        }

        pids.push_back(pid);
    }

    for (size_t i = 0; i < pipefds.size(); ++i) {
        if (pipefds[i] >= 0) {
            close(pipefds[i]);
            pipefds[i] = -1;
        }
    }

    for (size_t i = 0; i < pids.size(); ++i) {
        wait_for_process(pids[i]);
    }
}

bool set_variable(
    const std::string& expression,
    std::map<std::string, std::string>& local_vars
) {
    size_t equals = expression.find('=');

    if (equals == std::string::npos) {
        return false;
    }

    std::string name = lowercase(
        trim(expression.substr(0, equals))
    );

    std::string value =
    expression.substr(equals + 1);

    if (name.empty()) {
        return false;
    }

    for (size_t i = 0; i < name.length(); ++i) {
        if (
            !std::isalnum(
                static_cast<unsigned char>(name[i])
            ) &&
            name[i] != '_'
        ) {
            return false;
        }
    }

    local_vars[name] = value;

    return true;
}

void execute_single_command(
    const std::vector<std::string>& tokens,
    bool& should_exit,
    std::map<std::string, std::string>& local_vars,
    const std::string& input_file,
    const std::string& output_file,
    bool append_output
) {
    if (tokens.empty()) {
        return;
    }

    std::string command =
    lowercase(tokens[0]);

    if (command == "set") {
        if (
            tokens.size() == 2 &&
            set_variable(tokens[1], local_vars)
        ) {
            return;
        }

        if (
            tokens.size() >= 4 &&
            tokens[2] == "="
        ) {
            std::string expression =
            tokens[1] + "=" + tokens[3];

            for (size_t i = 4; i < tokens.size(); ++i) {
                expression += " ";
                expression += tokens[i];
            }

            if (set_variable(expression, local_vars)) {
                return;
            }
        }

        std::cerr
        << "set: usage: set name=value\n";

        return;
    }

    if (command == "export") {
        if (tokens.size() < 2) {
            std::cerr
            << "export: usage: export name=value\n";
            return;
        }

        std::string expression = tokens[1];

        for (size_t i = 2; i < tokens.size(); ++i) {
            expression += " ";
            expression += tokens[i];
        }

        size_t equals = expression.find('=');

        if (equals == std::string::npos) {
            const char* value =
            std::getenv(expression.c_str());

            if (value) {
                local_vars[
                    lowercase(expression)
                ] = value;
            }

            return;
        }

        std::string name = lowercase(
            trim(expression.substr(0, equals))
        );

        std::string value =
        expression.substr(equals + 1);

        if (name.empty()) {
            std::cerr
            << "export: invalid variable name\n";
            return;
        }

        setenv(
            name.c_str(),
               value.c_str(),
               1
        );

        local_vars[name] = value;

        return;
    }

    if (command == "cd") {
        std::string target;

        if (tokens.size() == 1) {
            const char* home = std::getenv("HOME");
            target = home ? home : "/";
        } else if (tokens.size() == 2) {
            target = tokens[1];

            if (target == "-") {
                target = previous_path;
            }
        } else {
            std::cerr
            << "cd: too many arguments\n";
            return;
        }

        const char* home = std::getenv("HOME");

        if (home && target == "~") {
            target = home;
        } else if (
            home &&
            target.rfind("~/", 0) == 0
        ) {
            target =
            std::string(home) +
            target.substr(1);
        }

        if (target.empty()) {
            target = "/";
        }

        if (target[0] != '/') {
            target = join_path(path, target);
        }

        target = normalize_path(target);

        if (chdir(target.c_str()) != 0) {
            std::cerr
            << "cd: "
            << std::strerror(errno)
            << ": "
            << target
            << "\n";
            return;
        }

        previous_path = path;
        path = target;

        setenv(
            "PWD",
            path.c_str(),
               1
        );

        setenv(
            "OLDPWD",
            previous_path.c_str(),
               1
        );

        return;
    }

    if (command == "refresh") {
        load_aliases();
        load_commands();
        return;
    }

    if (command == "exit") {
        should_exit = true;
        return;
    }

    if (command == "chroot") {
        if (tokens.size() != 2) {
            std::cerr
            << "chroot: usage: chroot <directory>\n";
            return;
        }

        if (change_root(tokens[1])) {
            setenv(
                "PWD",
                "/",
                1
            );

            load_aliases();
            load_commands();
        }

        return;
    }

    if (command == "shll") {
        if (tokens.size() < 2) {
            std::cerr
            << "shll: expected file\n";
            return;
        }

        parse_shll(
            tokens[1],
            local_vars
        );

        return;
    }

    if (
        command.length() > 5 &&
        command.substr(command.length() - 5) == ".shll"
    ) {
        parse_shll(
            command,
            local_vars
        );

        return;
    }

    execute_external(
        tokens,
        true,
        input_file,
        output_file,
        append_output
    );
}

void parse_shll(
    const std::string& file,
    std::map<std::string, std::string>& local_vars
) {
    std::ifstream input(file.c_str());

    if (!input.is_open()) {
        std::cerr
        << "shll: "
        << file
        << ": "
        << std::strerror(errno)
        << "\n";
        return;
    }

    std::string line;
    bool script_exit = false;

    while (
        std::getline(input, line) &&
        !script_exit &&
        !should_exit_shell
    ) {
        line = trim(line);

        if (line.empty() || line[0] == '#') {
            continue;
        }

        line = expand_vars(
            line,
            local_vars
        );

        line = expand_environment_variables(line);

        std::vector<std::string> tokens =
        tokenize_command(line);

        if (tokens.empty()) {
            continue;
        }

        execute_pipeline(
            tokens,
            script_exit,
            local_vars
        );
    }
}

void shell_signal_handler(int signal_number) {
    if (signal_number == SIGINT) {
        const char message[] = "\n";
        write(
            STDOUT_FILENO,
            message,
            sizeof(message) - 1
        );
    }
}

int main() {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, shell_signal_handler);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    if (!std::getenv("PATH")) {
        setenv(
            "PATH",
            "/usr/local/bin:/usr/bin:/bin",
            1
        );
    }

    load_aliases();
    load_commands();

    char cwd[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        path = cwd;
    } else {
        path = "/";
    }

    previous_path = path;

    std::string hostname = load_hostname();

    setenv(
        "HOSTNAME",
        hostname.c_str(),
           1
    );

    setenv(
        "PWD",
        path.c_str(),
           1
    );

    std::map<std::string, std::string> session_vars;
    std::vector<std::string> history;

    const char* user = std::getenv("USER");

    if (!user) {
        user = "user";
    }

    std::cout << VERSION << '\n';

    while (!should_exit_shell) {
        const char* hostname_env =
        std::getenv("HOSTNAME");

        if (!hostname_env) {
            hostname_env = "localhost";
        }

        std::string prompt =
        color_green +
        std::string(user) +
        "@" +
        hostname_env +
        color_reset +
        ":" +
        color_blue +
        get_prompt_path() +
        color_reset +
        "$ ";

        std::string line =
        read_line(
            prompt,
            history
        );

        if (
            line.empty() &&
            !isatty(STDIN_FILENO) &&
            std::cin.eof()
        ) {
            break;
        }

        if (
            line.empty() &&
            isatty(STDIN_FILENO)
        ) {
            continue;
        }

        line = expand_vars(
            line,
            session_vars
        );

        line = expand_environment_variables(line);

        std::vector<std::string> tokens =
        tokenize_command(line);

        if (tokens.empty()) {
            continue;
        }

        execute_pipeline(
            tokens,
            should_exit_shell,
            session_vars
        );
    }

    set_raw_mode(false);

    return 0;
}
