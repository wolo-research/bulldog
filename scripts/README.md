Bulldog Scripts
======

###cmake.sh
```bash
#removes bin and build and rebuild and reinstall
./cmake.sh clean
```

###Quickmatch.sh
```bash
#creates folder run/quickmatch_[epoch] and runs play_match
./quickmatch.sh

#same as above, but rebuilds and reinstalls required binaries
./quickmatch clean
```

###debug_quickmatch.sh
```bash
#boot up dealer server and one example_player (from acpc)
cd scripts
./debug_quickmatch.sh
```