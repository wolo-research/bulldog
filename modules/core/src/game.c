/*
Copyright (C) 2011 by the Computer Poker Research Group, University of Alberta
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include <stdint.h>
#include "../include/bulldog/game.h"

#include "evalHandTables"

#ifdef _WIN32
//Windows, gotta make some tweeks
#define strncasecmp(x,y,z) _strnicmp(x,y,z)
#endif

static enum ActionType charToAction[256] = {
    /* 0x0X */
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    /* 0x1X */
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    /* 0x2X */
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    /* 0x3X */
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    /* 0x4X */
    a_invalid, a_invalid, a_raise, a_call,
    a_invalid, a_invalid, a_fold, a_invalid,
    a_invalid, a_invalid, a_invalid, a_call,
    a_invalid, a_invalid, a_invalid, a_invalid,
    /* 0x5X */
    a_invalid, a_invalid, a_raise, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    /* 0x6X */
    a_invalid, a_invalid, a_raise, a_call,
    a_invalid, a_invalid, a_fold, a_invalid,
    a_invalid, a_invalid, a_invalid, a_call,
    a_invalid, a_invalid, a_invalid, a_invalid,
    /* 0x7X */
    a_invalid, a_invalid, a_raise, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    /* 0x8X */
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    /* 0x9X */
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    /* 0xAX */
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    /* 0xBX */
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    /* 0xCX */
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    /* 0xDX */
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    /* 0xEX */
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    /* 0xFX */
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid,
    a_invalid, a_invalid, a_invalid, a_invalid
};

int rankCardset(const Cardset cards) {
  int postponed, r;
  Cardset sets;

  postponed = oneSuitVal[cards.bySuit[0]];
  if (oneSuitVal[cards.bySuit[1]] > postponed) {
    postponed = oneSuitVal[cards.bySuit[1]];
  }
  if (oneSuitVal[cards.bySuit[2]] > postponed) {
    postponed = oneSuitVal[cards.bySuit[2]];
  }
  if (oneSuitVal[cards.bySuit[3]] > postponed) {
    postponed = oneSuitVal[cards.bySuit[3]];
  }
  if (postponed >= HANDCLASS_STRAIGHT_FLUSH) {
    /* straight flush */

    return postponed;
  }

  sets.bySuit[0] = cards.bySuit[0] | cards.bySuit[1];
  sets.bySuit[1] = cards.bySuit[0] & cards.bySuit[1];
  sets.bySuit[2] = sets.bySuit[1] & cards.bySuit[2];
  sets.bySuit[1] |= sets.bySuit[0] & cards.bySuit[2];
  sets.bySuit[0] |= cards.bySuit[2];
  sets.bySuit[3] = sets.bySuit[2] & cards.bySuit[3];
  sets.bySuit[2] |= sets.bySuit[1] & cards.bySuit[3];
  sets.bySuit[1] |= sets.bySuit[0] & cards.bySuit[3];
  sets.bySuit[0] |= cards.bySuit[3];

  if (sets.bySuit[3]) {
    /* quads */

    r = topBit[sets.bySuit[3]];
    return quadsVal[r] + topBit[sets.bySuit[0] ^ (1 << r)];
  }

  if (sets.bySuit[2]) {
    /* trips or full house */

    r = topBit[sets.bySuit[2]];
    sets.bySuit[1] ^= (1 << r);
    if (sets.bySuit[1]) {
      /* full house */

      return tripsVal[r] + fullHouseOtherVal
          + topBit[sets.bySuit[1]];
    }

    if (postponed) {
      /* flush */

      return postponed;
    }

    postponed = anySuitVal[sets.bySuit[0]];
    if (postponed >= HANDCLASS_STRAIGHT) {
      /* straight */

      return postponed;
    }

    /* trips */
    sets.bySuit[0] ^= (1 << r);
    return tripsVal[r] + tripsOtherVal[sets.bySuit[0]];
  } else {

    if (postponed) {
      /* flush */

      return postponed;
    }

    postponed = anySuitVal[sets.bySuit[0]];
    if (postponed >= HANDCLASS_STRAIGHT) {
      /* straight */

      return postponed;
    }
  }

  if (sets.bySuit[1]) {
    /* pair or two pair */

    r = topBit[sets.bySuit[1]];
    sets.bySuit[0] ^= (1 << r);
    sets.bySuit[1] ^= (1 << r);
    if (sets.bySuit[1]) {
      /* two pair */

      sets.bySuit[0] ^= (1 << topBit[sets.bySuit[1]]);
      return pairsVal[r]
          + twoPairOtherVal[topBit[sets.bySuit[1]]]
          + topBit[sets.bySuit[0]];
    }

    return pairsVal[r] + pairOtherVal[sets.bySuit[0]];
  }

  return postponed;
}

Cardset emptyCardset() {
  Cardset c;

  c.cards = 0;

  return c;
}

void addCardToCardset(Cardset *c, int suit, int rank) {
  c->cards |= (uint64_t) 1 << ((suit << 4) + rank);
}

static int consumeSpaces(const char *string, int consumeEqual) {
  int i;

  for (i = 0; string[i] != 0
      && (isspace(string[i])
          || (consumeEqual && string[i] == '='));
       ++i) {
  }

  return i;
}

/* reads up to numItems with scanf format itemFormat from string,
   returning item i in *items[ i ]
   ignore the '=' character if consumeEqual is non-zero
   returns the number of characters consumed doing this in charsConsumed
   returns the number of items read */
static int readItems(const char *itemFormat, const int numItems,
                     const char *string, const int consumeEqual,
                     uint8_t *items, const size_t itemSize,
                     int *charsConsumed) {
  int i, c, r;
  char *fmt;

  i = strlen(itemFormat);
  fmt = (char *) malloc(i + 3);
  assert(fmt != 0);
  strcpy(fmt, itemFormat);
  fmt[i] = '%';
  fmt[i + 1] = 'n';
  fmt[i + 2] = 0;

  c = 0;
  for (i = 0; i < numItems; ++i) {

    c += consumeSpaces(&string[c], consumeEqual);
    if (sscanf(&string[c], fmt, &items[i], &r) < 1) {
      break;
    }
    c += r;
  }

  free(fmt);

  *charsConsumed = c;
  return i;
}

static int readItems32(const char *itemFormat, const int numItems,
                       const char *string, const int consumeEqual,
                       int32_t *items, const size_t itemSize,
                       int *charsConsumed) {
  int i, c, r;
  char *fmt;

  i = strlen(itemFormat);
  fmt = (char *) malloc(i + 3);
  assert(fmt != 0);
  strcpy(fmt, itemFormat);
  fmt[i] = '%';
  fmt[i + 1] = 'n';
  fmt[i + 2] = 0;

  c = 0;
  for (i = 0; i < numItems; ++i) {

    c += consumeSpaces(&string[c], consumeEqual);
    if (sscanf(&string[c], fmt, &items[i], &r) < 1) {
      break;
    }
    c += r;
  }

  free(fmt);

  *charsConsumed = c;
  return i;
}

