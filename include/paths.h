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

#define DIR_DELIM          '/'
#define PATH_DEFAULT_STATE PathStateUnfolded

typedef enum PathState {
    PathStateUnfolded = 0,
    PathStateFolded,
} PathState;

typedef struct PathLink {
    size_t index;
} PathLink;

typedef struct Path {
    char *line;
    PathState state;
    unsigned depth;
    cvector_vector_type(PathLink) subpaths;
} Path;

typedef struct UnfoldedPaths {
    PathLink *links;
    size_t len;
} UnfoldedPaths;

Path *get_path_from_link(PathLink link);
size_t get_paths(UnfoldedPaths *unfolded_paths, char **lines, size_t lines_l);
size_t unfold_path(UnfoldedPaths *unfolded_paths, size_t i);
void fold_path(UnfoldedPaths *unfolded_paths, size_t i);
void free_paths(UnfoldedPaths unfolded_paths);

#endif
