//
// Created by Carmen C on 21/2/2020.
//
#include "slumbot_connector.hpp"
#include <bulldog/game.h>

#include <cstdlib>
#include <cpprest/http_client.h>
#include <cpprest/filestream.h>
#include <bulldog/logger.hpp>
#include <sys/time.h>

//cpprest namespaces
//using namespace utility;                    // Common utilities like string conversions
//using namespace web;                        // Common features like URIs.
//using namespace web::http;                  // Common HTTP functionality
//using namespace web::http::client;          // HTTP client features
//using namespace concurrency::streams;       // Asynchronous streams

void from_json(const web::json::value &j, sSlumbotMatchState *s) {
  s->p1_ = j.has_field("p1") ? j.at("p1").as_integer() : 0;
  s->holes_ = j.has_field("holes") ? j.at("holes").as_string() : "";
  s->board_ = j.has_field("board") ? j.at("board").as_string() : "";
  s->action_ = j.has_field("action") ? j.at("action").as_string() : "";
  s->ps_ = j.has_field("ps") ? j.at("ps").as_integer() : 0;
  s->ourb_ = j.has_field("ourb") ? j.at("ourb").as_integer() : 0;
  s->oppb_ = j.has_field("oppb") ? j.at("oppb").as_integer() : 0;
  s->minb_ = j.has_field("minb") ? j.at("minb").as_integer() : 0;
  s->maxb_ = j.has_field("maxb") ? j.at("maxb").as_integer() : 0;

  //game ends state
  s->oppholes_ = j.has_field("oppholes") ? j.at("oppholes").as_string() : "";
  s->sd_ = j.has_field("sd") ? j.at("sd").as_integer() : 0;
  s->outcome_ = j.has_field("outcome") ? j.at("outcome").as_integer() : -1;
  s->stotal_ = j.has_field("stotal") ? j.at("stotal").as_integer() : 0;
  s->shands_ = j.has_field("shands") ? j.at("shands").as_integer() : 0;

  //account state values
  s->ltotal_ = j.has_field("ltotal") ? j.at("ltotal").as_integer() : 0;
  s->lconf_ = j.has_field("lconf") ? j.at("lconf").as_integer() : 0;
  s->lbtotal_ = j.has_field("lbtotal") ? j.at("lbtotal").as_integer() : 0;
  s->lbconf_ = j.has_field("lbconf") ? j.at("lbconf").as_integer() : 0;
  s->lhands_ = j.has_field("lhands") ? j.at("lhands").as_integer() : 0;
  s->sdtotal_ = j.has_field("sdtotal") ? j.at("sdtotal").as_integer() : 0;
  s->sdconf_ = j.has_field("sdconf") ? j.at("sdconf").as_integer() : 0;
  s->sdhands_ = j.has_field("sdhands") ? j.at("sdhands").as_integer() : 0;
  s->blbsdtotal_ = j.has_field("blbsdtotal") ? j.at("blbsdtotal").as_integer() : 0;
  s->blbsdconf_ = j.has_field("blbsdconf") ? j.at("blbsdconf").as_integer() : 0;
  s->blbsdhands_ = j.has_field("blbsdhands") ? j.at("blbsdhands").as_integer() : 0;
  s->clbsdtotal_ = j.has_field("clbsdtotal") ? j.at("clbsdtotal").as_integer() : 0;
  s->clbsdconf_ = j.has_field("lbsdconf") ? j.at("lbsdconf").as_integer() : 0;
  s->clbsdhands_ = j.has_field("clbsdhands") ? j.at("clbsdhands").as_integer() : 0;

  s->hip_ = j.has_field("hip") ? j.at("hip").as_integer() : 0; //always initialize to 0
};

SlumbotConnector::SlumbotConnector(const std::vector<std::string> &params) {
  username_ = params[0].c_str();
  password_ = params[1].c_str();
  iter_ = (unsigned int) std::strtoul(params[2].c_str(), nullptr, 0);
  iter_rec_ = (unsigned int) std::strtoul(params[2].c_str(), nullptr, 0);
  slumbot_match_state_ = new sSlumbotMatchState();
}