Game *readGame(FILE *file) {
  int stackRead, blindRead, raiseSizeRead, boardCardsRead, c, t;
  char line[MAX_LINE_LEN];
  Game *game;

  game = (Game *) malloc(sizeof(*game));
  assert(game != 0);
  stackRead = 4;
  for (c = 0; c < MAX_ROUNDS; ++c) {
    game->stack[c] = INT32_MAX;
  }
  blindRead = 0;
  raiseSizeRead = 0;
  game->bettingType = limitBetting;
  game->numPlayers = 0;
  game->numRounds = 0;
  for (c = 0; c < MAX_ROUNDS; ++c) {
    game->firstPlayer[c] = 1;
  }
  for (c = 0; c < MAX_ROUNDS; ++c) {
    game->maxRaises[c] = UINT8_MAX;
  }
  game->numSuits = 0;
  game->numRanks = 0;
  game->numHoleCards = 0;
  boardCardsRead = 0;

  game->use_state_stack = 0;

  while (fgets(line, MAX_LINE_LEN, file)) {

    if (line[0] == '#' || line[0] == '\n') {
      continue;
    }

    if (!strncasecmp(line, "end gamedef", 11)) {

      break;
    } else if (!strncasecmp(line, "gamedef", 7)) {

      continue;
    } else if (!strncasecmp(line, "stack", 5)) {

      stackRead = readItems32("%"SCNd32, MAX_PLAYERS, &line[5],
                              1, game->stack, 4, &c);
    } else if (!strncasecmp(line, "blind", 5)) {

      blindRead = readItems32("%"SCNd32, MAX_PLAYERS, &line[5],
                              1, game->blind, 4, &c);
    } else if (!strncasecmp(line, "raisesize", 9)) {

      raiseSizeRead = readItems32("%"SCNd32, MAX_PLAYERS, &line[9],
                                  1, game->raiseSize, 4, &c);
    } else if (!strncasecmp(line, "limit", 5)) {

      game->bettingType = limitBetting;
    } else if (!strncasecmp(line, "nolimit", 7)) {

      game->bettingType = noLimitBetting;
    } else if (!strncasecmp(line, "numplayers", 10)) {

      readItems("%"SCNu8, 1, &line[10], 1, &game->numPlayers, 1, &c);
    } else if (!strncasecmp(line, "numrounds", 9)) {

      readItems("%"SCNu8, 1, &line[9], 1, &game->numRounds, 1, &c);
    } else if (!strncasecmp(line, "firstplayer", 11)) {

      readItems("%"SCNu8, MAX_ROUNDS, &line[11],
                1, game->firstPlayer, 1, &c);
    } else if (!strncasecmp(line, "maxraises", 9)) {

      readItems("%"SCNu8, MAX_ROUNDS, &line[9],
                1, game->maxRaises, 1, &c);
    } else if (!strncasecmp(line, "numsuits", 8)) {

      readItems("%"SCNu8, 1, &line[8], 1, &game->numSuits, 1, &c);
    } else if (!strncasecmp(line, "numranks", 8)) {

      readItems("%"SCNu8, 1, &line[8], 1, &game->numRanks, 1, &c);
    } else if (!strncasecmp(line, "numholecards", 12)) {

      readItems("%"SCNu8, 1, &line[12], 1, &game->numHoleCards, 1, &c);
    } else if (!strncasecmp(line, "numboardcards", 13)) {

      boardCardsRead = readItems("%"SCNu8, MAX_ROUNDS, &line[13],
                                 1, game->numBoardCards, 1, &c);
    }
  }

  /* do sanity checks */
  if (game->numRounds == 0 || game->numRounds > MAX_ROUNDS) {

    fprintf(stderr, "invalid number of rounds: %"PRIu8"\n", game->numRounds);
    free(game);
    return NULL;
  }

  if (game->numPlayers < 2 || game->numPlayers > MAX_PLAYERS) {

    fprintf(stderr, "invalid number of players: %"PRIu8"\n",
            game->numPlayers);
    free(game);
    return NULL;
  }

  if (stackRead < game->numPlayers) {

    fprintf(stderr, "only read %d stack sizes, need %"PRIu8"\n",
            stackRead, game->numPlayers);
    free(game);
    return NULL;
  }

  if (blindRead < game->numPlayers) {

    fprintf(stderr, "only read %d blinds, need %"PRIu8"\n",
            blindRead, game->numPlayers);
    free(game);
    return NULL;
  }
  for (c = 0; c < game->numPlayers; ++c) {

    if (game->blind[c] > game->stack[c]) {
      fprintf(stderr, "blind for player %d is greater than stack size\n",
              c + 1);
      free(game);
      return NULL;
    }
  }

  if (game->bettingType == limitBetting
      && raiseSizeRead < game->numRounds) {

    fprintf(stderr, "only read %d raise sizes, need %"PRIu8"\n",
            raiseSizeRead, game->numRounds);
    free(game);
    return NULL;
  }

  for (c = 0; c < game->numRounds; ++c) {

    if (game->firstPlayer[c] == 0
        || game->firstPlayer[c] > game->numPlayers) {

      fprintf(stderr, "invalid first player %"PRIu8" on round %d\n",
              game->firstPlayer[c], c + 1);
      free(game);
      return NULL;
    }

    --game->firstPlayer[c];
  }

  if (game->numSuits == 0 || game->numSuits > MAX_SUITS) {

    fprintf(stderr, "invalid number of suits: %"PRIu8"\n", game->numSuits);
    free(game);
    return NULL;
  }

  if (game->numRanks == 0 || game->numRanks > MAX_RANKS) {

    fprintf(stderr, "invalid number of ranks: %"PRIu8"\n", game->numRanks);
    free(game);
    return NULL;
  }

  if (game->numHoleCards == 0 || game->numHoleCards > MAX_HOLE_CARDS) {

    fprintf(stderr, "invalid number of hole cards: %"PRIu8"\n",
            game->numHoleCards);
    free(game);
    return NULL;
  }

  if (boardCardsRead < game->numRounds) {

    fprintf(stderr, "only read %d board card numbers, need %"PRIu8"\n",
            boardCardsRead, game->numRounds);
    free(game);
    return NULL;
  }

  t = game->numHoleCards * game->numPlayers;
  for (c = 0; c < game->numRounds; ++c) {
    t += game->numBoardCards[c];
  }
  if (t > game->numSuits * game->numRanks) {

    fprintf(stderr, "too many hole and board cards for specified deck\n");
    free(game);
    return NULL;
  }
  return game;
}

void printGame(FILE *file, const Game *game) {
  int i;

  fprintf(file, "GAMEDEF\n");

  if (game->bettingType == noLimitBetting) {
    fprintf(file, "nolimit\n");
  } else {
    fprintf(file, "limit\n");
  }

  fprintf(file, "numPlayers = %"PRIu8"\n", game->numPlayers);

  fprintf(file, "numRounds = %"PRIu8"\n", game->numRounds);

  for (i = 0; i < game->numPlayers; ++i) {
    if (game->stack[i] < INT32_MAX) {

      fprintf(file, "stack =");
      for (i = 0; i < game->numPlayers; ++i) {
        fprintf(file, " %"PRId32, game->stack[i]);
      }
      fprintf(file, "\n");

      break;
    }
  }

  fprintf(file, "blind =");
  for (i = 0; i < game->numPlayers; ++i) {
    fprintf(file, " %"PRId32, game->blind[i]);
  }
  fprintf(file, "\n");

  if (game->bettingType == limitBetting) {

    fprintf(file, "raiseSize =");
    for (i = 0; i < game->numRounds; ++i) {
      fprintf(file, " %"PRId32, game->raiseSize[i]);
    }
    fprintf(file, "\n");
  }

  for (i = 0; i < game->numRounds; ++i) {
    if (game->firstPlayer[i] != 0) {

      fprintf(file, "firstPlayer =");
      for (i = 0; i < game->numRounds; ++i) {
        fprintf(file, " %d", game->firstPlayer[i] + 1);
      }
      fprintf(file, "\n");

      break;
    }
  }

  for (i = 0; i < game->numRounds; ++i) {
    if (game->maxRaises[i] != UINT8_MAX) {

      fprintf(file, "maxRaises =");
      for (i = 0; i < game->numRounds; ++i) {
        fprintf(file, " %"PRIu8, game->maxRaises[i]);
      }
      fprintf(file, "\n");

      break;
    }
  }

  fprintf(file, "numSuits = %"PRIu8"\n", game->numSuits);

  fprintf(file, "numRanks = %"PRIu8"\n", game->numRanks);

  fprintf(file, "numHoleCards = %"PRIu8"\n", game->numHoleCards);

  fprintf(file, "numBoardCards =");
  for (i = 0; i < game->numRounds; ++i) {
    fprintf(file, " %"PRIu8, game->numBoardCards[i]);
  }
  fprintf(file, "\n");

  fprintf(file, "END GAMEDEF\n");
}

