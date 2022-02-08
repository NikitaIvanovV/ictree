/*
 * Copyright 2022 Nikita Ivanov
 *
 * This file is part of ictree
 *
 * ictree is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * ictree is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ictree. If not, see <https://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termbox.h>
#include <unistd.h>

#include "args.h"
#include "lines.h"
#include "paths.h"
#include "utils.h"
#include "vector.h"

#define SCREEN_X      tb_width()
#define SCREEN_Y      tb_height()
#define PROMPT_HEIGHT 1
#define TREE_VIEW_X   SCREEN_X
#define TREE_VIEW_Y   (SCREEN_Y - PROMPT_HEIGHT)
#define TREE_VIEW_TOP pager_pos.y
#define TREE_VIEW_MID (pager_pos.y + (TREE_VIEW_Y / 2))
#define TREE_VIEW_BOT (pager_pos.y + TREE_VIEW_Y - 1)
#define MAX_PATHS     ((long)paths.len)

#define INDENT               2
#define ICON_STATUS_LEN      2
#define ICON_STATUS_DEFAULT  "• "
#define ICON_STATUS_FOLDED   "▶ "
#define ICON_STATUS_UNFOLDED "▼ "
#define ICON_ROOT_DIR        "/"

#define PROMPT_MAX_LEN   255
#define PROMPT_LEFT_PAD  1
#define PROMPT_RIGHT_PAD 1

#define RETURN_ON_TB_ERROR(func_call, msg)                 \
    do {                                                   \
        int ret = (func_call);                             \
        if (ret != TB_OK && ret != TB_ERR_OUT_OF_BOUNDS) { \
            set_errorf("%s: %s", msg, tb_strerror(ret));   \
            return 1;                                      \
        }                                                  \
    } while (0)

#define RETURN_ON_ERROR(func_call) \
    do {                           \
        if ((func_call) != 0) {    \
            return 1;              \
        }                          \
    } while (0)

typedef struct Pos {
    long x, y;
} Pos;

typedef enum UpdScrSignal {
    UpdScrSignalNo  = 0,
    UpdScrSignalYes = 1,
} UpdScrSignal;

typedef struct PromptMsg {
    char msg[PROMPT_MAX_LEN + 1];
    uint32_t fg, bg;
} PromptMsg;

struct SearchResults {
    PathLink *links;
    long *positions;
    size_t len;
};

enum Mode {
    ModeNormal = 1,
    ModeSearch = 2,
};

enum State {
    StateRunning,
    StateStop,
    StateQuit,
};

static char *program_path = NULL;
static char *output_str = NULL;

static enum Mode mode = ModeNormal;

static Options options;

static FILE *stream = NULL;
static int stream_file = 0;

static Lines lines;

static UnfoldedPaths paths = { .links = NULL, .len = 0 };
static size_t total_paths_l = 0;

static enum SearchDir search_dir;
static char search_query[1024];

static Pos pager_pos = {0, 0};
static long cursor_pos = 0;

static PromptMsg prompt_msg;

static volatile enum State state;

static int draw(void);
static int fold(void);
static int init_termbox(void);
static int is_search_result(Path *path);
static int open_file(char *name);
static int run(void);
static int setup_signals();
static int unfold(void);
static int update_screen(void);
static UpdScrSignal goto_parent(void);
static UpdScrSignal handle_key(struct tb_event ev);
static UpdScrSignal handle_mouse_click(int x, int y);
static UpdScrSignal handle_mouse(struct tb_event ev);
static void catch_error(int signo);
static void catch_stop(int signo);
static void catch_term(int signo);
static void center_cursor(void);
static void cleanup_lines(void);
static void cleanup_paths(void);
static void cleanup_termbox(void);
static void cleanup(void);
static void copy_path(void);
static void cursor_move(int i);
static void cursor_set(long p);
static void init_options(void);
static void init_search(int dir);
static void next_result(int invert_search);
static void output_path(void);
static void print_error(char *error_msg);
static void print_errorf(char *format, ...);
static void quit_search(void);
static void quit(void);
static void reset_prompt_msg(void);
static void reset_search_query(void);
static void scroll_x(int i);
static void scroll_y(int i);
static void scroll_y_raw(int i);
static void search(char *pattern);
static void set_default_prompt(void);
static void set_prompt_msg(char *msg);
static void set_prompt_msg_err(char *msg);
static void set_prompt_msg_errf(char *format, ...);
static void set_prompt_msgf(char *format, ...);
static void set_search_prompt(void);
static void stop(void);
static void toggle_fold(void);
static void update_search_input(struct tb_event ev);

static void print_error(char *error_msg)
{
    fprintf(stderr, "%s: %s\n", program_path, error_msg);
}

static void print_errorf(char *format, ...)
{
    char msg[ERROR_BUF_SIZE];
    FORMATTED_STRING(msg, format);
    print_error(msg);
}

static void reset_prompt_msg(void)
{
    memset(prompt_msg.msg, ' ', PROMPT_MAX_LEN);
    prompt_msg.msg[PROMPT_MAX_LEN] = '\0';
    prompt_msg.bg = TB_WHITE;
    prompt_msg.fg = TB_BLACK;
}

static void set_prompt_msg(char *msg)
{
    reset_prompt_msg();
    memcpy(prompt_msg.msg + PROMPT_LEFT_PAD, msg,
           MIN(strlen(msg), PROMPT_MAX_LEN - PROMPT_LEFT_PAD));
}

static void set_prompt_msgf(char *format, ...)
{
    char msg[PROMPT_MAX_LEN];
    FORMATTED_STRING(msg, format);
    set_prompt_msg(msg);
}

static void set_prompt_msg_err(char *msg)
{
    set_prompt_msg(msg);
    prompt_msg.bg = TB_RED;
    prompt_msg.fg = TB_WHITE;
}

static void set_prompt_msg_errf(char *format, ...)
{
    char msg[PROMPT_MAX_LEN];
    FORMATTED_STRING(msg, format);
    set_prompt_msg_err(msg);
}

static void init_options(void)
{
    options.filename = NULL;
    options.init_paths_state = PathStateUnfolded;
}

static void scroll_x(int i)
{
    pager_pos.x = MAX(0, pager_pos.x + i);
}

static void scroll_y(int i)
{
    pager_pos.y = MAX(0, MIN(MAX_PATHS - TREE_VIEW_Y, pager_pos.y + i));
    cursor_move(i);
}

static void scroll_y_raw(int i)
{
    pager_pos.y += i;
}

static void cursor_set(long p)
{
    cursor_pos = p;

    if (cursor_pos < TREE_VIEW_TOP && pager_pos.y < MAX_PATHS) {
        pager_pos.y = cursor_pos;
    } else if (cursor_pos > TREE_VIEW_BOT && pager_pos.y + TREE_VIEW_Y > 0) {
        pager_pos.y = cursor_pos - TREE_VIEW_Y + 1;
    }

    set_default_prompt();
}

static void cursor_move(int i)
{
    long new_p = cursor_pos + i;
    if (new_p < 0 || new_p >= MAX_PATHS)
        return;
    cursor_set(new_p);
}

static void center_cursor(void)
{
    scroll_y_raw(cursor_pos - TREE_VIEW_MID);
}

static UpdScrSignal goto_parent(void)
{
    Path *path = get_path_from_link(paths.links[cursor_pos]);

    if (!HAS_MAIN_PATH(*path))
        return UpdScrSignalNo;

    for (long i = cursor_pos - 1; i >= 0; i--) {
        if (PATH_LINKS_EQ(paths.links[i], path->mainpath)) {
            cursor_set(i);
            return UpdScrSignalYes;
        }
    }

    /* If mainpath exists, it must always be found */
    abort();
}

