/*
Copyright (C) 2011 by the Computer Poker Research Group, University of Alberta
*/

#ifndef _GAME_H
#define _GAME_H
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "rng.h"

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#define VERSION_MAJOR 2
#define VERSION_MINOR 0
#define VERSION_REVISION 0

#define MAX_ROUNDS 4
#define MAX_PLAYERS 10
#define MAX_BOARD_CARDS 7
#define MAX_HOLE_CARDS 3
#define MAX_NUM_ACTIONS 32
#define MAX_SUITS 4
#define MAX_RANKS 13
#define MAX_LINE_LEN 1024
#define UNDEFINED_CARD 60
#define NUM_ACTION_TYPES 3

/* high card	1287	0
   pair		3718	1287
   two pair	3601	5005
   trips	1014	8606
   straight	13	9620
   flush	1287	9633
   f_house	1014	10920
   quads	169	11934
   s_flush	13	12103 */
#define HANDCLASS_SINGLE_CARD 0
#define HANDCLASS_PAIR 1287
#define HANDCLASS_TWO_PAIR 5005
#define HANDCLASS_TRIPS 8606
#define HANDCLASS_STRAIGHT 9620
#define HANDCLASS_FLUSH 9633
#define HANDCLASS_FULL_HOUSE 10920
#define HANDCLASS_QUADS 11934
#define HANDCLASS_STRAIGHT_FLUSH 12103

static const int PREMATURE_FOLD = 5;

typedef union {
  uint16_t bySuit[4];
  uint64_t cards;
} Cardset;

enum BettingType { limitBetting, noLimitBetting };
enum ActionType {
  a_fold = 0, a_call = 1, a_raise = 2,
  a_invalid = NUM_ACTION_TYPES
};

enum RAISE_MODE {
  POT_NET,
  POT_AFTER_CALL,
  NET_OR_X,
  X_LAST_RAISE
};

enum BettingParseType { ACPCSTANDARD, SLUMBOTSTANDARD };

static const char actionChars[a_invalid + 1] = "fcr";

static const char suitChars[MAX_SUITS + 1] = "cdhs";
static const char rankChars[MAX_RANKS + 1] = "23456789TJQKA";

static const uint8_t SUIT_CLUB = 0;
static const uint8_t SUIT_DIAMOND = 1;
static const uint8_t SUIT_HEART = 2;
static const uint8_t SUIT_SPADE = 3;

static const uint8_t RANK_TWO = 0;
static const uint8_t RANK_THREE = 1;
static const uint8_t RANK_FOUR = 2;
static const uint8_t RANK_FIVE = 3;
static const uint8_t RANK_SIX = 4;
static const uint8_t RANK_SEVEN = 5;
static const uint8_t RANK_EIGHT = 6;
static const uint8_t RANK_NINE = 7;
static const uint8_t RANK_TEN = 8;
static const uint8_t RANK_JACK = 9;
static const uint8_t RANK_QUEEN = 10;
static const uint8_t RANK_KING = 11;
static const uint8_t RANK_ACE = 12;

typedef struct {
  enum ActionType type; /* is action a fold, call, or raise? */
  int32_t size; /* for no-limit raises, we need a size
		   MUST BE 0 IN ALL CASES WHERE IT IS NOT USED */
} Action;

typedef struct {

  /* stack sizes for each player */
  int32_t stack[MAX_PLAYERS];

  /* must set this to true if overriding player stack
   * otherwise initState will replace the changes
   */
  uint8_t use_state_stack;

  /* entry fee for game, per player */
  int32_t blind[MAX_PLAYERS];

  /* size of fixed raises for limitBetting games */
  int32_t raiseSize[MAX_ROUNDS];

  /* general class of game */
  enum BettingType bettingType;

  /* number of players in the game */
  uint8_t numPlayers;

  /* number of betting rounds */
  uint8_t numRounds;

  /* first player to act in a round */
  uint8_t firstPlayer[MAX_ROUNDS];

  /* number of bets/raises that may be made in each round */
  uint8_t maxRaises[MAX_ROUNDS];

  /* number of suits and ranks in the deck of cards */
  uint8_t numSuits;
  uint8_t numRanks;

  /* number of private player cards */
  uint8_t numHoleCards;

  /* number of shared public cards each round */
  uint8_t numBoardCards[MAX_ROUNDS];
} Game;

