#!/bin/sh
# SPDX-License-Identifier: LGPL-2.1-or-later
set -eu

if [ "$#" -ne 1 ]; then
    echo "Usage: sh $0 /absolute/path/to/extension.zip" >&2
    exit 2
fi

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
runtime_dir=$(mktemp -d "${TMPDIR:-/tmp}/bamboo-shell-test.XXXXXX")
cleanup() {
    # The document portal may mount a FUSE filesystem in the private runtime
    # directory. Detach that mount before removing our temporary files.
    if command -v fusermount3 >/dev/null 2>&1; then
        fusermount3 -uz "$runtime_dir/doc" 2>/dev/null || true
    fi
    rm -rf --one-file-system -- "$runtime_dir"
}
trap cleanup EXIT HUP INT TERM

# Isolate runtime sockets and GNOME's startup guard as well as the session bus.
# gnome-shell-test-tool separately creates temporary configuration/data dirs.
XDG_RUNTIME_DIR="$runtime_dir" timeout --kill-after=5s 45s dbus-run-session -- \
    gnome-shell-test-tool --headless --extension "$1" "$script_dir/test-shell.js"
