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

#include "args.h"
#include "error.h"
#include "options-msg.h"
#include "version.h"

#define VERSION_MSG         \
    "ictree v" VERSION "\n" \
    "Copyright 2022 Nikita Ivanov"

#define HELP_MSG "Usage: ictree [OPTION]... [FILE]\n" OPTIONS_MSG

static struct option long_opts[] = {
    { "version", no_argument, 0, 'v' },
    { "help", no_argument, 0, 'h' },
    { 0, 0, 0, 0 },
};

extern char *filename;

enum ArgAction process_args(int argc, char **argv)
{
    int c;
    int opt_i = 0;

    /* Don't let getopt print errors */
    opterr = 0;

    while (1) {
        c = getopt_long(argc, argv, "vh", long_opts, &opt_i);

        if (c == -1)
            break;

        switch (c) {
        case 'v':
            puts(VERSION_MSG);
            return ArgActionExit;
        case 'h':
            printf("%s", HELP_MSG);
            return ArgActionExit;
        default:
            set_errorf("invalid option: %s", argv[opt_i + 1]);
            return ArgActionError;
        }
    }

    if (opt_i + 1 < argc) {
        filename = argv[opt_i + 1];
    }

    return ArgActionDefault;
}
