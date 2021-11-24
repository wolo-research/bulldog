![](icon.jpg?raw=true)

## todos

## architecture
- core (logging, game def, hand eval and game state protocol)
- agent (connector, playing logic)
- engine (playing engine interface , ag, cfr, and train)

## build and run
```bash
### require_warn cmake 

git clone git://bulldog...
cd bulldog
git submodule init
git submodule update
git pull
git submodule update --recursive

## build using cmake
mkdir build
cd build
cmake ..
make
make test

### third party (best option 1)
1. git submodule add xxxx && git submodule init && git submodule update
2. hpp whole files
3. Cmake External
```

##Example Configurations
```bash
./bin/train

--cfr_config=cfr_simple.json
--ag_config=ag_simple.json
--log_level=debug
--log_output=train.log
--max_iter=10000
--exp_name=norm_max
--checkpoint_interval=1000
--save_final=true
--load_checkpoint=dcfr_naive-1007
--profiler_checkpoint=2 

./bin/agent
--game=holdem.nolimit.2p.game
--engine_params=engine_offline_simple.json
--connector=0
--connector_params=localhost,52001
--log_level=trace

```

##Prerequisite
brew install cpprestsdk & tbb
add cmake options in the IDE, -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl
```$xslt
sudo apt install libcpprest-dev
sudo apt install libtbb-dev
sudo apt-get install uuid-dev
```
libtorch on linux install

### remove a submodule you need to:
```$xslt
Delete the relevant section from the .gitmodules file.
Stage the .gitmodules changes git add .gitmodules
Delete the relevant section from .git/config.
Run git rm --cached path_to_submodule (no trailing slash).
Run rm -rf .git/modules/path_to_submodule (no trailing slash).
Commit git commit -m "Removed submodule "
Delete the now untracked submodule files rm -rf path_to_submodule
```

linux torch cmake fix: https://github.com/pytorch/pytorch/issues/40965

