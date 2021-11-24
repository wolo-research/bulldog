//
// Created by Isaac Zhang on 7/31/20.
//

#include "bulldog_sdk.h"

bool BulldogSDK::Ping(lLogger f) const {
  //Get session_id from server
  web::http::client::http_client_config cfg;
  cfg.set_timeout(std::chrono::seconds(2));
  web::http::client::http_client client(endpoint_, cfg);
  auto requestJson = client
      .request(web::http::methods::GET,
               web::http::uri_builder(PATH_PING)
                   .to_string())
      .then([&](const web::http::http_response &response) {
        f(const_cast<char *>("BULLDOG SDK: Ping() request return %d\n"), response.status_code());
        if (response.status_code() != web::http::status_codes::OK)
          throw std::runtime_error(utility::conversions::to_utf8string(response.to_string()));
      });
  //do the work
  try {
    f(const_cast<char *>("BULLDOG SDK: request to check heartbeat\n"));
    requestJson.wait();
    return true;
  }
  catch (const std::exception &e) {
    f(const_cast<char *>("BULLDOG SDK: %s\n"), e.what());
    return false;
  }
}

bool BulldogSDK::SendHeartbeat(lLogger f) const {
  //Get session_id from server
  web::http::client::http_client_config cfg;
  cfg.set_timeout(std::chrono::seconds(2));
  web::http::client::http_client client(endpoint_, cfg);
  auto requestJson = client
      .request(web::http::methods::GET,
               web::http::uri_builder(PATH_HEARTBEAT)
                   .append_query(PARAM_SESSION_ID, session_id_)
                   .to_string())
      .then([&](const web::http::http_response &response) {
        //f(const_cast<char *>("SDK: SendHeartbeat() request return %d\n"), response.status_code());
        if (response.status_code() != web::http::status_codes::OK)
          throw std::runtime_error(utility::conversions::to_utf8string(response.to_string()));
      });
  //do the work
  try {
    //f(const_cast<char *>("BULLDOG SDK: request to check heartbeat\n"));
    requestJson.wait();
    return true;
  }
  catch (const std::exception &e) {
    f(const_cast<char *>("BULLDOG SDK: %s\n"), e.what());
    return false;
  }
}

bool BulldogSDK::CreateTableSession(lLogger f, std::string game_conf, std::string table) {
  if (!session_id_.empty()) {
    f(const_cast<char *>("BULLDOG SDK: already have session id %s\n"), session_id_.c_str());
    return false;
  }

  //Get session_id from server
  auto requestJson = web::http::client::http_client(endpoint_)
      .request(web::http::methods::POST,
               web::http::uri_builder(PATH_TABLESESSION)
                   .append_query(PARAM_GAME,
                                 utility::conversions::to_string_t(game_conf)) //does not matter as it is standard
                   .append_query(PARAM_TABLE, utility::conversions::to_string_t(table))
                   .to_string())
      .then([&](const web::http::http_response &response) {
        f(const_cast<char *>("BULLDOG SDK: CreateTableSession retured %d\n"), response.status_code());
        if (response.status_code() == web::http::status_codes::Created) {
          auto json = response.extract_json().get();
          session_id_ = json.at(PARAM_SESSION_ID).as_string();
          f(const_cast<char *>("BULLDOG SDK: engine returns session_id = %s\n"), session_id_.c_str());
        } else if (response.status_code() == web::http::status_codes::BadRequest) {
          f(const_cast<char *>("BULLDOG SDK: bad request %s %s\n"), game_conf.c_str(), table.c_str());
          throw std::runtime_error(utility::conversions::to_utf8string(response.to_string()));
        } else if (response.status_code() == web::http::status_codes::ServiceUnavailable) {
          f(const_cast<char *>("BULLDOG SDK: service unavailable\n"));
          throw std::runtime_error(utility::conversions::to_utf8string(response.to_string()));
        }
      });
  try {
    f(const_cast<char *>("BULLDOG SDK: try creating table session\n"));
    requestJson.wait();
    return true;
  }
  catch (const std::exception &e) {
    f(const_cast<char *>("BULLDOG SDK: %s\n"), e.what());
    return false;
  }
}