uint8_t bcStart(const Game *game, const uint8_t round) {
  int r;
  uint8_t start;

  start = 0;
  for (r = 0; r < round; ++r) {

    start += game->numBoardCards[r];
  }

  return start;
}

uint8_t sumBoardCards(const Game *game, const uint8_t round) {
  int r;
  uint8_t total;

  total = 0;
  for (r = 0; r <= round; ++r) {
    total += game->numBoardCards[r];
  }

  return total;
}

static uint8_t nextPlayer(const Game *game, const State *state,
                          const uint8_t curPlayer) {
  uint8_t n;

  n = curPlayer;
  do {
    n = (n + 1) % game->numPlayers;
  } while (state->playerFolded[n]
      || state->spent[n] >= state->stackPlayer[n]);

  return n;
}

/*
 * warning, this method will run indefinitely on terminal node
 */
uint8_t currentPlayer(const Game *game, const State *state) {
  if (state->finished) {
    printf("warning. this node is terminal. can not evaluate the player. the program will halts if not exited");
    exit(EXIT_FAILURE);
  }
  /* if action has already been made, compute next player from last player */
  if (state->numActions[state->round]) {
    return nextPlayer(game, state, state->actingPlayer[state->round][state->numActions[state->round] - 1]);
  }

  /* first player in a round is determined by the game and round
     use nextPlayer() because firstPlayer[round] might be unable to act */
  return nextPlayer(game, state, game->firstPlayer[state->round]
      + game->numPlayers - 1);
}

uint8_t numRaises(const State *state) {
  int i;
  uint8_t ret;

  ret = 0;
  for (i = 0; i < state->numActions[state->round]; ++i) {
    if (state->action[state->round][i].type == a_raise) {
      ++ret;
    }
  }

  return ret;
}

uint8_t numFolded(const Game *game, const State *state) {
  int p;
  uint8_t ret;

  ret = 0;
  for (p = 0; p < game->numPlayers; ++p) {
    if (state->playerFolded[p]) {
      ++ret;
    }
  }

  return ret;
}

uint8_t numCalled(const Game *game, const State *state) {
  int i;
  uint8_t ret, p;

  ret = 0;
  for (i = state->numActions[state->round]; i > 0; --i) {

    p = state->actingPlayer[state->round][i - 1];

    if (state->action[state->round][i - 1].type == a_raise) {
      /* player initiated the bet, so they've called it */

      if (state->spent[p] < state->stackPlayer[p]) {
        /* player is not all-in, so they're still acting */

        ++ret;
      }

      /* this is the start of the current bet, so we're finished */
      return ret;
    } else if (state->action[state->round][i - 1].type == a_call) {

      if (state->spent[p] < state->stackPlayer[p]) {
        /* player is not all-in, so they're still acting */

        ++ret;
      }
    }
  }

  return ret;
}

uint8_t numAllIn(const Game *game, const State *state) {
  int p;
  uint8_t ret;

  ret = 0;
  for (p = 0; p < game->numPlayers; ++p) {
    if (state->spent[p] >= state->stackPlayer[p]) {
      ++ret;
    }
  }

  return ret;
}

uint8_t numActingPlayers(const Game *game, const State *state) {
  int p;
  uint8_t ret;

  ret = 0;
  for (p = 0; p < game->numPlayers; ++p) {
    if (state->playerFolded[p] == 0
        && state->spent[p] < state->stackPlayer[p]) {
      ++ret;
    }
  }

  return ret;
}

/*
 * InitState by default is not stack aware, simply uses stack defined in game config
 */
void initState(const Game *game, const uint32_t handId, State *state) {
  int p, r;

  state->handId = handId;

  state->round = 0;

  for (r = 0; r < game->numRounds; r++) {
    for (p = 0; p < game->numPlayers; p++) {
      state->sum_round_spent[r][p] = 0;
    }
  }

  state->maxSpent = 0;
  for (p = 0; p < game->numPlayers; ++p) {
    state->spent[p] = game->blind[p];
    state->sum_round_spent[state->round][p] = game->blind[p];
    // if not using the one of state, use the default one
    if (game->use_state_stack == 0)
      state->stackPlayer[p] = game->stack[p];

    if (game->blind[p] > state->maxSpent)
      state->maxSpent = game->blind[p];
    state->playerFolded[p] = 0;

    for (int c = 0; c < game->numHoleCards; c++)
      state->holeCards[p][c] = UNDEFINED_CARD;
  }

  if (game->bettingType == noLimitBetting) {
    /* no-limit games need to keep track of the minimum bet */

    if (state->maxSpent) {
      /* we'll have to call the big blind and then raise by that
	 amount, so the minimum raise-to is 2*maximum blinds */

      state->minNoLimitRaiseTo = state->maxSpent * 2;
    } else {
      /* need to bet at least one chip, and there are no blinds/ante */

      state->minNoLimitRaiseTo = 1;
    }
  } else {
    /* no need to worry about minimum raises outside of no-limit games */

    state->minNoLimitRaiseTo = 0;
  }

  for (r = 0; r < game->numRounds; ++r) {
    state->numActions[r] = 0;
  }

  for (int b = 0; b < MAX_BOARD_CARDS; b++) {
    state->boardCards[b] = UNDEFINED_CARD;
  }

  state->finished = 0;
}

static uint8_t dealCard(rng_state_t *rng, uint8_t *deck, const int numCards) {
  int i;
  uint8_t ret;

  i = genrand_int32(rng) % numCards;
  ret = deck[i];
  deck[i] = deck[numCards - 1];

  return ret;
}

void dealCards(const Game *game, rng_state_t *rng, State *state) {
  int r, s, numCards, i, p;
  uint8_t deck[MAX_RANKS * MAX_SUITS];

  numCards = 0;
  for (s = MAX_SUITS - game->numSuits; s < MAX_SUITS; ++s) {

    for (r = MAX_RANKS - game->numRanks; r < MAX_RANKS; ++r) {

      deck[numCards] = makeCard(r, s, game->numSuits);
      ++numCards;
    }
  }

  for (p = 0; p < game->numPlayers; ++p) {

    for (i = 0; i < game->numHoleCards; ++i) {

      state->holeCards[p][i] = dealCard(rng, deck, numCards);
      --numCards;
    }
  }

  s = 0;
  for (r = 0; r < game->numRounds; ++r) {

    for (i = 0; i < game->numBoardCards[r]; ++i) {

      state->boardCards[s] = dealCard(rng, deck, numCards);
      --numCards;
      ++s;
    }
  }
}

int checkUsed(const uint8_t *usedCards, int numUsed, int r, int s, int numSuits) {
  for (int j = 0; j < numUsed; j++) {
    if (usedCards[j] == makeCard(r, s, numSuits)) {
      return 1;
    }
  }
  return 0;
}

void dealRemainingCards(const Game *game, rng_state_t *rng, State *state) {
  int r, s, numCards, i, p;
  uint8_t deck[MAX_RANKS * MAX_SUITS];
  uint8_t usedCards[MAX_RANKS * MAX_SUITS];
  int numUsed = 0;
  for (p = 0; p < game->numPlayers; ++p) {
    for (i = 0; i < game->numHoleCards; ++i) {
      usedCards[numUsed] = state->holeCards[p][i];
      ++numUsed;
    }
  }
  s = 0;
  for (r = 0; r <= state->round; ++r) {
    for (i = 0; i < game->numBoardCards[r]; ++i) {
      usedCards[numUsed] = state->boardCards[s];
      ++numUsed;
      ++s;
    }
  }

  numCards = 0;
  for (s = MAX_SUITS - game->numSuits; s < MAX_SUITS; ++s) {
    for (r = MAX_RANKS - game->numRanks; r < MAX_RANKS; ++r) {
      if (checkUsed(usedCards, numUsed, r, s, game->numSuits) == 0) {
        deck[numCards] = makeCard(r, s, game->numSuits);
        ++numCards;
      }
    }
  }

  s = 0;
  for (r = 0; r < game->numRounds; ++r) {
    for (i = 0; i < game->numBoardCards[r]; ++i) {
      if (r > state->round) {
        state->boardCards[s] = dealCard(rng, deck, numCards);
        --numCards;
      }
      ++s;
    }
  }
}

