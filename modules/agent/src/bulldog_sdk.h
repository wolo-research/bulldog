//
// Created by Isaac Zhang on 7/30/20.
//

#ifndef BULLDOG_MODULES_AGENT_SRC_BULLDOG_SDK_H_
#define BULLDOG_MODULES_AGENT_SRC_BULLDOG_SDK_H_

#include <string>
#include <cpprest/http_client.h>
//#include <bulldog/logger.hpp>
#include "server_constants.h"
extern "C" {
#include "../../core/include/bulldog/game.h"
}

using lLogger = void (*)(char* format, ...);
class BulldogSDK {
 public:
  utility::string_t endpoint_;
  utility::string_t session_id_;
  bool in_hand_ = false;
  void SetEndpoint(const utility::string_t &endpoint) {
    endpoint_ = endpoint;
  }
  bool SendHeartbeat(lLogger f) const;
  bool Ping(lLogger f) const;
  bool CreateTableSession(lLogger f, std::string game_conf, std::string string);
  bool DeleteTableSession(lLogger f) const;
  bool EvalState(lLogger f, Game *game, MatchState &matchstate) const;
  bool ChangeBlinds(lLogger f, int smallblind, int bigblind);
  bool NewHand(lLogger f);
  bool EndHand(lLogger f);
  bool GetAction(lLogger f, Game *game, MatchState &new_match_state, Action &r_action, int i) const;
  bool AutoSearchEndpoint(lLogger f);
};

#endif //BULLDOG_MODULES_AGENT_SRC_BULLDOG_SDK_H_
