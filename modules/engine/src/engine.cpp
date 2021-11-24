//
// Created by Isaac Zhang on 7/21/20.
//

#include <bulldog/engine.h>
#include <cpprest/json.h>

int Engine::SetTableContext(const TableContext &table_context) {
  //check game type.
  auto compatible_flag = CompatibleGame(&table_context.session_game, normalized_game_);
  if (compatible_flag == 0) {
    logger::error("engine game not compatible with session");
    return GAME_TYPE_NOT_COMPATIBLE;
  }
  table_context_ = table_context;
  return NEW_SESSION_SUCCESS;
}

Engine::Engine(const char *engine_conf_file, Game *game) {
  normalized_game_ = game;
  owning_pool_ = true;
  std::filesystem::path dir(BULLDOG_DIR_CFG_ENG);
  std::filesystem::path filename(engine_conf_file);
  std::ifstream file(dir / filename);

  web::json::value data;
  if (file.good()) {
    std::stringstream buffer;
    buffer << file.rdbuf();
    data = web::json::value::parse(buffer);
    file.close();
  } else {
    logger::error("    [ENGINE %s] : unable to open file %s", engine_name_, dir / filename);
  }
  //solver meta
  auto engine_conf_str = std::string(engine_conf_file);
  engine_name_ = engine_conf_str.substr(0, engine_conf_str.length() - 5); //remove the .json
  bucket_pool_ = new BucketPool();
  if (data.has_field("random_action")) {
    random_action_ = true;
    return;
  }
  /*
   * configure offline blueprint
   */
  if (data.has_field("blueprint")) {
    blueprint_pool_ = new StrategyPool();
    auto blueprint_conf = data.at("blueprint");
    if (!blueprint_conf.has_field("strategy_prefix")) {
      logger::critical("    [ENGINE %s] : please fill in the must-have options for configuration || blueprint prefix",
                       engine_name_);
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
      logger::debug("    [ENGINE %s] : loading blueprint %s [disk %d]", engine_name_, name, disk_look_up);
      LoadAG(blueprint_i, name, bucket_pool_, nullptr);
      LoadStrategy(blueprint_i, STRATEGY_ZIPAVG, name, disk_look_up);
      //the blueprint is never stack aware. dont use compatible
      if (!Equal(normalized_game_, &blueprint_i->ag_->game_))
        logger::critical("    [ENGINE %s] : engine game != blueprint game", engine_name_);
      blueprint_pool_->AddStrategy(blueprint_i);
    }
    /*
    * configure action
    */
    if (blueprint_conf.has_field("action_chooser")) {
      auto action_conf = blueprint_conf.at("action_chooser");
      default_action_chooser_ = new ActionChooser();
      default_action_chooser_->ConfFromJson(action_conf);
    }
  }

  /*
   * configure online playing.
   */
  if (data.has_field("subgame_solvers")) {
    auto solvers = data.at("subgame_solvers").as_array();
    sgs_size_ = solvers.size();
    subgame_solvers_ = new SubgameSolver[sgs_size_];
    for (int i = 0; i < sgs_size_; i++) {
      subgame_solvers_[i].ConfigWithJson(solvers[i].as_string().c_str(), bucket_pool_);
      logger::debug("    [ENGINE %s] : loaded sgs %d/%d - %s",
                    engine_name_,
                    i + 1,
                    sgs_size_,
                    subgame_solvers_[i].name_);
    }
  }

  /*
   * all games must turn on to use the state stack, for engine.
   * and they should be exactly the same game. in fact
   */
  for (int i = 0; i < sgs_size_; i++)
    if (normalized_game_->use_state_stack == 1)
      //for slumbot/acpc, the game_.use_state_stack == 0;
      subgame_solvers_[i].ag_builder_->game_->use_state_stack = 1;

  for (int i = 0; i < sgs_size_; i++) {
    if (normalized_game_->use_state_stack == 1) {
      if (CompatibleGame(normalized_game_, subgame_solvers_[i].ag_builder_->game_) == 0)
        logger::critical("    [ENGINE %s] : engine game def != sgs game betting type, in game type", engine_name_);
    } else {
      if (!Equal(normalized_game_, subgame_solvers_[i].ag_builder_->game_))
        logger::critical("    [ENGINE %s]  : engine game def != sgs game def.", engine_name_);
    }
  }

  //daemon depends on subgame solving
  if (data.has_field("daemon") && sgs_size_ > 0)
    is_daemon_engine = data.at("daemon").as_bool();

  logger::debug("    [ENGINE %s] : solving engine properly configured. ready to go.", engine_name_);
  RefreshEngineState();
}

