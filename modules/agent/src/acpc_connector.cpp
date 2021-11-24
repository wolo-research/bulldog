//
// Created by Carmen C on 21/2/2020.
//

#include <iostream>
#include <vector>
#include <bulldog/logger.hpp>
#include "acpc_connector.hpp"
extern "C" {
#include "bulldog/net.h"
}

/**
 *
 * @param hostname,port
 */
AcpcConnector::AcpcConnector(const std::vector<std::string> &params) {
  char *c = new char[params[0].size() + 1];
  std::copy(params[0].begin(), params[0].end(), c);
  c[params[0].size()] = '\0';
  _host = c;
  _port = (uint16_t) std::strtoul(params[1].c_str(), nullptr, 0);
}

AcpcConnector::~AcpcConnector() {
  delete[] _host;
}

int AcpcConnector::connect() {
  int sock = connectTo(_host, _port);
  if (sock < 0) {
    return EXIT_FAILURE;
  }
  toServer_ = fdopen(sock, "w");
  fromServer_ = fdopen(sock, "r");
  if (toServer_ != nullptr && fromServer_ != nullptr) {
    /* Send version string to dealer */
    if(fprintf(this->toServer_, "VERSION:%" PRIu32".%" PRIu32".%" PRIu32"\n", VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION)!=14){
      logger::error("could not send version to dealer");
      return EXIT_FAILURE;
    }
    fflush(this->toServer_);
    return EXIT_SUCCESS;
  }
  logger::error("could not get socket streams");
  return EXIT_FAILURE;
}

int AcpcConnector::send() {
  if(fwrite(this->feed_, sizeof(char), this->feed_len_, toServer_) != (size_t) this->feed_len_){
    logger::error("problem sending information to server");
    return EXIT_FAILURE;
  }
  fflush(toServer_);
  return EXIT_SUCCESS;
}

int AcpcConnector::parse(const Game *game, MatchState *match_state){
  //this is the same as readMatchState(this->feed_, game, match_state);
  this->feed_len_ = readMatchState(this->feed_, game, match_state);
  if (this->feed_len_ < 0) {
    logger::error("ERROR: Could not read match state [%s]\n", feed_);
  }
  return this->feed_len_;
}

bool AcpcConnector::get(){
  char* game_feed = fgets(this->feed_, MAX_LINE_LEN, fromServer_);
  /* Ignore comments */
  if (game_feed!= nullptr && ((game_feed[0] == '#') || (game_feed[0] == ';'))) {
    game_feed = fgets(this->feed_, MAX_LINE_LEN, fromServer_);
  }
  return game_feed != nullptr;
}

int AcpcConnector::build(const Game *game, Action *action, State *state) {
  /* Start building the response to the server by adding a colon
     * (guaranteed to fit because we read a new-game_feed in fgets)
     */
  this->feed_[this->feed_len_] = ':';
  ++this->feed_len_;

  int act_str_len = printAction(game, action, MAX_LINE_LEN - this->feed_len_ - 2,
                                &this->feed_[this->feed_len_]);

  if (act_str_len < 0) {
    logger::error("ERROR: Response too long after printing action\n");
    return EXIT_FAILURE;
  }

  this->feed_len_ += act_str_len;
  this->feed_[this->feed_len_] = '\r';
  ++this->feed_len_;
  this->feed_[this->feed_len_] = '\n';
  ++this->feed_len_;

  return EXIT_SUCCESS;
}

