#Webserver
with engine ready for waiting

##REST Endpoints
####New Engine
with bulldog/config/games/holdem.nolimit.2p.10_20.game
```bash
method: POST
uri: http://192.168.1.90:8080/v1/bulldog/api/new/engine
params[example]:
    - players="2p"
    - betting_type="nolimit"
    - game="holdem"
    - blinds="10_20"
```

```bash
status: 201
{
    "engine": "random",
    "session_id": "7D256B5E-7120-4F14-8413-8CE5CCF2F7F9"
}
```
####New Hand
Not Implemented 

####Delete Engine
```bash
method: DELETE
uri: http://192.168.1.90:8080/v1/bulldog/api
params[example]:
    - session_id="7D256B5E-7120-4F14-8413-8CE5CCF2F7F9"
```
```bash
status: 200
"deleted engine"
```
####Get Action
```bash
method: GET
uri: http://192.168.1.90:8080/v1/bulldog/api/action
params="example":
    - session_id="7D256B5E-7120-4F14-8413-8CE5CCF2F7F9"
    - matchstate="MATCHSTATE:1:2:c:20000|20000:|4sQs"
```
```bash
status: 200
{
  "size": 0,
  "type": 1
}
```

#Game Agent
###Example
#####ACPC
```bash
./agent
--game=holdem.nolimit.2p.game
--engine_params=engine_offline_simple.json
--connector=0
--connector_params=localhost,52001
--log_level=trace

/* NOTES
acpc connector_params (in exact order):
1. acpc dealer host address
2. acpc dealer listening port
*/
```

#####Slumbot
```bash
./agent
--game=holdem.nolimit.2p.game
--engine_params=engine_offline_simple.json
--connector=1
--connector_params=woloai_test,loot,100
--log_level=debug
--log_output=slumbot_connector.log

/* NOTES
slumbot connector_params (in exact order):
1. username
2. password
3. iterations
*/
```

#TODO
- [x] add logging and execption raising and handling
- [x] abstract out action, instead of using game_feed
- [ ] game definition for connector and check compatibility with running game of agent
- [ ] all customizations such as p1_=0 in a connector file?
