#define _DEFAULT_SOURCE
#define _GNU_SOURCE

#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

/* ============================================================
 * CEDIT
 * Small terminal C/C++ editor
 * ============================================================ */

#define CTRL_KEY(k) ((k) & 0x1f)

#define EDITOR_VERSION "0.3.0"
#define EDITOR_TAB_STOP 4
#define EDITOR_QUIT_TIMES 2

#define ABUF_INIT {NULL, 0}

/* Highlight flags */
#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)

/* ============================================================
 * Keys
 * ============================================================ */

enum editorKey {
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

/* ============================================================
 * Syntax highlighting
 * ============================================================ */

enum editorHighlight {
    HL_NORMAL = 0,
    HL_COMMENT,
    HL_MLCOMMENT,
    HL_KEYWORD1,
    HL_KEYWORD2,
    HL_STRING,
    HL_NUMBER,
    HL_MATCH,
    HL_PREPROCESSOR,
    HL_FUNCTION,
    HL_TYPE,
    HL_OPERATOR
};

/* ============================================================
 * Editor row
 * ============================================================ */

typedef struct erow {
    int size;
    int rsize;

    char *chars;
    char *render;

    unsigned char *hl;

    int idx;
    int hl_open_comment;
} erow;

/* ============================================================
 * Syntax definition
 * ============================================================ */

struct editorSyntax {
    char *filetype;
    char **filematch;
    char **keywords;

    char *singleline_comment_start;
    char *multiline_comment_start;
    char *multiline_comment_end;

    int flags;
};

/* ============================================================
 * Editor configuration
 * ============================================================ */

struct editorConfig {
    struct termios orig_termios;

    int screenrows;
    int screencols;

    int numrows;

    int cx;
    int cy;

    int rx;

    int rowoff;
    int coloff;

    erow *row;

    int dirty;

    char *filename;

    char statusmsg[120];
    time_t statusmsg_time;

    struct editorSyntax *syntax;
};

struct editorConfig E;

/* ============================================================
 * C / C++ syntax database
 * ============================================================ */

char *C_HL_extensions[] = {
    ".c",
    ".h",
    ".cc",
    ".hh",
    ".cpp",
    ".hpp",
    ".cxx",
    ".hxx",
    NULL
};

char *C_HL_keywords[] = {
    /* Control flow */
    "if",
    "else",
    "for",
    "while",
    "do",
    "switch",
    "case",
    "default",
    "break",
    "continue",
    "return",
    "goto",

    /* C */
    "struct",
    "union",
    "enum",
    "typedef",
    "sizeof",
    "static",
    "extern",
    "const",
    "volatile",
    "restrict",
    "inline",

    /* C++ */
    "class",
    "public",
    "private",
    "protected",
    "virtual",
    "override",
    "final",
    "template",
    "typename",
    "namespace",
    "using",
    "this",
    "new",
    "delete",
    "try",
    "catch",
    "throw",
    "noexcept",
    "nullptr",
    "true",
    "false",
    "friend",
    "operator",
    "explicit",
    "constexpr",
    "consteval",
    "constinit",
    "static_cast",
    "dynamic_cast",
    "reinterpret_cast",
    "const_cast",

    /* Types */
    "void|",
    "char|",
    "short|",
    "int|",
    "long|",
    "float|",
    "double|",
    "signed|",
    "unsigned|",
    "bool|",
    "wchar_t|",
    "size_t|",
    "ptrdiff_t|",

    /* Standard library */
    "std|",
    "string|",
    "vector|",
    "array|",
    "map|",
    "unordered_map|",
    "set|",
    "unordered_set|",
    "list|",
    "deque|",
    "queue|",
    "stack|",
    "pair|",
    "tuple|",
    "optional|",
    "variant|",
    "unique_ptr|",
    "shared_ptr|",
    "weak_ptr|",

    /* Streams */
    "cout|",
    "cin|",
    "cerr|",
    "clog|",
    "endl|",

    /* Common C library */
    "printf|",
    "fprintf|",
    "sprintf|",
    "snprintf|",
    "scanf|",
    "sscanf|",
    "malloc|",
    "calloc|",
    "realloc|",
    "free|",
    "memcpy|",
    "memmove|",
    "memset|",
    "strlen|",
    "strcmp|",
    "strstr|",

    NULL
};

struct editorSyntax HLDB[] = {
    {
        "c/c++",
        C_HL_extensions,
        C_HL_keywords,
        "//",
        "/*",
        "*/",
        HL_HIGHLIGHT_NUMBERS |
        HL_HIGHLIGHT_STRINGS
    }
};

#define HLDB_ENTRIES \
(sizeof(HLDB) / sizeof(HLDB[0]))

/* ============================================================
 * Prototypes
 * ============================================================ */

void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen(void);
char *editorPrompt(char *prompt, void (*callback)(char *, int));

void editorUpdateRow(erow *row);
void editorUpdateSyntax(erow *row);

void editorRowInsertChar(erow *row, int at, int c);
void editorRowDelChar(erow *row, int at);
void editorRowAppendString(erow *row, char *s, size_t len);

void editorInsertRow(int at, char *s, size_t len);
void editorDelRow(int at);
void editorFreeRow(erow *row);

void editorInsertChar(int c);
void editorInsertNewline(void);
void editorDelChar(void);

void editorMoveCursor(int key);

/* ============================================================
 * Terminal
 * ============================================================ */

void die(const char *s)
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

void disableRawMode(void)
{
    if (tcsetattr(
        STDIN_FILENO,
        TCSAFLUSH,
        &E.orig_termios) == -1) {
        die("tcsetattr");
        }
}

void enableRawMode(void)
{
    if (tcgetattr(
        STDIN_FILENO,
        &E.orig_termios) == -1) {
        die("tcgetattr");
        }

        atexit(disableRawMode);

    struct termios raw = E.orig_termios;

    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_oflag &= ~(OPOST);
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_cflag |= CS8;

    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(
        STDIN_FILENO,
        TCSAFLUSH,
        &raw) == -1) {
        die("tcsetattr");
        }
}

/* ============================================================
 * Input
 * ============================================================ */

int editorReadKey(void)
{
    int nread;
    char c;

    while ((nread = read(
        STDIN_FILENO,
        &c,
        1)) != 1) {

        if (nread == -1 &&
            errno != EAGAIN) {
            die("read");
            }
        }

        if (c == '\x1b') {
            char seq[3];

            if (read(STDIN_FILENO, &seq[0], 1) != 1) {
                return '\x1b';
            }

            if (read(STDIN_FILENO, &seq[1], 1) != 1) {
                return '\x1b';
            }

            if (seq[0] == '[') {

                if (seq[1] >= '0' &&
                    seq[1] <= '9') {

                    if (read(
                        STDIN_FILENO,
                        &seq[2],
                        1) != 1) {
                        return '\x1b';
                        }

                        if (seq[2] == '~') {
                            switch (seq[1]) {
                                case '1':
                                case '7':
                                    return HOME_KEY;

                                case '3':
                                    return DEL_KEY;

                                case '4':
                                case '8':
                                    return END_KEY;

                                case '5':
                                    return PAGE_UP;

                                case '6':
                                    return PAGE_DOWN;
                            }
                        }
                    } else {
                        switch (seq[1]) {
                            case 'A':
                                return ARROW_UP;

                            case 'B':
                                return ARROW_DOWN;

                            case 'C':
                                return ARROW_RIGHT;

                            case 'D':
                                return ARROW_LEFT;

                            case 'H':
                                return HOME_KEY;

                            case 'F':
                                return END_KEY;
                        }
                    }
            } else if (seq[0] == 'O') {
                switch (seq[1]) {
                    case 'H':
                        return HOME_KEY;

                    case 'F':
                        return END_KEY;
                }
            }

            return '\x1b';
        }

        return c;
}