/* check whether some portions of a state are equal,
   common to both statesEqual and matchStatesEqual */
static int statesEqualCommon(const Game *game, const State *a,
                             const State *b) {
  int r, i, t;

  /* is it the same hand? */
  if (a->handId != b->handId) {
    return 0;
  }

  /* make sure the betting is the same */
  if (a->round != b->round) {
    return 0;
  }

  for (r = 0; r <= a->round; ++r) {

    if (a->numActions[r] != b->numActions[r]) {
      return 0;
    }

    for (i = 0; i < a->numActions[r]; ++i) {

      if (a->action[r][i].type != b->action[r][i].type) {
        return 0;
      }
      if (a->action[r][i].size != b->action[r][i].size) {
        return 0;
      }
    }
  }

  /* spent, maxSpent, actingPlayer, finished, and playerFolded are
     all determined by the betting taken, so if it's equal, so are
     they (at least for valid states) */

  /* are the board cards the same? */
  t = sumBoardCards(game, a->round);
  for (i = 0; i < t; ++i) {

    if (a->boardCards[i] != b->boardCards[i]) {
      return 0;
    }
  }

  /* all tests say states are equal */
  return 1;
}

int StateEqual(const Game *game, const State *a, const State *b) {
  int p, i;

  if (!statesEqualCommon(game, a, b)) {
    return 0;
  }

  /* are all the hole cards the same? */
  for (p = 0; p < game->numPlayers; ++p) {

    for (i = 0; i < game->numHoleCards; ++i) {
      if (a->holeCards[p][i] != b->holeCards[p][i]) {
        return 0;
      }
    }
  }

  return 1;
}

//copy reference state to target state
//(handID, hold cards and board cards, stack)
void StateTableInfoCopy(const Game *game, State *ref_state, State *target_state) {
  for (int p = 0; p < game->numPlayers; ++p) {
    //copy the stack
    target_state->stackPlayer[p] = ref_state->stackPlayer[p];
    for (int i = 0; i < game->numHoleCards; ++i) {
      //copy hole cards
      target_state->holeCards[p][i] = ref_state->holeCards[p][i];
    }
  }

  target_state->handId = ref_state->handId;

  /* spent, maxSpent, actingPlayer, finished, and playerFolded are
     all determined by the betting taken, so if it's equal, so are
     they (at least for valid states) */

  /* are the board cards the same? */
  int t = sumBoardCards(game, ref_state->round);
  for (int i = 0; i < t; ++i) {
    target_state->boardCards[i] = ref_state->boardCards[i];
  }

}

int matchStatesEqual(const Game *game, const MatchState *a,
                     const MatchState *b) {
  int p, i;

  if (a->viewingPlayer != b->viewingPlayer) {
    return 0;
  }

  if (!statesEqualCommon(game, &a->state, &b->state)) {
    return 0;
  }

  /* are the viewing player's hole cards the same? */
  p = a->viewingPlayer;
  for (i = 0; i < game->numHoleCards; ++i) {
    if (a->state.holeCards[p][i] != b->state.holeCards[p][i]) {
      return 0;
    }
  }

  return 1;
}

int raiseIsValid(const Game *game, const State *curState,
                 int32_t *minSize, int32_t *maxSize) {
  int p;

  if (numRaises(curState) >= game->maxRaises[curState->round]) {
    /* already made maximum number of raises */

    return 0;
  }

  if (curState->numActions[curState->round] + game->numPlayers
      > MAX_NUM_ACTIONS) {
    /* 1 raise + NUM PLAYERS-1 calls is too many actions */

    fprintf(stderr, "WARNING: #actions in round is too close to MAX_NUM_ACTIONS, forcing call/fold\n");
    return 0;
  }

  if (numActingPlayers(game, curState) <= 1) {
    /* last remaining player can't bet if there's no one left to call
       (this check is needed if the 2nd last player goes all in, and
       the last player has enough stack left to bet) */

    return 0;
  }

  if (game->bettingType != noLimitBetting) {
    /* if it's not no-limit betting, don't worry about sizes */

    *minSize = 0;
    *maxSize = 0;
    return 1;
  }

  p = currentPlayer(game, curState);
  *minSize = curState->minNoLimitRaiseTo;
  *maxSize = curState->stackPlayer[p];

  /* handle case where remaining player stack is too small */
  if (*minSize > curState->stackPlayer[p]) {
    /* can't handle the minimum bet size - can we bet at all? */

    if (curState->maxSpent >= curState->stackPlayer[p]) {
      /* not enough money to increase current bet */

      return 0;
    } else {
      /* can raise by going all-in */

      *minSize = *maxSize;
      return 1;
    }
  }

  return 1;
}

/**
 * todo: assert again if the no limit holdem rules apply well, in the reraise case
 *
 * after make it stack aware, non dolce game
 *
 * e.g.
 * For example, let's say you are playing NL holden with blinds at 100-200. You are first to act after the flop and you bet 500.
 * A shortstacked player then shoves all-in for 700. Two other players call behind him. Now it's back up to you and you want to reraise
 * . This scenario leads to arguments all of the time and I've seen lots of floor staff get it wrong.
 * Rules are going to be different from place to place, but under the TDA rules,
 * it does not open up the action again to you unless the shortstacked player's raise is at least a full raise.
 * So in the example I gave, the shortstacked player would have to have made it 1000 for you to be able to reraise when it got back to you.
 *
 * tryfixing = try rounding to min or max
 */
int isValidAction(const Game *game, const State *curState,
                  const int tryFixing, Action *action) {
  int min, max, p;

  if (stateFinished(curState) || action->type == a_invalid) {
    return 0;
  }

  p = currentPlayer(game, curState);

  if (action->type == a_raise) {

    if (!raiseIsValid(game, curState, &min, &max)) {
      /* there are no valid raise sizes */

      return 0;
    }

    if (game->bettingType == noLimitBetting) {
      /* no limit games have a size */

      if (action->size < min) {
        /* bet size is too small */

        if (!tryFixing) {

          return 0;
        }
        fprintf(stderr, "WARNING: raise of %d was fixed to %d\n",
                action->size, min);
        action->size = min;
      } else if (action->size > max) {
        /* bet size is too big */

        if (!tryFixing) {

          return 0;
        }
        fprintf(stderr, "WARNING: raise of %d was fixed to %d\n",
                action->size, max);
        action->size = max;
      }
    }
  } else if (action->type == a_fold) {

    if (curState->spent[p] == curState->maxSpent
        || curState->spent[p] == curState->stackPlayer[p]) {
      /* player has already called all bets, or is all-in  but still decides to fold*/
      return PREMATURE_FOLD;
    }

    if (action->size != 0) {

      fprintf(stderr, "WARNING: size given for fold\n");
      action->size = 0;
    }
  } else {
    /* everything else */
    //in case of call

    if (action->size != 0) {

      fprintf(stderr, "WARNING: size given for something other than a no-limit raise\n");
      action->size = 0;
    }
  }

  return 1;
}

/*
 * this method is not save, if you have none actions
 */
Action GetLastActionFromState(State *reference_state) {
  int r = reference_state->round;
  Action last_action;
  //find the final round with actions.
  while (reference_state->numActions[r] == 0) {
    r--;
    if (r == -1) {
      printf("no actions at allllll");
      exit(-1);
    }
  }
  return reference_state->action[r][reference_state->numActions[r] - 1];
}

int TotalAction(State *state) {
  int total_num_actions = 0;
  for (int r = 0; r <= state->round; r++)
    total_num_actions += state->numActions[r];
  return total_num_actions;
}

