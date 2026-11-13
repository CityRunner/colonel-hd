# sci-platform

The platform part of the program (spec section 4.2), shared by the program
(`src/app`) and the development tools: files, threads, the analysis cache
and the composition of the game's frame. It depends on sci-core,
sci-resource, sci-pic, sci-vector, sci-render, sci-vm and sci-audio
(`CMakeLists.txt`; `scripts/check-deps.py` checks the list).

Public interface (`include/sci/platform`):

- `data_folder.hpp`: the data folder (`--data`, portable, per user; B-03)
  and the game folder in its `game/`.
- `config.hpp`: `colonel-hd.ini`, its reader, its writer for a new file and
  the update in place of an existing one (B-04).
- `game_files.hpp`, `identify.hpp`: reading a game folder and identifying
  the release by its resource hashes.
- `picture_hints.hpp`: loading the hint files (spec section 6.7).
- `analysis_cache.hpp`: the scenes of every picture, analysed once and kept
  in `cache/` (`docs/formats/analysis-cache.md`).
- `hd_pictures.hpp`, `game_frame.hpp`: the HD background and priority map
  of the picture shown, and the game's frame in original or HD mode
  (`docs/hd-composition.md`).
- `mt32_roms.hpp`: finding and identifying the user's MT-32 ROMs and
  choosing the music device.
- `save_folder.hpp`: the saved games, `colonel-hd-NNN.sav`.
- `thread_executor.hpp`, `background_task.hpp`: worker threads and the
  one background task the main thread starts and collects without waiting.

Tests: `tests/unit/platform` (`sci-platform-tests`).