/* ============================================================
 * Window
 * ============================================================ */

int getCursorPosition(int *rows, int *cols)
{
    char buf[32];
    unsigned int i = 0;

    if (write(
        STDOUT_FILENO,
        "\x1b[6n",
        4) != 4) {
        return -1;
        }

        while (i < sizeof(buf) - 1) {
            if (read(
                STDIN_FILENO,
                &buf[i],
                1) != 1) {
                break;
                }

                if (buf[i] == 'R') {
                    break;
                }

                i++;
        }

        buf[i] = '\0';

        if (buf[0] != '\x1b' ||
            buf[1] != '[') {
            return -1;
            }

            if (sscanf(
                &buf[2],
                "%d;%d",
                rows,
                cols) != 2) {
                return -1;
                }

                return 0;
}

int getWindowSize(int *rows, int *cols)
{
    struct winsize ws;

    if (ioctl(
        STDOUT_FILENO,
        TIOCGWINSZ,
        &ws) == -1 ||
        ws.ws_col == 0) {

        if (write(
            STDOUT_FILENO,
            "\x1b[999C\x1b[999B",
            12) != 12) {
            return -1;
            }

            return getCursorPosition(rows, cols);
        }

        *cols = ws.ws_col;
        *rows = ws.ws_row;

        return 0;
}

/* ============================================================
 * Syntax helpers
 * ============================================================ */

int is_separator(int c)
{
    return isspace((unsigned char)c) ||
    c == '\0' ||
    strchr(
        ",.()+-/*=~%<>[];:{}!&|^?",
           c) != NULL;
}

int is_operator_char(char c)
{
    return strchr(
        "+-*/%=!<>|&^~?:",
        c) != NULL;
}

/*
 * Check whether a rendered line is an #include line.
 *
 * Examples:
 *
 * #include <iostream>
 *   #include "stdio.h"
 *
 * Leading whitespace is allowed.
 */
int is_include_line(const char *s, int len)
{
    int i = 0;

    while (i < len &&
        isspace((unsigned char)s[i])) {
        i++;
        }

        if (i >= len || s[i] != '#') {
            return 0;
        }

        i++;

    while (i < len &&
        isspace((unsigned char)s[i])) {
        i++;
        }

        if (i + 7 > len) {
            return 0;
        }

        if (strncmp(
            &s[i],
            "include",
            7) != 0) {
            return 0;
            }

            i += 7;

        return i >= len ||
        isspace((unsigned char)s[i]) ||
        s[i] == '<' ||
        s[i] == '"';
}

/* ============================================================
 * Syntax highlighting
 * ============================================================ */

int editorSyntaxToColor(int hl)
{
    switch (hl) {
        case HL_COMMENT:
        case HL_MLCOMMENT:
            return 36; /* cyan */

        case HL_KEYWORD1:
            return 33; /* yellow */

        case HL_KEYWORD2:
        case HL_TYPE:
            return 32; /* green */

        case HL_STRING:
            return 35; /* magenta */

        case HL_NUMBER:
            return 31; /* red */

        case HL_MATCH:
            return 44; /* blue background */

        case HL_PREPROCESSOR:
            return 91; /* bright red */

        case HL_FUNCTION:
            return 96; /* bright cyan */

        case HL_OPERATOR:
            return 94; /* bright blue */

        default:
            return 37;
    }
}

