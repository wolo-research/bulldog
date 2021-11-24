//
//  Created by Ivan Mejia on 12/24/16.
//
// MIT License
//
// Copyright (c) 2016 ivmeroLabs.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

extern "C"
{
#ifdef WIN32
#include <Rpc.h>
#else
#include <uuid/uuid.h>
#endif
}

#include "std_micro_service.hpp"
#include "bulldog_controller.hpp"
#include "server_constants.h"

extern "C" {
#include "bulldog/game.h"
}

using namespace web;
using namespace http;

static std::string newUUID() {
#ifdef WIN32
  UUID uuid;
    UuidCreate ( &uuid );

    unsigned char * str;
    UuidToStringA ( &uuid, &str );

    std::string s( ( char* ) str );

    RpcStringFreeA ( &str );
#else
  uuid_t uuid;
  uuid_generate_random(uuid);
  char s[37];
  uuid_unparse(uuid, s);
#endif
  return s;
}

void BulldogController::initRestOpHandlers() {
  _listener.support(methods::GET, std::bind(&BulldogController::handleGet, this, std::placeholders::_1));
  _listener.support(methods::PUT, std::bind(&BulldogController::handlePut, this, std::placeholders::_1));
  _listener.support(methods::POST, std::bind(&BulldogController::handlePost, this, std::placeholders::_1));
  _listener.support(methods::DEL, std::bind(&BulldogController::handleDelete, this, std::placeholders::_1));
  _listener.support(methods::PATCH, std::bind(&BulldogController::handlePatch, this, std::placeholders::_1));
}

std::string WstringToString(const std::wstring &str) {// wstring转string
  unsigned len = str.size() * 4;
  setlocale(LC_CTYPE, "");
  char *p = new char[len];
  wcstombs(p, str.c_str(), len);
  std::string str1(p);
  delete[] p;
  return str1;
}

void BulldogController::handleGet(http_request request) {
  auto path = requestPath(request);
  auto http_vars = uri::split_query(request.request_uri().query());
  if (!path.empty()) {
    /*
      * PING
      */
    if (path[0] == PATH_PING) {
      logger::debug("SERVER GET: %s", request.request_uri().to_string());
      request.reply(status_codes::OK, "bulldog is live and well");
      return;
    }

    /*
      * HEARTBEAT
      */
    if (path[0] == PATH_HEARTBEAT) {
      if (http_vars.find(PARAM_SESSION_ID) == http_vars.end()
          || sessions_.find(http_vars[PARAM_SESSION_ID]) == sessions_.end()) {
        //not found
        request.reply(status_codes::BadRequest, "invalid session id");
        return;
      }
      auto session_id = http_vars[PARAM_SESSION_ID];
      sessions_[session_id].last_access_timestamp_ = std::chrono::steady_clock::now();
      request.reply(status_codes::OK);
      return;
    }

    /*
     * GET_ACTION
     */
    if (path[0] == PATH_GET_ACTION) {
      logger::debug("SERVER GET: %s", request.request_uri().to_string());
      //check required parameters
      std::string required[3] = {PARAM_SESSION_ID, PARAM_MATCHSTATE, "timeout"};
      for (auto &i : required) {
        if (http_vars.find(i) == http_vars.end()) {
          //not found
          request.reply(status_codes::BadRequest, "please provide all necessary parameters");
          return;
        }
      }
      auto session_id = http_vars[PARAM_SESSION_ID];
      if (sessions_.find(session_id) == sessions_.end()) {
        request.reply(status_codes::BadRequest, "invalid session id");
        return;
      }

      int timeout = std::atoi(http_vars["timeout"].c_str());
      auto matchstate_first_decoded = web::uri::decode(http_vars[PARAM_MATCHSTATE]);
      auto matchstate_str = web::uri::decode(matchstate_first_decoded);
      logger::debug("requested for action from matchstate %s", matchstate_str);

      auto engine = sessions_[session_id].engine_;
      //first do translation
      MatchState normalized_match_state;
      auto translate_result = engine->TranslateToNormState(matchstate_str, normalized_match_state);
      if (translate_result == MATCH_STATE_PARSING_FAILURE) {
        request.reply(status_codes::BadRequest, "invalid matchstate");
        return;
      }
      // then get action
      Action r_action;
      auto get_action_result = engine->GetActionBySession(normalized_match_state, r_action, timeout);
      if (get_action_result == GET_ACTION_SUCCESS) {
        auto response = json::value::object();
        response["type"] = json::value::number(r_action.type);
        response["size"] = json::value::number(r_action.size);
        request.reply(status_codes::OK, response);
        logger::debug("SERVER: return action to sdk %d", actionToCode(&r_action));
        return;
      } else {
        request.reply(status_codes::BadRequest, "get action failed");
      }
    }

    //else
    request.reply(status_codes::NotFound);
  }else{
    logger::debug("SERVER GET: %s", request.request_uri().to_string());
  }
}

