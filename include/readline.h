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

#ifndef READLINE_H
#define READLINE_H

#define READLINE_LINE_BUF_LEN 64

typedef struct {
    enum {
        ReadlineCurLeft,
        ReadlineCurRight,
        ReadlineHistUp,
        ReadlineHistDown,
        ReadlineType,
        ReadlineDelete,
        ReadlineBackspace,
        ReadlineEnter,
        ReadlineClear,
    } type;
    char ch;
} ReadlineEvent;

typedef struct ReadlineLine {
    char buf[READLINE_LINE_BUF_LEN];
    int len;
    struct ReadlineLine *prev;
    struct ReadlineLine *next;
} ReadlineLine;

typedef struct {
    int cursor;
    ReadlineLine *line;
    ReadlineLine *last_line;
} ReadlineCtx;

void cleanup_readline_ctx(ReadlineCtx *ctx);
void init_readline_ctx(ReadlineCtx *ctx);
void readline_send(ReadlineCtx *ctx, ReadlineEvent ev);

#endif
