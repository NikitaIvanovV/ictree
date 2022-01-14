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

#include "paths.h"
#include "utils.h"
#include "vector.h"

static cvector_vector_type(Path) paths;

/*
 * Find length of the first component path component
 */
static unsigned long get_first_component_length(char *path)
{
    char c;
    unsigned long len = strlen(path);

    for (unsigned long i = 0; i < len; i++) {
        c = path[i];
        if (c == DIR_DELIM) {
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

void fold_path(UnfoldedPaths *unfolded_paths, size_t i)
{
    assert(i < unfolded_paths->len);

    Path *p = get_path_from_link(unfolded_paths->links[i]);

    if (cvector_size(p->subpaths) == 0)
        goto end;

    unsigned depth = p->depth;

    size_t k;

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
    unfolded_paths->len -= k - i - 1;

end:

    p->state = PathStateFolded;
}

char *get_full_path(Path *p)
{
    PathLink mainpath_l;
    unsigned long str_l;
    cvector_vector_type(Path *) stack = NULL;

    while (1) {
        cvector_push_back(stack, p);
        mainpath_l = p->mainpath;
        if (mainpath_l.index == (size_t)-1)
            break;
        p = get_path_from_link(mainpath_l);
    }

    str_l = 0;
    for (long i = cvector_size(stack) - 1; i >= 0; i--) {
        str_l += strlen(stack[i]->line);
        str_l += 1; /* For dir delimeter */
    }

    char *str = malloc((str_l + 1) * sizeof(char));
    str[0] = '\0';

    for (long i = cvector_size(stack) - 1; i >= 0; i--) {
        strcat(str, stack[i]->line);
        if (i > 0)
            strcat(str, "/");
    }

    cvector_free(stack);

    return str;
}

void free_full_path(char *str)
{
    free(str);
}

size_t get_paths(UnfoldedPaths *unfolded_paths, char **lines, size_t lines_l)
{
    Path p;
    PathLink pl;
    char *line;
    size_t i;
    unsigned long comp_len, line_off, depth;

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

        if (strlen(line) == 0 && depth > 0) {
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
        p.mainpath = (PathLink){ -1 };
        p.state = PATH_DEFAULT_STATE;
        p.depth = depth;
        pl = (PathLink){cvector_size(paths)};

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
            p.mainpath = mainpath_l;
            if (mainpath->state == PathStateUnfolded) {
                cvector_push_back(unfolded_paths->links, pl);
            }
        } else {
            cvector_push_back(unfolded_paths->links, pl);
        }

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
        }
        cvector_free(paths);
        paths = NULL;
    }
    if (unfolded_paths.links != NULL) {
        cvector_free(unfolded_paths.links);
        unfolded_paths.links = NULL;
    }
}
