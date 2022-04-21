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
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ictree. If not, see <https://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "utils.h"

#define MAX_CONF_LINE_LEN (CMD_MAX_LEN + 64)

#define IS_CHAR_WHITESPACE(c) ((c) == ' ' || (c) == '\t' || (c) == '\n')

#define READ_CONF_ERR "Failed to read config: "

static Command **command;

static void add_command(char ch, char *s)
{
    Command *new = malloc(sizeof(Command));
    assert(new != NULL);

    new->ch = ch;

    memset(new->cmd, 0, CMD_MAX_LEN);
    strncpy(new->cmd, s, CMD_MAX_LEN - 1);

    new->next = *command;
    *command = new;
}

static char *walk_line(int *length, char *s)
{
    char *word = NULL;

    /* Find beginning of word */
    while (1) {
        if (*s == '\0')
            break;

        if (!IS_CHAR_WHITESPACE(*s)) {
            word = s;
            break;
        }

        s++;
    }

    if (word == NULL)
        return NULL;

    /* Find word length */
    do {
        s++;
    } while (*s != '\0' && !IS_CHAR_WHITESPACE(*s));

    *length = s - word;
    return word;
}

#define CHECK_WALK_LINE_ERR                                                     \
    do {                                                                        \
        if (word == NULL) {                                                     \
            set_errorf(READ_CONF_ERR "line %d: map requires two arguments", n); \
            return 1;                                                           \
        }                                                                       \
    } while (0)

static int parse_line(int n, char *s)
{
    int len;
    char ch, *word;

    n++;

    /* Get command name */
    word = walk_line(&len, s);

    /* Ignore a line consisting of whitespaces or starts with #
     * or " */
    if (word == NULL || word[0] == '"' || word[0] == '#')
        return 0;

    if (strncmp(word, "map", len) != 0) {
        set_errorf(READ_CONF_ERR "line %d: unknown command %.*s", n, len, word);
        return 1;
    }

    s = word + len;

    /* Get char */
    word = walk_line(&len, s);

    CHECK_WALK_LINE_ERR;

    if (len > 1) {
        set_errorf(READ_CONF_ERR "line %d: mapping cannot consist of multiple characters: %.*s", n, len, word);
        return 1;
    }

    ch = word[0];

    s = word + len;

    /* Get shell command */
    word = walk_line(&len, s);

    CHECK_WALK_LINE_ERR;

    len = strlen(word);
    if (len >= CMD_MAX_LEN) {
        set_errorf(READ_CONF_ERR "line %d: command length cannot be longer than or equal to %d", n, CMD_MAX_LEN);
        return 1;
    }

    word[len - 1] = '\0'; /* Remove \n */
    add_command(ch, word);

    return 0;
}

static int parse_config(FILE *f)
{
    static char buf[MAX_CONF_LINE_LEN + 1];

    int n = 0;

    while (1) {
        if (fgets(buf, MAX_CONF_LINE_LEN, f) == NULL) {
            if (errno == EINTR) {
                continue;
            } else {
                break;
            }
        }

        if (parse_line(n, buf) != 0)
            return 1;

        n++;
    }

    return 0;
}

int read_config(Command **cmd)
{
    int ret = 0;

    assert(*cmd == NULL);
    command = cmd;

    static char config_path[FILENAME_MAX];

    char *xdg_conf_p = getenv("XDG_CONFIG_HOME");
    if (xdg_conf_p != NULL) {
        strncpy(config_path, xdg_conf_p, FILENAME_MAX - 1);
    } else {
        strncpy(config_path, getenv("HOME"), FILENAME_MAX - 1);
        strncat(config_path, "/.config", FILENAME_MAX - 1);
    }

    strncat(config_path, "/ictree/config", FILENAME_MAX - 1);

    FILE *f = fopen(config_path, "r");
    if (f == NULL) {
        /* Ignore if config file does not exist */
        if (errno == ENOENT)
            return 0;

        set_errorf(READ_CONF_ERR "%s", strerror(errno));
        return 1;
    }

    if (parse_config(f) != 0)
        ret = 1;

    fclose(f);
    return ret;
}

void free_command(Command *command)
{
    Command *t;

    while (command != NULL) {
        t = command->next;
        free(command);
        command = t;
    }
}
