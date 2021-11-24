#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
HOME_DIR="$(dirname "$DIR")"

NOW=$(date +"%s")
RUN_DIR="${HOME_DIR}"/run/"quickmatch_${NOW}"
mkdir -p "${RUN_DIR}"

if [[ "$1" == "clean" ]]; then
  "$DIR"/cmake.sh clean
fi

cp "${HOME_DIR}"/scripts/play_match.pl "${RUN_DIR}"
cp "${HOME_DIR}"/scripts/${3:-alice}.sh "${RUN_DIR}"
cp "${HOME_DIR}"/scripts/${4:-bob}.sh "${RUN_DIR}"
cp "${HOME_DIR}"/config/games/holdem.limit.2p.reverse_blinds.game "${RUN_DIR}"
cp "${HOME_DIR}"/bin/acpc/dealer "${RUN_DIR}"
cp "${HOME_DIR}"/config/solver/train_vcfr.json "${RUN_DIR}"

cd "${RUN_DIR}" || exit

echo "Running quickmatch.sh in $(pwd)"
export CALLED_FROM_QUICKMATCH=yes
./play_match.pl localmatch holdem.limit.2p.reverse_blinds.game ${2:-1000} 1 player1 ./${3:-alice}.sh player2 ./${4:-bob}.sh

echo "Removing run files"
rm "${RUN_DIR}"/play_match.pl
rm "${RUN_DIR}"/${3:-alice}.sh
rm "${RUN_DIR}"/${4:-bob}.sh
rm "${RUN_DIR}"/holdem.limit.2p.reverse_blinds.game
rm "${RUN_DIR}"/dealer
rm "${RUN_DIR}"/train_vcfr.json