typedef struct {
  uint32_t handId;

  /* largest bet so far, including all previous rounds */
  int32_t maxSpent;

  /* minimum number of chips a player must have spend in total to raise
     only used for noLimitBetting games */
  int32_t minNoLimitRaiseTo;

  /* spent[ p ] gives the total amount put into the pot by player p */
  int32_t spent[MAX_PLAYERS];

  /* stackPlayer is by player, not seat */
  int32_t stackPlayer[MAX_PLAYERS];

  /* spent[ r ][ p ] gives the total amount put into the boy by player p up to a particular round */
  int32_t sum_round_spent[MAX_ROUNDS][MAX_PLAYERS];

  /* action[ r ][ i ] gives the i'th action in round r */
  Action action[MAX_ROUNDS][MAX_NUM_ACTIONS];

  /* actingPlayer[ r ][ i ] gives the player who made action i in round r
     we can always figure this out from the actions taken, but it's
     easier to just remember this in multiplayer (because of folds) */
  uint8_t actingPlayer[MAX_ROUNDS][MAX_NUM_ACTIONS];

  /* numActions[ r ] gives the number of actions made in round r.
   * the actual value of it, starting from 1 not 0
   * e.g. cc = 2 actions. cr600c = 3 actions*/
  uint8_t numActions[MAX_ROUNDS];

  /* current round: a value between 0 and game.numRounds-1
     a showdown is still in numRounds-1, not a separate round */
  uint8_t round;

  /* finished is non-zero if and only if the game is over */
  uint8_t finished;

  /* playerFolded[ p ] is non-zero if and only player p has folded */
  uint8_t playerFolded[MAX_PLAYERS];

  /* public cards (including cards which may not yet be visible to players) */
  uint8_t boardCards[MAX_BOARD_CARDS];

  /* private cards */
  uint8_t holeCards[MAX_PLAYERS][MAX_HOLE_CARDS];
} State;

typedef struct {
  State state;
  uint8_t viewingPlayer;
} MatchState;

int rankCardset(const Cardset cards);
Cardset emptyCardset();
void addCardToCardset(Cardset *c, int suit, int rank);

/* returns a game structure, or NULL on failure */
Game *readGame(FILE *file);

// return 1 if equals, 0 otherwise
int Equal(Game *this_game, Game *that_game);

void printGame(FILE *file, const Game *game);

/* initialise a state so that it is at the beginning of a hand
   DOES NOT DEAL OUT CARDS */
void initState(const Game *game, const uint32_t handId, State *state);

/* shuffle a deck of cards and deal them out, writing the results to state */
void dealCards(const Game *game, rng_state_t *rng, State *state);
void dealRemainingCards(const Game *game, rng_state_t *rng, State *state);

//copy a -> b
void StateTableInfoCopy(const Game *game, State *ref_state, State *target_state);

int matchStatesEqual(const Game *game, const MatchState *a,
                     const MatchState *b);

/* check if a raise is possible, and the range of valid sizes
   returns non-zero if raise is a valid action and sets *minSize
   and maxSize, or zero if raise is not valid */
int raiseIsValid(const Game *game, const State *curState,
                 int32_t *minSize, int32_t *maxSize);

/* check if an action is valid

   if tryFixing is non-zero, try modifying the given action to produce
   a valid action, as in the AAAI rules.  Currently this only means
   that a no-limit raise will be modified to the nearest valid raise size

   returns non-zero if final action/size is valid for state, 0 otherwise */
int isValidAction(const Game *game, const State *curState,
                  const int tryFixing, Action *action);

/* record the given action in state
    does not check that action is valid */
void doAction(const Game *game, const Action *action, State *state);
int TotalAction(State* state);
/*
 * target_state should be precisely one step backward from the reference state
 * return -1 if fails. 0 otherwise.
 */
int StepBackAction(const Game *game, State *reference_state, State *target_state, int back_steps);
Action GetLastActionFromState(State *reference_state);

/* returns non-zero if hand is finished, zero otherwise */
#define stateFinished(constStatePtr) ((constStatePtr)->finished)

/* get the current player to act in the state */
uint8_t currentPlayer(const Game *game, const State *state);

/* number of raises in the current round */
uint8_t numRaises(const State *state);

/* number of players who have folded */
uint8_t numFolded(const Game *game, const State *state);

/* number of players who have called the current bet (or initiated it)
   doesn't count non-acting players who are all-in */
uint8_t numCalled(const Game *game, const State *state);

/* number of players who are all-in */
uint8_t numAllIn(const Game *game, const State *state);