void editorUpdateSyntax(erow *row)
{
    int alloc_size =
    row->rsize > 0 ? row->rsize : 1;

    row->hl = realloc(
        row->hl,
        alloc_size
    );

    if (row->rsize > 0) {
        memset(
            row->hl,
            HL_NORMAL,
            row->rsize
        );
    }

    if (E.syntax == NULL) {
        return;
    }

    char **keywords =
    E.syntax->keywords;

    char *scs =
    E.syntax->singleline_comment_start;

    char *mcs =
    E.syntax->multiline_comment_start;

    char *mce =
    E.syntax->multiline_comment_end;

    int scs_len =
    scs ? strlen(scs) : 0;

    int mcs_len =
    mcs ? strlen(mcs) : 0;

    int mce_len =
    mce ? strlen(mce) : 0;

    int in_comment =
    row->idx > 0 &&
    E.row[row->idx - 1].hl_open_comment;

    int in_string = 0;
    int prev_sep = 1;

    int preprocessor = 0;

    int first_nonspace = 0;

    while (first_nonspace < row->rsize &&
        isspace((unsigned char)
        row->render[first_nonspace])) {
        first_nonspace++;
        }

        if (first_nonspace < row->rsize &&
            row->render[first_nonspace] == '#') {
            preprocessor = 1;

        /*
         * Give the whole preprocessor line its
         * base color first. Strings/header names
         * will override it below.
         */
        for (int j = first_nonspace;
             j < row->rsize;
            j++) {
            row->hl[j] =
            HL_PREPROCESSOR;
            }
            }

            int include_line =
            is_include_line(
                row->render,
                row->rsize
            );

            int i = 0;

            while (i < row->rsize) {
                char c = row->render[i];

                unsigned char prev_hl =
                (i > 0)
                ? row->hl[i - 1]
                : HL_NORMAL;

                /* ----------------------------------------------------
                 * Multi-line comments
                 * ---------------------------------------------------- */

                if (in_comment) {
                    row->hl[i] = HL_MLCOMMENT;

                    if (mce_len &&
                        !strncmp(
                            &row->render[i],
                            mce,
                            mce_len)) {

                        int n = mce_len;

                    if (i + n > row->rsize) {
                        n = row->rsize - i;
                    }

                    memset(
                        &row->hl[i],
                        HL_MLCOMMENT,
                        n
                    );

                    i += mce_len;
                    in_comment = 0;
                    prev_sep = 1;

                    continue;
                            }

                            i++;
                            continue;
                }

                /* ----------------------------------------------------
                 * Single-line comments
                 * ---------------------------------------------------- */

                if (!in_string &&
                    scs_len &&
                    !strncmp(
                        &row->render[i],
                        scs,
                        scs_len)) {

                    memset(
                        &row->hl[i],
                        HL_COMMENT,
                        row->rsize - i
                    );

                    break;
                        }

                        /* ----------------------------------------------------
                         * Start multi-line comment
                         * ---------------------------------------------------- */

                        if (!in_string &&
                            mcs_len &&
                            !strncmp(
                                &row->render[i],
                                mcs,
                                mcs_len)) {

                            memset(
                                &row->hl[i],
                                HL_MLCOMMENT,
                                mcs_len
                            );

                            i += mcs_len;
                            in_comment = 1;

                            continue;
                                }

                                /* ----------------------------------------------------
                                 * C++ #include <header>
                                 *
                                 * Treat the entire angle-bracket header as a string.
                                 * This makes:
                                 *
                                 * #include <iostream>
                                 *
                                 * behave more naturally.
                                 * ---------------------------------------------------- */

                                if (include_line &&
                                    !in_string &&
                                    c == '<') {

                                    int j = i + 1;

                                while (j < row->rsize &&
                                    row->render[j] != '>') {
                                    j++;
                                    }

                                    if (j < row->rsize) {
                                        memset(
                                            &row->hl[i],
                                            HL_STRING,
                                            j - i + 1
                                        );

                                        i = j + 1;
                                        prev_sep = 1;

                                        continue;
                                    }
                                    }

                                    /* ----------------------------------------------------
                                     * Strings
                                     * ---------------------------------------------------- */

                                    if (E.syntax->flags &
                                        HL_HIGHLIGHT_STRINGS) {

                                        if (in_string) {
                                            row->hl[i] = HL_STRING;

                                            if (c == '\\' &&
                                                i + 1 < row->rsize) {

                                                row->hl[i + 1] =
                                                HL_STRING;

                                            i += 2;
                                            continue;
                                                }

                                                if (c == in_string) {
                                                    in_string = 0;
                                                }

                                                i++;
                                                prev_sep = 1;

                                                continue;
                                        }

                                        if (c == '"' ||
                                            c == '\'') {

                                            in_string = c;

                                        row->hl[i] =
                                        HL_STRING;

                                        i++;
                                        continue;
                                            }
                                        }

                                        /* ----------------------------------------------------
                                         * Numbers
                                         * ---------------------------------------------------- */

                                        if (E.syntax->flags &
                                            HL_HIGHLIGHT_NUMBERS) {

                                            if ((isdigit(
                                                (unsigned char)c) &&
                                                (prev_sep ||
                                                prev_hl == HL_NUMBER)) ||
                                                (c == '.' &&
                                                prev_hl == HL_NUMBER)) {

                                                row->hl[i] =
                                                HL_NUMBER;

                                            i++;

                                            prev_sep = 0;

                                            continue;
                                                }
                                            }

                                            /* ----------------------------------------------------
                                             * Operators
                                             * ---------------------------------------------------- */

                                            if (is_operator_char(c) &&
                                                !preprocessor) {

                                                row->hl[i] =
                                                HL_OPERATOR;
                                                }

                                                /* ----------------------------------------------------
                                                 * Keywords / identifiers
                                                 * ---------------------------------------------------- */

                                                if (prev_sep &&
                                                    (isalpha((unsigned char)c) ||
                                                    c == '_')) {

                                                    int start = i;

                                                while (i < row->rsize &&
                                                    (isalnum(
                                                        (unsigned char)
                                                        row->render[i]) ||
                                                        row->render[i] == '_')) {
                                                    i++;
                                                        }

                                                        int word_len = i - start;

                                                        int found_keyword = 0;

                                                        for (int j = 0;
                                                             keywords[j] != NULL;
                                                    j++) {

                                                            int klen =
                                                            strlen(keywords[j]);

                                                            int kw2 =
                                                            keywords[j][klen - 1] == '|';

                                                            if (kw2) {
                                                                klen--;
                                                            }

                                                            if (klen == word_len &&
                                                                !strncmp(
                                                                    &row->render[start],
                                                                    keywords[j],
                                                                    klen)) {

                                                                /*
                                                                 * Only accept the keyword if the
                                                                 * character following it is a
                                                                 * separator.
                                                                 */
                                                                if (i < row->rsize &&
                                                                    !is_separator(
                                                                        row->render[i])) {
                                                                    continue;
                                                                        }

                                                                        int color =
                                                                        kw2
                                                                        ? HL_KEYWORD2
                                                                        : HL_KEYWORD1;

                                                                        memset(
                                                                            &row->hl[start],
                                                                            color,
                                                                            word_len
                                                                        );

                                                                        found_keyword = 1;

                                                                        /*
                                                                         * Types are green, even if they
                                                                         * happen to be followed by '('.
                                                                         */
                                                                        if (kw2) {
                                                                            memset(
                                                                                &row->hl[start],
                                                                                HL_TYPE,
                                                                                word_len
                                                                            );
                                                                        }

                                                                        break;
                                                                    }
                                                    }

                                                    if (!found_keyword) {
                                                        int after = i;

                                                        while (after < row->rsize &&
                                                            isspace(
                                                                (unsigned char)
                                                                row->render[after])) {
                                                            after++;
                                                                }

                                                                if (after < row->rsize &&
                                                                    row->render[after] == '(') {

                                                                    memset(
                                                                        &row->hl[start],
                                                                        HL_FUNCTION,
                                                                        word_len
                                                                    );
                                                                    }
                                                    }

                                                    prev_sep = 0;
                                                    continue;
                                                    }

                                                    prev_sep =
                                                    is_separator(
                                                        (unsigned char)c
                                                    );

                                                    i++;
            }

            int changed =
            row->hl_open_comment != in_comment;

            row->hl_open_comment =
            in_comment;

            if (changed &&
                row->idx + 1 < E.numrows) {

                editorUpdateSyntax(
                    &E.row[row->idx + 1]
                );
                }
}

/* ============================================================
 * Syntax selection
 * ============================================================ */

void editorSelectSyntaxHighlight(void)
{
    E.syntax = NULL;

    if (E.filename == NULL) {
        return;
    }

    char *ext =
    strrchr(
        E.filename,
        '.'
    );

    for (unsigned int j = 0;
         j < HLDB_ENTRIES;
    j++) {

        struct editorSyntax *s =
        &HLDB[j];

        unsigned int i = 0;

        while (s->filematch[i]) {
            int is_ext =
            s->filematch[i][0] == '.';

            if ((is_ext &&
                ext &&
                !strcmp(
                    ext,
                    s->filematch[i])) ||
                    (!is_ext &&
                    strstr(
                        E.filename,
                        s->filematch[i]))) {

                E.syntax = s;

            for (int filerow = 0;
                 filerow < E.numrows;
                filerow++) {

                editorUpdateSyntax(
                    &E.row[filerow]
                );
                }

                return;
                        }

                        i++;
        }
    }
}

/* ============================================================
 * Row rendering
 * ============================================================ */