static int unfold(void)
{
    if (get_path_from_link(paths.links[cursor_pos])->state == PathStateUnfolded) {
        return 0;
    }

    unfold_path(&paths, cursor_pos);

    return 1;
}

static int fold(void)
{
    if (get_path_from_link(paths.links[cursor_pos])->state == PathStateFolded) {
        return 0;
    }

    fold_path(&paths, cursor_pos);

    return 1;
}

static void toggle_fold(void)
{
    switch (get_path_from_link(paths.links[cursor_pos])->state) {
    case PathStateFolded:
        assert(unfold() == 1);
        break;
    case PathStateUnfolded:
        assert(fold() == 1);
        break;
    }
}

static void quit(void)
{
    state = StateQuit;
}

static void stop(void)
{
    state = StateStop;
}

static void output_path(void)
{
    Path *p = get_path_from_link(paths.links[cursor_pos]);
    output_str = p->full_path;

    quit();
}

static void init_search(int dir)
{
    switch (dir) {
    case 1:
        search_dir = SearchDirForward;
        break;
    case -1:
        search_dir = SearchDirBackward;
        break;
    default:
        abort();
        break;
    }

    mode = ModeSearch;

    set_search_prompt();
}

static void quit_search(void)
{
    mode = ModeNormal;
    reset_search_query();
    tb_hide_cursor();
}