Engine::Engine(const char *engine_conf_file, Game *game, BucketPool *bucket_pool, StrategyPool *blueprint_pool) {
  //fixed blueprint pool
  normalized_game_ = game;
  bucket_pool_ = bucket_pool;
  blueprint_pool_ = blueprint_pool;

  std::filesystem::path dir(BULLDOG_DIR_CFG_ENG);
  std::filesystem::path filename(engine_conf_file);
  std::ifstream file(dir / filename);

  web::json::value data;
  if (file.good()) {
    std::stringstream buffer;
    buffer << file.rdbuf();
    data = web::json::value::parse(buffer);
    file.close();
  } else {
    logger::error("    [ENGINE %s] : unable to open file %s", engine_name_, dir / filename);
  }
  //solver meta
  auto engine_conf_str = std::string(engine_conf_file);
  engine_name_ = engine_conf_str.substr(0, engine_conf_str.length() - 5); //remove the .json

  /*
    * configure action
    */
  if (data.has_field("random_action"))
    random_action_ = data.at("random_action").as_bool();

  if (data.has_field("blueprint")) {
    auto blueprint_conf = data.at("blueprint");
    if (blueprint_conf.has_field("action_chooser")) {
      auto action_conf = blueprint_conf.at("action_chooser");
      default_action_chooser_ = new ActionChooser();
      default_action_chooser_->ConfFromJson(action_conf);
    }
    if (blueprint_pool->pool_.size() == 0) {
      //uninitialized blueprint pool, initialize now
      if (!blueprint_conf.has_field("strategy_prefix")) {
        logger::critical("cannot read blueprint from %s || blueprint prefix", engine_conf_str);
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
        if (!Equal(normalized_game_, &blueprint_i->ag_->game_))
          logger::critical("engine game != blueprint game");
        blueprint_pool_->AddStrategy(blueprint_i);
      }
    }
  }

  /*
   * configure online playing.
   */
  if (data.has_field("subgame_solvers")) {
    auto solvers = data.at("subgame_solvers").as_array();
    sgs_size_ = solvers.size();
    subgame_solvers_ = new SubgameSolver[sgs_size_];
    for (int i = 0; i < sgs_size_; i++) {
      subgame_solvers_[i].ConfigWithJson(solvers[i].as_string().c_str(), bucket_pool_);
      logger::debug("    [ENGINE %s] : loaded sgs %d/%d - %s",
                    engine_name_,
                    i + 1,
                    sgs_size_,
                    subgame_solvers_[i].name_);
    }
  }

  /*
   * all games must turn on to use the state stack, for engine.
   * and they should be exactly the same game. in fact
   */
  for (int i = 0; i < sgs_size_; i++)
    if (normalized_game_->use_state_stack == 1)
      //for slumbot/acpc, the game_.use_state_stack == 0;
      subgame_solvers_[i].ag_builder_->game_->use_state_stack = 1;

  for (int i = 0; i < sgs_size_; i++) {
    if (normalized_game_->use_state_stack == 1) {
      if (CompatibleGame(normalized_game_, subgame_solvers_[i].ag_builder_->game_) == 0)
        logger::critical("    [ENGINE %s] : engine game def != sgs game betting type, in game type", engine_name_);
    } else {
      if (!Equal(normalized_game_, subgame_solvers_[i].ag_builder_->game_))
        logger::critical("    [ENGINE %s]  : engine game def != sgs game def.", engine_name_);
    }
  }

  //daemon depends on subgame solving
  if (data.has_field("daemon") && sgs_size_ > 0)
    is_daemon_engine = data.at("daemon").as_bool();

  logger::debug("    [ENGINE %s] : solving engine properly configured. ready to go.", engine_name_);
  RefreshEngineState();
}

