# tools/common

Code shared by the development tools (not by the program), built as the
static library `sci_tools_common`:

- `game_folder.hpp`: reading a game folder for sci-resource and writing
  output files.
- `picture.hpp`: drawing the pictures of a game folder through path 1.
- `png.hpp`: a minimal PNG writer for development images (no compression
  library).
- `interface_check.hpp`: milestone 6b's checks of a composed game frame
  (windows, text, cursor; `docs/hd-composition.md`, "The interface at
  every N"); `sci-platform-tests` compiles it too
  (`tests/unit/platform/interface_check_test.cpp`).
