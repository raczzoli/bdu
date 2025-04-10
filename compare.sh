#!/bin/bash

printf "\n====== RUNNING BDU (Better DU) ======\n"

start_time=$(date +%s.%N)

./bdu

end_time=$(date +%s.%N)
elapsed=$(echo "$end_time - $start_time" | bc)

printf "Took: %.3f seconds\n\n" "$elapsed"

printf "====== RUNNING DU ======\n"

start_time=$(date +%s.%N)

du -h --max-depth=1 /home/rng89

end_time=$(date +%s.%N)
elapsed=$(echo "$end_time - $start_time" | bc)

printf "Took: %.3f seconds\n\n" "$elapsed"