/**
 * The engine will finally pick action from the last playbook.
 * - the first playbook is always the blueprint
 * - if new sgs succeed, add the new strategy to the new playbook. else, fall back to the last playbook
 * - get action from the playbooks, nested, from back to front
 *
 * @param new_match_state
 * @param timeout_ms
 * @param r_action
 * @return
 */
int Engine::GetAction(MatchState *new_match_state, Action &r_action, double timeout_ms) {
  if (random_action_) {
    return GetRandomAction(new_match_state, r_action);
  }
  //stop prior unfinished solving
  AsynStopDaemonSolving();
  AsynStopCFRSolving();
  logger::debug("===== [ENGINE %s]: handle get action request =====", engine_name_);
  if (!InputSanityCheck(new_match_state)) {
    return GET_ACTION_FAILURE;
  }
  if (!EngineStateStaleCheck(new_match_state)) {
    RefreshEngineState();   //force reset new Hand
  }
  /*
   * Pre GetAction
   */
  busy_flag_ = true;
  SimpleTimer timer;
  last_matchstate_ = *new_match_state;
  //assign a blueprint by stack
  if (playbook_stack_.empty()) {
    auto blueprint = blueprint_pool_->FindStrategy(new_match_state, normalized_game_);
    playbook_stack_.emplace_back(blueprint, default_action_chooser_, STRATEGY_ZIPAVG);
  }

  /*
   * CHECK if SGS should on
   */
  SubgameSolver *selected_sgs = nullptr;
  int cfr_return_code = -1;
  auto candidate_node_conditions = playbook_stack_.back().strategy_->FindSortedMatchedNodes(new_match_state->state);
  auto best_match = candidate_node_conditions.at(0);
  for (auto s = 0; s < sgs_size_; s++) {
    if (subgame_solvers_[s].CheckTriggerCondition(best_match)) {
      selected_sgs = &subgame_solvers_[s];
      /*
       * trying to build new playbook
       * todo:
       * do ASYN
       * - gen multi subgame
       * - solve and gen multiple playbook_pending
       * - select the best (indicator? step back) from the valid playbooks.
       */

      /*
       * step 1: build subgame
       */
      auto sgs_ag = new AbstractGame();
      //todo: the pot requirement is by the match state, not by the new root. change?
      int subgame_built_code = selected_sgs->BuildSubgame(sgs_ag,
                                                          playbook_stack_.back().strategy_,
                                                          best_match,
                                                          new_match_state);
      //do nothing if subgame is skipped
      if (subgame_built_code == SKIP_RESOLVING_SUBGAME) {
        logger::debug("    [ENGINE %s] : build subgame skipped [code %s]",
                      engine_name_,
                      SubgameBuiltCodeMap[subgame_built_code]);
        delete sgs_ag;
        break;
      }

      /*
       * step 2: nested range estimate
       */
      bool estimate_success;
      for (auto i = playbook_stack_.size() - 1; i >= 0; i--) {
        auto cursor_strategy = playbook_stack_.at(i).strategy_;
        STRATEGY_TYPE avg_type = playbook_stack_.at(i).playing_strategy_;
        estimate_success = cursor_strategy->EstimateNewAgReach(sgs_ag,
                                                               new_match_state,
                                                               avg_type);
        if (estimate_success) {
          break;
        } else {
          logger::warn("    [ENGINE %s] : range estimate fails. try next strategy", engine_name_);
        }
      }
      //delete sgs if all failed.
      if (!estimate_success) {
        logger::warn("range estimate fails for all history strategy. big problem. skip sgs");
        delete sgs_ag;
        break;
      }

      /*
       * step 3: asyn CFR subgame solving
       */
      sgs_strategy_stack_.push_back(new Strategy(sgs_ag));
      Strategy *&new_strategy = sgs_strategy_stack_.back();
      new_strategy->name_ = selected_sgs->name_;  //to make the destruction recognizable
      new_strategy->InitMemoryAndValue(selected_sgs->cfr_->cfr_param_.cfr_mode_);

      cfr_return_code = AsynStartCFRSolving(selected_sgs, new_strategy, timeout_ms - timer.GetLapseFromBegin());
      if (cfr_return_code < 0) {
        logger::error("    [ENGINE %s] : cfr solving error in subgame", engine_name_);
        return GET_ACTION_FAILURE;
      }

      /*
       * step 4: validate playbook pending
       */
      auto playbook_pending = PlayBook{sgs_strategy_stack_.back(),
                                       selected_sgs->action_chooser_,
                                       selected_sgs->playing_strategy_};
      if (ValidatePlaybook(playbook_pending, new_match_state, subgame_built_code)) {
        playbook_stack_.push_back(playbook_pending);
      } else {
        logger::warn("skipping this new strategy.");
      }
      //stop other sgs attempt
      break;
    }
  }

  /*
   * GET ACTION from the last playbook
   */
  auto pb_depth = playbook_stack_.size();
  for (auto pb_it = pb_depth - 1; pb_it >= 0; pb_it--) {
    //try to find from this playbook
    auto pb = playbook_stack_.at(pb_it);
    logger::debug("    [ENGINE %s] : get action from %d/%d playbooks (%s)",
                  engine_name_,
                  pb_depth - pb_it,
                  pb_depth,
                  pb.strategy_->name_);
    auto matches_pb = pb.strategy_->FindSortedMatchedNodes(new_match_state->state);
    for (auto m : matches_pb) {
      if (pb.strategy_->IsStrategyInitializedForMyHand(m.matched_node_,
                                                       pb.playing_strategy_,
                                                       new_match_state)) {
        m.Print("try to get action from this node : ");
        //also check if the path is strong when using blueprint. skip if not
        if (!IsNestedSgsStarted()) {
          //skip if at root node, i.e. first to act.
          if (m.matched_node_ != pb.strategy_->ag_->root_node_) {
            logger::debug("check reach on blueprint");
            std::array<sHandBelief, 2> new_base_reach;
            new_base_reach[0].CopyValue(&pb.strategy_->ag_->root_hand_belief_[0]);
            new_base_reach[1].CopyValue(&pb.strategy_->ag_->root_hand_belief_[1]);
            auto estimate_return_code = pb.strategy_->EstimateReachProbAtNode(new_match_state,
                                                                              m.matched_node_,
                                                                              new_base_reach,
                                                                              pb.playing_strategy_,
                                                                              DEFAULT_BAYESIAN_TRANSITION_FILTER);
            if (estimate_return_code != RANGE_ESTIMATE_SUCCESS) {
              m.matched_node_->PrintState("[ blueprint ] node not reachable");
              logger::warn("[ blueprint ] node not reachable [%s]", RangeEstimateCodeMap[estimate_return_code]);
              continue;
            }
          }
        }

        m.matched_node_->PrintState("getting action from node : ");
        pb.strategy_->PickAction(new_match_state,
                                 pb.action_chooser_,
                                 r_action,
                                 pb.playing_strategy_,
                                 m.matched_node_);
        logger::debug("    [ENGINE %s] : pick action [%c%d] with mode [%s]",
                      engine_name_,
                      actionChars[r_action.type],
                      r_action.size,
                      StrategyToNameMap[pb.playing_strategy_]);
        busy_flag_ = false;
        return GET_ACTION_SUCCESS;
      } else {
        m.matched_node_->PrintState(" skip virgin node in get_action_final: ");
      }
    }
  }
  logger::error("not a single action any history playbook valid. wrong wrong");
  AsynStartDaemonSolving(selected_sgs, cfr_return_code);
  return GET_ACTION_FAILURE;
}

