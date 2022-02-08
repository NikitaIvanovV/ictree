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

#include <getopt.h>
#include <stdio.h>
#include <string.h>

#include "args.h"
#include "error.h"
#include "gen/options-msg.h"
#include "version.h"

#define VERSION_MSG         \
    "ictree v" VERSION "\n" \
    "Copyright 2022 Nikita Ivanov"

#define HELP_MSG "Usage: ictree [OPTION...] [FILE]\n" OPTIONS_MSG

static struct option long_opts[] = {
    { "fold",       no_argument,        NULL,  'f' },
    { "separator",  required_argument,  NULL,  's' },
    { "version",    no_argument,        NULL,  'v' },
    { "help",       no_argument,        NULL,  'h' },
    { 0,            0,                  NULL,  0   },
};

#define SHORT_OPTIONS "fs:vh"

enum ArgAction process_args(Options *options, int argc, char **argv)
{
    int c;

    while (1) {
        c = getopt_long(argc, argv, SHORT_OPTIONS, long_opts, NULL);

        if (c == -1)
            break;

        switch (c) {
        case 0:
            break;
        case 'f':
            options->init_paths_state = PathStateFolded;
            break;
        case 's':
            if (strlen(optarg) != 1) {
                set_error("directory separator must a single character");
                return ArgActionErrorReport;
            }
            options->separator = optarg[0];
            break;
        case 'v':
            puts(VERSION_MSG);
            return ArgActionExit;
            break;
        case 'h':
            puts(HELP_MSG);
            return ArgActionExit;
            break;
        case '?':
            return ArgActionError;
        default:
            fprintf(stderr, "getopt() returned invalid code: %d\n", c);
            abort();
        }
    }

    if (optind < argc) {
        options->filename = argv[optind];
    }

    return ArgActionDefault;
}
