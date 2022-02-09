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
#include <stdlib.h>
#include <string.h>

#include "readline.h"
#include "utils.h"

static ReadlineLine *new_line(ReadlineLine *prev) {
    ReadlineLine *line = malloc(sizeof(ReadlineLine));
    assert(line != NULL);

    memset(line->buf, 0, READLINE_LINE_BUF_LEN);
    line->len = 0;
    line->next = NULL;
    line->prev = prev;
    if (prev != NULL)
        prev->next = line;

    return line;
}

static void free_line(ReadlineLine *line)
{
    free(line);
}

static void insert_char(ReadlineCtx *ctx, char ch)
{
    int cur = ctx->cursor;
    ReadlineLine *line = ctx->line;

    /* Buffer is full? */
    if (line->len >= READLINE_LINE_BUF_LEN - 1)
        return;

    if (cur < line->len) {
        memmove(line->buf + ((cur + 1) * sizeof(char)),
                line->buf + (cur * sizeof(char)), line->len - cur);
    }

    line->buf[cur] = ch;
    line->len++;
    ctx->cursor++;
}

static void del_char(ReadlineCtx *ctx, int cur)
{
    ReadlineLine *line = ctx->line;

    if (cur <= 0 || cur > line->len)
        return;

    memmove(line->buf + ((cur - 1) * sizeof(char)),
            line->buf + (cur * sizeof(char)),
            line->len - cur + 1); /* +1 to also move \0 */

    line->len--;
}

static void shift_cursor(ReadlineCtx *ctx, int i)
{
    int new_cur = MIN(ctx->line->len, MAX(0, ctx->cursor + i));
    ctx->cursor = new_cur;
}

static void set_new_line(ReadlineCtx *ctx, ReadlineLine *line)
{
    if (line == NULL)
        return;

    ctx->line = line;
    ctx->cursor = line->len;
}

static void history_remove_line(ReadlineLine *line)
{
    ReadlineLine *prev = line->prev, *next = line->next;

    if (prev != NULL)
        prev->next = next;

    if (next != NULL)
        next->prev = prev;

    line->prev = NULL;
    line->next = NULL;
}

static void history_insert_line(ReadlineLine *line, ReadlineLine *before)
{
    ReadlineLine *after = before->prev;

    before->prev = line;
    if (after != NULL)
        after->next = line;
    line->prev = after;
    line->next = before;
}

static void add_history(ReadlineCtx *ctx)
{
    ctx->cursor = 0;

    ReadlineLine *blank_line;

    if (ctx->line != ctx->last_line) {
        history_remove_line(ctx->line);

        if (ctx->line->len <= 0) {
            free_line(ctx->line);
            ctx->line = ctx->last_line;
            return;
        }

        history_insert_line(ctx->line, ctx->last_line);
        blank_line = ctx->last_line;
    } else {
        if (ctx->line->len <= 0)
            return;

        blank_line = new_line(ctx->last_line);
    }

    ctx->line = blank_line;
    ctx->last_line = ctx->line;
}

static void clear_current_line(ReadlineCtx *ctx)
{
    ctx->cursor = 0;
    ctx->line = ctx->last_line;
    ctx->line->len = 0;
    ctx->line->buf[0] = '\0';
}

void init_readline_ctx(ReadlineCtx *ctx)
{
    ctx->cursor = 0;
    ctx->line = new_line(NULL);
    ctx->last_line = ctx->line;
}

void cleanup_readline_ctx(ReadlineCtx *ctx)
{
    ReadlineLine *t, *line = ctx->last_line;

    while (line != NULL) {
        t = line;
        line = line->prev;
        free_line(t);
    }
}

void readline_send(ReadlineCtx *ctx, ReadlineEvent ev)
{
    switch (ev.type) {
    case ReadlineType:
        insert_char(ctx, ev.ch);
        break;
    case ReadlineBackspace:
        del_char(ctx, ctx->cursor);
        ctx->cursor--;
        break;
    case ReadlineDelete:
        del_char(ctx, ctx->cursor + 1);
        break;
    case ReadlineCurRight:
        shift_cursor(ctx, 1);
        break;
    case ReadlineCurLeft:
        shift_cursor(ctx, -1);
        break;
    case ReadlineHistUp:
        set_new_line(ctx, ctx->line->prev);
        break;
    case ReadlineHistDown:
        set_new_line(ctx, ctx->line->next);
        break;
    case ReadlineEnter:
        add_history(ctx);
        break;
    case ReadlineClear:
        clear_current_line(ctx);
        break;
    default:
        abort();
    }
}