int Engine::RefreshEngineState() {
  logger::debug("    [ENGINE %s] : ===== REFRESH STATE =====", engine_name_);
  logger::debug("    [ENGINE %s] : safe delete [%d playbooks] [%d new strategy]",
                engine_name_,
                playbook_stack_.size(),
                sgs_strategy_stack_.size());
  AsynStopDaemonSolving();
  AsynStopCFRSolving();
  busy_flag_ = false;
  playbook_stack_.clear();
  for (auto s : sgs_strategy_stack_)
    delete s;
  sgs_strategy_stack_.clear();
  return NEW_HAND_SUCCESS;
}

void Engine::EvalShowdown(MatchState &match_state) {
  //no need to analyze blueprint
  if (!IsNestedSgsStarted()) {
    logger::debug("    [ENGINE %s] : skip final state eval for blueprint-played games", engine_name_);
    return;
  }
  if (match_state.state.round == HOLDEM_ROUND_PREFLOP) {
    logger::debug("    [ENGINE %s] : skip final state eval for PREFLOP", engine_name_);
    return;
  }

  /*
   * pre printing
   */
  auto net_win = valueOfState(normalized_game_, &match_state.state, match_state.viewingPlayer);
  char match_state_line[1024];
  printMatchState(normalized_game_, &match_state, 1024, match_state_line);
  auto opp_pos = match_state.viewingPlayer - 1;
  auto opp_c1 = match_state.state.holeCards[opp_pos][0];
  auto opp_c2 = match_state.state.holeCards[opp_pos][1];
  if (opp_c1 >= HOLDEM_MAX_CARDS || opp_c2 >= HOLDEM_MAX_CARDS) {
    logger::debug("skip showdown eval: illegal hands for opponents %d %d", opp_c1, opp_c2);
    return;
  }
  auto opp_hand_vdx = ToVectorIndex(opp_c1, opp_c2);
  logger::debug(" [ENGINE %s] :  showdown eval [net %f] [opp hand %s] [%d playbooks]",
                engine_name_,
                net_win,
                VectorIdxToString(opp_hand_vdx),
                playbook_stack_.size());
  logger::debug(" [ENGINE %s] :  final state : %s", engine_name_, match_state_line);
  //eval all playbooks except blueprint
  for (auto pb_it = playbook_stack_.size() - 1; pb_it > 0; pb_it--) {
    //estimate by connonical group
    auto pb_strategy = playbook_stack_.at(pb_it).strategy_;
    auto strategy_root_round = pb_strategy->ag_->root_state_.round;

    //using the root state of the strategy.
    Board_t board{};
    BoardFromState(normalized_game_, &pb_strategy->ag_->root_state_, &board);

    //right now we only care about the grouped value of the real hand.
    auto real_b = pb_strategy->ag_->bucket_reader_.GetBucketWithHighLowBoard(
        opp_c1,
        opp_c2,
        &board,
        strategy_root_round);

    double real_b_canon_sum = 0.0;
    std::set<Bucket_t> seen_b;
    for (auto i = 0; i < FULL_HAND_BELIEF_SIZE; i++) {
      double belief = pb_strategy->ag_->root_hand_belief_[opp_pos].belief_[i];
      //skip 0 values and board (0 values)
      if (belief == 0.0)
        continue;

      auto high_low = FromVectorIndex(i);
      auto b = pb_strategy->ag_->bucket_reader_.GetBucketWithHighLowBoard(high_low.first,
                                                                          high_low.second,
                                                                          &board,
                                                                          strategy_root_round);
      if (b == real_b)
        real_b_canon_sum += belief;

      seen_b.insert(b);
    }

    //ready to print
    auto opp_hand_belief = pb_strategy->ag_->root_hand_belief_[opp_pos].belief_[opp_hand_vdx];
    pb_strategy->ag_->root_node_->PrintState("    strategy root : ");
    logger::debug(
        "    [strategy %s] [%s %f] [weight %f] [bucket_sum %f] [all_buckets_count %d]",
        pb_strategy->name_,
        opp_hand_belief > 0 ? "hit" : "miss",
        opp_hand_belief,
        real_b_canon_sum * seen_b.size(),
        real_b_canon_sum,
        seen_b.size());
  }
}

