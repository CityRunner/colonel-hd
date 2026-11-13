# tools/playthrough

The scripts of the playthrough (milestone 9); `docs/playthrough.md`
describes the playthrough, its checkpoints and its results.

- `check.sh`: runs `tests/playthrough/playthrough.txt` in the reference
  ScummVM and in sci-vm (`tools/vm-trace/run-scenario.sh`) and checks where
  the game is at each checkpoint (`where.py`).
- `where.py`: prints the act and room of the game at the ticks of a globals
  file.
- `program-run.sh`: runs the playthrough in the program itself, headless
  under Xvfb, with a data folder of its own (`--data`), and checks its exit
  status, its log and its end state against sci-vm-trace's.
- `act-saves.sh`: makes a save at the start of each act through the game's
  own save dialog, for the customer's play test.

Each script's header gives its options and an example command.
