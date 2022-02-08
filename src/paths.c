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

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <regex.h>

#include "error.h"
#include "paths.h"
#include "utils.h"
#include "vector.h"

#define REGEX_ERR_HANDLER(ret)                                    \
    do {                                                          \
        char err_buf[64];                                         \
        regerror(ret, &search_ctx.reg, err_buf, LENGTH(err_buf)); \
        set_errorf("regex failed: %s", err_buf);                  \
    } while (0)

typedef struct SearchContext {
    regex_t reg;
    char *pattern;
    enum SearchDir dir;
    int full_path;
    int init;
} SearchContext;

static SearchContext search_ctx = { .init = 0 };

static cvector_vector_type(Path) paths;

static char sep;

/*
 * Find length of the first component path component
 */
static unsigned long get_first_component_length(char *path)
{
    char c;
    unsigned long len = strlen(path);

    for (unsigned long i = 0; i < len; i++) {
        c = path[i];
        if (c == sep) {
            return i;
        }
    }

    return len;
}

Path *get_path_from_link(PathLink link)
{
    return paths + link.index;
}

/*
 * Warning: you should check if a path is already unfolded or not: unfolding an
 * unfolded path causes it to appear multiple times in the tree
 */
size_t unfold_path(UnfoldedPaths *unfolded_paths, size_t i)
{
    assert(i < unfolded_paths->len);

    Path *p = get_path_from_link(unfolded_paths->links[i]);

    size_t first_subpath_i, j, off = 0, subpaths_l = cvector_size(p->subpaths);

    if (subpaths_l == 0)
        goto end;

    first_subpath_i = i + 1;

    /* Move paths down in the array to make room for subpaths of *p */
    memmove(unfolded_paths->links + first_subpath_i + subpaths_l,
            unfolded_paths->links + first_subpath_i,
            (unfolded_paths->len - first_subpath_i) * sizeof(PathLink));

    unfolded_paths->len += subpaths_l;
    assert(unfolded_paths->len <= cvector_size(paths));

    /* Add subpaths */
    for (j = 0; j < subpaths_l; j++) {
        unfolded_paths->links[first_subpath_i + j] = p->subpaths[j];
    }

    /* Unfold subpaths. This part (or the whole thing) needs to optimized
     * because unfolding a path with many subpaths may be quite slow
     * compared to folding. */
    for (j = 0; j < subpaths_l; j++) {
        if (get_path_from_link(p->subpaths[j])->state == PathStateUnfolded) {
            off += unfold_path(unfolded_paths, first_subpath_i + j + off);
        }
    }

end:

    p->state = PathStateUnfolded;
    return off + subpaths_l;
}

size_t fold_path(UnfoldedPaths *unfolded_paths, size_t i)
{
    assert(i < unfolded_paths->len);

    size_t k, diff = 0;

    Path *p = get_path_from_link(unfolded_paths->links[i]);

    if (cvector_size(p->subpaths) == 0)
        goto end;

    unsigned depth = p->depth;

    /* Find the first path that is not a subpath of *p */
    for (k = i + 1; k < unfolded_paths->len; k++) {
        Path *p2 = get_path_from_link(unfolded_paths->links[k]);
        if (p2->depth <= depth) {
            break;
        }
    }

    /* Move all the paths after *p (that are not its subpaths) to a
     * position right after *p, thus erasing the subpaths */
    memmove(unfolded_paths->links + i + 1, unfolded_paths->links + k,
            (unfolded_paths->len - k) * sizeof(PathLink));
    diff = k - i - 1;
    unfolded_paths->len -= diff;

end:

    p->state = PathStateFolded;
    return diff;
}

void unfold_nested_path(UnfoldedPaths *unfolded_paths, Path *path, size_t *pos)
{
    Path *p = path;
    cvector_vector_type(Path *) queue = NULL;

    while (1) {
        cvector_push_back(queue, p);

        if (!HAS_MAIN_PATH(*p))
            break;

        p = get_path_from_link(p->mainpath);
    }

    size_t u_i = 0;
    long i = cvector_size(queue) - 1;
    while (1) {
        p = get_path_from_link(unfolded_paths->links[u_i]);
        if (p == queue[i]) {
            if (p->state == PathStateFolded)
                unfold_path(unfolded_paths, u_i);
            if (p == path) {
                if (pos != NULL)
                    *pos = u_i;
                break;
            }
            i--;
            assert(i >= 0);
        }
        u_i++;
        assert(u_i < unfolded_paths->len);
    }

    cvector_free(queue);
}

static char *get_full_path(PathLink *links, size_t links_l, char *line)
{
    unsigned long i;
    unsigned long str_l;

    str_l = 0;
    for (i = 0; i < links_l; i++) {
        str_l += strlen(get_path_from_link(links[i])->line);
        str_l += 1; /* For dir delimeter */
    }

    if (strlen(line) == 0)
        line = "/";

    str_l += strlen(line);

    char *str = calloc(str_l + 1, sizeof(char));

    for (i = 0; i < links_l; i++) {
        strncat(str, get_path_from_link(links[i])->line, str_l);
        strncat(str, "/", str_l);
    }

    strncat(str, line, str_l);

    return str;
}

