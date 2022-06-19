#!/bin/sh

text="$(MANWIDTH=80 man -l "$1" | col -bx)" || {
    echo 'Error: failed to get man page content' >&2
    exit 1
}

body() {
    echo "$text" |
        sed -n "/^$1/,/^[A-Z]/"'{/^[A-Z]\|^\s*$/!p}'
}

rm_indent() {
    sed 's/^\s*//'
}

make_macro() {
    echo "#define $1 \\"
    sed -e 's/^/"/' -e 's/$/"/' -e '$!s/$/ "\\n" \\/'
}

msg() {
    printf 'Usage:\t'
    body SYNOPSIS | rm_indent
    echo
    echo 'Options:'
    body OPTIONS
}

msg | make_macro 'HELP_MSG'
