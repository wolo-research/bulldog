//
// Created by Isaac Zhang on 4/4/20.
//
#include <bulldog/card_util.h>
#include <pokerstove/peval/CardSetGenerators.h>

int nCk_card(int m, int n) {
  int mat[52][52];
  int i, j;
  if (n > m) return 0;
  if ((n == 0) || (m == n)) return 1;
  for (j = 0; j < n; j++) {
    mat[0][j] = 1;
    if (j == 0) {
      for (i = 1; i <= m - n; i++) mat[i][j] = i + 1;
    } else {
      for (i = 1; i <= m - n; i++) mat[i][j] = mat[i - 1][j] + mat[i][j - 1];
    }
  }
  return (mat[m - n][n - 1]);
}

void BoardFromState(Game *game, State *state, Board_t *board) {
  for (unsigned char &card : board->cards)
    card = IMPOSSIBLE_CARD; // in case the init value crashes with cards,wrongfully.
  int board_card_idx = 0;
  for (auto r = 0; r <= state->round; r++) {
    for (auto bc = 0; bc < game->numBoardCards[r]; bc++) {
      board->cards[board_card_idx] = state->boardCards[board_card_idx];
      board_card_idx++;
    }
  }
}

Cardset CardsetFromString(const std::string &str) {
  Cardset c = emptyCardset();
  for (size_t i = 0; i < str.size(); i += 2) {
    int rank = std::strchr(rankChars, str[i]) - rankChars;
    int suit = std::strchr(suitChars, str[i + 1]) - suitChars;
    addCardToCardset(&c, suit, rank);
  }
  return c;
}

std::string CardsToString(uint64_t cardmask) {
  std::string out = "";
  uint8_t card;
  while (cardmask) {
    card = __builtin_ctzll(cardmask);
    out += rankChars[card & 15]; // 1111
    out += suitChars[card >> 4];
    cardmask &= cardmask - 1;  // clear the least significant bit set
  }
  return out;
}

uint64_t Canonize(uint64_t cardmask) {
  std::string instr = CardsToString(cardmask);
  uint64_t ps_cardmask = 0;
  for (size_t i = 0; i < instr.size(); i += 2) {
    // skip whitespace
    if (instr[i] == ' ') {
      i -= 1;
      continue;
    }
    int rank = std::strchr(rankChars, instr[i]) - rankChars;
    int suit = std::strchr(suitChars, instr[i + 1]) - suitChars;
    int code = rank + suit * 13;
    uint64_t mask = ((uint64_t) 1 << code);
    ps_cardmask |= mask;
  }

  int num_rank = 13;
  int num_suit = 4;
  int smasks[num_suit];
  for (int i = 0; i < num_suit; i++) {
    smasks[i] = (static_cast<int>(ps_cardmask >> ((i) * num_rank)) & 0x1FFF);
  }
  std::sort(smasks, smasks + num_suit);

  ps_cardmask = static_cast<uint64_t>(smasks[3]) |
      static_cast<uint64_t>(smasks[2]) << num_rank |
      static_cast<uint64_t>(smasks[1]) << num_rank * 2 |
      static_cast<uint64_t>(smasks[0]) << num_rank * 3;

  std::string out = "";
  static const std::string cardstrings = "2c3c4c5c6c7c8c9cTcJcQcKcAc"
                                         "2d3d4d5d6d7d8d9dTdJdQdKdAd"
                                         "2h3h4h5h6h7h8h9hThJhQhKhAh"
                                         "2s3s4s5s6s7s8s9sTsJsQsKsAs";
  uint64_t v = ps_cardmask;
  while (v) {
    out += cardstrings.substr(__builtin_ctzll(v) * 2, 2);
    v &= v - 1;  // clear the least significant bit set
  }

  return CardsetFromString(out).cards;
}

int GenRndNumber(int start, int end) {
  std::random_device rand_dev;
  std::mt19937 rnd_gen(rand_dev());
  std::uniform_int_distribution<int> distr(start, end);
  return distr(rnd_gen);
}

int RandomPick(const float *avg, int child_size) {
  std::random_device g_rand_dev;
  std::mt19937 g_generator(g_rand_dev());
  std::discrete_distribution<> distr(avg, avg + child_size);
  return distr(g_generator);
}

