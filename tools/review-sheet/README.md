# review-sheet

Writes the visual review sheets of addendum A4 part B
(`docs/SPEC_ADDENDA.md`) for a picture stack: the overview sheet with the
grid, the same pair without it, and enlarged crops of the cells, quarters
or ranges the customer names. The frames are composed by the program's own
code (`sci/platform/game_frame.hpp`), from the stack as the interpreter
shows it alone (`tools/common/stack_screen.hpp`, which `sci-hd-frames
--stack` uses too): what the game shows in original and in HD mode at N.

    sci-review-sheet <game folder> <output folder> --stack <n>[+<n>...]
                     [--room <n>] [--scale <n>] [--sheet-version <n>]
                     [--date <yyyy-mm-dd>] [--hints <directory> | --no-hints]
                     [--cache <directory>]
                     [--crop <ref>[,<ref>...]] [--crop-scale <n>]
                     [--crop-layout side|stacked]

For example, in the toolchain container:

    docker/run.sh cmake --build build/release --target sci-review-sheet
    SIERRA_HD_MOUNTS=/path/to/game:/game docker/run.sh \
        build/release/tools/review-sheet/sci-review-sheet /game reports/images/milestone-10/sheets \
        --stack 74+174 --room 74 --date 2026-10-30
    SIERRA_HD_MOUNTS=/path/to/game:/game docker/run.sh \
        build/release/tools/review-sheet/sci-review-sheet /game reports/images/milestone-10/sheets \
        --stack 12 --crop B3.2,A2-A4 --date 2026-10-30

- Without `--crop` it writes `pic-<n>[+<n>...]-sheet.png`, the overview of
  B.1: HD on top, original below, each 320N x 200N (N = `--scale`, default
  4: 1280 x 800) including the menu bar's 10 rows, which stay blank as in
  the game. The grid is drawn over the picture area only: cell lines
  (columns 1-5 at x = 0, 64, 128, 192, 256, 320; rows A-C at picture
  y = 0, 63, 127, 190) thin, semi-transparent white outlined in black;
  quarter lines (x = 32, 96, 160, 224, 288; y = 32, 95, 159) thinner and
  dashed; a label such as "B3" on a dark backing in each cell's top left
  corner, quarters unlabelled. `pic-...-plain.png` is the same pair
  without the grid.
- The header holds the room and picture numbers, the sheet version
  (`--sheet-version`, default 1) and the date (`--date`, default today;
  pass it for reproducible sheets), and says which image is which.
  `--room` defaults to the stack's first picture.
- `--crop` writes `pic-...-crop-<ref>.png` for every reference (B.4): a
  cell `B3`, a quarter `B3.2` or a range `A2-A4` (the rectangle spanning
  both ends, which may be cells or quarters); the row letter may be lower
  case. Each crop is the area plus half a cell (32 original pixels) on
  every side, clamped to the picture area, at N = `--crop-scale` (default
  8), HD and original side by side (HD left) or with `--crop-layout
  stacked` one above the other (HD on top), with the grid; every cell in
  view is labelled at the top left corner of its visible part. The header
  names the reference and the crop's picture coordinates.
- `--hints` defaults to `hints`, the repository's hint files as the
  program uses them (run from the repository root); `--no-hints` renders
  without. `--cache <directory>` keeps the analysed scenes between runs.

The geometry and the parsing of references are in
`tools/common/review_grid.hpp`, tested by `sci-tools-tests`. The text is a
3 x 5 bitmap font built into the tool (capitals, digits and a few signs),
so the sheets use no font of the game or of the system. The PNG files are
stored uncompressed (`tools/common/png.hpp`): about 6.4 MB per overview
image at N = 4.

The output is derived from the game art: write it only to git-ignored
places such as `reports/images/` (B.5) or `build/`; never commit it.
