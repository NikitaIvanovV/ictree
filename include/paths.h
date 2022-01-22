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

#ifndef PATHS_H
#define PATHS_H

#include <stdlib.h>

#include "vector.h"

#define DIR_DELIM    '/'

#define NO_MAIN_PATH        ((PathLink){ -1 })
#define PATH_LINKS_EQ(a, b) ((a).index == (b).index)
#define HAS_MAIN_PATH(path) ((path).mainpath.index != NO_MAIN_PATH.index)

typedef enum PathState {
    PathStateUnfolded = 0,
    PathStateFolded,
} PathState;

typedef struct PathLink {
    size_t index;
} PathLink;

typedef struct Path {
    char *line;
    char *full_path;
    PathState state;
    unsigned depth;
    PathLink mainpath;
    cvector_vector_type(PathLink) subpaths;
} Path;

typedef struct UnfoldedPaths {
    PathLink *links;
    size_t len;
} UnfoldedPaths;

Path *get_path_from_link(PathLink link);
long search_path(PathLink **links, char *pattern);
size_t fold_path(UnfoldedPaths *unfolded_paths, size_t i);
size_t get_paths(UnfoldedPaths *unfolded_paths, char **lines, size_t lines_l, PathState init_state);
size_t unfold_path(UnfoldedPaths *unfolded_paths, size_t i);
void free_paths(UnfoldedPaths unfolded_paths);
void free_search_path(PathLink *links);
void unfold_nested_path(UnfoldedPaths *unfolded_paths, Path *path, size_t *pos);

#endif