//do all action again to the empty target_state
int StepBackAction(const Game *game, State *reference_state, State *target_state, int back_steps) {
  initState(game, 0, target_state);
  int total_num_actions = TotalAction(reference_state);
  int to_do_actions = total_num_actions - back_steps;
  if (to_do_actions == 0)
    return 0;
  //copy reference to target
  StateTableInfoCopy(game, reference_state, target_state);
  if (to_do_actions < 0) {
    printf("to_do_action < 0; taking too many steps back (%d)\n", back_steps);
    return -1;
  }

  for (int r = 0; r <= reference_state->round; r++) {
    for (int a = 0; a < reference_state->numActions[r]; a++) {
      doAction(game, &reference_state->action[r][a], target_state);
      to_do_actions--;
      if (to_do_actions == 0)
        return 0;
    }
  }
  return -1;
}

void doAction(const Game *game, const Action *action, State *state) {
  int p = currentPlayer(game, state);

  assert(state->numActions[state->round] < MAX_NUM_ACTIONS);

  state->action[state->round][state->numActions[state->round]] = *action;
  state->actingPlayer[state->round][state->numActions[state->round]] = p;
  ++state->numActions[state->round];

  switch (action->type) {
    case a_fold:

      state->playerFolded[p] = 1;
      break;

    case a_call:

      if (state->maxSpent > state->stackPlayer[p]) {
        /* calling puts player all-in */

        state->spent[p] = state->stackPlayer[p];
        state->sum_round_spent[state->round][p] = state->stackPlayer[p];
      } else {
        /* player matches the bet by spending same amount of money */

        state->spent[p] = state->maxSpent;
        state->sum_round_spent[state->round][p] = state->maxSpent;
      }
      break;

    case a_raise:

      if (game->bettingType == noLimitBetting) {
        /* no-limit betting uses size in action */

        assert(action->size > state->maxSpent);
        assert(action->size <= state->stackPlayer[p]);

        /* next raise must call this bet, and raise by at least this much */
        //it is correct. y - x >= x - maxspent. ->>> y >= 2x - maxspent
        if (action->size + action->size - state->maxSpent
            > state->minNoLimitRaiseTo) {

          state->minNoLimitRaiseTo
              = action->size + action->size - state->maxSpent;
        }
        state->maxSpent = action->size;
      } else {
        /* limit betting uses a fixed amount on top of current bet size */

        if (state->maxSpent + game->raiseSize[state->round]
            > state->stackPlayer[p]) {
          /* raise puts player all-in */

          state->maxSpent = state->stackPlayer[p];
        } else {
          /* player raises by the normal limit size */

          state->maxSpent += game->raiseSize[state->round];
        }
      }

      state->spent[p] = state->maxSpent;
      state->sum_round_spent[state->round][p] = state->maxSpent;
      break;

    default:fprintf(stderr, "ERROR: trying to do invalid action %d", action->type);
      assert(0);
  }

  /* see if the round or game has ended */
  if (numFolded(game, state) + 1 >= game->numPlayers) {
    /* only one player left - game is immediately over, no showdown */

    state->finished = 1;
  } else if (numCalled(game, state) >= numActingPlayers(game, state)) {
    /* >= 2 non-folded players, all acting players have called */

    if (numActingPlayers(game, state) > 1) {
      /* there are at least 2 acting players */

      if (state->round + 1 < game->numRounds) {
        /* active players move onto next round */

        ++state->round;

        /* minimum raise-by is reset to minimum of big blind or 1 chip */
        state->minNoLimitRaiseTo = 1;
        for (p = 0; p < game->numPlayers; ++p) {

          if (game->blind[p] > state->minNoLimitRaiseTo) {

            state->minNoLimitRaiseTo = game->blind[p];
          }
        }

        /* we finished at least one round, so raise-to = raise-by + maxSpent */
        state->minNoLimitRaiseTo += state->maxSpent;
      } else {
        /* no more betting rounds, so we're totally finished */

        state->finished = 1;
      }
    } else {
      /* not enough players for more betting, but still need a showdown */

      state->finished = 1;
      state->round = game->numRounds - 1;
    }
  }
}

int rankHand(const Game *game, const State *state,
             const uint8_t player) {
  int i;
  Cardset c = emptyCardset();

  for (i = 0; i < game->numHoleCards; ++i) {

    addCardToCardset(&c, suitOfCard(state->holeCards[player][i],
                                    game->numSuits),
                     rankOfCard(state->holeCards[player][i],
                                game->numSuits));
  }

  for (i = 0; i < sumBoardCards(game, state->round); ++i) {

    addCardToCardset(&c, suitOfCard(state->boardCards[i], game->numSuits),
                     rankOfCard(state->boardCards[i], game->numSuits));
  }

  return rankCardset(c);
}

double valueOfState(const Game *game, const State *state,
                    const uint8_t player) {
  double value;
  int p, numPlayers, playerIdx, numWinners, newNumPlayers;
  int32_t size, spent[MAX_PLAYERS];
  int rank[MAX_PLAYERS], winRank;

  if (state->playerFolded[player]) {
    /* folding player loses all spent money */

    return (double) -state->spent[player];
  }

  if (numFolded(game, state) + 1 == game->numPlayers) {
    /* everyone else folded, so player takes the pot */

    value = 0.0;
    for (p = 0; p < game->numPlayers; ++p) {
      if (p == player) { continue; }

      value += (double) state->spent[p];
    }

    return value;
  }

  /* there's a showdown, and player is particpating.  Exciting! */

  /* make up a list of players */
  numPlayers = 0;
  playerIdx = -1; /* useless, but gets rid of a warning */
  for (p = 0; p < game->numPlayers; ++p) {

    if (state->spent[p] == 0) {
      continue;
    }

    if (state->playerFolded[p]) {
      /* folding players have a negative rank so they lose to a real hand
         we have also tested for fold, so p can't be the player of interest */

      rank[numPlayers] = -1;
    } else {
      /* p is participating in a showdown */

      if (p == player) {
        playerIdx = numPlayers;
      }

      if (state->holeCards[p][game->numHoleCards - 1] == UNDEFINED_CARD) {
        //player mucked
        rank[numPlayers] = -1;
      } else {
        rank[numPlayers] = rankHand(game, state, p);
      }
    }

    spent[numPlayers] = state->spent[p];
    ++numPlayers;
  }
  assert(numPlayers > 1);

  /* go through all the sidepots player is participating in */
  value = 0.0;
  while (1) {

    /* find the smallest remaining sidepot, largest rank,
        and number of winners with largest rank */
    size = INT32_MAX;
    winRank = 0;
    numWinners = 0;
    for (p = 0; p < numPlayers; ++p) {
      assert(spent[p] > 0);

      if (spent[p] < size) {
        size = spent[p];
      }

      if (rank[p] > winRank) {
        /* new largest rank - only one player with this rank so far */

        winRank = rank[p];
        numWinners = 1;
      } else if (rank[p] == winRank) {
        /* another player with highest rank */

        ++numWinners;
      }
    }

    if (rank[playerIdx] == winRank) {
      /* player has spent size, and splits pot with other winners */

      value += (double) (size * (numPlayers - numWinners))
          / (double) numWinners;
    } else {
      /* player loses this pot */

      value -= (double) size;
    }

    /* update list of players for next pot */
    newNumPlayers = 0;
    for (p = 0; p < numPlayers; ++p) {

      spent[p] -= size;
      if (spent[p] == 0) {
        /* player p is not participating in next side pot */

        if (p == playerIdx) {
          /* p is the player of interest, so we're done */

          return value;
        }

        continue;
      }

      if (p == playerIdx) {
        /* p is the player of interest, so note the new index */

        playerIdx = newNumPlayers;
      }

      if (p != newNumPlayers) {
        /* put entry p into new position */

        spent[newNumPlayers] = spent[p];
        rank[newNumPlayers] = rank[p];
      }
      ++newNumPlayers;
    }
    numPlayers = newNumPlayers;
  }
}

/* read actions from a string, updating state with the actions
   reading is terminated by '\0' and ':'
   returns number of characters consumed, or -1 on failure
   state will be modified, even on failure */