/*
 * engine game is for the engine, 50/100 blinds
 * server game_ is the same.
 * while session game should change with session, especially need to update the blind.
 */

void BulldogController::handlePost(http_request request) {
  logger::debug("SERVER POST: %s", request.request_uri().to_string());
  auto path = requestPath(request);
  auto http_vars = uri::split_query(request.request_uri().query());

  if (!path.empty()) {
    if (path[0] == PATH_EVALSTATE) {
      /*
       * EVAL STATE WITH HOLE CARDS ONLY
       */

      //validate request
      if(http_vars.find(PARAM_SESSION_ID) == http_vars.end()
          || sessions_.find(http_vars[PARAM_SESSION_ID]) == sessions_.end()) {
        request.reply(status_codes::BadRequest, "invalid session id");
        return;
      }
      if(http_vars.find(PARAM_MATCHSTATE) == http_vars.end()){
        request.reply(status_codes::BadRequest, "missing matchstate");
        return;
      }

      auto session_id = http_vars[PARAM_SESSION_ID];
      auto engine = sessions_[session_id].engine_;
      auto matchstate_first_decoded = web::uri::decode(http_vars[PARAM_MATCHSTATE]);
      auto matchstate_str = web::uri::decode(matchstate_first_decoded);
      MatchState normalized_match_state;
      auto translate_result = engine->TranslateToNormState(matchstate_str, normalized_match_state);
      if (translate_result == MATCH_STATE_PARSING_FAILURE) {
        request.reply(status_codes::BadRequest, "invalid matchstate");
        return;
      }
      engine->EvalShowdown(normalized_match_state);
      request.reply(status_codes::OK);
      return;
    }

    /*
   * REQUEST TABLE SESSION
   */
    if (path[0] == PATH_TABLESESSION) {
      /*
       * DELETE IDLE SESSIONS
       */
      auto now = std::chrono::steady_clock::now();
      for (auto iter = sessions_.begin(); iter != sessions_.end();) {
        auto time_idle =
            std::chrono::duration_cast<std::chrono::seconds>(now - iter->second.last_access_timestamp_).count();
        if (time_idle > 10) {
          delete iter->second.engine_;
          logger::info("SERVER deleted %s (idle overtime)", iter->first);
          sessions_.erase(iter++);
        } else {
          ++iter;
        }
      }

      /*
       * CREATE_TABLE SESSION
       */
      //check if server already at max capacity
      if (sessions_.size() >= max_capacity_) {
        request.reply(status_codes::ServiceUnavailable, "sorry we are at max capacity, try again later");
        return;
      }
      //check legitimacy
      auto game = http_vars[PARAM_GAME];
      auto table = http_vars[PARAM_TABLE];

      auto session_id = newUUID();
      mutex_.lock();
      sessions_[session_id].engine_ = new Engine(default_engine_conf_.c_str(),
                                                 &default_normalized_game_,
                                                 bucket_pool_,
                                                 blueprint_pool_);
      sessions_[session_id].engine_->engine_name_ += "_" + session_id.substr(0, 8);
      sessions_[session_id].last_access_timestamp_ = std::chrono::steady_clock::now();
      TableContext session;
      session.session_game = default_normalized_game_;
      session.table_name_ = table;
      if (sessions_[session_id].engine_->SetTableContext(session) != NEW_SESSION_SUCCESS) {
        logger::error("SERVER: create session failed");
        request.reply(status_codes::BadRequest, "can not create the session");
      }
      mutex_.unlock();
      logger::debug("SERVER: created session id = %s", session_id);

      auto response = json::value::object();
      response["engine"] = json::value::string(sessions_[session_id].engine_->GetName());
      response[PARAM_SESSION_ID] = json::value::string(session_id);
      request.reply(status_codes::Created, response);
      return;
    }
    /*
     * new hand and end hand
     */
    if (path[0] == PATH_NEW_HAND) {
      if (http_vars.find(PARAM_SESSION_ID) == http_vars.end()
          || sessions_.find(http_vars[PARAM_SESSION_ID]) == sessions_.end()) {
        //not found
        request.reply(status_codes::BadRequest, "invalid session id");
        return;
      }
      auto session_id = http_vars[PARAM_SESSION_ID];
      sessions_[session_id].engine_->RefreshEngineState();
      request.reply(status_codes::OK, "set new hand success");
      return;
    }

    if (path[0] == PATH_END_HAND) {
      if (http_vars.find(PARAM_SESSION_ID) == http_vars.end()
          || sessions_.find(http_vars[PARAM_SESSION_ID]) == sessions_.end()) {
        //not found
        request.reply(status_codes::BadRequest, "invalid session id");
        return;
      }
      request.reply(status_codes::OK, "set new hand success");
      return;
    }

    if (path[0] == PATH_EDITSESSION) {
      //check required parameters
      std::string required[3] = {PARAM_SESSION_ID, PARAM_SMALLBLIND, PARAM_BIGBLIND};
      for (auto &i : required) {
        if (http_vars.find(i) == http_vars.end()) {
          //not found
          request.reply(status_codes::BadRequest, "please provide all necessary parameters");
          return;
        }
      }

      auto matchstate_str = web::uri::decode(http_vars[PARAM_MATCHSTATE]);
      int sb = std::atoi(http_vars[PARAM_SMALLBLIND].c_str());
      int bb = std::atoi(http_vars[PARAM_BIGBLIND].c_str());
      logger::debug("requested to change blind [sb %d] [bb %d]", sb, bb);
      if (sb == 0 || bb == 0) {
        request.reply(status_codes::BadRequest, "please provide valid blinds > 0");
        return;
      }

      auto session_id = http_vars[PARAM_SESSION_ID];
      if (sessions_.find(session_id) != sessions_.end()) {
        auto engine = sessions_[session_id].engine_;
        int bb_pos = BigBlindPosition(&engine->table_context_.session_game);
        engine->table_context_.session_game.blind[bb_pos] = bb;
        engine->table_context_.session_game.blind[1 - bb_pos] = sb;
        sessions_[session_id].last_access_timestamp_ = std::chrono::steady_clock::now();
        request.reply(status_codes::OK, "change blind success");
      } else {
        //session id not found
        request.reply(status_codes::BadRequest, "invalid session id");
        return;
      };
    }
  }
  //else
  request.reply(status_codes::NotFound);
}