/**
 * do match state translation and get action and do backward translation accordingly
 */
int Engine::GetActionBySession(MatchState &normalized_match_state, Action &r_action, int timeout_ms) {
  if (busy_flag_) {
    logger::error("    [ENGINE %s] : still busy from the last request man");
    return GET_ACTION_FAILURE;
  }

  if (timeout_ms <= 1000) {
    logger::warn("    [ENGINE %s] : time_out %d is too short", engine_name_, timeout_ms);
    return GET_ACTION_FAILURE;
  }
  //leave 500ms leeway for other miscel expenses
  auto code = GetAction(&normalized_match_state, r_action, timeout_ms - 500);
  if (code != GET_ACTION_SUCCESS) {
    logger::error("    [ENGINE %s] : get action failure for some reason", engine_name_);
    return GET_ACTION_FAILURE;
  }

  /*
   * in case of using blueprint, the action may not be correct.
   *  match state may be mapped to another blueprint stack state,
   *  where the return action needs to put into context
   */
  if (!isValidAction(normalized_game_, &normalized_match_state.state, 0, &r_action)) {
    logger::debug("    [ENGINE %s] : invalid action from engine: %c%d",
                  engine_name_,
                  actionChars[r_action.type],
                  r_action.size);
//    if (IsNestedSgsStarted())
//      logger::warn("    [ENGINE %s] : sgs does not give invalid action", engine_name_);
    Action original_action = r_action;
    //try to fix it.
    if (!isValidAction(normalized_game_, &normalized_match_state.state, 1, &r_action)) {
      logger::warn("    [ENGINE %s] : unable to fix action", engine_name_);
      //normally it is a r20000 invalid. return call as a hack
      r_action.type = a_call;
    }
    if (abs(original_action.size - r_action.size) >= 1000) {
      logger::warn("    [ENGINE %s] : invalid action has been SUBSTANTIALLY fixed to: %c%d",
                   engine_name_,
                   actionChars[r_action.type],
                   r_action.size);
    } else {
      logger::debug("    [ENGINE %s] : invalid action has been MARGINALLY fixed to: %c%d",
                    engine_name_,
                    actionChars[r_action.type],
                    r_action.size);
    }
  }
  /*
   * need to do translation in any case.
   */
  //todo: put the bet scaler into the table context?
  double bet_scaler = (double) BigBlind(normalized_game_) / BigBlind(&table_context_.session_game);
  if (r_action.type == a_raise) {
    int adjusted_raise = (int) round(r_action.size / bet_scaler);
    r_action.size = adjusted_raise;
  }

  logger::debug("    [ENGINE %s] : return translated action %d", engine_name_, actionToCode(&r_action));
  return GET_ACTION_SUCCESS;
}

