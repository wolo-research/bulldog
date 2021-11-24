#!/bin/bash

[ -z $CALLED_FROM_QUICKMATCH ] && echo "[ERROR] To start this please run ./quickmatch.sh"

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
HOME_DIR="$(dirname "$(dirname "$DIR")")"

"${HOME_DIR}"/bin/agent \
            --game=holdem.limit.2p.reverse_blinds.game \
            --engine_params=train_vcfr.json \
            --connector=0 \
            --connector_params=$1,$2