#!/bin/sh
set -eux

. ./extra/travis/env.sh

cmake . \
  -DCMAKE_INCLUDE_PATH="$CACHE_DIR/usr/lib" \
  -DENABLE_TESTS=ON -DENABLE_WERROR=ON \
  -DENABLE_DBUS=ON \
  -DENABLE_AUTOUPDATE=ON
make -j`nproc`

./run_tests.sh