static void search(char *pattern)
{
    if (strlen(pattern) == 0)
        return;

    if (init_paths_search(pattern, search_dir) != 0) {
        set_prompt_msg_err(get_error());
    }

    next_result(0);
}

static void update_search_input(struct tb_event ev)
{
    char buf[] = {0, 0};

    if (ev.ch != 0) {
        buf[0] = ev.ch;
        strncat(search_query, buf, LENGTH(search_query) - 1);
    } else if (ev.key == TB_KEY_BACKSPACE2) {
        search_query[strlen(search_query) - 1] = '\0';
    }

    set_search_prompt();
}

static void reset_search_query(void)
{
    search_query[0] = '\0';
}

static void next_result(int invert_search)
{
    int ret;
    Path *p;
    PathLink result;
    size_t pos;

    ret = search_path(&result, paths.links[cursor_pos], invert_search);
    if (ret != 0) {
        set_prompt_msg_err(get_error());
        return;
    }

    if (IS_NO_LINK(result)) {
        set_prompt_msg_errf("Pattern not found");
        return;
    }

    p = get_path_from_link(result);
    unfold_nested_path(&paths, p, &pos);
    cursor_set(pos);
}

static void set_search_prompt(void)
{
    char *s;

    switch (search_dir) {
    case SearchDirForward:
        s = "/";
        break;
    case SearchDirBackward:
        s = "?";
        break;
    default:
        abort();
        break;
    }

    char msg[PROMPT_MAX_LEN];
    int len = snprintf(msg, LENGTH(msg) - 1, "%s%s", s, search_query);
    set_prompt_msg(msg);
    tb_set_cursor(len + PROMPT_LEFT_PAD, TREE_VIEW_Y + PROMPT_HEIGHT - 1);
}

static int is_search_result(Path *path)
{
    enum MatchStatus st = path_match_pattern(path);

    if (st == MatchStatusOk)
        return 1;

    return 0;
}

static void set_default_prompt(void)
{
    set_prompt_msg(get_path_from_link(paths.links[cursor_pos])->full_path);
}

static int draw(void)
{
    Path *path;
    char *path_line, *status_icon;
    int i, x, y, fg, bg;
    long first_c_x;
    unsigned long indent, char_off, subpaths_l;

    /* Draw tree */

    y = 0;
    while (y < TREE_VIEW_Y) {
        i = pager_pos.y + y;

        if (i < 0) {
            y++;
            continue;
        } else if (i >= MAX_PATHS) {
            break;
        }

        path = get_path_from_link(paths.links[i]);
        subpaths_l = path->subpaths_l;
        path_line = path->line;
        indent = path->depth * INDENT;

        if (strlen(path_line) == 0)
            path_line = ICON_ROOT_DIR;

        first_c_x = pager_pos.x - indent;

        /* If beginning of a line is to the left of the border of the screen,
         * chop the beginning of the line and print it from x=0.
         * Otherwise, just set x to the proper value. */
        if (first_c_x > 0) {
            char_off = MIN(strlen(path_line), (unsigned)first_c_x);
            x = 0;
        } else {
            char_off = 0;
            x = -first_c_x;
        }

        if (i == cursor_pos) {
            fg = TB_BLACK | TB_BOLD;
            bg = TB_WHITE;
        } else if (is_search_result(path)) {
            fg = TB_BLACK;
            bg = TB_YELLOW;
        } else {
            fg = TB_WHITE;
            bg = TB_DEFAULT;
        }

        status_icon = ICON_STATUS_DEFAULT;
        if (subpaths_l > 0) {
            switch (path->state) {
            case PathStateUnfolded:
                status_icon = ICON_STATUS_UNFOLDED;
                break;
            case PathStateFolded:
                status_icon = ICON_STATUS_FOLDED;
                break;
            }
        }

        RETURN_ON_TB_ERROR(
                tb_printf(x, y, fg, bg, "%s%s", status_icon, path_line + char_off),
                "failed to print path");
        if (x + strlen(path->line) >= (unsigned long)TREE_VIEW_X)
            RETURN_ON_TB_ERROR(
                    tb_set_cell(TREE_VIEW_X - 1, y, '>', TB_BLACK, TB_WHITE),
                    "failed to print '>' symbol");
        if (char_off > 0)
            RETURN_ON_TB_ERROR(
                    tb_set_cell(0, y, '<', TB_BLACK, TB_WHITE),
                    "failed to print '<' symbol");
        y++;
    }

    /* Draw prompt */

    x = 0;
    y = TREE_VIEW_Y + PROMPT_HEIGHT - 1;

    fg = prompt_msg.fg;
    bg = prompt_msg.bg;

    RETURN_ON_TB_ERROR(
            tb_print(x, y, fg, bg, prompt_msg.msg),
            "failed to print prompt message");

    char ind[PROMPT_MAX_LEN];
    snprintf(ind, PROMPT_MAX_LEN - PROMPT_RIGHT_PAD, "   %zu/%zu", paths.links[cursor_pos].index + 1, total_paths_l);
    for (i = 0; i < PROMPT_RIGHT_PAD; i++) {
       strncat(ind, " ", PROMPT_MAX_LEN - 1);
    }
    x = TREE_VIEW_X - strlen(ind);
    RETURN_ON_TB_ERROR(
            tb_print(x, y, fg, bg, ind),
            "failed to print prompt message");

    return 0;
}

