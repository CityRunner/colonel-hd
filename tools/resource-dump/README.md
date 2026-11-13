# resource-dump

`sci-resource-dump` loads every resource of a game folder through
sci-resource and writes them in the layout of the reference ScummVM's dump
(`tools/scummvm-patches/0001-dump-resources.patch`), so that the two can be
compared byte for byte by `tools/resource-compare` (milestone 1):

- `<output>/<type>.<number>`: the resource data (number with at least
  three digits);
- `<output>/manifest.txt`: `<type> <number> <size> <loaded type>.<loaded
  number> <source>`, or `<type> <number> FAILED`.

```
COLONEL_HD_MOUNTS=/path/to/game:/game docker/run.sh \
    build/release/tools/resource-dump/sci-resource-dump /game build/resource-dump
```