Colex ComputeColex(uint64_t cardmask) {
  std::string instr = CardsToString(cardmask);
  uint64_t ps_cardmask = 0;
  for (size_t i = 0; i < instr.size(); i += 2) {
    // skip whitespace
    if (instr[i] == ' ') {
      i -= 1;
      continue;
    }
    int rank = std::strchr(rankChars, instr[i]) - rankChars;
    int suit = std::strchr(suitChars, instr[i + 1]) - suitChars;
    int code = rank + suit * 13;
    uint64_t mask = ((uint64_t) 1 << code);
    ps_cardmask |= mask;
  }

  size_t value = 0;
  std::vector<int> cards(instr.size() / 2);
  size_t n = 0;
  for (size_t i = 0; i < 52; i++)
    if (ps_cardmask & ((uint64_t) 1 << i))
      cards[n++] = i;

  for (size_t i = 0; i < cards.size(); i++) {
    size_t code = cards[i];
    if (code >= i + 1)
      value += nCk_card(code, i + 1);
  }
  return value;
}

int RankHand(Card_t high, Card_t low, Board_t *board) {
  if (board->HandCrash(Hand_t{high, low}))
    return -1;
  Cardset c = emptyCardset();
  addCardToCardset(&c, suitOfCard(high, 4), rankOfCard(high, 4));
  addCardToCardset(&c, suitOfCard(low, 4), rankOfCard(low, 4));
  for (auto card : board->cards)
    addCardToCardset(&c, suitOfCard(card, 4), rankOfCard(card, 4));
  return rankCardset(c);
}

int RankHand(Hand_t &hand, Board_t *board) {
  return RankHand(hand.high_card, hand.low_card, board);
}

void SampleRandomFullBoard(State &state, Game *game, Board_t &board, std::vector<Board_t> &my_flops) {
  //add cards that already existed
  auto sum_bc = 0;
  if (state.round == 0 && !my_flops.empty()) {
    //if first round, we choose flop from my_flops and fill the rest randomly
    //GenRndNumber is inclusive [0,n]
    int num_cards_flop = game->numBoardCards[1];
    int rnd = GenRndNumber(0, my_flops.size() - 1);
    for (auto i = 0; i < num_cards_flop; i++) {
      board.cards[i] = my_flops[rnd].cards[i];
    }
    sum_bc = num_cards_flop;
  } else {
    // get board from state directly.
//    BoardFromState(game, &state, &board);
    for (int i = 0; i <= state.round; i++)
      sum_bc += game->numBoardCards[i];
    for (int i = 0; i < sum_bc; i++)
      board.cards[i] = state.boardCards[i];
  }

  HoldemDeck deck{board};
  deck.Shuffle();
  for (int bs = sum_bc; bs < HOLDEM_MAX_BOARD; bs++)
    board.cards[bs] = deck.cards_[bs];
}

void SampleSequentialFullBoard(State &state,
                               Game *game,
                               Board_t &board,
                               int &cur_flop_idx,
                               const std::vector<Board_t> &my_flop) {
  //add cards that already existed
  auto sum_bc = 0;
  if (state.round == 0 && !my_flop.empty()) {
    //sequentially rotate flops
    int num_cards_flop = game->numBoardCards[1];
    for (auto i = 0; i < num_cards_flop; i++) {
      board.cards[i] = my_flop[cur_flop_idx].cards[i];
    }
    cur_flop_idx++;
    if (cur_flop_idx == (int) my_flop.size()) {
      cur_flop_idx = 0;
    }
    sum_bc = num_cards_flop;
  } else {
    //no public flop assignment, just copy the existing cards over
    for (int i = 0; i <= state.round; i++)
      sum_bc += game->numBoardCards[i];
    for (int i = 0; i < sum_bc; i++)
      board.cards[i] = state.boardCards[i];
  }

  if (sum_bc < HOLDEM_MAX_CARDS) {
    HoldemDeck deck{board};
    deck.Shuffle();
    for (int bs = sum_bc; bs < HOLDEM_MAX_BOARD; bs++)
      board.cards[bs] = deck.cards_[bs];
  }
}

void AddCardTToCardset(Cardset *c, uint8_t card) {
  addCardToCardset(c, suitOfCard(card, 4), rankOfCard(card, 4));
}

