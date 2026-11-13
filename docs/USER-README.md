# colonel-hd 1.0

colonel-hd is a new interpreter for Sierra's *The Colonel's Bequest*
(1989), with an HD mode that redraws the backgrounds as sharp vector art
at any whole-number scale. The game itself plays exactly as the original.

**Release 1.0.** The whole game plays from the start to the ending, with
music and sound, saving and restoring. It also contains the room viewer of
release 0.1.

## What you need

- Your own copy of *The Colonel's Bequest*, English DOS version (the GOG
  release works). colonel-hd contains no game data.
- One of:
  - Windows 10 or 11 (x64 or ARM64);
  - Linux with glibc 2.38 or newer, for example Ubuntu 24.04, Debian 13 or
    Fedora 39, on x64 or ARM64, with X11 or Wayland.
- For Roland MT-32 music (optional): your own MT-32 or CM-32L ROM files.
  Without them, the music plays on the AdLib.

## Starting

Unpack the archive anywhere, copy the game's files (the folder that
contains `RESOURCE.MAP`) into its `game/` folder and start `colonel-hd`
(on Windows `colonel-hd.exe`). If `game/` is empty, the program asks for
the game folder the first time. You can also give it on the command line:

    colonel-hd "C:\GOG Games\Colonel's Bequest"

The program checks that the folder holds a supported release. The tested
release is the English DOS 3.5" floppy version 1.000.046. The 5.25"
version is accepted with a warning, and its pictures are drawn without the
HD hints.

The first start analyses all pictures for the HD mode, behind a progress
bar, and keeps the result in `cache/`; later starts are quick.

## Where the program keeps its files

The program keeps `colonel-hd.ini`, `game/`, `rom/`, `saves/`, `cache/`
and `files/` in one data folder:

1. the folder given with `--data <folder>`, if any;
2. else the program's own folder, if it holds `colonel-hd.ini` or `game/`
   and can be written: the unpacked archive is used as it is ("portable");
3. else a folder of your user account: `%APPDATA%\colonel-hd` on Windows,
   `~/.local/share/colonel-hd` on Linux (or `$XDG_DATA_HOME/colonel-hd`),
   created on first use.

The `hints` folder always stays next to the program.

## Playing

The game is played as the original: type commands (`look`, `open door`,
`talk to fifi`), walk with the mouse or the arrow keys, and use the menu
bar (Escape) or the game's function keys, for example F5 to save and F7 to
restore.

| Key | Action |
|---|---|
| F12 | HD or original mode |
| F11 or Alt+Enter | Fullscreen |

The scale N is always a whole number. The game is 320 x 200 original
pixels, shown at 320N x 200N screen pixels, with black bars around it
where the window is larger.

## Saves

The game's own save and restore dialogs work as in the original. Each
save is one file, `saves/colonel-hd-001.sav` to `colonel-hd-099.sav`, in
the data folder. A save holds the game's identity: it restores only with
the same release of the game. Files the game itself writes go to `files/`.

## Music: AdLib or MT-32

The music plays on an emulated AdLib by default. For the Roland MT-32:

1. You need two ROM files: a control ROM and a PCM ROM, of the
   MT-32 or the CM-32L, with names ending in `.rom` (any names; the
   program recognises the contents).
2. Put them in `rom/` in the data folder, or set `mt32_rom` in
   `colonel-hd.ini` to their folder; and set `music = mt32`.

If the ROMs are missing or not recognised, the program says so and plays
on the AdLib.

## Configuration

`colonel-hd.ini` is in the data folder. The program changes it only when
you changed something it keeps (the HD or original mode, the viewer's
scale, a game folder you chose when asked), and then only that value's
line; your comments and other lines stay as they are. A game folder given
on the command line is not written. Delete the file to start again from
the defaults. Its keys:

| Key | Values |
|---|---|
| `game_folder` | the game folder (empty: `game/` in the data folder) |
| `mode` | `hd` or `original` |
| `scale` | `auto` (the largest that fits) or 1 to 16 |
| `priority_snap` | `on`: characters pass behind the foreground in whole original pixels, as in the original game; `off` (default): along its smooth edges |
| `music` | `adlib`, `mt32` or `off` |
| `mt32_rom` | the folder of the MT-32 ROMs (empty: `rom/` in the data folder) |
| `volume` | 0 to 100 |
| `saves` | the folder of the saves (empty: `saves/` in the data folder) |
| `log` | more messages on the console: `vm` (script problems the program works around), `resources` (resource loading), both comma-separated, or empty (default); `--log` on the command line overrides it for one run |

The `hints` folder holds the scene-analysis hints for the HD mode. Keep it
next to the program; without it, HD pictures are drawn without them.

## The room viewer

    colonel-hd --viewer "C:\GOG Games\Colonel's Bequest"

shows every background picture, every animated view (sprite) and every
font, in HD or in the original look.

| Key | Action |
|---|---|
| Left, Right | Previous or next picture, view or font |
| Up, Down | Views: previous or next animation loop |
| Tab | Switch between pictures, views and fonts |
| H | HD or original mode |
| P | Pictures: show the priority map (the depth information) |
| + and - | Larger or smaller scale; 0: the largest that fits |
| Space | Views: stop or start the animation |
| F11 or Alt+Enter | Fullscreen |
| Escape | Quit |

`colonel-hd --help` lists the keys and options.

## Options for testing

These are for testing the program, not for playing; `colonel-hd --help`
describes them.

| Option | Effect |
|---|---|
| `--replay <file>` | Plays the input recorded in a replay file into the game (`docs/formats/replay-and-trace.md` in the source); the keyboard and mouse still work. |
| `--seed <n>` | The random seed (default 0), so that a replay plays the same each time. |
| `--fast` | Runs the game as fast as it can instead of in real time; most of the music is dropped. |
| `--end-state <file>` | Writes the game's tick, global variables and state hash to the file when it ends. |
| `--timing <file>` | With `--replay`: plays it as fast as it can, presents every frame, and writes the frame times, room entries, first launch and peak memory to the file (`docs/timing.md` in the source). |
| `--scale <n>` | With `--timing`: the scale N to measure at, 1 to 16, also when the window is smaller. |

## Licence

colonel-hd is free software under the GNU General Public License, version 3
or later; see `licenses/`. It includes SDL 3 (zlib licence), Nuked OPL3
(LGPL 2.1 or later) and Munt mt32emu (LGPL 2.1 or later). *The Colonel's
Bequest* is a trademark of its owners; colonel-hd is not affiliated with
them.
