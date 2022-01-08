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
#include <termbox.h>

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
#define MAX_PATHS ((long)paths.len)

#define INDENT               2
#define ICON_STATUS_LEN      2
#define ICON_STATUS_DEFAULT  "• "
#define ICON_STATUS_UNFOLDED "▶ "
#define ICON_STATUS_FOLDED   "▼ "
#define ICON_ROOT_DIR        "/"

typedef struct Pos {
    long x, y;
} Pos;

static char *program_path = NULL;

static char **lines = NULL;
static size_t lines_l = 0;
static UnfoldedPaths paths = { .links = NULL, .len = 0 };
static size_t total_paths_l = 0;

static Pos pager_pos = {0, 0};
static long cursor_pos = 0;

static int running = 1;

static int fold(void);
static int handle_key(struct tb_event ev);
static int handle_mouse_click(int x, int y);
static int handle_mouse(struct tb_event ev);
static int run(void);
static int unfold(void);
static void catch_error(int signo);
static void center_cursor(void);
static void cleanup_lines_list(void);
static void cleanup_paths(void);
static void cleanup(void);
static void cursor_move(int i);
static void cursor_set(long p);
static void draw(void);
static void print_error(char *error_msg);
static void print_errorf(char *format, ...);
static void scroll_x(int i);
static void scroll_y(int i);
static void scroll_y_raw(int i);
static void toggle_fold(void);
static void update_screen(void);

static void print_error(char *error_msg)
{
    fprintf(stderr, "%s: %s\n", program_path, error_msg);
}

static void print_errorf(char *format, ...)
{
    va_list args;
    char msg[ERROR_BUF_SIZE];
    va_start(args, format);
    vsnprintf(msg, ERROR_BUF_SIZE, format, args);
    va_end(args);
    print_error(msg);
}

static void scroll_x(int i)
{
    pager_pos.x += i;
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

static void draw(void)
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

        tb_printf(x, y, fg, bg, "%s%s", status_icon, path_line + char_off);
        if (x + strlen(path->line) >= (unsigned long)TREE_VIEW_X)
            tb_set_cell(TREE_VIEW_X - 1, y, '>', TB_BLACK, TB_WHITE);
        if (char_off > 0)
            tb_set_cell(0, y, '<', TB_BLACK, TB_WHITE);
        y++;
    }

    /* Draw prompt */

    char ind[SCREEN_X];
    snprintf(ind, SCREEN_X, "%ld/%ld", paths.links[cursor_pos].index + 1, total_paths_l);
    x = TREE_VIEW_X - strlen(ind);
    y = TREE_VIEW_Y + PROMPT_HEIGHT - 1;
    tb_print(x, y, TB_WHITE, TB_DEFAULT, ind);
}

static void update_screen(void)
{
    tb_clear();
    draw();
    tb_present();
}

#define CONTROL_ACTION(action) \
    do {                       \
        action;                \
        return 1;              \
    } while (0)

#define SCROLL_X 4
#define SCROLL_Y 1

static int handle_key(struct tb_event ev)
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
    }

    return 0;
}

static int handle_mouse(struct tb_event ev)
{
    switch (ev.key) {
    case TB_KEY_MOUSE_LEFT:
        return handle_mouse_click(ev.x, ev.y);
    case TB_KEY_MOUSE_WHEEL_DOWN:
        CONTROL_ACTION(scroll_y(SCROLL_Y));
    case TB_KEY_MOUSE_WHEEL_UP:
        CONTROL_ACTION(scroll_y(-SCROLL_Y));
    }

    return 0;
}

static int handle_mouse_click(int x, int y)
{
    long p = TREE_VIEW_TOP + y;
    if (p < 0 || p >= MAX_PATHS || y >= TREE_VIEW_Y)
        return 0;
    cursor_set(p);
    toggle_fold();
    return 1;
}

static int run(void)
{
    int ret;
    struct tb_event ev;

    update_screen();

    while (1) {
        if ((ret = tb_poll_event(&ev)) != TB_OK) {
            if (ret == TB_ERR_POLL && tb_last_errno() == EINTR)
                continue;
            set_errorf("failed to poll termbox event: %d", ret);
            return 1;
        }
        switch (ev.type) {
        case TB_EVENT_KEY:
            if (handle_key(ev) == 1) {
                update_screen();
            }
            break;
        case TB_EVENT_MOUSE:
            if (handle_mouse(ev) == 1) {
                update_screen();
            }
            break;
        case TB_EVENT_RESIZE:
            update_screen();
            break;
        }
        if (running != 1) {
            break;
        }
    }

    return 0;
}

static void cleanup_lines_list(void)
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
    tb_shutdown();
    cleanup_paths();
    cleanup_lines_list();
#ifdef DEV
    if (debug_file != NULL)
        fclose(debug_file);
#endif
}

static void catch_error(int signo)
{
    tb_shutdown();
}

int main(int argc, char *argv[])
{
    int ret;
    FILE *stream = stdin;
    program_path = argc >= 1 ? argv[0] : "ictree";

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
        print_errorf("failed to init termbox: %d", ret);
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
