# local/

Folders for your own files. Git ignores everything in them; nothing you put
here is ever committed.

- `game/`: your copy of The Colonel's Bequest (the folder with
  `RESOURCE.MAP`).
- `rom/`: your MT-32 or CM-32L ROM files, if you want MT-32 music.
- `saves/`: saved games.

Start a development build with this folder as its data folder:
`colonel-hd --data local`. It then keeps `colonel-hd.ini`, `cache/` and
`files/` here too, finds the game in `game/` and the ROMs in `rom/`, and
saves to `saves/` (B-03). The development tools take these folders as
their examples, for instance
`COLONEL_HD_MOUNTS=$PWD/local/game:/game docker/run.sh ...`.