SlumbotConnector::~SlumbotConnector() {
  delete slumbot_match_state_;
}

int SlumbotConnector::connect() {
  //Initialize sessionId
  struct timeval tp{};
  gettimeofday(&tp, nullptr);
  long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
  std::srand(time(nullptr));
  int r = std::rand() % 1000000;
  long int sessionid = (ms * 1000000 + r);
  sid_ = sessionid;
  logger::trace("session id: " + std::to_string(sessionid));

  //Login
  auto requestJson = web::http::client::http_client(U(base_url_))
      .request(web::http::methods::GET,
               web::http::uri_builder(U(cgi_uri_))
                   .append_query(U("type"), U("login"))
                   .append_query(U("username"), U(username_))
                   .append_query(U("pw"), U(password_))
                   .to_string())

          // GetAvg the response.
      .then([=](const web::http::http_response& response) {
        if (response.status_code() != 200) {
          logger::error("login returned " + std::to_string(response.status_code()));
        }
        return response.extract_json(true);
      })
      .then([=](const web::json::value &jsonObject) {
        logger::debug("login returned:" + jsonObject.serialize());
        outcome_flag_ = jsonObject.has_field("outcome");
        from_json(jsonObject, slumbot_match_state_);
      });

  // Wait for the concurrent tasks to finish.
  try {
    requestJson.wait();
  } catch (const std::exception &e) {
    logger::error(e.what());
    return EXIT_FAILURE;
  }
  return EXIT_FAILURE;
}

int SlumbotConnector::send() {
  struct timeval tp{};
  gettimeofday(&tp, nullptr);
  long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
  ++slumbot_match_state_->ai_; //increment action count
  logger::trace(
      "action=" + action_str_ +
          "&sid=" + std::to_string(sid_) +
          "&un=" + username_ +
          "&ai=" + std::to_string(slumbot_match_state_->ai_) +
          "&_=" + std::to_string(ms));
  auto requestJson = web::http::client::http_client(U(base_url_))
      .request(web::http::methods::GET,
               web::http::uri_builder(U(cgi_uri_))
                   .append_query(U("type"), U("play"))
                   .append_query(U("action"), U(action_str_))
                   .append_query(U("sid"), U(sid_))
                   .append_query(U("un"), U(username_))
                   .append_query(U("ai"), U(slumbot_match_state_->ai_))
                   .append_query(U("_"), U(ms))
                   .to_string())
      .then([](const web::http::http_response &response) {
        if (response.to_string().find("Error") != std::string::npos) {
          throw std::runtime_error("slumbot_connector::send action returned Error");
        }
        return response.extract_json(true);
      }).then([&](const web::json::value &jsonObject) {
        logger::trace("slumbot_connector::send action returned:" + jsonObject.serialize());
        action_str_ = "";
        outcome_flag_ = jsonObject.has_field("outcome");
        from_json(jsonObject, slumbot_match_state_);
      });
  try {
    requestJson.wait();
    return EXIT_SUCCESS;
  } catch (const std::exception &e) {
    logger::error(e.what());
    return EXIT_FAILURE;
  }
}

