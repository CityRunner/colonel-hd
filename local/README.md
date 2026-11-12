# local/

Folders for your own files. Git ignores everything in them; nothing you put
here is ever committed.

- `game/`: your copy of The Colonel's Bequest (the folder with
  `RESOURCE.MAP`).
- `rom/`: your MT-32 or CM-32L ROM files, if you want MT-32 music.
- `saves/`: saved games.

The program does not look here by itself yet: give it the game folder on
the command line or in `sierra-hd.ini` (`game_folder`, `mt32_rom`,
`saves`). The development tools take these folders as their examples, for
instance `SIERRA_HD_MOUNTS=$PWD/local/game:/game docker/run.sh ...`.
