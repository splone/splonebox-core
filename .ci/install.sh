#!/usr/bin/env bash

set -e
set -o pipefail

if [[ -n "${CI_TARGET}" ]]; then
  exit
fi

if [[ "${TRAVIS_OS_NAME}" == osx ]]; then
  brew install gettext
  brew reinstall -s libtool
fi

# Use default CC to avoid compilation problems when installing Python modules.
echo "Install coveralls for Python 2."
CC=cc pip2.7 -q install --user --upgrade cpp-coveralls