int Engine::TranslateToNormState(const std::string &match_state_str, MatchState &normalized_match_state) {
  logger::debug("    [ENGINE %s] :     real match state = %s", engine_name_, match_state_str);
  //real match state should be translated with the game def of the session. or u may have invalid betting ctions
  MatchState real_match_state;
  if (readMatchState(match_state_str.c_str(), &table_context_.session_game, &real_match_state) == -1)
    return MATCH_STATE_PARSING_FAILURE;

  double bet_scaler = (double) BigBlind(normalized_game_) / BigBlind(&table_context_.session_game);
  if (bet_scaler == 0) {
    logger::error("    [ENGINE %s] : invalid betting scaler in translation == 0", engine_name_);
    return MATCH_STATE_PARSING_FAILURE;
  }

  /*
   * construct the normalized match state
   */
  normalized_match_state.viewingPlayer = real_match_state.viewingPlayer;
  //init a state, like anything else, would not copy the stack, as it should be stakc-aware
  initState(normalized_game_, 0, &normalized_match_state.state);
  //copy table info (handID, hold cards and board cards)
  StateTableInfoCopy(normalized_game_, &real_match_state.state, &normalized_match_state.state);
  //scale the stack
  normalized_match_state.state.stackPlayer[0] *= bet_scaler;
  normalized_match_state.state.stackPlayer[1] *= bet_scaler;

  //copy all actions over but scale with bet_scaler
  int round = real_match_state.state.round;
  for (int r = 0; r <= round; r++)
    for (int a = 0; a < real_match_state.state.numActions[r]; a++) {
      auto real_action = real_match_state.state.action[r][a];
      if (real_action.type == a_raise)
        real_action.size *= bet_scaler; // check if the type conversion makes problem
      doAction(normalized_game_, &real_action, &normalized_match_state.state);
    }

  char norm_state_str[MAX_LINE_LEN];
  printMatchState(normalized_game_, &normalized_match_state, MAX_LINE_LEN, norm_state_str);
  logger::debug("    [ENGINE %s] : translated match state %s", engine_name_, norm_state_str);

  return MATCH_STATE_PARSING_SUCCESS;
}

