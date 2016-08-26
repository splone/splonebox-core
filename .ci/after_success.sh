#!/usr/bin/env bash

set -e
set -o pipefail

if [[ -n "${GCOV}" ]]; then
  coveralls -e test -e out/CMakeFiles/CompilerIdC/CMakeCCompilerId.c -e out/CMakeFiles/CompilerIdCXX/CMakeCXXCompilerId.cpp --gcov "$(which "${GCOV}")" --encoding iso-8859-1 -b out || echo 'coveralls upload failed.'
fi
