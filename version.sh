#!/bin/sh

ver() {
	git describe 2>/dev/null ||
		sed '/\<VERSION\>/!d; s/.*"\([^"]*\)".*/v\1/' "$1"
}

ver "$@" | sed 'y/./_/'
