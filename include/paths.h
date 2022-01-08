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
