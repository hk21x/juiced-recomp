# Achievements

Conditions on guest memory in RetroAchievements' syntax (`sdk/runtime/core/achievements.cpp`), checked once a frame;
unlocks are kept in `achievements.toml` beside the exe. The set is `src/game/achievement_set.cpp`.

## The set (first version: race results)

| id | title | for | points |
|---|---|---|---|
| finish_race | Off the Line | Finish a race | 5 |
| podium | On the Box | Finish a race in the top three | 5 |
| win_race | Winner's Circle | Win a race | 10 |
| win_circuit | Lap Leader | Win a circuit race | 10 |
| win_sprint | Straight Shooter | Win a sprint heat | 10 |
| sprint_10s | Ten Second Car | Finish a sprint heat in under ten seconds | 25 |

Tested headless (2026-11-17): Arcade's sprint finished 4th unlocks finish_race alone; finished 2nd, finish_race and
podium; a circuit race not finished unlocks nothing. No run has won yet, so the three wins and the ten seconds are
checked by reading only (the place field read 4 and 2 where the results page showed 4th and 2nd).

## Memory map

- The game object: `[0x372B54]`. `+0x3C` set in a race; `+0x34` the race; `+0x6C` its setup: `+4` the kind (5 a
  circuit race, 6 a sprint; Showoff's and Solo's not yet read), `+0xC` a sprint's heats, `+0x10` the track's name.
- The player's car: `[[[[race + 0xC]]] + 4]`, the first view's car, as the results page (0x63640) takes it.
- Its race record: `[car + 0x494]`. `+0xF0` the race clock (seconds, float), `+0xF8` the finishing time, `+0xFC` and
  `+0x100` the lap, `+0x104` and `+0x114` the place now, `+0x110` the place it finished in (10 until then), `+0x118`
  set once it has finished (each heat of a sprint).
- Times are float seconds (0x62740 formats one as mm:ss:hundredths).

## Still to map (for a fuller set)

- Career: cash, respect by crew, cars owned, pink slips (the save is `E:\UDATA\545100ef\<id>\juicey.dat`, 51049
  bytes, plain; a driver's stats page 0xDB100 reads races, wins and time at +0xB0, +0xB4, +0xB8 of a driver object).
- Arcade: the series unlocked and races completed (ArcadeUnlock.sav in the binary's strings).
- Showoff: the score and the moves (the HUD's "score", "combo", "Points" gadgets).
