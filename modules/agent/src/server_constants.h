//
// Created by Isaac Zhang on 7/30/20.
//

#ifndef BULLDOG_MODULES_AGENT_SRC_SERVER_CONSTANTS_H_
#define BULLDOG_MODULES_AGENT_SRC_SERVER_CONSTANTS_H_
#include <string>
#include <vector>

/*
 * PATH
 */
//the router must support dhcp for the device
static utility::string_t SERVER_DEFAULT_ENDPOINT = U("http://host_auto_ip4:8080/v1/bulldog/api");
static utility::string_t WOLO001_DEFAULT_ENDPOINT = U("http://192.168.1.210:8080/v1/bulldog/api");
static utility::string_t ISAAC_MBP_DEFAULT_ENDPOINT = U("http://192.168.1.177:8080/v1/bulldog/api");
static utility::string_t CARMEN_MBP_DEFAULT_ENDPOINT = U("http://192.168.1.27:8080/v1/bulldog/api");
static utility::string_t PATH_GET_ACTION = U("getaction");
static utility::string_t PATH_TABLESESSION = U("tablesession");
static utility::string_t PATH_EVALSTATE = U("evalstate");
static utility::string_t PATH_EDITSESSION = U("editsession");
static utility::string_t PATH_HEARTBEAT = U("heartbeat");
static utility::string_t PATH_PING = U("ping");
static utility::string_t PATH_NEW_HAND = U("newhand");
static utility::string_t PATH_END_HAND = U("endhand");

static std::vector<utility::string_t> ENDPOINT_CANDIDATES{
    WOLO001_DEFAULT_ENDPOINT,
    CARMEN_MBP_DEFAULT_ENDPOINT,
    ISAAC_MBP_DEFAULT_ENDPOINT
};
/*
 * PARAM
 */
static utility::string_t PARAM_GAME = U("game");
static utility::string_t PARAM_TABLE = U("table");
static utility::string_t PARAM_SMALLBLIND = U("smallblind");
static utility::string_t PARAM_BIGBLIND = U("bigblind");
static utility::string_t PARAM_SESSION_ID = U("session_id");
static utility::string_t PARAM_MATCHSTATE = U("matchstate");

/*
 * POKER ROOM
 */
static std::string TABLE_POKERSTAR_LITE_NLH2_50_100 = "POKERSTAR_LITE_NLH2_50_100";

/*
 * GAME
 */
static std::string GAME_NLH2 = "nlh2_100bb.game";
#endif //BULLDOG_MODULES_AGENT_SRC_SERVER_CONSTANTS_H_
