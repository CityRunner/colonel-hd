# graphics-dump

`sci-graphics-dump` draws every picture and decodes every view and font of
a game folder through the project's libraries and writes them in the
layout of the reference ScummVM's graphics dump
(`tools/scummvm-patches/0002-dump-graphics.patch`), so that the two can be
compared byte for byte by `tools/graphics-compare` (milestone 2).

    COLONEL_HD_MOUNTS=/path/to/game:/game docker/run.sh \
        build/release/tools/graphics-dump/sci-graphics-dump /game build/graphics-dump