void BulldogController::handleDelete(http_request request) {
  logger::debug("SERVER DELETE: %s", request.request_uri().to_string());
  auto path = requestPath(request);
  auto http_vars = uri::split_query(request.request_uri().query());
  auto session_id = http_vars[PARAM_SESSION_ID];
  if (!path.empty()) {
    if (path[0] == PATH_TABLESESSION) {
      if (http_vars.find(PARAM_SESSION_ID) == http_vars.end()
          || sessions_.find(http_vars[PARAM_SESSION_ID]) == sessions_.end()) {
        //not found
        request.reply(status_codes::BadRequest, "invalid session id");
        return;
      }
      mutex_.lock();
      delete sessions_[session_id].engine_;
      sessions_.erase(session_id);
      mutex_.unlock();

      logger::info("deleted %s", session_id);
      request.reply(status_codes::OK, "deleted table session");
      return;
    }
  }
  request.reply(status_codes::NotFound);
}

void BulldogController::handlePatch(http_request request) {
  request.reply(status_codes::NotImplemented, responseNotImpl(methods::PATCH));
}
void BulldogController::handlePut(http_request request) {
  request.reply(status_codes::NotImplemented, responseNotImpl(methods::PUT));
}
void BulldogController::handleHead(http_request request) {
  request.reply(status_codes::NotImplemented, responseNotImpl(methods::HEAD));
}
void BulldogController::handleOptions(http_request request) {
  request.reply(status_codes::NotImplemented, responseNotImpl(methods::OPTIONS));
}
void BulldogController::handleTrace(http_request request) {
  request.reply(status_codes::NotImplemented, responseNotImpl(methods::TRCE));
}
void BulldogController::handleConnect(http_request request) {
  request.reply(status_codes::NotImplemented, responseNotImpl(methods::CONNECT));
}
void BulldogController::handleMerge(http_request request) {
  request.reply(status_codes::NotImplemented, responseNotImpl(methods::MERGE));
}

