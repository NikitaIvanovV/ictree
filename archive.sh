#!/bin/sh

err() {
    echo "$@" >&2
    exit 1
}

file="$1"
[ -z "$file" ] && err "Filename was not given"

list="$(mktemp archive-list.XXXXXX)"
trap 'rm "$list"' EXIT

git ls-files --full-name --recurse-submodules > "$list"

format="${file#*.}"

case "$format" in
    tar.gz)
        tar -czf "$file" --no-recursion --verbatim-files-from -T "$list"
        ;;
    zip)
        zip -q "$file" -@ < "$list"
        ;;
    *)
        err "Invalid archive format $format"
        ;;
esac