void editorUpdateRow(erow *row)
{
    int tabs = 0;

    for (int j = 0;
         j < row->size;
    j++) {

        if (row->chars[j] == '\t') {
            tabs++;
        }
    }

    free(row->render);

    row->render =
    malloc(
        row->size +
        tabs *
        (EDITOR_TAB_STOP - 1) +
        1
    );

    int idx = 0;

    for (int j = 0;
         j < row->size;
    j++) {

        if (row->chars[j] == '\t') {
            row->render[idx++] = ' ';

            while (idx %
                EDITOR_TAB_STOP != 0) {
                row->render[idx++] =
                ' ';
                }
        } else {
            row->render[idx++] =
            row->chars[j];
        }
    }

    row->render[idx] = '\0';
    row->rsize = idx;

    editorUpdateSyntax(row);
}

/* ============================================================
 * Rows
 * ============================================================ */

void editorInsertRow(
    int at,
    char *s,
    size_t len)
{
    if (at < 0 ||
        at > E.numrows) {
        return;
        }

        E.row = realloc(
            E.row,
            sizeof(erow) *
            (E.numrows + 1)
        );

    memmove(
        &E.row[at + 1],
        &E.row[at],
        sizeof(erow) *
        (E.numrows - at)
    );

    E.row[at].idx = at;
    E.row[at].size = len;

    E.row[at].chars =
    malloc(len + 1);

    memcpy(
        E.row[at].chars,
        s,
        len
    );

    E.row[at].chars[len] =
    '\0';

    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    E.row[at].hl = NULL;
    E.row[at].hl_open_comment = 0;

    E.numrows++;

    for (int j = at + 1;
         j < E.numrows;
    j++) {

        E.row[j].idx = j;
    }

    editorUpdateRow(
        &E.row[at]
    );

    E.dirty++;
}

void editorFreeRow(erow *row)
{
    free(row->render);
    free(row->chars);
    free(row->hl);
}

void editorDelRow(int at)
{
    if (at < 0 ||
        at >= E.numrows) {
        return;
        }

        editorFreeRow(
            &E.row[at]
        );

    memmove(
        &E.row[at],
        &E.row[at + 1],
        sizeof(erow) *
        (E.numrows - at - 1)
    );

    E.numrows--;

    for (int j = at;
         j < E.numrows;
    j++) {

        E.row[j].idx = j;
    }

    E.dirty++;
}

/* ============================================================
 * Character insertion / deletion
 * ============================================================ */

void editorRowInsertChar(
    erow *row,
    int at,
    int c)
{
    if (at < 0 ||
        at > row->size) {
        at = row->size;
        }

        row->chars =
        realloc(
            row->chars,
            row->size + 2
        );

    memmove(
        &row->chars[at + 1],
        &row->chars[at],
        row->size - at + 1
    );

    row->chars[at] = c;
    row->size++;

    editorUpdateRow(row);

    E.dirty++;
}

void editorRowDelChar(
    erow *row,
    int at)
{
    if (at < 0 ||
        at >= row->size) {
        return;
        }

        memmove(
            &row->chars[at],
            &row->chars[at + 1],
            row->size - at
        );

    row->size--;

    editorUpdateRow(row);

    E.dirty++;
}

void editorRowAppendString(
    erow *row,
    char *s,
    size_t len)
{
    row->chars =
    realloc(
        row->chars,
        row->size + len + 1
    );

    memcpy(
        &row->chars[row->size],
        s,
        len
    );

    row->size += len;

    row->chars[row->size] =
    '\0';

    editorUpdateRow(row);

    E.dirty++;
}

/* ============================================================
 * Indentation helpers
 * ============================================================ */

int get_leading_indent(
    erow *row)
{
    int spaces = 0;

    for (int i = 0;
         i < row->size;
    i++) {

        if (row->chars[i] == ' ') {
            spaces++;
        } else if (row->chars[i] == '\t') {
            spaces += EDITOR_TAB_STOP;
        } else {
            break;
        }
    }

    return spaces;
}

/*
 * Look backwards from the cursor and determine whether
 * the meaningful character immediately before it is '{'.
 *
 * Example:
 *
 * if (x) {|
 *
 * returns true.
 */
int cursor_preceded_by_open_brace(
    erow *row)
{
    int i = E.cx - 1;

    while (i >= 0 &&
        isspace(
            (unsigned char)
            row->chars[i])) {
        i--;
            }

            return i >= 0 &&
            row->chars[i] == '{';
}

/*
 * Look forward from the cursor and determine whether
 * the meaningful character is '}'.
 *
 * Example:
 *
 * {|}
 *
 * returns true.
 */
int cursor_followed_by_closing_brace(
    erow *row)
{
    int i = E.cx;

    while (i < row->size &&
        isspace(
            (unsigned char)
            row->chars[i])) {
        i++;
            }

            return i < row->size &&
            row->chars[i] == '}';
}

/*
 * Does the text before the cursor end in an opening brace?
 */
int line_before_cursor_opens_block(
    erow *row)
{
    return cursor_preceded_by_open_brace(row);
}

/*
 * Does the text after the cursor begin with a closing brace?
 */
int line_after_cursor_closes_block(
    erow *row)
{
    return cursor_followed_by_closing_brace(row);
}

/* ============================================================
 * Smart newline
 * ============================================================ */

