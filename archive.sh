#!/bin/sh

err() {
    echo "$@" >&2
    exit 1
}

format="$1"
[ -z "$format" ] && err "Archive format was not given"

list="$(mktemp archive-list.XXXXXX)"

git ls-files --full-name --recurse-submodules > "$list"

file="ictree-$(git describe).$format"

case "$format" in
    tar.gz)
        tar -czf "$file" --no-recursion --verbatim-files-from -T "$list"
        ;;
    zip)
        zip -q "$file" -@ < "$list"
        ;;
    *)
        rm "$list"
        err "Invalid archive format $format"
        ;;
esac

rm "$list"