void AddBoardToCardSetByRound(Cardset *c, Board_t *board, int round) {
  switch (round) {
    case 1 : {
      AddCardTToCardset(c, board->cards[0]);
      AddCardTToCardset(c, board->cards[1]);
      AddCardTToCardset(c, board->cards[2]);
      break;
    }
    case 2 : {
      AddCardTToCardset(c, board->cards[3]);
      break;
    }
    case 3 : {
      AddCardTToCardset(c, board->cards[4]);
      break;
    }
    default:logger::critical("unsupported round number. %d", round);
  }
}
void EnrichCardSetToRound(Cardset *c, Card_t high, Card_t low, Board_t *board, int round) {
  AddCardTToCardset(c, high);
  AddCardTToCardset(c, low);
  for (int r = 1; r <= round; r++) {
    AddBoardToCardSetByRound(c, board, r);
  }
}
void EnrichCardSetToRound(Cardset *c, Hand_t *hand, Board_t *board, int round) {
  EnrichCardSetToRound(c, hand->high_card, hand->low_card, board, round);
}
Colex ComputeColexFromAllCards(Card_t high, Card_t low, Board_t &board, int round) {
  auto c = emptyCardset();
  EnrichCardSetToRound(&c, high, low, &board, round);
  return ComputeColex(Canonize(c.cards));
}
Cardset BoardToCardSet(uint8_t *board, int num_dealt_cards) {
  auto cardset = emptyCardset();
  for (int i = 0; i < num_dealt_cards; i++) {
    AddCardTToCardset(&cardset, board[i]);
  }
  return cardset;
}
//(51, 50) = 1325, (1,0) = 0. high * (high - 1) / 2 + low
VectorIndex ToVectorIndex(Card_t high, Card_t low) {
  if (low > high) {
    auto temp = high;
    high = low;
    low = temp;
  }
  VectorIndex v = (high * high - high) / 2 + low;
  if (v > 1325){
    logger::critical("vector index out of bound %d | high %d | low %d", v, high, low);
  }
  return v;
}
//(51, 50) = 1325, (1,0) = 0, reverse
std::pair<Card_t, Card_t> FromVectorIndex(VectorIndex vector_idx) {
  if (vector_idx > 1325)
    logger::critical("from vector index out of bound %d", vector_idx);
  Card_t high = round(sqrt(vector_idx * 2 + 2)); //the plus two is a hack. (51,0)->1275, if not with +2, high will be 50
  Card_t low = vector_idx - ((high * high - high) / 2);
  return std::make_pair(high, low);
}
bool VectorIdxClash(VectorIndex lhs, VectorIndex rhs) {
  auto hl_lhs = FromVectorIndex(lhs);
  auto hl_rhs = FromVectorIndex(rhs);
  //gaurantee no crash
  if (hl_lhs.first < hl_rhs.second) return false;
  if (hl_lhs.second > hl_rhs.first) return false;
  //check crash
  if (hl_lhs.first == hl_rhs.first) return true;
  if (hl_lhs.first == hl_rhs.second) return true;
  if (hl_lhs.second == hl_rhs.first) return true;
  return hl_lhs.second == hl_rhs.second;
}
std::string VectorIdxToString(VectorIndex i) {
  auto high_low = FromVectorIndex(i);
  auto c1 = emptyCardset();
  AddCardTToCardset(&c1, high_low.first);
  AddCardTToCardset(&c1, high_low.second);
  return CardsToString(c1.cards);
}

bool VectorIdxClashCard(VectorIndex vid, Card_t c) {
  auto high_low = FromVectorIndex(vid);
  return (high_low.first == c || high_low.second == c);
}

std::set<Colex> GetCannoSetByBoard(Board_t *board, int round) {
  std::set<Colex> colex_set;
  for (Card_t low = 0; low < HOLDEM_MAX_CARDS - 1; low++) {
    for (Card_t high = low + 1; high < HOLDEM_MAX_CARDS; high++) {
      auto hand = Hand_t{high, low};
      if (board->HandCrash(hand)) continue;
      auto colex = ComputeColexFromAllCards(high, low, *board, round);
      colex_set.insert(colex);
    }
  }
  return colex_set;
}