bool BulldogSDK::DeleteTableSession(lLogger f) const {
  if (session_id_.empty()) {
    f(const_cast<char *>("BULLDOG SDK: not yet have session id\n"));
    return false;
  }

  //Get session_id from server
  auto requestJson = web::http::client::http_client(endpoint_)
      .request(web::http::methods::DEL,
               web::http::uri_builder(PATH_TABLESESSION)
                   .append_query(PARAM_SESSION_ID, session_id_) //does not matter as it is standard
                   .to_string())
      .then([&](const web::http::http_response &response) {
        f(const_cast<char *>("BULLDOG SDK: DeleteTableSession() retured %d\n"), response.status_code());
        switch (response.status_code()) {
          case web::http::status_codes::OK:break;
          case web::http::status_codes::NotFound://logger::debug("SDK: invalid path");
            throw std::runtime_error(utility::conversions::to_utf8string(response.to_string()));
          case web::http::status_codes::BadRequest://logger::debug("SDK: invalid session_id. bad request");
            throw std::runtime_error(utility::conversions::to_utf8string(response.to_string()));
        }
      });
  try {
    f(const_cast<char *>("BULLDOG SDK: try deleting table session\n"));
    requestJson.wait();
    return true;
  }
  catch (const std::exception &e) {
    f(const_cast<char *>("BULLDOG SDK: %s\n"), e.what());
    return false;
  }
}

bool BulldogSDK::NewHand(lLogger f) {
  if (in_hand_) {
    f(const_cast<char *>("BULLDOG SDK: should call EndHand after a hand\n"));
    return false;
  }
  //let the server node
  auto requestJson = web::http::client::http_client(endpoint_)
      .request(web::http::methods::POST,
               web::http::uri_builder(PATH_NEW_HAND)
                   .append_query(PARAM_SESSION_ID, session_id_)
                   .to_string())
      .then([&](const web::http::http_response &response) {
        f(const_cast<char *>("BULLDOG SDK: NewHand() retured %d\n"), response.status_code());
        switch (response.status_code()) {
          case web::http::status_codes::OK:break;
          case web::http::status_codes::NotFound://logger::debug("SDK: in valid path path");
            throw std::runtime_error(utility::conversions::to_utf8string(response.to_string()));
          default:
            throw std::runtime_error(utility::conversions::to_utf8string(response.to_string()));
        }
      });
  try {
    f(const_cast<char *>("BULLDOG SDK: try creating table session\n"));
    requestJson.wait();
    in_hand_ = true;
    return true;
  }
  catch (const std::exception &e) {
    f(const_cast<char *>("BULLDOG SDK: %s\n"), e.what());
    return false;
  }
}

bool BulldogSDK::EndHand(lLogger f) {
  if (!in_hand_) {
    f(const_cast<char *>("BULLDOG SDK: you are not in a hand right now\n"));
    return false;
  }

  auto requestJson = web::http::client::http_client(endpoint_)
      .request(web::http::methods::POST,
               web::http::uri_builder(PATH_END_HAND)
                   .append_query(PARAM_SESSION_ID, session_id_)
                   .to_string())
      .then([&](const web::http::http_response &response) {
        f(const_cast<char *>("BULLDOG SDK: EndHand() retured %d\n"), response.status_code());
        switch (response.status_code()) {
          case web::http::status_codes::OK:break;
          case web::http::status_codes::NotFound:;
            throw std::runtime_error("SDK: invalid path");
          default:throw std::runtime_error(utility::conversions::to_utf8string(response.to_string()));
        }
      });

  try {
    f(const_cast<char *>("BULLDOG SDK: try EndHand()\n"));
    requestJson.wait();
    in_hand_ = false;
    return true;
  }
  catch (const std::exception &e) {
    f(const_cast<char *>("BULLDOG SDK: %s\n"), e.what());
    return false;
  }
}