void editorInsertNewline(void)
{
    /*
     * If there are no rows, create one first.
     */
    if (E.cy == E.numrows) {
        editorInsertRow(
            E.numrows,
            "",
            0
        );
    }

    erow *row =
    &E.row[E.cy];

    /*
     * Base indentation comes from the current line.
     */
    int base_indent =
    get_leading_indent(row);

    /*
     * Determine what is immediately before and
     * after the cursor.
     */
    int opens_block =
    line_before_cursor_opens_block(row);

    int closes_block =
    line_after_cursor_closes_block(row);

    /*
     * We are specifically inside an automatically
     * created pair:
     *
     * {|}
     *
     * or:
     *
     * {   |   }
     */
    int inside_empty_pair =
    opens_block &&
    closes_block;

    /*
     * Child indentation.
     *
     * If the current line ends with `{`,
     * indent one level.
     */
    int child_indent =
    base_indent;

    if (opens_block) {
        child_indent +=
        EDITOR_TAB_STOP;
    }

    /*
     * Text to the right of the cursor.
     */
    int right_start = E.cx;

    while (right_start < row->size &&
        (row->chars[right_start] == ' ' ||
        row->chars[right_start] == '\t')) {
        right_start++;
        }

        int right_len =
        row->size - E.cx;

    char *right =
    malloc(right_len + 1);

    memcpy(
        right,
        &row->chars[E.cx],
        right_len
    );

    right[right_len] = '\0';

    /*
     * --------------------------------------------------------
     * SPECIAL CASE:
     *
     * {
     *     |
     * }
     *
     * When pressing Enter between `{` and `}`, VS Code-style
     * behavior is to create TWO lines and put the cursor on
     * the indented middle line.
     *
     * Existing:
     *
     *     {|}
     *
     * becomes:
     *
     *     {
     *         |
     *     }
     * --------------------------------------------------------
     */
    if (inside_empty_pair) {
        /*
         * Remove everything after the cursor from
         * the original line.
         */
        row->size = E.cx;
        row->chars[E.cx] = '\0';

        editorUpdateRow(row);

        /*
         * The line containing the user's code gets
         * one extra indentation level.
         */
        char *middle =
        malloc(
            child_indent + 1
        );

        memset(
            middle,
            ' ',
            child_indent
        );

        middle[child_indent] =
        '\0';

        editorInsertRow(
            E.cy + 1,
            middle,
            child_indent
        );

        free(middle);

        /*
         * Put the closing brace on another line at
         * the parent's indentation level.
         */
        char *closing =
        malloc(
            base_indent + 2
        );

        memset(
            closing,
            ' ',
            base_indent
        );

        closing[base_indent] = '}';
        closing[base_indent + 1] =
        '\0';

        /*
         * The original `}` was removed from the
         * current line, so insert the closing line.
         */
        editorInsertRow(
            E.cy + 2,
            closing,
            base_indent + 1
        );

        free(closing);

        E.cy++;
        E.cx = child_indent;

        free(right);

        return;
    }

    /*
     * --------------------------------------------------------
     * NORMAL SPLIT
     * --------------------------------------------------------
     */

    /*
     * Remove everything to the right of the cursor
     * from the current line.
     */
    row->size = E.cx;
    row->chars[E.cx] = '\0';

    editorUpdateRow(row);

    /*
     * If the new line starts with `}`, it should be
     * aligned with the parent block instead of the
     * child block.
     *
     * Example:
     *
     *     if (x) {
     *         |
     *     }
     *
     * Pressing Enter immediately before `}` should
     * preserve the closing brace indentation.
     */
    int new_indent =
    child_indent;

    if (right_start < row->size + right_len &&
        right[0] == '}') {

        new_indent =
        base_indent;
        }

        /*
         * If the text to the right contains leading
         * whitespace before `}`, strip that whitespace.
         */
        int right_content_start = 0;

        while (right_content_start < right_len &&
            (right[right_content_start] == ' ' ||
            right[right_content_start] == '\t')) {
            right_content_start++;
            }

            int right_content_len =
            right_len - right_content_start;

        /*
         * If the right-hand text starts with a closing
         * brace, it gets the parent indentation.
         */
        if (right_content_start < right_len &&
            right[right_content_start] == '}') {

            new_indent =
            base_indent;
            }

            /*
             * Create the new line.
             */
            int new_len =
            new_indent +
            right_content_len;

            char *newchars =
            malloc(new_len + 1);

            memset(
                newchars,
                ' ',
                new_indent
            );

            if (right_content_len > 0) {
                memcpy(
                    &newchars[new_indent],
                    &right[right_content_start],
                    right_content_len
                );
            }

            newchars[new_len] = '\0';

            editorInsertRow(
                E.cy + 1,
                newchars,
                new_len
            );

            free(newchars);
            free(right);

            E.cy++;
            E.cx = new_indent;
}

/* ============================================================
 * #include helper
 * ============================================================ */

int editorCursorInIncludeContext(
    erow *row)
{
    if (E.cx < 0 ||
        E.cx > row->size) {
        return 0;
        }

        /*
         * Look at the text before the cursor.
         */
        int len = E.cx;

    int i = 0;

    while (i < len &&
        isspace(
            (unsigned char)
            row->chars[i])) {
        i++;
            }

            if (i >= len ||
                row->chars[i] != '#') {
                return 0;
                }

                i++;

            while (i < len &&
                isspace(
                    (unsigned char)
                    row->chars[i])) {
                i++;
                    }

                    if (i + 7 > len) {
                        return 0;
                    }

                    if (strncmp(
                        &row->chars[i],
                        "include",
                        7) != 0) {
                        return 0;
                        }

                        i += 7;

                    /*
                     * After "include", allow whitespace.
                     */
                    while (i < len &&
                        isspace(
                            (unsigned char)
                            row->chars[i])) {
                        i++;
                            }

                            /*
                             * If we are typing the header name, angle
                             * brackets should auto-pair.
                             */
                            return i <= len;
}

/* ============================================================
 * Smart character insertion
 * ============================================================ */

void editorInsertChar(int c)
{
    if (E.cy == E.numrows) {
        editorInsertRow(
            E.numrows,
            "",
            0
        );
    }

    erow *row =
    &E.row[E.cy];

    /*
     * --------------------------------------------------------
     * Special C++ include handling
     *
     * #include <|
     *
     * becomes:
     *
     * #include <|>
     * --------------------------------------------------------
     */
    if (c == '<' &&
        editorCursorInIncludeContext(row)) {

        /*
         * Do not create a second pair if a > is already
         * immediately under the cursor.
         */
        if (E.cx < row->size &&
            row->chars[E.cx] == '>') {

            E.cx++;
        return;
            }

            editorRowInsertChar(
                row,
                E.cx,
                '<'
            );

            E.cx++;

            editorRowInsertChar(
                row,
                E.cx,
                '>'
            );

            return;
        }

        /*
         * If typing > and one is already directly under
         * the cursor, jump over it.
         */
        if (c == '>' &&
            E.cx < row->size &&
            row->chars[E.cx] == '>') {

            E.cx++;
        return;
            }

            /*
             * Automatic pairs.
             */
            char closing = 0;

            switch (c) {
                case '(':
                    closing = ')';
                    break;

                case '[':
                    closing = ']';
                    break;

                case '{':
                    closing = '}';
                    break;

                case '"':
                    closing = '"';
                    break;

                case '\'':
                    closing = '\'';
                    break;
            }

            if (closing) {
                /*
                 * Don't duplicate quotes if the closing quote
                 * already exists at the cursor.
                 */
                if ((c == '"' ||
                    c == '\'') &&
                    E.cx < row->size &&
                    row->chars[E.cx] == c) {

                    E.cx++;
                return;
                    }

                    editorRowInsertChar(
                        row,
                        E.cx,
                        c
                    );

                    E.cx++;

                    editorRowInsertChar(
                        row,
                        E.cx,
                        closing
                    );

                    return;
            }

            /*
             * Skip over an existing closing character.
             */
            if ((c == ')' ||
                c == ']' ||
                c == '}') &&
                E.cx < row->size &&
                row->chars[E.cx] == c) {

                E.cx++;
            return;
                }

                editorRowInsertChar(
                    row,
                    E.cx,
                    c
                );

                E.cx++;
}

/* ============================================================
 * Delete
 * ============================================================ */

void editorDelChar(void)
{
    if (E.cy == E.numrows) {
        return;
    }

    if (E.cx == 0 &&
        E.cy == 0) {
        return;
        }

        erow *row =
        &E.row[E.cy];

    /*
     * Delete empty pairs:
     *
     * (|)
     *
     * becomes:
     *
     * |
     */
    if (E.cx > 0 &&
        E.cx < row->size) {

        char left =
        row->chars[E.cx - 1];

    char right =
    row->chars[E.cx];

    if ((left == '(' &&
        right == ')') ||
        (left == '[' &&
        right == ']') ||
        (left == '{' &&
        right == '}') ||
        (left == '"' &&
        right == '"') ||
        (left == '\'' &&
        right == '\'') ||
        (left == '<' &&
        right == '>' &&
        editorCursorInIncludeContext(row))) {

        editorRowDelChar(
            row,
            E.cx
        );

        editorRowDelChar(
            row,
            E.cx - 1
        );

        E.cx--;

        return;
        }
        }

        if (E.cx > 0) {
            editorRowDelChar(
                row,
                E.cx - 1
            );

            E.cx--;
            return;
        }

        /*
         * Backspace at the beginning of a line joins
         * it with the previous line.
         */
        E.cx =
        E.row[E.cy - 1].size;

        editorRowAppendString(
            &E.row[E.cy - 1],
            row->chars,
            row->size
        );

        editorDelRow(E.cy);

        E.cy--;
}