/*
 * solve it the subgame solving is on
 * and the last strategy is not yet converged (all command finished)
 */
void Engine::AsynStartDaemonSolving(SubgameSolver *sgs, int checkpoint) {
  if (is_daemon_engine) {
    if (checkpoint < CFR_SOLVING_TERMINATED_ASYNC) {
      logger::debug("    [ENGINE %s] : DAEMON solving not needed", engine_name_);
      return;
    }
    //solve with no time limit.
    logger::debug("    [ENGINE %s] : DAEMON solving started at cfr checkpoint %d",
                  engine_name_,
                  checkpoint);
    auto target_strategy = playbook_stack_.back().strategy_;
    auto blueprint = playbook_stack_.front().strategy_;
    if (blueprint == nullptr || target_strategy == nullptr) {
      logger::critical("does not make sense");
    }
    daemon_cancel_token_ = false;
    std::thread([&] {
      auto cfr_result_future = std::async(std::launch::async,
                                          CFR::AsyncCfrSolving,
                                          sgs->cfr_,
                                          target_strategy,
                                          blueprint,
                                          sgs->convergence_state_,
                                          std::ref(daemon_cancel_token_),
                                          checkpoint);
      try {
        int cfr_return_code = cfr_result_future.get();
        logger::debug("    [ENGINE %s] : DAEMON solving ended with status %s",
                      engine_name_,
                      PrintCfrResultCode(cfr_return_code));
      } catch (std::exception &e) {
        logger::error(e.what());
      }

    }).detach();
  }
}

void Engine::AsynStopDaemonSolving() {
  if (is_daemon_engine && !daemon_cancel_token_) {
    logger::debug("    [ENGINE %s] : asyn stopping DAEMON solving", engine_name_);
    daemon_cancel_token_ = true;
    //sleep for 200ms to make sure the DAEMON solving ended before proceeding
    logger::debug("thread sleeping for 100 ms to wait for DAEMON graceful finishing");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
//    daemon_cancel_token_ = false;
  };
}

bool Engine::InputSanityCheck(MatchState *new_match_state) {
  //check if the match state is valid. e.g. does it have the same board card.
  if (!IsStateCardsValid(normalized_game_, &new_match_state->state)) {
    logger::debug("match states invalid. probably got the same cards");
    return false;
  }
  return true;
}

bool Engine::IsNestedSgsStarted() {
  return playbook_stack_.size() > 1;
}