static int update_screen(void)
{
    RETURN_ON_TB_ERROR(
            tb_clear(),
            "failed to clear screen");

    RETURN_ON_ERROR(draw());

    RETURN_ON_TB_ERROR(
        tb_present(),
        "failed to synchronize the internal buffer with the terminal");

    return 0;
}

#define CONTROL_ACTION(action)  \
    do {                        \
        action;                 \
        return UpdScrSignalYes; \
    } while (0)

#define SCROLL_X 4
#define SCROLL_Y 1

static UpdScrSignal handle_key(struct tb_event ev)
{
    if (mode == ModeSearch) {
        if (ev.key == TB_KEY_ENTER) {
            search(search_query);
            quit_search();
        } else if (ev.key == TB_KEY_ESC) {
            quit_search();
        } else {
            update_search_input(ev);
        }
        return UpdScrSignalYes;
    }

    switch (ev.key) {
    case TB_KEY_CTRL_E:
        CONTROL_ACTION(scroll_y(SCROLL_Y));
    case TB_KEY_CTRL_Y:
        CONTROL_ACTION(scroll_y(-SCROLL_Y));
    case TB_KEY_CTRL_D:
        CONTROL_ACTION(scroll_y(TREE_VIEW_Y / 2));
    case TB_KEY_CTRL_U:
        CONTROL_ACTION(scroll_y(-TREE_VIEW_Y / 2));
    case TB_KEY_CTRL_F:
        CONTROL_ACTION(scroll_y(TREE_VIEW_Y));
    case TB_KEY_CTRL_B:
        CONTROL_ACTION(scroll_y(-TREE_VIEW_Y));
    case TB_KEY_ARROW_DOWN:
        CONTROL_ACTION(cursor_move(1));
    case TB_KEY_ARROW_UP:
        CONTROL_ACTION(cursor_move(-1));
    case TB_KEY_ARROW_LEFT:
        CONTROL_ACTION(scroll_x(-SCROLL_X));
    case TB_KEY_ARROW_RIGHT:
        CONTROL_ACTION(scroll_x(SCROLL_X));
    case TB_KEY_ENTER:
        CONTROL_ACTION(toggle_fold());
    case TB_KEY_CTRL_Z:
        CONTROL_ACTION(raise(SIGTSTP));
    case TB_KEY_ESC:
        CONTROL_ACTION(quit());
    }

    switch (ev.ch) {
    case ' ':
        CONTROL_ACTION(toggle_fold(); scroll_y(SCROLL_Y));
    case 'q':
        CONTROL_ACTION(quit());
    case 'j':
        CONTROL_ACTION(cursor_move(1));
    case 'k':
        CONTROL_ACTION(cursor_move(-1));
    case 'h':
        CONTROL_ACTION(scroll_x(-SCROLL_X));
    case 'l':
        CONTROL_ACTION(scroll_x(SCROLL_X));
    case 'z':
        CONTROL_ACTION(center_cursor());
    case 'g':
        CONTROL_ACTION(cursor_set(0));
    case 'G':
        CONTROL_ACTION(cursor_set(MAX_PATHS - 1));
    case 'p':
        return goto_parent();
    case '/':
        CONTROL_ACTION(init_search(1));
    case '?':
        CONTROL_ACTION(init_search(-1));
    case 'n':
        CONTROL_ACTION(next_result(0));
    case 'N':
        CONTROL_ACTION(next_result(1));
    case 'y':
        CONTROL_ACTION(copy_path());
    case 'o':
        CONTROL_ACTION(output_path());
    }

    return UpdScrSignalNo;
}

