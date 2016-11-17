#!/usr/bin/env bash

set -e
set -o pipefail

if [[ -n "${GCOV}" ]]; then
  coveralls -e test -e build/CMakeFiles/CompilerIdC/CMakeCCompilerId.c -e build/CMakeFiles/CompilerIdCXX/CMakeCXXCompilerId.cpp --gcov "$(which "${GCOV}")" --encoding iso-8859-1 -b build || echo 'coveralls upload failed.'
fi