/* ============================================================
 * File handling
 * ============================================================ */

void editorOpen(char *filename)
{
    free(E.filename);

    E.filename =
    strdup(filename);

    editorSelectSyntaxHighlight();

    FILE *fp =
    fopen(filename, "r");

    if (!fp) {
        die("fopen");
    }

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    while ((linelen =
        getline(
            &line,
            &linecap,
            fp)) != -1) {

        while (linelen > 0 &&
            (line[linelen - 1] == '\n' ||
            line[linelen - 1] == '\r')) {
            linelen--;
            }

            editorInsertRow(
                E.numrows,
                line,
                linelen
            );
            }

            free(line);
            fclose(fp);

            E.dirty = 0;

            editorSelectSyntaxHighlight();
}

char *editorRowsToString(
    int *buflen)
{
    int totlen = 0;

    for (int j = 0;
         j < E.numrows;
    j++) {

        totlen +=
        E.row[j].size + 1;
    }

    *buflen = totlen;

    char *buf =
    malloc(
        totlen > 0
        ? totlen
        : 1
    );

    char *p = buf;

    for (int j = 0;
         j < E.numrows;
    j++) {

        memcpy(
            p,
            E.row[j].chars,
            E.row[j].size
        );

        p += E.row[j].size;

        *p = '\n';
        p++;
    }

    return buf;
}

void editorSave(void)
{
    if (E.filename == NULL) {
        E.filename =
        editorPrompt(
            "Save as: %s (ESC to cancel)",
                     NULL
        );

        if (E.filename == NULL) {
            editorSetStatusMessage(
                "Save aborted"
            );

            return;
        }

        editorSelectSyntaxHighlight();
    }

    int len;

    char *buf =
    editorRowsToString(&len);

    int fd =
    open(
        E.filename,
         O_RDWR | O_CREAT,
         0644
    );

    if (fd != -1) {
        if (ftruncate(
            fd,
            len) != -1) {

            if (write(
                fd,
                buf,
                len) == len) {

                close(fd);
            free(buf);

        E.dirty = 0;

        editorSetStatusMessage(
            "%d bytes written",
            len
        );

        return;
                }
            }

            close(fd);
    }

    free(buf);

    editorSetStatusMessage(
        "Cannot save: %s",
        strerror(errno)
    );
}

/* ============================================================
 * Prompt
 * ============================================================ */

char *editorPrompt(
    char *prompt,
    void (*callback)(char *, int))
{
    size_t bufsize = 128;

    char *buf =
    malloc(bufsize);

    size_t buflen = 0;

    buf[0] = '\0';

    while (1) {
        editorSetStatusMessage(
            prompt,
            buf
        );

        editorRefreshScreen();

        int c =
        editorReadKey();

        if (c == DEL_KEY ||
            c == CTRL_KEY('h') ||
            c == BACKSPACE) {

            if (buflen != 0) {
                buf[--buflen] = '\0';
            }
            } else if (c == '\x1b') {

                editorSetStatusMessage("");

                if (callback) {
                    callback(buf, c);
                }

                free(buf);

                return NULL;
            } else if (c == '\r') {

                if (buflen != 0) {
                    editorSetStatusMessage("");

                    if (callback) {
                        callback(buf, c);
                    }

                    return buf;
                }
            } else if (!iscntrl(
                (unsigned char)c) &&
                c < 128) {

                if (buflen ==
                    bufsize - 1) {

                    bufsize *= 2;

                buf =
                realloc(
                    buf,
                    bufsize
                );
                    }

                    buf[buflen++] =
                    c;

                    buf[buflen] =
                    '\0';
                }

                if (callback) {
                    callback(buf, c);
                }
    }
}

/* ============================================================
 * Append buffer
 * ============================================================ */

struct abuf {
    char *b;
    int len;
};

void abAppend(
    struct abuf *ab,
    const char *s,
    int len)
{
    char *newbuf =
    realloc(
        ab->b,
        ab->len + len
    );

    if (newbuf == NULL) {
        return;
    }

    memcpy(
        &newbuf[ab->len],
        s,
        len
    );

    ab->b = newbuf;
    ab->len += len;
}

void abFree(struct abuf *ab)
{
    free(ab->b);
}

/* ============================================================
 * Cursor conversion
 * ============================================================ */

int editorRowCxToRx(
    erow *row,
    int cx)
{
    int rx = 0;

    for (int j = 0;
         j < cx;
    j++) {

        if (row->chars[j] == '\t') {
            rx +=
            EDITOR_TAB_STOP -
            (rx %
            EDITOR_TAB_STOP) -
            1;
        }

        rx++;
    }

    return rx;
}

int editorRowRxToCx(
    erow *row,
    int rx)
{
    int cur_rx = 0;

    for (int cx = 0;
         cx < row->size;
    cx++) {

        if (row->chars[cx] == '\t') {
            cur_rx +=
            EDITOR_TAB_STOP -
            (cur_rx %
            EDITOR_TAB_STOP) -
            1;
        }

        cur_rx++;

        if (cur_rx > rx) {
            return cx;
        }
    }

    return row->size;
}

/* ============================================================
 * Search
 * ============================================================ */

void editorFindCallback(
    char *query,
    int key)
{
    static int last_match = -1;
    static int direction = 1;

    static int saved_hl_line;
    static char *saved_hl = NULL;

    if (saved_hl) {
        memcpy(
            E.row[saved_hl_line].hl,
            saved_hl,
            E.row[saved_hl_line].rsize
        );

        free(saved_hl);
        saved_hl = NULL;
    }

    if (key == '\r' ||
        key == '\x1b') {

        last_match = -1;
    direction = 1;

    return;
        }

        if (key == ARROW_RIGHT ||
            key == ARROW_DOWN) {

            direction = 1;

            } else if (key == ARROW_LEFT ||
                key == ARROW_UP) {

                direction = -1;

                } else {

                    last_match = -1;
                    direction = 1;
                }

                if (last_match == -1) {
                    direction = 1;
                }

                int current =
                last_match;

                for (int i = 0;
                     i < E.numrows;
    i++) {

                    current += direction;

                    if (current == -1) {
                        current =
                        E.numrows - 1;
                    } else if (current ==
                        E.numrows) {

                        current = 0;
                        }

                        erow *row =
                        &E.row[current];

                    char *match =
                    strstr(
                        row->render,
                        query
                    );

                    if (match) {
                        last_match = current;

                        E.cy = current;

                        E.cx =
                        editorRowRxToCx(
                            row,
                            match -
                            row->render
                        );

                        E.rowoff =
                        E.numrows;

                        saved_hl_line =
                        current;

                        saved_hl =
                        malloc(
                            row->rsize
                        );

                        memcpy(
                            saved_hl,
                            row->hl,
                            row->rsize
                        );

                        memset(
                            &row->hl[
                                match -
                                row->render
                            ],
                            HL_MATCH,
                            strlen(query)
                        );

                        break;
                    }
    }
}