static void free_full_path(char *str)
{
    free(str);
}

static int init_search_ctx(SearchContext *ctx, char *pattern, enum SearchDir dir)
{
    int ret;

    assert(!ctx->init);

    ctx->pattern = strdup(pattern);

    if ((ret = regcomp(&ctx->reg, ctx->pattern, REG_EXTENDED)) != 0)
        return ret;

    /* Perform search by full path if pattern contains DIR_DELIM */
    ctx->full_path = strchr(ctx->pattern, sep) != NULL;

    ctx->dir = dir;
    ctx->init = 1;

    return 0;
}

static void deinit_search_ctx(SearchContext *ctx)
{
    assert(ctx->init);

    regfree(&ctx->reg);
    free(ctx->pattern);
    ctx->init = 0;
}

size_t get_paths(UnfoldedPaths *unfolded_paths, char **lines, size_t lines_l, char separator, PathState init_state)
{
    Path p;
    PathLink pl;
    char *line;
    size_t i;
    unsigned long comp_len, line_off, depth;

    sep = separator;

    paths = NULL;
    unfolded_paths->links = NULL;

    /* Set initial capacity for vectors */
    cvector_grow(paths, lines_l);
    cvector_grow(unfolded_paths->links, lines_l);

    cvector_vector_type(PathLink) stack = NULL;

    i = 0, line_off = 0;
    while (i < lines_l) {
        if (line_off == 0)
            depth = 0;

        line = lines[i] + line_off;
        comp_len = get_first_component_length(line);

        line_off += comp_len + 1;

        if (comp_len == 0 && depth > 0) {
            i++;
            line_off = 0;
            continue;
        }

        /* If it's the last component of path, start processing next line.
         * Otherwise replace DIR_DELIM with 0 to make a new string */
        if (line[comp_len] == '\0') {
            i++;
            line_off = 0;
        } else {
            line[comp_len] = '\0';
        }

        p.line = line;
        p.subpaths = NULL;
        p.subpaths_l = 0;
        p.mainpath = NO_LINK;
        p.state = init_state;
        p.depth = depth;
        pl = (PathLink){ cvector_size(paths) };

        if (depth < cvector_size(stack)) {
            if (strcmp(p.line, get_path_from_link(stack[depth])->line) == 0) {
                depth++;
                continue;
            }
            cvector_set_size(stack, depth);
        }

        if (cvector_size(stack) > 0) {
            PathLink mainpath_l = stack[cvector_size(stack) - 1];
            Path *mainpath = get_path_from_link(mainpath_l);
            cvector_push_back(mainpath->subpaths, pl);
            mainpath->subpaths_l++;
            p.mainpath = mainpath_l;
            if (mainpath->state == PathStateUnfolded) {
                cvector_push_back(unfolded_paths->links, pl);
            }
        } else {
            cvector_push_back(unfolded_paths->links, pl);
        }

        p.full_path = get_full_path(stack, cvector_size(stack), p.line);

        cvector_push_back(stack, pl);
        cvector_push_back(paths, p);
        depth++;
    }

    cvector_free(stack);
    unfolded_paths->len = cvector_size(unfolded_paths->links);
    return cvector_size(paths);
}

void free_paths(UnfoldedPaths unfolded_paths)
{
    if (paths != NULL) {
        for (size_t i = 0; i < cvector_size(paths); i++) {
            cvector_free(paths[i].subpaths);
            free_full_path(paths[i].full_path);
        }
        cvector_free(paths);
        paths = NULL;
    }

    if (unfolded_paths.links != NULL) {
        cvector_free(unfolded_paths.links);
        unfolded_paths.links = NULL;
    }

    if (search_ctx.init) {
        deinit_search_ctx(&search_ctx);
    }
}

int init_paths_search(char *pattern, enum SearchDir dir)
{
    int ret;

    if (search_ctx.init) {
        deinit_search_ctx(&search_ctx);
    }

    if ((ret = init_search_ctx(&search_ctx, pattern, dir)) != 0) {
        REGEX_ERR_HANDLER(ret);
        return 1;
    }

    return 0;
}

enum MatchStatus path_match_pattern(Path *path)
{
    int ret;
    char *s;

    if (!search_ctx.init)
        return MatchStatusFail;

    s = search_ctx.full_path ? path->full_path : path->line;
    ret = regexec(&search_ctx.reg, s, 0, NULL, 0);

    if (ret == 0)
        return MatchStatusOk;

    if (ret == REG_NOMATCH) {
        return MatchStatusFail;
    }

    REGEX_ERR_HANDLER(ret);
    return MatchStatusErr;
}

int search_path(PathLink *match, PathLink start, int invert_dir)
{
    if (!search_ctx.init) {
        set_error("Search query was not given");
        return 1;
    }

    int dir = search_ctx.dir;
    if (invert_dir)
        dir *= -1;

    long i = start.index + dir;
    while (i < (long)cvector_size(paths) && i >= 0) {
        enum MatchStatus st = path_match_pattern(paths + i);
        if (st == 0) {
            *match = (PathLink){ .index = i };
            return 0;
        } else if (st == MatchStatusErr) {
            return 1;
        }
        i += dir;
    }

    *match = NO_LINK;
    return 0;
}
