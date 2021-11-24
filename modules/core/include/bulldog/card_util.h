//
// Created by Isaac Zhang on 4/3/20.
//

#ifndef BULLDOG_MODULES_CORE_INCLUDE_BULLDOG_CARD_UTIL_H_
#define BULLDOG_MODULES_CORE_INCLUDE_BULLDOG_CARD_UTIL_H_

#include <vector>
#include <set>
#include <bulldog/logger.hpp>
#include <filesystem>
#include <random>
#include <pokerstove/util/combinations.h>
#include <pokerstove/peval/CardSet.h>
#include <pokerstove/peval/Card.h>

extern "C" {
#include "bulldog/game.h"
}

const int HOLDEM_ROUND_RIVER = 3;
const int HOLDEM_ROUND_TURN = 2;
const int HOLDEM_ROUND_FLOP = 1;
const int HOLDEM_ROUND_PREFLOP = 0;

const int HOLDEM_MAX_BOARD = 5;
const int HOLDEM_MAX_HOLE_CARDS = 2;
const int HOLDEM_MAX_CARDS = 52;
const int HOLDEM_MAX_ROUNDS = 4;
const int HOLDEM_MAX_HANDS_PERMUTATION = 1326;
const int HOLDEM_MAX_HANDS_PERMUTATION_EXCLUDE_BOARD = 1081;
const double IMPOSSIBLE_HAND_VALUE = -1231512341234; // a dirty hack.
const double IMPOSSIBLE_RANK_PROB = 0.0;
const uint8_t IMPOSSIBLE_CARD = 60;
using Card_t = uint8_t;
using Colex = unsigned int;
static std::map<int, int> HoldemSumBoardMap{
    {0, 0},
    {1, 3},
    {2, 4},
    {3, 5}
};
struct Hand_t {
  Card_t high_card;
  Card_t low_card;

  bool Crash(Hand_t &that) const {
    if (this->low_card == that.low_card) return true;
    if (this->high_card == that.high_card) return true;
    if (this->high_card == that.low_card) return true;
    return this->low_card == that.high_card;
  }
};

struct Board_t {
  Card_t cards[HOLDEM_MAX_BOARD];
  bool CardCrash(Card_t check_card) {
    for (unsigned char card : cards)
      if (card == check_card) return true;
    return false;
  }
  bool CardCrashTillRound(Card_t check_card, uint8_t r) {
    //must be false for preflop
    if (r == HOLDEM_ROUND_PREFLOP)
      return false;

    for (auto round = HOLDEM_ROUND_FLOP; round <= r; round++)
      if (CardCrashAtRound(check_card, round))
        return true;
    return false;
  }
  bool CardCrashAtRound(Card_t check_card, uint8_t r) {
    switch (r) {
      case HOLDEM_ROUND_FLOP : {
        if (cards[0] == check_card) return true;
        if (cards[1] == check_card) return true;
        if (cards[2] == check_card) return true;
        break;
      }
      case HOLDEM_ROUND_TURN : {
        if (cards[3] == check_card) return true;
        break;
      }
      case HOLDEM_ROUND_RIVER : {
        if (cards[4] == check_card) return true;
        break;
      }
      default:
        logger::critical("not board card at round %d", r);
    }
    return false;
  }
  bool HandCrash(Hand_t check_hand) {
    return CardCrash(check_hand.high_card) || CardCrash(check_hand.low_card);
  }
  bool Equals(Board_t &that) {
    for (int i = 0; i < HOLDEM_MAX_BOARD; i++)
      if (cards[i] != that.cards[i]) return false;
    return true;
  }
  void Print() {
    logger::debug("board: %d %d %d %d %d", cards[0], cards[1], cards[2], cards[3], cards[4]);
  }
};

struct HoldemDeck {
  std::vector<Card_t> cards_;
  explicit HoldemDeck(Board_t &board) {
    for (int c = 0; c < HOLDEM_MAX_CARDS; c++) {
      if (!board.CardCrash(c))
        cards_.emplace_back(c);
    }
  }
  void Shuffle() {
    auto rd = std::random_device{};
    auto rng = std::default_random_engine{rd()};
    std::shuffle(cards_.begin(), cards_.end(), rng);
  }
};

int nCk_card(int m, int n);

void BoardFromState(Game *game, State *state, Board_t *board);
void SampleRandomFullBoard(State &state, Game *game, Board_t &board, std::vector<Board_t> &my_flops);
void SampleSequentialFullBoard(State &state,
                               Game *game,
                               Board_t &board,
                               int &cur_flop_idx,
                               const std::vector<Board_t> &my_flop);



/*
 * canonical computation
 */
uint64_t Canonize(uint64_t cardmask);
Colex ComputeColex(uint64_t cardmask);
Colex ComputeColexFromAllCards(Card_t high, Card_t low, Board_t &board, int round);


/*
 * cardset functions
 */
Cardset CardsetFromString(const std::string &str);
std::string CardsToString(uint64_t cardmask);
void AddCardTToCardset(Cardset *c, uint8_t card);
void AddBoardToCardSetByRound(Cardset *c, Board_t *board, int round);
void EnrichCardSetToRound(Cardset *c, Card_t high, Card_t low, Board_t *board, int round);
void EnrichCardSetToRound(Cardset *c, Hand_t *hand, Board_t *board, int round);
std::set<Colex> GetCannoSetByBoard(Board_t *board, int round);

/*
 * Random functions
 */
int GenRndNumber(int start, int end);
int RandomPick(const float *avg, int child_size);
template<class T>
int RndXorShift(const T *choices,
                int size,
                unsigned long &seed_x,
                unsigned long &seed_y,
                unsigned long &seed_z,
                unsigned long mod) {
  seed_x ^= seed_x << 16;
  seed_x ^= seed_x >> 5;
  seed_x ^= seed_x << 1;

  unsigned long t = seed_x;
  seed_x = seed_y;
  seed_y = seed_z;
  seed_z = t ^ seed_x ^ seed_y;
  auto chosen = seed_z % mod;

  float sum = 0.0;
  for (auto a = 0; a < size; a++) {
    if (choices[a] == 0 || choices[a] == -1) // mask and 0
      continue;
    sum += choices[a] * mod;
    if (chosen <= sum) {
      return a;
    }
  }

  logger::error("failed to pick. [size %d] [chosen %d]", size, chosen);
  for (int i = 0 ; i < size; i++){
    auto v = choices[i];
    logger::debug("%f", v);
    if (v < 0 && v != -1)
      logger::error("negative value in prob vector [%d] %f", i , v);
  }
  return -1;
}

/**
 * for fast terminal value eval.
 */
int RankHand(Card_t high, Card_t low, Board_t *board);
int RankHand(Hand_t &hand, Board_t *board);

/*
 * Vector Index for Holdem
 */
using VectorIndex = uint16_t;
VectorIndex ToVectorIndex(Card_t high, Card_t low);
std::pair<Card_t, Card_t> FromVectorIndex(VectorIndex vector_idx);
bool VectorIdxClash(VectorIndex lhs, VectorIndex rhs);
bool VectorIdxClashCard(VectorIndex vid, Card_t c);
std::string VectorIdxToString(VectorIndex i);

#endif //BULLDOG_MODULES_CORE_INCLUDE_BULLDOG_CARD_UTIL_H_