static UpdScrSignal handle_mouse(struct tb_event ev)
{
    switch (ev.key) {
    case TB_KEY_MOUSE_LEFT:
        return handle_mouse_click(ev.x, ev.y);
    case TB_KEY_MOUSE_WHEEL_DOWN:
        CONTROL_ACTION(scroll_y(SCROLL_Y));
    case TB_KEY_MOUSE_WHEEL_UP:
        CONTROL_ACTION(scroll_y(-SCROLL_Y));
    }

    return UpdScrSignalNo;
}

static UpdScrSignal handle_mouse_click(int x, int y)
{
    long p = TREE_VIEW_TOP + y;

    if (p < 0 || p >= MAX_PATHS || y >= TREE_VIEW_Y)
        return UpdScrSignalNo;

    cursor_set(p);
    toggle_fold();
    return UpdScrSignalYes;
}

#define SETUP_SIGNAL(sig, fun)                            \
    do {                                                  \
        if (signal(sig, fun) == SIG_ERR) {                \
            set_error("failed to setup " #sig " signal"); \
            return 1;                                     \
        }                                                 \
    } while (0)

static int setup_signals()
{
    SETUP_SIGNAL(SIGABRT, catch_error);
    SETUP_SIGNAL(SIGSEGV, catch_error);
    SETUP_SIGNAL(SIGINT, catch_term);
    SETUP_SIGNAL(SIGTERM, catch_term);
    SETUP_SIGNAL(SIGTSTP, catch_stop);

    return 0;
}

static void catch_error(int signo)
{
    cleanup_termbox();
}

static void catch_term(int signo)
{
    quit();
}

static void catch_stop(int signo)
{
    stop();
}

#define FAILED_TO_COPY_ERR_MSG "Failed to copy"

static void copy_path(void)
{
    char *full_path = NULL;
    char *xsel_args[] = { "xsel", "--clipboard", NULL };
    char *wl_copy_args[] = { "wl-copy", NULL };

    int fd[2], fd_r, fd_w;
    if (pipe(fd) == -1) {
        set_prompt_msg_err(FAILED_TO_COPY_ERR_MSG ": pipe() failed");
        return;
    }

    fd_r = fd[0];
    fd_w = fd[1];

    int pid = fork();
    if (pid == -1) {
        set_prompt_msg_err(FAILED_TO_COPY_ERR_MSG ": fork() failed");
        return;
    } else if (pid == 0) {
        close(fd_w);

        /* Supress all output to prevent xsel from messing up the UI */
        int null = open("/dev/null", O_WRONLY);
        dup2(null, STDOUT_FILENO);
        dup2(null, STDERR_FILENO);
        close(null);

        dup2(fd_r, STDIN_FILENO);
        close(fd_r);

        execvp(xsel_args[0], xsel_args);
        if (errno == ENOENT) {
          execvp(wl_copy_args[0], wl_copy_args);
          if (errno == ENOENT) {
            exit(127);
          }
        }
        exit(EXIT_FAILURE);
    }

    close(fd_r);

    int status;
    Path *p = get_path_from_link(paths.links[cursor_pos]);
    full_path = p->full_path;

    if (write(fd_w, full_path, strlen(full_path) + 1) == -1) {
        set_prompt_msg_err(FAILED_TO_COPY_ERR_MSG ": write() failed");
        return;
    }
    close(fd_w);

    if (waitpid(pid, &status, 0) == -1) {
        set_prompt_msg_err(FAILED_TO_COPY_ERR_MSG ": waitpid() failed");
        return;
    }

    if (WIFEXITED(status)) {
        int es = WEXITSTATUS(status);
        if (es == 127) {
            set_prompt_msg_errf(FAILED_TO_COPY_ERR_MSG ": neither xsel nor wl-copy were found", es);
            return;
        } else if (es != 0) {
            set_prompt_msg_errf(FAILED_TO_COPY_ERR_MSG ": exited with code %d", es);
            return;
        }
    }

    set_prompt_msgf("Copied: %s", full_path);
}

static int run(void)
{
    int ret;
    struct tb_event ev;

    set_default_prompt();

    RETURN_ON_ERROR(update_screen());

    while (1) {
        ret = tb_peek_event(&ev, 10);
        if (ret == TB_OK) {
            switch (ev.type) {
            case TB_EVENT_KEY:
                if (handle_key(ev) == UpdScrSignalYes) {
                    RETURN_ON_ERROR(update_screen());
                }
                break;
            case TB_EVENT_MOUSE:
                if (handle_mouse(ev) == UpdScrSignalYes) {
                    RETURN_ON_ERROR(update_screen());
                }
                break;
            case TB_EVENT_RESIZE:
                RETURN_ON_ERROR(update_screen());
                break;
            }
        } else if (ret == TB_ERR_POLL && tb_last_errno() == EINTR) {
            continue;
        } else if (ret != TB_ERR_NO_EVENT) {
            set_errorf("failed to poll termbox event: %s", tb_strerror(ret));
            return 1;
        }

        if (state != StateRunning) {
            break;
        }
    }

    return 0;
}

static int open_file(char *name)
{
    int ret;

    errno = 0;
    FILE *f = fopen(name, "r");
    ret = errno;

    if (f == NULL) {
        if (errno == ENOENT) {
            set_errorf("file '%s' does not exist", name);
        } else {
            set_errorf("failed to open file '%s': %d", name, ret);
        }
        return 1;
    }

    stream = f;
    stream_file = 1;
    return 0;
}

static int init_termbox(void)
{
    int ret;
    if ((ret = tb_init()) != TB_OK) {
        set_errorf("failed to init termbox: %s", tb_strerror(ret));
        return 1;
    }

    state = StateRunning;
    return 0;
}

static void cleanup_termbox(void)
{
    int ret = tb_shutdown();
    if (ret == TB_OK || ret == TB_ERR_NOT_INIT)
        return;
    print_errorf("failed to shutdown termbox: %s", tb_strerror(ret));
}

static void cleanup_lines(void)
{
    free_lines(&lines);
}

static void cleanup_paths(void)
{
    free_paths(paths);
}

static void cleanup(void)
{
    cleanup_termbox();
    cleanup_paths();
    cleanup_lines();
    if (stream_file) {
        fclose(stream);
    }
#ifdef DEV
    if (debug_file != NULL)
        fclose(debug_file);
#endif
}

int main(int argc, char *argv[])
{
    int ret;
    program_path = argc >= 1 ? argv[0] : "ictree";
    stream = stdin;

    init_options();

    ret = process_args(&options, argc, argv);
    switch (ret) {
    case ArgActionErrorReport:
        print_error(get_error());
        return EXIT_FAILURE;
    case ArgActionError:
        return EXIT_FAILURE;
    case ArgActionExit:
        return EXIT_SUCCESS;
    }

    if (options.filename != NULL) {
        if (open_file(options.filename) != 0) {
            print_error(get_error());
            return EXIT_FAILURE;
        }
    }

    if (setup_signals() != 0) {
        print_error(get_error());
        return EXIT_FAILURE;
    }

#ifdef DEV
    debug_file = fopen(DEBUG_FILE, "w");
    if (debug_file == NULL) {
        print_errorf("failed to open file for debugging: %s", DEBUG_FILE);
        return EXIT_FAILURE;
    }
#endif

    /* Get and process input */
    lines = get_lines(stream);
    sort_lines(lines);
    total_paths_l = get_paths(&paths, lines.lines, lines.lines_l, options.init_paths_state);

init_tb:

    /* Setup termbox */
    if (init_termbox() != 0) {
        print_error(get_error());
        cleanup();
        return EXIT_FAILURE;
    }

    tb_set_input_mode(TB_INPUT_ESC | TB_INPUT_MOUSE);
    tb_hide_cursor();

    ret = run();

    /* TSTP signal handler */
    if (state == StateStop) {
        cleanup_termbox(); /* Restore initial terminal state */
        signal(SIGTSTP, SIG_DFL); /* Set default handler */
        raise(SIGTSTP); /* Stop process */
        signal(SIGTSTP, catch_stop); /* Process is continued: restore signal handler */
        goto init_tb; /* Init termbox again */
    }

    if (output_str != NULL) {
        cleanup_termbox();
        puts(output_str);
    }

    cleanup();

    if (ret != 0) {
        print_error(get_error());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