bool BulldogSDK::GetAction(lLogger f, Game *game, MatchState &new_match_state, Action &r_action, int timeout) const {
  f(const_cast<char *>("BULLDOG SDK - get action from sdk\n"));
  char match_state_str[MAX_LINE_LEN];
  printMatchState(game, &new_match_state, MAX_LINE_LEN, match_state_str);
  auto match_str = utility::conversions::to_string_t(match_state_str);

  auto requestJson = web::http::client::http_client(endpoint_)
      .request(web::http::methods::GET,
               web::http::uri_builder(PATH_GET_ACTION)
                   .append_query(U("session_id"), session_id_)
                   .append_query(U("matchstate"), web::uri::encode_data_string(match_str))
                   .append_query(U("timeout"), timeout)
                   .to_string())
      .then([=](const web::http::http_response &response) {
//        f(const_cast<char *>("BULLDOG SDK: returned %d\n"), response.status_code());
        if (response.status_code() == web::http::status_codes::OK) {
          return response.extract_json(true);
        }
        return pplx::task<web::json::value>();
      })
      .then([&](const web::json::value &jsonObject) {
        auto action_type = jsonObject.at(U("type")).as_integer();
        if (action_type == a_call) {
          r_action.type = a_call;
        } else if (action_type == a_raise) {
          r_action.type = a_raise;
        } else if (action_type == a_fold) {
          r_action.type = a_fold;
        } else {
          throw std::runtime_error("SDK:  no such action type");
        }
        r_action.size = jsonObject.at(U("size")).as_integer();
        f(const_cast<char *>("BULLDOG SDK: returned %c%d\n"), actionChars[r_action.type], r_action.size);
      });
  try {
    f(const_cast<char *>("BULLDOG SDK: try GetAction\n"));
    requestJson.wait();
    return true;
  }
  catch (const std::exception &e) {
    f(const_cast<char *>("BULLDOG SDK: %s\n"), e.what());
    return false;
  }
}
bool BulldogSDK::ChangeBlinds(lLogger f, int smallblind, int bigblind) {
  if (session_id_.empty()) {
    f(const_cast<char *>("BULLDOG SDK: should create session first\n"));
    return false;
  }

  //Get session_id from server
  auto requestJson = web::http::client::http_client(endpoint_)
      .request(web::http::methods::POST,
               web::http::uri_builder(PATH_EDITSESSION)
                   .append_query(PARAM_SESSION_ID, session_id_)
                   .append_query(PARAM_SMALLBLIND, utility::conversions::to_string_t(std::to_string(smallblind)))
                   .append_query(PARAM_BIGBLIND, utility::conversions::to_string_t(std::to_string(bigblind)))
                   .to_string())
      .then([&](const web::http::http_response &response) {
        f(const_cast<char *>("BULLDOG SDK: ChangeBlinds returned %d\n"), response.status_code());
        if (response.status_code() != web::http::status_codes::OK) {
          f(const_cast<char *>("BULLDOG SDK: bad request [sb %d]  [bb %d]\n"), smallblind, bigblind);
          throw std::runtime_error(utility::conversions::to_utf8string(response.to_string()));
        }
      });
  try {
    f(const_cast<char *>("BULLDOG SDK: try creating table session\n"));
    requestJson.wait();
    return true;
  }
  catch (const std::exception &e) {
    f(const_cast<char *>("BULLDOG SDK: %s\n", e.what()));
    return false;
  }
}

bool BulldogSDK::AutoSearchEndpoint(lLogger f) {
  for (const auto &e : ENDPOINT_CANDIDATES) {
    SetEndpoint(e);
    if (Ping(f))
      return true;
  }
  f(const_cast<char *>("BULLDOG SDK: cant find viable endpoint\n"));
  return false;
}

bool BulldogSDK::EvalState(lLogger f, Game *game, MatchState &matchstate) const {
  if (session_id_.empty()) {
    f(const_cast<char *>("BULLDOG SDK: should create session first\n"));
    return false;
  }

  //check that game has finished
  if(!stateFinished(&matchstate.state))
    return false;

  //check that all hole cards are present
  for(auto p=0; p < game->numPlayers; p++){
    for(auto c=0; c < game->numHoleCards; c++){
      if(matchstate.state.holeCards[p][c] == UNDEFINED_CARD) {
        f(const_cast<char *>("BULLDOG SDK: undefined card, skip sending to server\n"));
        return false;
      }
    }
  }

  char match_state_str[MAX_LINE_LEN];
  printMatchState(game, &matchstate, MAX_LINE_LEN, match_state_str);
  auto matchstate_str = utility::conversions::to_string_t(match_state_str);

  auto requestJson = web::http::client::http_client(endpoint_)
      .request(web::http::methods::POST,
               web::http::uri_builder(PATH_EVALSTATE)
                   .append_query(U("session_id"), session_id_)
                   .append_query(U("matchstate"), web::uri::encode_data_string(matchstate_str))
                   .to_string())
      .then([&](const web::http::http_response &response) {
        f(const_cast<char *>("BULLDOG SDK: returned %d\n"), response.status_code());
        if (response.status_code() != web::http::status_codes::OK) {
          throw std::runtime_error(utility::conversions::to_utf8string(response.to_string()));
        }
      });
  try {
    f(const_cast<char *>("BULLDOG SDK: try GetAction\n"));
    requestJson.wait();
    return true;
  }
  catch (const std::exception &e) {
    f(const_cast<char *>("BULLDOG SDK: %s\n"), e.what());
    return false;
  }
}
