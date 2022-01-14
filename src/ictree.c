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

#ifdef DEV
FILE *debug_file = NULL;
#define DEBUG_FILE "debug"
#define DEBUG(...)                        \
    do {                                  \
        fprintf(debug_file, __VA_ARGS__); \
        fflush(debug_file);               \
    } while (0)
#endif

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
#define ICON_STATUS_UNFOLDED "▶ "
#define ICON_STATUS_FOLDED   "▼ "
#define ICON_ROOT_DIR        "/"

#define PROMPT_MAX_LEN 255

#define FAILED_TO_COPY_ERR_MSG "Failed to copy"

#define RETURN_ON_TB_ERROR(func_call, msg)                 \
    do {                                                   \
        int ret = (func_call);                             \
        if (ret != TB_OK && ret != TB_ERR_OUT_OF_BOUNDS) { \
            set_errorf("%s: %s", msg, tb_strerror(ret));   \
            return 1;                                      \
        }                                                  \
    } while (0);

#define RETURN_ON_ERROR(func_call) \
    do {                           \
        if ((func_call) != 0) {    \
            return 1;              \
        }                          \
    } while (0);

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

char *filename = NULL;

static char *program_path = NULL;

static FILE *stream = NULL;
static int stream_file = 0;

static char **lines = NULL;
static size_t lines_l = 0;
static UnfoldedPaths paths = { .links = NULL, .len = 0 };
static size_t total_paths_l = 0;

static Pos pager_pos = {0, 0};
static long cursor_pos = 0;

static PromptMsg prompt_msg;

static int running = 1;

