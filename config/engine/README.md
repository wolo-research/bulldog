##Engine Configuration

###CFR
name: (train/sgs)\_r(starting round(s))\_cfr_(platform)\_(sb)\_(bb).json

type_StartingRound_platform_game_stack

builder_r0_slumbot_nlh2_100bb.json

example train file:
```json
{
  "cfr": {
    "name": "slumbot_v6",
    "num_threads": -1,
    "thread_policy_inner": "sequenced",
    "rollout": {
      "pruning":{
        "regret_thres": 0.95,
        "prob": 0.95,
        "first_iter": 100000
      }
    },
    "rollin": {
      "estimator": {
        "my": "weighted_resp",
        "opp": "sum_resp"
      }
    },
    "regret_matching": {
      "discounting" : {
        "first_iter":10000,
        "in_effect_iter": 300000,
        "interval": 10000
      },
      "floor" : -5.0,
      "avg_update": "classic"
    }
  }
}
```

###AG Builder
name: (train/sgs)\_r(starting round(s))\_builder_(platform)\_(sb)\_(bb).json

example builder file:
```json
{
  "ag_builder": {
    "game_file": "holdem.nolimit.2p.game",
    "root_state": "0",
    "action_abs": {
      "pot_mode": "net",
      "ratios":  [
        [[0.5, 0.75, 1.0, 2.0],[1.0, 2.0],[1.0, -1],[1.0, -1],[1.0, -1]],
        [[1.0, 2.0],[1.0, -1],[1.0],[1.0],[1.0]],
        [[1.0, 2.0],[1.0, -1],[1.0],[1.0],[1.0]],
        [[1.0],[1.0],[1.0, -1],[1.0],[1.0]]
      ]
    },
    "card_abs": ["colex","hierarchical_60_500_1","hierarchical_60_500_2","hierarchical_60_500_3"]
  }
}
```