static int readBetting(const char *string, const Game *game, State *state) {
  int c, r;
  Action action;

  c = 0;
  while (1) {

    if (string[c] == 0) {
      break;
    }

    if (string[c] == ':') {
      ++c;
      break;
    }

    /* ignore / character */
    if (string[c] == '/') {
      ++c;
      continue;
    }

    r = readAction(&string[c], game, &action);
    if (r < 0) {
      printf("read state wrong: read betting actions failed at position %d.\n", c);
      printf("string = %s", string);
      return -1;
    }

    if (!isValidAction(game, state, 0, &action)) {
      return -1;
    }

    doAction(game, &action, state);
    c += r;
  }

  return c;
}

int readStack(const char *string, const Game *game, State *state) {
  int c, ret;
  char *ptr, *ptr2;
  c = 0;

  //for our stack-aware acpc protocol. if no stack were specified it will be:
  if (string[0] == ':') {
    //no stack to read
    ++c;
    return c;
  }

  //in acpc original one, it will be directly the card
  uint8_t card;
  if (readCard(game, &string[0], &card) == 2) {
    //it means it is a card, return 0
    return 0;
  } else if (readCard(game, &string[1], &card) == 2) {
    //when i am siting on the second seat of the table. todo: it is a hack for 2p game
    return 0;
  }

  ret = strtol(string, &ptr, 10);
  state->stackPlayer[0] = ret;
  c += (ptr - string);

  for (int i = 1; i < game->numPlayers; i++) {
    ret = strtol(ptr + 1, &ptr2, 10);
    c += (ptr2 - ptr);
    ptr = ptr2;

    state->stackPlayer[i] = ret;
  }

  if (string[c] == ':')
    ++c;

  return c;
}

int printBetting(const Game *game, const State *state,
                 const int maxLen, char *string) {
  int i, a, c, r;

  c = 0;
  for (i = 0; i <= state->round; ++i) {

    /* print state separator */
    if (i != 0) {

      if (c >= maxLen) {
        return -1;
      }
      string[c] = '/';
      ++c;
    }

    /* print betting for round */
    for (a = 0; a < state->numActions[i]; ++a) {

      r = printAction(game, &state->action[i][a],
                      maxLen - c, &string[c]);
      if (r < 0) {
        return -1;
      }
      c += r;
    }
  }

  if (c >= maxLen) {
    return -1;
  }
  string[c] = 0;

  return c;
}

int printStack(const Game *game, const State *state,
               const int maxLen, char *string) {
  int c, r;

  if (maxLen == 0) {
    return -1;
  }

  c = 0;

  for (int i = 0; i < game->numPlayers; i++) {
    if (i > 0) {
      string[c] = '|';
      ++c;
    }
    r = sprintf(string + c, "%d", state->stackPlayer[i]);
    if (r < 0) {
      return -1;
    }
    c += r;
  }

  return c;
}

static int readHoleCards(const char *string, const Game *game,
                         State *state) {
  int p, c, r, num;

  c = 0;
  //first check for if we need to skip the ":". it is a hack for the stack-aware read_stack change.
  if (string[c] == ':') {
    ++c;
  }

  for (p = 0; p < game->numPlayers; ++p) {

    /* check for player separator '|' */
    if (p != 0) {
      if (string[c] == '|') {
        ++c;
      }
    }

    num = readCards(game,
                    &string[c],
                    game->numHoleCards,
                    state->holeCards[p],
                    &r);
    if (num == 0) {
      /* no cards for player p */

      continue;
    }
    if (num != game->numHoleCards) {
      /* read some cards, but not enough - bad! */

      return -1;
    }
    c += r;
  }

  return c;
}

static int printAllHoleCards(const Game *game, const State *state,
                             const int maxLen, char *string) {
  int p, c, r;

  c = 0;
  for (p = 0; p < game->numPlayers; ++p) {

    /* print player separator '|' */
    if (p != 0) {

      if (c >= maxLen) {
        return -1;
      }
      string[c] = '|';
      ++c;
    }

    r = printCards(game,
                   game->numHoleCards,
                   state->holeCards[p],
                   maxLen - c,
                   &string[c]);
    if (r < 0) {
      return -1;
    }
    c += r;
  }

  if (c >= maxLen) {
    return -1;
  }
  string[c] = 0;

  return c;
}

static int printPlayerHoleCards(const Game *game, const State *state,
                                const uint8_t player,
                                const int maxLen, char *string) {
  int p, c, r;

  c = 0;
  for (p = 0; p < game->numPlayers; ++p) {

    /* print player separator '|' */
    if (p != 0) {

      if (c >= maxLen) {
        return -1;
      }
      string[c] = '|';
      ++c;
    }

    if (p != player) {
      /* don't print other player's cards unless there was a showdown
          and they didn't fold */

      if (!stateFinished(state)) {
        continue;
      }

      if (state->playerFolded[p]) {
        continue;
      }

      if (numFolded(game, state) + 1 == game->numPlayers) {
        continue;
      }
    }

    r = printCards(game,
                   game->numHoleCards,
                   state->holeCards[p],
                   maxLen - c,
                   &string[c]);
    if (r < 0) {
      return -1;
    }
    c += r;
  }

  if (c >= maxLen) {
    return -1;
  }
  string[c] = 0;

  return c;
}

static int readBoardCards(const char *string, const Game *game,
                          State *state) {
  int i, c, r;

  c = 0;
  for (i = 0; i <= state->round; ++i) {

    /* check for round separator '/' */
    if (i != 0) {
      if (string[c] == '/') {
        ++c;
      }
    }

    if (readCards(game,
                  &string[c],
                  game->numBoardCards[i],
                  &state->boardCards[bcStart(game, i)],
                  &r)
        != game->numBoardCards[i]) {
      /* couldn't read the required number of cards - bad! */
      printf("read state wrong: boardcard at round %d \n", i);
      return -1;
    }
    c += r;
  }

  return c;
}

static int printBoardCards(const Game *game, const State *state,
                           const int maxLen, char *string) {
  int i, c, r;

  c = 0;
  for (i = 0; i <= state->round; ++i) {

    /* print round separator '/' */
    if (i != 0) {

      if (c >= maxLen) {
        return -1;
      }
      string[c] = '/';
      ++c;
    }

    r = printCards(game,
                   game->numBoardCards[i],
                   &state->boardCards[bcStart(game, i)],
                   maxLen - c,
                   &string[c]);
    if (r < 0) {
      return -1;
    }
    c += r;
  }

  if (c >= maxLen) {
    return -1;
  }
  string[c] = 0;

  return c;
}

static int readStateCommon(const char *string, const Game *game,
                           State *state) {
  uint32_t handId;
  int c, r;

  /* HEADER */
  c = 0;

  /* HEADER:handId */
  if (sscanf(string, ":%"SCNu32"%n", &handId, &r) < 1) {
    printf("read state wrong: hand_id \n");
    return -1;
  }
  c += r;

  initState(game, handId, state);

  /* HEADER:handId: */
  if (string[c] != ':') {
    printf("read state wrong: should be : after hand_id\n");
    return -1;
  }
  ++c;

  /* HEADER:handId:stack: */
  r = readStack(&string[c], game, state);
  if (r < 0) {
    printf("read state wrong: read stack failed.\n");
    return -1;
  }
  c += r;

  /* HEADER:handId:stack:betting: */
  r = readBetting(&string[c], game, state);
  if (r < 0) {
    printf("read state wrong: read betting actions failed.\n");
    return -1;
  }
  c += r;

  /* HEADER:handId:stack:betting:holeCards */
  r = readHoleCards(&string[c], game, state);
  if (r < 0) {
    printf("read state wrong: read hold cards failed\n");
    return -1;
  }
  c += r;

  /* HEADER:handId:stack:betting:holeCards boardCards */
  r = readBoardCards(&string[c], game, state);
  if (r < 0) {

    return -1;
  }
  c += r;

  return c;
}