//if the last sgs solves to the ALL_COMMAND_FINISH,then sgs_cancel token would be true.
void Engine::AsynStopCFRSolving() {
  if (!sgs_cancel_token_) {
    logger::debug("Asyn stopping CFR");
    sgs_cancel_token_ = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

bool Engine::EngineStateStaleCheck(MatchState *new_match_state) {
  //check game state continuity.
  if (!playbook_stack_.empty() && InSameMatch(normalized_game_, &last_matchstate_, new_match_state) > 0) {
    logger::warn("    [ENGINE %s] : match state not continuted. force set new hand", engine_name_);
    return false;
  }
  return true;
}

int Engine::GetRandomAction(MatchState *new_match_state, Action &r_action) {
  auto act_abs = Fcpa();
  act_abs.raise_mode_ = NET_OR_X;
  act_abs.raise_config_.emplace_back(RaiseConfig{POT_NET, 1.0, 0, 10});
  act_abs.raise_config_.emplace_back(RaiseConfig{X_LAST_RAISE, 1.0, 0, 10});
  std::vector<Action> actions = act_abs.GetCandidateActions(normalized_game_, new_match_state->state);
  std::random_device rand_dev;
  std::mt19937 generator(rand_dev());
  std::uniform_int_distribution<int> distr(0, actions.size() - 1);
  r_action = actions[distr(generator)];
  logger::debug("ENGINE: generate random action %d", actionToCode(&r_action));
  return GET_ACTION_SUCCESS;
}

/**
 * inited + reachable, for all players.
 * @param playbook
 * @param new_match_state
 * @param subgame_built_code
 * @return
 */
bool Engine::ValidatePlaybook(PlayBook &playbook, MatchState *new_match_state, int subgame_built_code) {
  Strategy *&new_strategy = playbook.strategy_;
  auto selected_candidates = new_strategy->FindSortedMatchedNodes(new_match_state->state);
  int cursor = 1;
  for (auto selected_candidate : selected_candidates) {
    selected_candidate.Print("matched node from new strategy:");
    if (selected_candidate.off_tree_dist_> 1500)
      break;

    auto inited = new_strategy->IsStrategyInitializedForMyHand(selected_candidate.matched_node_,
                                                               STRATEGY_WAVG,
                                                               new_match_state);
    logger::require_warn(inited,
                         "virgin node in playbook_pending [sgs %s] [subgame_built_code %s]",
                         playbook.strategy_->name_,
                         SubgameBuiltCodeMap[subgame_built_code]);
    bool reached;
    if (selected_candidate.matched_node_ == new_strategy->ag_->root_node_) {
      reached = true;
    } else {
      std::array<sHandBelief, 2> new_base_reach;
      new_base_reach[0].CopyValue(&new_strategy->ag_->root_hand_belief_[0]);
      new_base_reach[1].CopyValue(&new_strategy->ag_->root_hand_belief_[1]);
      auto estimate_return_code = new_strategy->EstimateReachProbAtNode(new_match_state,
                                                                        selected_candidate.matched_node_,
                                                                        new_base_reach,
                                                                        STRATEGY_WAVG,
                                                                        DEFAULT_BAYESIAN_TRANSITION_FILTER);
      reached = estimate_return_code == RANGE_ESTIMATE_SUCCESS;
      logger::require_warn(reached,
                           "[ new strategy ] node not reachable [%s]",
                           RangeEstimateCodeMap[estimate_return_code]);
    }
    if (inited && reached){
      return true;
    } else {
      logger::warn("    try %d -> sgs generates invalid nodes", cursor++);
    }
    //else. try next node
    //todo: maybe break when the l2 distance is too much away.
  }
  return false;
}

int Engine::AsynStartCFRSolving(SubgameSolver *selected_sgs, Strategy *&new_strategy, double remaining_ms) {
  sgs_cancel_token_ = false; //safegaurding
  auto cfr_result_future = std::async(std::launch::async,
                                      CFR::AsyncCfrSolving,
                                      selected_sgs->cfr_,
                                      new_strategy,
                                      playbook_stack_.front().strategy_,
                                      selected_sgs->convergence_state_,
                                      std::ref(sgs_cancel_token_),
                                      0);
  //asyn stop if timesup
  int async_span_count = (int) round(remaining_ms / 100);
  std::chrono::milliseconds span(100);
  int count = 0;
  while (cfr_result_future.wait_for(span) == std::future_status::timeout) {
    count++;
    if (count == async_span_count) {
      AsynStopCFRSolving();
    }
  }
  if (count == async_span_count)
    logger::debug("    [ENGINE %s] : force action return after %f ms", engine_name_, remaining_ms);
  return cfr_result_future.get();
}

std::string Engine::GetName() const {
  return engine_name_;
}

Game *Engine::GetGame() {
  return normalized_game_;
}

Engine::~Engine() {
  RefreshEngineState();//clearing up old states.
  sleep(3); //give it 3 seconds to process.
  delete[] subgame_solvers_;
  if (owning_pool_) {
    delete blueprint_pool_;
    delete bucket_pool_;
  }
  delete default_action_chooser_;
  logger::info("graceful shutdown | engine %s", engine_name_);
}
