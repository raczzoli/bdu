# bdu — better du

`bdu` (better du) is an alternative to the legacy Linux `du` command. It uses multithreaded scanning and also has a (builtin) sort by size and a bit nicer output. 
While still in baby shoes, the target is to be fully compatible with "du", using the same command line arguments.

## Features

- Multithreaded – takes advantage of modern multi-core CPUs
- Fast – optimized for performance on large filesystems
- Output can be sorted, filtered, or exported in various formats (plain text, json, html etc)
- Written in C, with no external libraries or dependencies
- On a web server with 541 GB of data and 39,321,600 inodes, bdu completed the full filesystem tree scan in just 18 seconds using 12 threads, compared to du, which took 174 seconds to execute — resulting in an 85% performance improvement.

## Installation
  - git clone https://github.com/raczzoli/bdu.git
  - make
  - make install

## Example usages
- bdu --max-depth=2 /home
- bdu --max-depth=0 /home (equivalent to -hs option)
- bdu --max-depth=2 --output-format=json /home
