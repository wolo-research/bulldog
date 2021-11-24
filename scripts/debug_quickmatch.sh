#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
HOME_DIR="$(dirname "$DIR")"

NOW=$(date +"%s")
RUN_DIR="${HOME_DIR}"/run/"debug_quickmatch_${NOW}"
mkdir -p "${RUN_DIR}"

cp "${HOME_DIR}"/scripts/example_player "${RUN_DIR}"
cp "${HOME_DIR}"/config/games/holdem.nolimit.2p.game "${RUN_DIR}"
cp "${HOME_DIR}"/bin/acpc/dealer "${RUN_DIR}"

cd "${RUN_DIR}" || exit

echo "Running dealer"
./dealer -p 52000,52001 debug_match holdem.nolimit.2p.game  1 0 Alice Bob &
sleep 1
echo "Start one example player on port 52000"
./example_player holdem.nolimit.2p.game localhost 52000