//same as above, but allows custom read betting function
static int readStateCommonPlus(const char *string, const Game *game,
                               State *state, int (*f)(const char *string, const Game *game, State *state)) {
  uint32_t handId;
  int c, r;

  /* HEADER */
  c = 0;

  /* HEADER:handId */
  if (sscanf(string, ":%"SCNu32"%n", &handId, &r) < 1) {
    return -1;
  }
  c += r;

  initState(game, handId, state);

  /* HEADER:handId: */
  if (string[c] != ':') {
    return -1;
  }
  ++c;

  /* HEADER:handId:stack: */
  r = readStack(&string[c], game, state);
  if (r < 0) {
    printf("read state wrong: read stack failed.\n");
    return -1;
  }
  c += r;

  /* HEADER:handId:stack:betting: */
  r = f(&string[c], game, state);
  if (r < 0) {
    return -1;
  }
  c += r;

  /* HEADER:handId:stack:betting:holeCards */
  r = readHoleCards(&string[c], game, state);
  if (r < 0) {
    printf("read state wrong: read hold cards wrong.\n");
    return -1;
  }
  c += r;

  /* HEADER:handId:betting:stack:holeCards boardCards */
  r = readBoardCards(&string[c], game, state);
  if (r < 0) {
    return -1;
  }
  c += r;

  return c;
}

int readState(const char *string, const Game *game, State *state) {
  int c, r;

  /* HEADER = STATE */
  if (strncmp(string, "STATE", 5) != 0) {
    return -1;
  }
  c = 5;

  /* read rest of state */
  r = readStateCommon(&string[5], game, state);
  if (r < 0) {
    return -1;
  }
  c += r;

  return c;
}

int readMatchState(const char *string, const Game *game,
                   MatchState *state) {
  int c, r;

  /* HEADER = MATCHSTATE:player */
  if (sscanf(string, "MATCHSTATE:%"SCNu8"%n",
             &state->viewingPlayer, &c) < 1
      || state->viewingPlayer >= game->numPlayers) {
    return -1;
  }

  /* read rest of state */
  r = readStateCommon(&string[c], game, &state->state);
  if (r < 0) {
    return -1;
  }
  c += r;

  return c;
}

int readMatchStatePlus(const char *string, const Game *game, MatchState *state,
                       int (*f)(const char *string, const Game *game, State *state)) {
  int c, r;

  /* HEADER = MATCHSTATE:player */
  if (sscanf(string, "MATCHSTATE:%"SCNu8"%n",
             &state->viewingPlayer, &c) < 1
      || state->viewingPlayer >= game->numPlayers) {
    return -1;
  }

  /* read rest of state */
  r = readStateCommonPlus(&string[c], game, &state->state, f);
  if (r < 0) {
    return -1;
  }
  c += r;

  return c;
}

static int printStateCommon(const Game *game, const State *state,
                            const int maxLen, char *string) {
  int c, r;

  /* HEADER */
  c = 0;

  /* HEADER:handId: */
  r = snprintf(&string[c], maxLen - c, ":%"PRIu32":", state->handId);
  if (r < 0) {
    return -1;
  }
  c += r;
  if (c >= maxLen) {
    return -1;
  }

  /* HEADER:handId:stack: */
  r = printStack(game, state, maxLen - c, &string[c]);
  if (r < 0) {
    return -1;
  }
  c += r;
  if (c >= maxLen) {
    return -1;
  }
  string[c] = ':';
  ++c;

  /* HEADER:handId:stack:betting: */
  r = printBetting(game, state, maxLen - c, &string[c]);
  if (r < 0) {
    return -1;
  }
  c += r;
  if (c >= maxLen) {
    return -1;
  }
  string[c] = ':';
  ++c;

  return c;
}

int printState(const Game *game, const State *state,
               const int maxLen, char *string) {
  int c, r;

  c = 0;

  /* STATE */
  r = snprintf(&string[c], maxLen - c, "STATE");
  if (r < 0) {
    return -1;
  }
  c += r;

  /* STATE:handId:betting: */
  r = printStateCommon(game, state, maxLen - c, &string[c]);
  if (r < 0) {
    return -1;
  }
  c += r;

  /* STATE:handId:betting:holeCards */
  r = printAllHoleCards(game, state, maxLen - c, &string[c]);
  if (r < 0) {
    return -1;
  }
  c += r;

  /* STATE:handId:betting:holeCards boardCards */
  r = printBoardCards(game, state, maxLen - c, &string[c]);
  if (r < 0) {
    return -1;
  }
  c += r;

  if (c >= maxLen) {
    return -1;
  }
  string[c] = 0;

  return c;
}

int printMatchState(const Game *game, const MatchState *state,
                    const int maxLen, char *string) {
  int c, r;

  c = 0;

  /* MATCHSTATE:player */
  r = snprintf(&string[c], maxLen - c, "MATCHSTATE:%"PRIu8,
               state->viewingPlayer);
  if (r < 0) {
    return -1;
  }
  c += r;

  /* MATCHSTATE:player:handId:betting: */
  r = printStateCommon(game, &state->state, maxLen - c, &string[c]);
  if (r < 0) {
    return -1;
  }
  c += r;

  /* MATCHSTATE:player:handId:betting:holeCards */
  r = printPlayerHoleCards(game, &state->state, state->viewingPlayer,
                           maxLen - c, &string[c]);
  if (r < 0) {
    return -1;
  }
  c += r;

  /* MATCHSTATE:player:handId:betting:holeCards boardCards */
  r = printBoardCards(game, &state->state, maxLen - c, &string[c]);
  if (r < 0) {
    return -1;
  }
  c += r;

  if (c >= maxLen) {
    return -1;
  }
  string[c] = 0;

  return c;
}

int readAction(const char *string, const Game *game, Action *action) {
  int c, r;

  action->type = charToAction[(uint8_t) string[0]];
  if (action->type < 0) {
    return -1;
  }
  c = 1;

  if (action->type == a_raise && game->bettingType == noLimitBetting) {
    /* no-limit bet/raise needs to read a size */

    if (sscanf(&string[c], "%"SCNd32"%n", &action->size, &r) < 1) {
      return -1;
    }
    c += r;
  } else {
    /* size is zero for anything but a no-limit raise */

    action->size = 0;
  }

  return c;
}

int printAction(const Game *game, const Action *action,
                const int maxLen, char *string) {
  int c, r;

  if (maxLen == 0) {
    return -1;
  }

  c = 0;

  string[c] = actionChars[action->type];
  ++c;

  if (game->bettingType == noLimitBetting && action->type == a_raise) {
    /* 2010 AAAI no-limit format has a size for bet/raise */

    r = snprintf(&string[c], maxLen - c, "%"PRId32, action->size);
    if (r < 0) {
      return -1;
    }
    c += r;
  }

  if (c >= maxLen) {
    return -1;
  }
  string[c] = 0;

  return c;
}

int readCard(const Game *game, const char *string, uint8_t *card) {
  char *spos;
  uint8_t c;

  if (string[0] == 0) {
    return -1;
  }
  spos = strchr(rankChars, toupper(string[0]));
  if (spos == 0) {
//    printf("%s\n", string);
//    printf("card string contains illegal representation other than 23456789TJQKA\n");
    return -1;
  }
  c = spos - rankChars;

  if (string[1] == 0) {
    return -1;
  }
  spos = strchr(suitChars, tolower(string[1]));
  if (spos == 0) {
    return -1;
  }

  *card = makeCard(c, spos - suitChars, game->numSuits);

  return 2;
}

int readCards(const Game *game,
              const char *string,
              const int maxCards,
              uint8_t *cards,
              int *charsConsumed) {
  int i, c, r;

  c = 0;
  for (i = 0; i < maxCards; ++i) {

    r = readCard(game, &string[c], &cards[i]);
    if (r < 0) {
      break;
    }
    c += r;
  }

  *charsConsumed = c;
  return i;
}

