//
// Created by Carmen C on 21/2/2020.
//

#ifndef BULLDOG_MODULES_AGENT_INCLUDE_ACPC_CONNECTOR_HPP_
#define BULLDOG_MODULES_AGENT_INCLUDE_ACPC_CONNECTOR_HPP_

#include "base_connector.hpp"
#include <vector>

class AcpcConnector : public BaseConnector{
 public:
  ~AcpcConnector();
  AcpcConnector(const std::vector<std::string> &params);
  int connect() override;
  int send() override;
  int parse(const Game *game, MatchState *match_state) override;
  int build(const Game *game, Action *action, State *state) override;
  bool get() override;
 private:
  char* _host;
  uint16_t _port;
  FILE *toServer_{};
  FILE *fromServer_{};
  char feed_[MAX_LINE_LEN]{};
  int feed_len_;
};

#endif //BULLDOG_MODULES_AGENT_INCLUDE_ACPC_CONNECTOR_HPP_