void editorFind(void)
{
    int saved_cx = E.cx;
    int saved_cy = E.cy;

    int saved_coloff =
    E.coloff;

    int saved_rowoff =
    E.rowoff;

    char *query =
    editorPrompt(
        "Search: %s (ESC/Arrows/Enter)",
                 editorFindCallback
    );

    if (query) {
        free(query);
    } else {
        E.cx = saved_cx;
        E.cy = saved_cy;
        E.coloff = saved_coloff;
        E.rowoff = saved_rowoff;
    }
}

/* ============================================================
 * Scrolling
 * ============================================================ */

void editorScroll(void)
{
    E.rx = 0;

    if (E.cy < E.numrows) {
        E.rx =
        editorRowCxToRx(
            &E.row[E.cy],
            E.cx
        );
    }

    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }

    if (E.cy >=
        E.rowoff +
        E.screenrows) {

        E.rowoff =
        E.cy -
        E.screenrows +
        1;
        }

        if (E.rx < E.coloff) {
            E.coloff = E.rx;
        }

        if (E.rx >=
            E.coloff +
            E.screencols) {

            E.coloff =
            E.rx -
            E.screencols +
            1;
            }
}

/* ============================================================
 * Status bar
 * ============================================================ */

void editorDrawStatusBar(
    struct abuf *ab)
{
    abAppend(
        ab,
        "\x1b[7m",
        4
    );

    char status[80];
    char rstatus[80];

    int len =
    snprintf(
        status,
        sizeof(status),
             "%.20s - %d lines %s",
             E.filename
             ? E.filename
             : "[No Name]",
             E.numrows,
             E.dirty
             ? "(modified)"
             : ""
    );

    int rlen =
    snprintf(
        rstatus,
        sizeof(rstatus),
             "%s | %d/%d",
             E.syntax
             ? E.syntax->filetype
             : "plain",
             E.cy + 1,
             E.numrows
    );

    if (len > E.screencols) {
        len = E.screencols;
    }

    abAppend(
        ab,
        status,
        len
    );

    while (len <
        E.screencols) {

        if (E.screencols - len ==
            rlen) {

            abAppend(
                ab,
                rstatus,
                rlen
            );

        break;
            }

            abAppend(
                ab,
                " ",
                1
            );

            len++;
        }

        abAppend(
            ab,
            "\x1b[m",
            3
        );

        abAppend(
            ab,
            "\r\n",
            2
        );
}

void editorDrawMessageBar(
    struct abuf *ab)
{
    abAppend(
        ab,
        "\x1b[K",
        3
    );

    int msglen =
    strlen(E.statusmsg);

    if (msglen >
        E.screencols) {

        msglen =
        E.screencols;
        }

        if (msglen &&
            time(NULL) -
            E.statusmsg_time < 5) {

            abAppend(
                ab,
                E.statusmsg,
                msglen
            );
            }
}

/* ============================================================
 * Screen rendering
 * ============================================================ */

void editorDrawRows(
    struct abuf *ab)
{
    for (int y = 0;
         y < E.screenrows;
    y++) {

        int filerow =
        y + E.rowoff;

        if (filerow >=
            E.numrows) {

            if (E.numrows == 0 &&
                y ==
                E.screenrows / 3) {

                char welcome[80];

            int welcomelen =
            snprintf(
                welcome,
                sizeof(welcome),
                     "cedit %s  |  c/c++ editor",
                     EDITOR_VERSION
            );

            if (welcomelen >
                E.screencols) {

                welcomelen =
                E.screencols;
                }

                int padding =
                (E.screencols -
                welcomelen) / 2;

            if (padding) {
                abAppend(
                    ab,
                    "~",
                    1
                );

                padding--;
            }

            while (padding-- > 0) {
                abAppend(
                    ab,
                    " ",
                    1
                );
            }

            abAppend(
                ab,
                welcome,
                welcomelen
            );

                } else {

                    abAppend(
                        ab,
                        "~",
                        1
                    );
                }

            } else {

                int len =
                E.row[filerow].rsize -
                E.coloff;

                if (len < 0) {
                    len = 0;
                }

                if (len >
                    E.screencols) {

                    len =
                    E.screencols;
                    }

                    char *c =
                    &E.row[filerow]
                    .render[E.coloff];

                unsigned char *hl =
                &E.row[filerow]
                .hl[E.coloff];

                int current_color =
                -1;

                for (int j = 0;
                     j < len;
                j++) {

                    if (iscntrl(
                        (unsigned char)c[j])) {

                        char sym =
                        (c[j] <= 26)
                        ? '@' + c[j]
                        : '?';

                    abAppend(
                        ab,
                        "\x1b[7m",
                        4
                    );

                    abAppend(
                        ab,
                        &sym,
                        1
                    );

                    abAppend(
                        ab,
                        "\x1b[m",
                        3
                    );

                    if (current_color != -1) {
                        char buf[16];

                        int clen =
                        snprintf(
                            buf,
                            sizeof(buf),
                                 "\x1b[%dm",
                                 current_color
                        );

                        abAppend(
                            ab,
                            buf,
                            clen
                        );
                    }

                    continue;
                        }

                        if (hl[j] ==
                            HL_NORMAL) {

                            if (current_color != -1) {
                                abAppend(
                                    ab,
                                    "\x1b[39m",
                                    5
                                );

                                current_color =
                                -1;
                            }

                            abAppend(
                                ab,
                                &c[j],
                                1
                            );

                            continue;
                            }

                            int color =
                            editorSyntaxToColor(
                                hl[j]
                            );

                            if (color !=
                                current_color) {

                                current_color =
                                color;

                            char buf[16];

                            int clen =
                            snprintf(
                                buf,
                                sizeof(buf),
                                     "\x1b[%dm",
                                     color
                            );

                            abAppend(
                                ab,
                                buf,
                                clen
                            );
                                }

                                abAppend(
                                    ab,
                                    &c[j],
                                    1
                                );
                }

                abAppend(
                    ab,
                    "\x1b[39m",
                    5
                );
            }

            abAppend(
                ab,
                "\x1b[K",
                3
            );

            abAppend(
                ab,
                "\r\n",
                2
            );
    }
}

