# bdu — better du

`bdu` (better du) is an alternative to the legacy Linux `du` command. It uses multithreaded scanning and also has a (builtin) sort by size and a bit nicer output. 
While still in baby shoes, the target is to be fully compatible with "du", using the same command line arguments.

## Features

- Multithreaded – takes advantage of modern multi-core CPUs
- Fast – optimized for performance on large filesystems
- Output can be sorted, filtered, or exported in various formats (plain text, json, html etc)
- Written in C, with no external libraries or dependencies

## Installation
  - git clone https://github.com/raczzoli/bdu.git
  - make
  - make install

## Example usages
- bdu --max-depth=2 /home
- bdu --max-depth=0 /home (equivalent to -hs option)

## Example output
-------------------------------------------
   5.12G /home/rng89/workspace/linux/
           3.63G /home/rng89/workspace/linux/.git/
           1.01G /home/rng89/workspace/linux/drivers/
         147.33M /home/rng89/workspace/linux/arch/
          79.68M /home/rng89/workspace/linux/tools/
          69.71M /home/rng89/workspace/linux/Documentation/
          54.83M /home/rng89/workspace/linux/include/
          50.45M /home/rng89/workspace/linux/sound/
          49.55M /home/rng89/workspace/linux/fs/
          36.71M /home/rng89/workspace/linux/net/
          14.18M /home/rng89/workspace/linux/kernel/
           8.40M /home/rng89/workspace/linux/lib/
           5.70M /home/rng89/workspace/linux/mm/
           4.16M /home/rng89/workspace/linux/scripts/
           3.81M /home/rng89/workspace/linux/crypto/
           3.64M /home/rng89/workspace/linux/security/
           2.08M /home/rng89/workspace/linux/block/
           1.55M /home/rng89/workspace/linux/samples/
           1.01M /home/rng89/workspace/linux/rust/
         720.00K /home/rng89/workspace/linux/io_uring/
         312.00K /home/rng89/workspace/linux/virt/
         276.00K /home/rng89/workspace/linux/ipc/
         272.00K /home/rng89/workspace/linux/LICENSES/
         196.00K /home/rng89/workspace/linux/init/
          76.00K /home/rng89/workspace/linux/certs/
          68.00K /home/rng89/workspace/linux/usr/
           4.00K /home/rng89/workspace/linux/.vscode/
-------------------------------------------
Number of threads used: 12
Active workers at the end: 0
Took: 0.00 seconds