int printCard(const Game *game,
              const uint8_t card,
              const int maxLen,
              char *string) {
  if (3 > maxLen) {
    return -1;
  }

  if (card == UNDEFINED_CARD) {
    return 0;
  }

  string[0] = rankChars[rankOfCard(card, game->numSuits)];
  string[1] = suitChars[suitOfCard(card, game->numSuits)];
  string[2] = 0;

  return 2;
}

int printCards(const Game *game,
               const int numCards,
               const uint8_t *cards,
               const int maxLen,
               char *string) {
  int i, c, r;

  if (numCards == 0) {

    if (maxLen == 0) {

      return -1;
    }
    string[0] = 0;
    return 0;
  }

  c = 0;

  for (i = 0; i < numCards; ++i) {

    r = printCard(game, cards[i], maxLen - c, &string[c]);
    if (r < 0) {
      return -1;
    }
    c += r;
  }

  /* no need to null terminate, we know for sure that printCard does */

  return c;
}

int32_t actionToCode(Action *action) {
  switch (action->type) {
    case a_fold:return -1;
    case a_call:return 0;
    case a_raise:return action->size;
    default:exit(EXIT_FAILURE);
  }
}

int Equal(Game *this_game, Game *that_game) {
  if (this_game->numPlayers != that_game->numPlayers) return 0;
  if (this_game->bettingType != that_game->bettingType) return 0;
  if (this_game->numHoleCards != that_game->numHoleCards) return 0;
  if (this_game->numRanks != that_game->numRanks) return 0;
  if (this_game->numRounds != that_game->numRounds) return 0;
  if (this_game->numSuits != that_game->numSuits) return 0;
  for (int i = 0; i < 4; i++) {
    if (this_game->numBoardCards[i] != that_game->numBoardCards[i]) return 0;
    if (this_game->firstPlayer[i] != that_game->firstPlayer[i]) return 0;
    if (this_game->maxRaises[i] != that_game->maxRaises[i]) return 0;
    if (this_game->bettingType == limitBetting)
      if (this_game->raiseSize[i] != that_game->raiseSize[i]) return 0;
  }
  for (int i = 0; i < that_game->numPlayers; i++) {
    if (this_game->blind[i] != that_game->blind[i]) return 0;
  }

  //stack source have to be the same
//  if (this_game->use_state_stack != that_game->use_state_stack) return 0;
//  //if using the game source, the stack depth should be the same
//  if (this_game->use_state_stack == 0)
//    for (int i = 0; i < that_game->numPlayers; i++)
//      if (this_game->stack[i] != that_game->stack[i])
//        return 0;

  return 1;
}

int BigBlind(Game *game) {
  int blind_0 = game->blind[0];
  int blind_1 = game->blind[1];
  //support reverse blind game
  int bigblind = blind_0 > blind_1 ? blind_0 : blind_1;
  return bigblind;
}
int GameDefaultStackDepth(Game *game) {
  int bigblind = BigBlind(game);
  int stack = game->stack[0];
  if (game->stack[0] != game->stack[1]) {
    printf("always use same stack in the game def");
    exit(1);
  }
  return stack / bigblind;
}
int StateStackDepth(State *state, Game *game) {
  int stack_0 = state->stackPlayer[0];
  int stack_1 = state->stackPlayer[1];
  int min_stack = stack_0 < stack_1 ? stack_0 : stack_1;
  int match_state_depth = min_stack / BigBlind(game); //it is a hack. we use only 100 in the engine.
  return match_state_depth;
}

/*
 * return 0 if the same match.
 */
int InSameMatch(Game *game, MatchState *old_state, MatchState *new_state) {
  if (old_state->viewingPlayer != new_state->viewingPlayer) return 1;
  if (old_state->state.handId != new_state->state.handId) return 1;
  //check actions. all actions in old_state should be in the new one
  for (int r = 0; r <= old_state->state.round; r++) {
    for (int a = 0; a < old_state->state.numActions[r]; a++) {
      if (old_state->state.action[r][a].type != new_state->state.action[r][a].type) return 1;
      if (old_state->state.action[r][a].type == a_raise)
        if (old_state->state.action[r][a].size != new_state->state.action[r][a].size) return 1;
    }
  }
  //check the hold cards of the viewing player
  int viewing_Player = old_state->viewingPlayer;
  if (old_state->state.holeCards[viewing_Player][0] != new_state->state.holeCards[viewing_Player][0]) return 1;
  if (old_state->state.holeCards[viewing_Player][1] != new_state->state.holeCards[viewing_Player][1]) return 1;

  return 0;
}

int32_t GetRaiseBase(Game *game, State *state, enum RAISE_MODE mode) {
  switch (mode) {
    case POT_NET:
    case POT_AFTER_CALL: {
      int32_t pot = 0;
      /* Check for pot-size raise being valid.  First, get the pot size. */
      int acting_player = currentPlayer(game, state);
      for (int p = 0; p < game->numPlayers; ++p) {
        pot += state->spent[p];
      }

      /* Add amount needed to call.  This gives the size of a pot-sized raise */
      if (mode == POT_AFTER_CALL) {
        int amount_to_call = state->maxSpent - state->spent[acting_player];
        pot += amount_to_call;
      }
      return pot;
    }
    case X_LAST_RAISE: {
      /*
       * check if the actor is facing the frist raise action
       * i.e. previous actors all checked.
       * if so, return the pot net (but this is handle at the outside
       * otherwise, return the last action raise of this round so far
       */
      int r = state->round;
//      int with_raise_action = WithEarlyRaiseAction(state);
//      //no one raise. just return  POT NET
//      if (with_raise_action == 0)
//        return GetPot(game, state, POT_NET);

      //get last betting action. may not the the action immediately before in following cases
      //- small blind calling big blind. the big blind bet is the last bet
      //- or in multi player games, we should trace back to the last raiser
      int32_t bb = BigBlind(game);
      if (r == 0) {
        //no action yet, small blind act, return just big blind
        if (state->numActions[0] == 0)
          return bb;

        //small blind calls, just return the bb
        if (state->numActions[0] == 1 && state->action[0][0].type == a_call)
          return bb;
      }

      //otherwise, get the last raise actions at the round and find out what it actually raise.
      Action last_raise;
      last_raise.type = a_raise;
      int raiser = -1;
      for (int a = state->numActions[r] - 1; a >= 0; a--) {
        if (state->action[r][a].type == a_raise) {
          last_raise.size = state->action[r][a].size;
          raiser = state->actingPlayer[r][a];
          break;
        }
      }
      if (raiser == -1) {
        printf("there should be raiser");
        exit(1);
      }

      //return the net spent of this round.
      int round_raise_net = state->sum_round_spent[r][raiser];
      if (r > 0)
        round_raise_net -= state->sum_round_spent[r - 1][raiser];
      return round_raise_net;
    }
    case NET_OR_X:
      printf("should not use net_or_x here. force exit");
      exit(1);
      break;
  }
}

/*
 * return 1 if compatible
 */
int CompatibleGame(const Game *lhs, Game *rhs) {
  if (lhs->bettingType != rhs->bettingType) return 0;
  if (lhs->numPlayers != rhs->numPlayers) return 0;
  if (lhs->numHoleCards != rhs->numHoleCards) return 0;
  if (lhs->numRounds != rhs->numRounds) return 0;
  if (lhs->numRanks != rhs->numRanks) return 0;
  return 1;
}
int BigBlindPosition(Game *game) {
  return game->blind[0] > game->blind[1] ? 0 : 1;
}

int32_t PotStake(State *state) {
  //the min of both spent
  int32_t spent_0 = state->spent[0];
  int32_t spent_1 = state->spent[1];
  return spent_0 < spent_1 ? spent_0 : spent_1;
}

int SumBettingPatternDiff(State *lhs, State *rhs) {
  int round = lhs->round; //assuming they are on the same round
  int sum = 0;
  for (int r = 0; r <= round; r++) {
    sum += abs(lhs->numActions[r] - rhs->numActions[r]);
  }
  return sum;
}