void editorRefreshScreen(void)
{
    editorScroll();

    struct abuf ab =
    ABUF_INIT;

    /*
     * Hide cursor.
     */
    abAppend(
        &ab,
        "\x1b[?25l",
        6
    );

    /*
     * Move cursor to top-left.
     */
    abAppend(
        &ab,
        "\x1b[H",
        3
    );

    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);

    /*
     * Put cursor back where it belongs.
     */
    char buf[32];

    snprintf(
        buf,
        sizeof(buf),
             "\x1b[%d;%dH",
             (E.cy -
             E.rowoff) + 1,
             (E.rx -
             E.coloff) + 1
    );

    abAppend(
        &ab,
        buf,
        strlen(buf)
    );

    /*
     * Show cursor.
     */
    abAppend(
        &ab,
        "\x1b[?25h",
        6
    );

    write(
        STDOUT_FILENO,
        ab.b,
        ab.len
    );

    abFree(&ab);
}

/* ============================================================
 * Status messages
 * ============================================================ */

void editorSetStatusMessage(
    const char *fmt,
    ...)
{
    va_list ap;

    va_start(ap, fmt);

    vsnprintf(
        E.statusmsg,
        sizeof(E.statusmsg),
              fmt,
              ap
    );

    va_end(ap);

    E.statusmsg_time =
    time(NULL);
}

/* ============================================================
 * Cursor movement
 * ============================================================ */

void editorMoveCursor(int key)
{
    erow *row =
    (E.cy >= E.numrows)
    ? NULL
    : &E.row[E.cy];

    switch (key) {
        case ARROW_LEFT:

            if (E.cx != 0) {
                E.cx--;

            } else if (E.cy > 0) {
                E.cy--;

                E.cx =
                E.row[E.cy].size;
            }

            break;

        case ARROW_RIGHT:

            if (row &&
                E.cx < row->size) {

                E.cx++;

                } else if (row &&
                    E.cx ==
                    row->size) {

                    E.cy++;
                E.cx = 0;
                    }

                    break;

        case ARROW_UP:

            if (E.cy != 0) {
                E.cy--;
            }

            break;

        case ARROW_DOWN:

            if (E.cy <
                E.numrows) {

                E.cy++;
                }

                break;
    }

    row =
    (E.cy >= E.numrows)
    ? NULL
    : &E.row[E.cy];

    int rowlen =
    row ? row->size : 0;

    if (E.cx > rowlen) {
        E.cx = rowlen;
    }
}

/* ============================================================
 * Key processing
 * ============================================================ */

void editorProcessKeypress(void)
{
    static int quit_times =
    EDITOR_QUIT_TIMES;

    int c =
    editorReadKey();

    switch (c) {

        /* ----------------------------------------------------
         * Enter
         * ---------------------------------------------------- */

        case '\r':
            editorInsertNewline();
            break;

            /* ----------------------------------------------------
             * Quit
             * ---------------------------------------------------- */

            case CTRL_KEY('q'):

                if (E.dirty &&
                    quit_times > 0) {

                    editorSetStatusMessage(
                        "Unsaved changes! "
                        "Press Ctrl-Q %d more time(s) to quit.",
                                           quit_times
                    );

                quit_times--;

                return;
                    }

                    write(
                        STDOUT_FILENO,
                        "\x1b[2J",
                        4
                    );

                    write(
                        STDOUT_FILENO,
                        "\x1b[H",
                        3
                    );

                    exit(0);

                    /* ----------------------------------------------------
                     * Save
                     * ---------------------------------------------------- */

                    case CTRL_KEY('s'):
                        editorSave();
                        break;

                        /* ----------------------------------------------------
                         * Search
                         * ---------------------------------------------------- */

                        case CTRL_KEY('f'):
                            editorFind();
                            break;

                            /* ----------------------------------------------------
                             * Home / End
                             * ---------------------------------------------------- */

                            case HOME_KEY:
                                E.cx = 0;
                                break;

                            case END_KEY:

                                if (E.cy <
                                    E.numrows) {

                                    E.cx =
                                    E.row[E.cy].size;
                                    }

                                    break;

                                /* ----------------------------------------------------
                                 * Delete
                                 * ---------------------------------------------------- */

                                case BACKSPACE:
                                case CTRL_KEY('h'):
                                case DEL_KEY:

                                    if (c == DEL_KEY) {
                                        editorMoveCursor(
                                            ARROW_RIGHT
                                        );
                                    }

                                    editorDelChar();
                                    break;

                                    /* ----------------------------------------------------
                                     * Page movement
                                     * ---------------------------------------------------- */

                                    case PAGE_UP:
                                    case PAGE_DOWN: {

                                        if (c == PAGE_UP) {
                                            E.cy =
                                            E.rowoff;

                                        } else {

                                            E.cy =
                                            E.rowoff +
                                            E.screenrows -
                                            1;

                                            if (E.cy >
                                                E.numrows) {

                                                E.cy =
                                                E.numrows;
                                                }
                                        }

                                        int times =
                                        E.screenrows;

                                        while (times--) {
                                            editorMoveCursor(
                                                c == PAGE_UP
                                                ? ARROW_UP
                                                : ARROW_DOWN
                                            );
                                        }

                                        break;
                                    }

                                    /* ----------------------------------------------------
                                     * Arrows
                                     * ---------------------------------------------------- */

                                    case ARROW_UP:
                                    case ARROW_DOWN:
                                    case ARROW_LEFT:
                                    case ARROW_RIGHT:

                                        editorMoveCursor(c);
                                        break;

                                        /* ----------------------------------------------------
                                         * Escape / refresh
                                         * ---------------------------------------------------- */

                                        case CTRL_KEY('l'):
                                        case '\x1b':
                                            break;

                                            /* ----------------------------------------------------
                                             * TAB
                                             *
                                             * Insert four spaces.
                                             * ---------------------------------------------------- */

                                            case '\t':

                                                for (int i = 0;
                                                     i < EDITOR_TAB_STOP;
        i++) {

                                                    editorInsertChar(' ');
        }

        break;

        /* ----------------------------------------------------
         * Normal character
         * ---------------------------------------------------- */

        default:
            editorInsertChar(c);
            break;
    }

    quit_times =
    EDITOR_QUIT_TIMES;
}

/* ============================================================
 * Initialization
 * ============================================================ */

void initEditor(void)
{
    E.cx = 0;
    E.cy = 0;

    E.rx = 0;

    E.numrows = 0;
    E.row = NULL;

    E.rowoff = 0;
    E.coloff = 0;

    E.dirty = 0;

    E.filename = NULL;

    E.statusmsg_time = 0;
    E.statusmsg[0] = '\0';

    E.syntax = NULL;

    if (getWindowSize(
        &E.screenrows,
        &E.screencols) == -1) {

        die("getWindowSize");
        }

        /*
         * Reserve:
         *
         * 1 line for status
         * 1 line for messages
         */
        E.screenrows -= 2;

        if (E.screenrows < 1) {
            E.screenrows = 1;
        }
}

/* ============================================================
 * Main
 * ============================================================ */

int main(
    int argc,
    char *argv[])
{
    enableRawMode();

    initEditor();

    if (argc >= 2) {
        editorOpen(argv[1]);
    }

    editorSetStatusMessage(
        "Ctrl-S Save | Ctrl-F Find | "
        "Ctrl-Q Quit | I <3 C++q"
    );

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}