static int draw(void);
static int fold(void);
static int open_file(char *name);
static int run(void);
static int unfold(void);
static int update_screen(void);
static UpdScrSignal handle_key(struct tb_event ev);
static UpdScrSignal handle_mouse_click(int x, int y);
static UpdScrSignal handle_mouse(struct tb_event ev);
static void catch_error(int signo);
static void center_cursor(void);
static void cleanup_lines(void);
static void cleanup_paths(void);
static void cleanup_termbox(void);
static void cleanup(void);
static void copy_path(void);
static void cursor_move(int i);
static void cursor_set(long p);
static void print_error(char *error_msg);
static void print_errorf(char *format, ...);
static void reset_prompt_msg(void);
static void scroll_x(int i);
static void scroll_y(int i);
static void scroll_y_raw(int i);
static void set_prompt_msg(char *msg);
static void set_prompt_msg_err(char *msg);
static void set_prompt_msg_errf(char *format, ...);
static void set_prompt_msgf(char *format, ...);
static void toggle_fold(void);

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
    memcpy(prompt_msg.msg, msg, MIN(strlen(msg), PROMPT_MAX_LEN));
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
        subpaths_l = cvector_size(path->subpaths);
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
            fg = TB_BLACK;
            bg = TB_WHITE;
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
    snprintf(ind, PROMPT_MAX_LEN, "   %ld/%ld", paths.links[cursor_pos].index + 1, total_paths_l);
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
    }

    switch (ev.ch) {
    case 'q':
        CONTROL_ACTION(running = 0);
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
    case 'y':
        CONTROL_ACTION(copy_path());
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

static void copy_path(void)
{
    char *full_path = NULL;
    char *args[] = { "/bin/sh", "-c", "xsel --clipboard", NULL };

    int fd[2], fd_r, fd_w;
    if (pipe(fd) == -1) {
        set_prompt_msg_err(FAILED_TO_COPY_ERR_MSG ": pipe() failed");
        goto cleanup;
    }

    fd_r = fd[0];
    fd_w = fd[1];

    int pid = fork();
    if (pid == -1) {
        set_prompt_msg_err(FAILED_TO_COPY_ERR_MSG ": fork() failed");
        goto cleanup;
    } else if (pid == 0) {
        close(fd_w);

        /* Supress all output to prevent xsel from messing up the UI */
        int null = open("/dev/null", O_WRONLY);
        dup2(null, STDOUT_FILENO);
        dup2(null, STDERR_FILENO);
        close(null);

        dup2(fd_r, STDIN_FILENO);
        close(fd_r);

        execv(args[0], args);
        exit(EXIT_FAILURE);
    }

    close(fd_r);

    int status;
    Path *p = get_path_from_link(paths.links[cursor_pos]);
    full_path = get_full_path(p);

    if (write(fd_w, full_path, strlen(full_path) + 1) == -1) {
        set_prompt_msg_err(FAILED_TO_COPY_ERR_MSG ": write() failed");
    }
    close(fd_w);

    if (waitpid(pid, &status, 0) == -1) {
        set_prompt_msg_err(FAILED_TO_COPY_ERR_MSG ": waitpid() failed");
        goto cleanup;
    }

    if (WIFEXITED(status)) {
        int es = WEXITSTATUS(status);
        if (es == 127) {
            set_prompt_msg_errf(FAILED_TO_COPY_ERR_MSG ": xsel not found", es);
            goto cleanup;
        } else if (es != 0) {
            set_prompt_msg_errf(FAILED_TO_COPY_ERR_MSG ": exited with code %d", es);
            goto cleanup;
        }
    }

    set_prompt_msgf("Copied: %s", full_path);

cleanup:
    if (full_path != NULL) {
        free_full_path(full_path);
    }
}

static int run(void)
{
    int ret;
    struct tb_event ev;

    reset_prompt_msg();

    RETURN_ON_ERROR(update_screen());

    while (1) {
        if ((ret = tb_poll_event(&ev)) != TB_OK) {
            if (ret == TB_ERR_POLL && tb_last_errno() == EINTR)
                continue;
            set_errorf("failed to poll termbox event: %s", tb_strerror(ret));
            return 1;
        }
        switch (ev.type) {
        case TB_EVENT_KEY:
            if (handle_key(ev) == UpdScrSignalYes) {
                RETURN_ON_ERROR(update_screen());
                reset_prompt_msg();
            }
            break;
        case TB_EVENT_MOUSE:
            if (handle_mouse(ev) == UpdScrSignalYes) {
                RETURN_ON_ERROR(update_screen());
                reset_prompt_msg();
            }
            break;
        case TB_EVENT_RESIZE:
            RETURN_ON_ERROR(update_screen());
            break;
        }
        if (running != 1) {
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

static void cleanup_termbox(void)
{
    int ret = tb_shutdown();
    if (ret == TB_OK || ret == TB_ERR_NOT_INIT)
        return;
    print_errorf("failed to shutdown termbox: %s", tb_strerror(ret));
}

static void cleanup_lines(void)
{
    if (lines == NULL)
        return;
    free_lines(lines, lines_l);
    lines = NULL;
    lines_l = 0;
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

static void catch_error(int signo)
{
    cleanup_termbox();
}

int main(int argc, char *argv[])
{
    int ret;
    program_path = argc >= 1 ? argv[0] : "ictree";
    stream = stdin;

    ret = process_args(argc, argv);
    switch (ret) {
    case ArgActionError:
        print_error(get_error());
        return EXIT_FAILURE;
    case ArgActionExit:
        return EXIT_SUCCESS;
    }

    if (filename != NULL) {
        if (open_file(filename) != 0) {
            print_error(get_error());
            return EXIT_FAILURE;
        }
    }

    if (signal(SIGABRT, catch_error) == SIG_ERR) {
        print_error("failed to setup abort signal");
        return EXIT_FAILURE;
    }

    if (signal(SIGSEGV, catch_error) == SIG_ERR) {
        print_error("failed to setup segmentation fault signal");
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
    get_lines(&lines, &lines_l, stream);
    sort_lines(lines, lines_l);
    total_paths_l = get_paths(&paths, lines, lines_l);

    /* Setup termbox */
    if ((ret = tb_init()) != TB_OK) {
        print_errorf("failed to init termbox: %s", tb_strerror(ret));
        cleanup();
        return EXIT_FAILURE;
    }

    tb_set_input_mode(TB_INPUT_ESC | TB_INPUT_MOUSE);
    tb_hide_cursor();

    ret = run();
    cleanup();

    if (ret != 0) {
        print_error(get_error());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