/* number of players who can still act (ie not all-in or folded) */
uint8_t numActingPlayers(const Game *game, const State *state);

/* get the index into array state.boardCards[] for the first board
   card in round (where the first round is round 0) */
uint8_t bcStart(const Game *game, const uint8_t round);

/* get the total number of board cards dealt out after (zero based) round */
uint8_t sumBoardCards(const Game *game, const uint8_t round);

/* returns an int representing the rank of a hand, higher rank the better */
int rankHand(const Game *game, const State *state,
             const uint8_t player);

/* return the value of a finished hand for a player
   returns a double because pots may be split when players tie
   WILL HAVE UNDEFINED BEHAVIOUR IF HAND ISN'T FINISHED
   (stateFinished(state)==0) */
double valueOfState(const Game *game, const State *state,
                    const uint8_t player);

/* returns number of characters consumed on success, -1 on failure
   state will be modified even on a failure to read */
int readState(const char *string, const Game *game, State *state);

/* returns number of characters consumed on success, -1 on failure
   state will be modified even on a failure to read */
int readMatchState(const char *string, const Game *game, MatchState *state);

/* allows custom betting parser */
int readMatchStatePlus(const char *string,
                       const Game *game,
                       MatchState *state,
                       int (*f)(const char *string, const Game *game, State *state));

/* print a state to a string, as viewed by viewingPlayer
   returns the number of characters in string, or -1 on error
   DOES NOT COUNT FINAL 0 TERMINATOR IN THIS COUNT!!! */
int printState(const Game *game, const State *state,
               const int maxLen, char *string);

/* print a state to a string, as viewed by viewingPlayer
   returns the number of characters in string, or -1 on error
   DOES NOT COUNT FINAL 0 TERMINATOR IN THIS COUNT!!! */
int printMatchState(const Game *game, const MatchState *state,
                    const int maxLen, char *string);

/* read an action, returning the action in the passed pointer
   action and size will be modified even on a failure to read
   returns number of characters consumed on succes, -1 on failure */
int readAction(const char *string, const Game *game, Action *action);

/* print an action to a string
   returns the number of characters in string, or -1 on error
   DOES NOT COUNT FINAL 0 TERMINATOR IN THIS COUNT!!! */
int printAction(const Game *game, const Action *action,
                const int maxLen, char *string);
/* print actions to a string
   returns number of characters printed to string, or -1 on failure
   DOES NOT COUNT FINAL 0 TERMINATOR IN THIS COUNT!!! */
int printBetting(const Game *game, const State *state,
                 const int maxLen, char *string);
int printStack(const Game *game, const State *state,
               const int maxLen, char *string);

/* returns number of characters consumed, or -1 on error
   on success, returns the card in *card */
int readCard(const Game *game, const char *string, uint8_t *card);

/* read up to maxCards cards
   returns number of cards successfully read
   returns number of characters consumed in charsConsumed */
int readCards(const Game *game,
              const char *string,
              const int maxCards,
              uint8_t *cards,
              int *charsConsumed);

int readStack(const char *string, const Game *game, State *state);

/* print a card to a string
   returns the number of characters in string, or -1 on error
   DOES NOT COUNT FINAL 0 TERMINATOR IN THIS COUNT!!! */
int printCard(const Game *game,
              const uint8_t card,
              const int maxLen,
              char *string);

/* print out numCards cards to a string
   returns the number of characters in string
   DOES NOT COUNT FINAL 0 TERMINATOR IN THIS COUNT!!! */
int printCards(const Game *game,
               const int numCards,
               const uint8_t *cards,
               const int maxLen,
               char *string);


/*
 * BULLDOG newly added
 */
int32_t actionToCode(Action *action);
int BigBlind(Game* game);
int GameDefaultStackDepth(Game* game);
int StateStackDepth(State* state, Game* game);
int InSameMatch(Game* game, MatchState* old_state, MatchState* new_state);
int32_t GetRaiseBase(Game* game, State* state, enum RAISE_MODE mode);
int CompatibleGame(const Game *lhs, Game* rhs);
int BigBlindPosition(Game* game);
int32_t PotStake(State* state); //only support 2p now, return the min
int SumBettingPatternDiff(State* lhs, State* rhs);
int StateEqual(const Game *game, const State *a, const State *b);


#define rankOfCard(card, numSuits) ((card)/(numSuits))
#define suitOfCard(card, numSuits) ((card)%(numSuits))
#define makeCard(rank, suit, numSuits) ((rank)*(numSuits)+(suit))

#endif
