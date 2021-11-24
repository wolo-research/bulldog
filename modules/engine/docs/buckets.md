```bash
bin/build_buckets
--private_cards=2
--public_cards=4
--clusters=200
--threads=2
--loglevel=debug

./build_buckets --mode=hierarchical --private_cards=2 --public_cards=4 --clusters=5000 --threads=24 --loglevel=info --logfile=turn_150k_bucket.log &
```

####caveats
1. thread=0 will run without calling pthreads
2. if dimension of data is 1, euclidean distance will be used, otherwise earth mover's distance
3. reads features by features_{private_cards}_{public_cards}_hand_histo.bin
4. saves bucket map to buckets_{private_cards}_{public_cards}.bin
5. outputs logs to kmeans_{date}.log