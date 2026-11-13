# btgarecomp-gang-mod

More friendly and enemy tanks in every campaign mission of the
[BattleTanx: Global Assault recomp](https://github.com/wootbeer/btgarecomp).

Made from the [mod template](https://github.com/wootbeer/btgarecomp-mod-template).

## What it does
In each campaign mission, the mod adds the tanks you pick in the Mods menu:

- **Extra Allies** (0-10): AI tanks in your gang. They show up next to you
  about two seconds into the mission.
- **Ally Class**: Random, Same as Yours, or any of the 11 tank classes.
- **Ally Orders**:
  - **Auto** (default): Hunt in missions where the goal is to destroy all the
    enemy tanks (SF Airport, Truck Stop, White House, Tower of London, Champs
    Elysee, Assault on SF, Shore Patrol), Escort in the rest (get somewhere,
    rescue, protect or escort, destroy a target).
  - **Escort You**: they stay with you and attack enemies that come near you
    or them, then come back.
  - **Hunt Enemies**: they go after the enemy on their own, like the
    mission's own allies.
- **Extra Enemies** (0-15): AI tanks in the enemy gang. They come in when the
  first enemy tank shows up, at the spot its gang brings in its tanks (or next
  to it).
- **Enemy Class**: Random, Same as Theirs, or any of the 11 tank classes.
- **Enemy Orders**: Same as Theirs (what the mission's own tanks from that
  spot do, or Hunt if those just wait) or Hunt (they come after you and your
  allies).

The classes are M1A1, Mototank, Goliath, FLP-E Tank, Hovertank, Rattler,
Rhino, M-2 Hydra, Inferno, Marksman and Hornet. The Attack Boat and the boss
tanks aren't in the list.

All added tanks are the game's own AI tanks, made the same way the game makes
them. Each side gets its tanks once per mission. Restarting a mission adds
them again.

### Things to know
- **Class changes start with the next mission.** A level only loads the tank
  models it needs. The mod adds the classes you picked to every level's list
  (the same list the *All Tanks All Levels* cheat changes), and the level reads
  that list when it loads. If a class isn't loaded yet, the mod uses the
  mission's own class for that tank instead.
- **Random** loads every ordinary tank class, as *All Tanks All Levels* does.
  That needs more memory. btgarecomp has had room for it since 0.1.4.
- The game has 24 tank slots. The mod always leaves 4 free for the game's own
  tanks, so in a crowded mission you can get fewer than you asked for.
- The arrows over tanks (and their marks on the map) only show while your
  own tank has radar, for the mission's tanks and the added ones alike.
- Missions only. Other modes (Tank Wars, Battlelord and the rest) are left
  alone.

## Installing
1. Get `btga_gang_mod.nrm` from [Releases](https://github.com/wootbeer/btgarecomp-gang-mod/releases),
   or from the Artifacts of the latest [Build run](https://github.com/wootbeer/btgarecomp-gang-mod/actions).
2. Open the recomp's Mods tab and drop the `.nrm` file in.
3. Turn the mod on, set its options, and start a campaign mission.

Needs btgarecomp 0.1.8 or newer.

## Building
Same as the template:
- `clang` 18, `lld`, `make` and `zip`.
- `RecompModTool` from the [N64Recomp releases](https://github.com/N64Recomp/N64Recomp/releases/tag/mod-tool-release),
  in this folder.

Run `make nrm`. The mod is `build/btga_gang_mod.nrm`. Pushing to GitHub
builds it too (see the Build action's Artifacts). Pushing a new `version` in
`mod.toml` to `main` makes a release.

## Files
| File | What it is |
|---|---|
| `src/main.c` | The mod. |
| `include/btga.h` | Game addresses and functions the mod uses. |
| `NOTES.md` | How the game makes tanks, and where each address comes from. |
| `mod.toml` | Mod info and the Mods menu options. |
| `thumb.png` | The picture in the Mods menu. |