bool SlumbotConnector::get() {
  //Determine it its time for next hand
  if (slumbot_match_state_->hip_ == 0 && outcome_flag_) {
    //outcome updated by send, continue
    outcome_flag_ = false;
    return true;
  } else if (slumbot_match_state_->hip_ == 0 && iter_ > 0) {
    logger::info("******ITERATION: " + std::to_string(iter_) + "******");
    logger::trace("requesting for next hand from slumbot");
    struct timeval tp{};
    gettimeofday(&tp, nullptr);
    long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
    slumbot_match_state_->ai_ = 0;
    auto requestJson = web::http::client::http_client(U(base_url_))
        .request(web::http::methods::GET,
                 web::http::uri_builder(U(cgi_uri_))
                     .append_query(U("type"), U("play"))
                     .append_query(U("action"), U("nh"))
                     .append_query(U("sid"), U(sid_))
                     .append_query(U("un"), U(username_))
                     .append_query(U("ai"), U(slumbot_match_state_->ai_))
                     .append_query(U("_"), U(ms))
                     .to_string())

            // GetAvg the response.
        .then([](const web::http::http_response &response) {
          if (response.to_string().find("Error") != std::string::npos) {
            throw std::runtime_error("slumbot_connector::get next_hand returned Error");
          }
          return response.extract_json(true);
        })
        .then([=](const web::json::value &jsonObject) {
          logger::trace("slumbot_connector::get next_hand returned:" + jsonObject.serialize());
          from_json(jsonObject, slumbot_match_state_);
        });
    // Wait for the concurrent tasks to finish.
    try {
      requestJson.wait();
    } catch (const std::exception &e) {
      logger::error(e.what());
      return false;
    }
    --iter_;
    return true;
  } else if (slumbot_match_state_->hip_ == 1) {
    //all other cases, previous communication with slumbot has updated state
    logger::trace("matchstate should have been updated by previous send, skip directly to parse");
    return true;
  }

  if (iter_ == 0) {
    logger::trace("finished all iterations");
    return false;
  }

  logger::error("case not handled yet");
  return false;
}

int SlumbotConnector::parse(const Game *game, MatchState *state) {
  if (slumbot_match_state_->p1_ >= game->numPlayers) {
    logger::critical("viewing player recieved from slumbot is not compatible with game");
  }
  initState(game, iter_rec_-iter_, &state->state);

  //In slumbot, p1_=1 is the small blind
  //todo: should we move customizations such as below to a connector file?
  state->viewingPlayer = slumbot_match_state_->p1_ == 1 ? 0 : 1;
  std::string holes_str;
  if (state->viewingPlayer == 0) {
    holes_str += slumbot_match_state_->holes_;
    holes_str += "|";
    holes_str += slumbot_match_state_->oppholes_;
  } else {
    holes_str += slumbot_match_state_->oppholes_;
    holes_str += "|";
    holes_str += slumbot_match_state_->holes_;
  }
  std::string board_str;
  if (slumbot_match_state_->board_.empty()) {
    board_str = "";
  } else {
    board_str += "/";
    board_str += slumbot_match_state_->board_;
  }

  std::string acpc_format =
      "MATCHSTATE:" + std::to_string(state->viewingPlayer) + ":" + std::to_string(state->state.handId) +
          ":" + ":" + slumbot_match_state_->action_ + ":" + holes_str + board_str;
  logger::trace("slumbot_connector state in acpc_format: " + acpc_format);

  //use readMatchStatePlus you'd like to supply custom ReadBettingFunction
  int len = readMatchStatePlus(acpc_format.c_str(), game, state, bsbrReadBetting);

  char line[MAX_LINE_LEN];
  printMatchState(game, state, MAX_LINE_LEN, line);
  std::string line2(line);
  logger::trace("slumbot_connector state parsed: " + line2);

  if (len < 0) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

//build slumbot responses
int SlumbotConnector::build(const Game *game, Action *action, State *state) {
  //todo: maybe wrap this acpcstandard to slumbnot standard in a function? but all you need is the action
  action_str_ = "";
  if (action->type == a_call && slumbot_match_state_->oppb_ == slumbot_match_state_->ourb_) {
    action_str_ += 'k';
  } else if (action->type == a_raise) {
    action_str_ += 'b';
  } else {
    action_str_ += actionChars[action->type];
  }

  if (game->bettingType == noLimitBetting && action->type == a_raise) {
    long int action_size = actionTranslate_bsbg2bsbr(action, state, game);

    if (action_size < (slumbot_match_state_->minb_) || action_size > (slumbot_match_state_->maxb_)) {
      logger::critical("engine action size:" + std::to_string(action_size) + ", raise size must be in the range of "
                       + std::to_string(slumbot_match_state_->minb_) + " - "
                       + std::to_string(slumbot_match_state_->maxb_));
    }

    if (action_size < 0) {
      logger::critical("raise cannot be less than 0, " + std::to_string(action_size) + "recieved");
    }
    action_str_ += std::to_string(action_size);
  }

  logger::trace("slumbot_connector built action string " + action_str_);
  return EXIT_SUCCESS;
}