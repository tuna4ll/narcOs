#!/bin/sh
set -eu
branch="${LIMINE_BRANCH:-v12.x}"
if [ ! -d limine/.git ]; then
    git clone --depth=1 --branch "$branch" https://github.com/limine-bootloader/limine.git limine
fi
make -C limine