json::value BulldogController::responseNotImpl(const http::method &method) {
  auto response = json::value::object();
  response["serviceName"] = json::value::string("Bulldog");
  response["http_method"] = json::value::string(method);
  return response;
}

bool BulldogController::LoadDefault(std::string engine_file_name,
                                    std::string game_conf) {
  //Initiate Game
  Game *game = nullptr;;
  std::filesystem::path game_dir(BULLDOG_DIR_CFG_GAME);
  FILE *file = fopen((game_dir / game_conf).c_str(), "r");
  if (file == nullptr) {
    logger::critical("Failed to find file %s", (game_dir / game_conf));
  }
  game = readGame(file);
  if (game == nullptr) {
    logger::critical("Failed to read content of game file %s", (game_dir / game_conf));
  }
  //must be stack aware
  game->use_state_stack = 1;
  default_normalized_game_ = *game;
  default_engine_conf_ = engine_file_name;

  //configure default blueprint
  std::filesystem::path eng_dir(BULLDOG_DIR_CFG_ENG);
  std::ifstream blueprint_f(eng_dir / default_engine_conf_);
  web::json::value data;
  if (blueprint_f.good()) {
    std::stringstream buffer;
    buffer << blueprint_f.rdbuf();
    data = web::json::value::parse(buffer);
    blueprint_f.close();
  } else {
    logger::error("unable to open file %s", eng_dir / default_engine_conf_);
  }

  if (data.has_field("blueprint")) {
    auto blueprint_conf = data.at("blueprint");
    if (!blueprint_conf.has_field("strategy_prefix")) {
      logger::critical("cannot read blueprint from %s || blueprint prefix", default_engine_conf_);
    }
    bool disk_look_up = false;
    if (blueprint_conf.has_field("disk")) {
      disk_look_up = blueprint_conf.at("disk").as_bool();
    }
    //iteratively load all the blueprint
    auto pool = blueprint_conf.at("strategy_prefix").as_array();
    for (auto &i : pool) {
      const std::string &name = i.as_string();
      auto *blueprint_i = new Strategy();
      logger::debug("Loading blueprint %s [disk %d]", name, disk_look_up);
      LoadAG(blueprint_i, name, bucket_pool_, nullptr);
      LoadStrategy(blueprint_i, STRATEGY_ZIPAVG, name, disk_look_up);
      //the blueprint is never stack aware. dont use compatible
      if (!Equal(&default_normalized_game_, &blueprint_i->ag_->game_))
        logger::critical("engine game != blueprint game");
      blueprint_pool_->AddStrategy(blueprint_i);
    }
  }

  return true;
}

bool BulldogController::Cleanup() {
  return true;
}
