#!/bin/sh

text="$(MANWIDTH=80 man -P cat -l "$1")" || exit 1

echo '#define OPTIONS_MSG \'
echo '"\\n" \'
echo '"Options:\\n" \'
echo "$text" |
    sed -n '/^OPTIONS/,$p' |
    sed '1d;  /^[A-Z]/q' |
    sed '$d; s/^/"/; s/$/\\n" \\/' |
    sed '$s/ \\$//'
