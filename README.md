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

## Random example usages (consult --help for all options)
- bdu --max-depth=2 /home
- bdu --max-depth=2 --threads=12 /home (if "--threads" is not present the number of threads will be set to the number of (virtual) CPU cores available on the machine)
- bdu -s /home (equivalent to du -s /home or du -hs /home - bdu by default prints the sizes in human readable format, for ex: 10G)
- bdu --max-depth=2 --output-format=json /home - also "text" or "html"
- bdu --max-depth=1 --warn-at=100M --critical-at=20G /home - the size of entries greater than 100M will be colored yellow, and greater than 20G will be colored red - not necessarily useful, just for fun :)
- bdu --max-depth=2 --output-format=json --output-file=./out.txt /home - writes the results in the specified file

## Sorting results (default is by "size" in descending order )
- bdu --max-depth=2 --sort-by=[name/size/date] --sort-order=[asc/desc] /home - without brackets of course :)