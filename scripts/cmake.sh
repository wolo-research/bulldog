#!/bin/bash

if [ -z "$OPENSSL_ROOT_DIR" ]; then
    export OPENSSL_ROOT_DIR=/usr/local/opt/openssl
fi

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
HOME_DIR="$(dirname "$DIR")"

if [[ "$1" == "clean" ]]; then
  rm -rf "${HOME_DIR}"/build
  rm -rf "${HOME_DIR}"/bin
  mkdir "${HOME_DIR}"/build
  cd "${HOME_DIR}"/build || exit
  cmake ..
  make
  make install
  cd ..
fi