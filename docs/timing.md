# The timing run (B-10)

`colonel-hd --timing` measures the program on a machine: it plays the
replay of the whole game as fast as it can, presents every frame at a
fixed scale N, and writes what it measured to a CSV file. The run itself
is the same game as always (the same end state as `sci-vm-trace`); only
the pacing differs, as with `--fast`.

## For the customer: the run on the reference laptop

Once, on a 4-core x86-64 laptop of about 2020 with integrated graphics,
under Windows (spec section 10.3, `docs/ASSUMPTIONS.md`):

1. **The build.** In GitHub, Actions, workflow "Release", "Run workflow"
   on the branch named in the iteration 2 report (P7); when it is green,
   download the artifact `colonel-hd-<version>-windows-x64` and unpack it
   into an empty folder.
2. **The two files of the run.** From the repository, at the same commit,
   put next to `colonel-hd.exe`:
   - `tests/playthrough/playthrough.txt` (the replay: the input of the
     whole game, made by us; no game data), and
   - `tools/timing/run-timing.cmd`.
3. **Run it** from a command prompt in that folder, with your game folder
   (the GOG copy):

       run-timing.cmd "C:\GOG Games\The Colonel's Bequest"

   Close other programs first and keep the laptop on mains power. The
   window shows the game playing by itself; it ends on its own. The four
   runs (N = 1, 2, 5 and 10) take some minutes each; a window larger than
   the screen at N = 10 is expected (the frame is still made and
   presented at N = 10).
4. **Send** the four files `timing-1.csv`, `timing-2.csv`, `timing-5.csv`
   and `timing-10.csv` from that folder, with the laptop's model, CPU,
   graphics and Windows version. They hold times and memory only.

The folder `timing-data` the script makes holds the analysis cache and
can be deleted afterwards.

## The CSV

One `key,value` per line, then one `room,<room>,<ms>` line per room
entry, in order:

| Key | What |
|---|---|
| `colonel-hd timing` | the format, 1 |
| `version` | the program's version |
| `scale` | N |
| `first_launch_scenes`, `first_launch_ms` | the pictures analysed before the game started and how long it took (0 with a warm cache: only the first run of `run-timing.cmd` has a cold one) |
| `frames` | the frames presented |
| `frame_ms_mean`, `_median`, `_p90`, `_p99`, `_worst` | the main thread's time per presented frame: composing it at N, uploading the changed rectangles, presenting; nearest-rank percentiles |
| `frames_over_16.7ms` | frames over a 60 Hz display's frame time |
| `room_entries`, `room_entry_ms_median`, `room_entry_ms_worst` | per new picture: from the request of its HD images to the first frame composed with them (the game goes on meanwhile; the frame shown is held, then the picture at 1x, `docs/hd-composition.md`) |
| `peak_memory_kib` | the process's peak resident memory (peak working set on Windows) |
| `room,<room>,<ms>` | each room entry: the room number (global 11) and its time |

The targets (spec section 10.3, B-09): no frame over 16.7 ms and room
entry within 1 s at N = 10; first launch within 15 s.

## In the development container

`tools/playthrough/program-run.sh --scale <N> --timing <csv>` runs the
same under Xvfb (software rendering: the frame times include it) and also
checks the end state; `tools/timing/run-timing.sh` is the Linux version
of the script above for a machine with a display.
