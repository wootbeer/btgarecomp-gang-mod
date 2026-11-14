# Notes: how the game makes tanks

Found by disassembling the USA 1.0 ROM (`vram = rom offset + 0x80070000`).
Function names are the ones in `syms/battletanxga.us.rev0.syms.toml`.

## Tank objects
- 24 slots of `0x4CC` bytes from `0x801AD128` (`0x7320` bytes, cleared by
  `func_8008AD80` at each level start). `0x801AD120` (u16) counts the tanks
  made.
- A slot is in use while bit 0 of `+0x1E0` is set.

| Offset | What |
|---|---|
| `+0x08` / `+0x0C` / `+0x10` | x / z / y (f32) |
| `+0x20` | heading (u16) |
| `+0x94` | map layer (passed to the spot check and effects) |
| `+0x95` | gang index, `0x7F` = none |
| `+0x98` | tank type |
| `+0x9C` | control: 0 human, 1 or 2 AI, 3 special, 4 none |
| `+0x1D0` | gang pointer |
| `+0x1D4` / `+0x1D8` | health / max health |
| `+0x1E0` | flags (bit 0 in use) |
| `+0x4A0` | the tank generator that made it, or NULL |

## Making a tank: `func_8008AE04(TankSpawn*)`
Takes a 0x2C-byte record (`TankSpawn` in `include/btga.h`):

| Offset | What | Goes to |
|---|---|---|
| `0x00` / `0x04` / `0x08` | x / z / y | `+0x08` / `+0x0C` / `+0x10` |
| `0x0C` | heading | `+0x20` |
| `0x0E` | layer | `+0x94` |
| `0x0F` | gang | `+0x95` |
| `0x10` | tank type | `+0x98` |
| `0x14` | control (0 human, 1/2 AI) | `+0x9C`, through `func_80089F40` |
| `0x18` | AI state. 7, 8, 14, 15, 16 start right away (`func_80086208`). Anything else: the tank first drives clear of its spot (state 25, `func_80085C30` / `func_80085D58`), then goes to this state | `+0x18C`, then `+0x168` |
| `0x1C` | flags, OR 1 | `+0x1E0` |
| `0x20` / `0x24` | weapons, given with `func_8008ECB4` | |
| `0x28` | tank generator, or NULL | `+0x4A0` |

It returns the tank, or NULL when no slot is free. A human tank (control 0)
takes an AI tank's slot when none is free.

When a tank dies (`0x8008BC80`, `0x80095044`) the game checks `+0x4A0` for
NULL before counting it off its generator, so tanks with no generator are fine.

## Who calls it
- `func_800DEB1C`: a **tank generator** (object type `0x0D`, the update
  callback set at `0x800A25A4`) making an enemy tank. The generator's fields:
  `+0x0C`/`+0x10` x/z, `+0x14` heading, `+0x16` layer, `+0x17` gang, `+0x18`
  tanks left (`0xFFFF` = no limit), `+0x1C` tank-type mask (it cycles through
  them), `+0x20` time of the last one, `+0x24` delay, `+0x28` kind, `+0x29`
  first AI state, `+0x2A` most alive at once, `+0x2B` alive now. The record it
  passes: control 2 if the gang has flag 2 (`gang +0x0A`), else 1; weapons
  from `gang +0x1F4` / `+0x1F8`; generator in `0x28`.
- `func_800A9080`: a gang making a tank at its base (control 0, the player's
  Change Tank). Called from `func_800D6EEC`.
- The calls at `0x800A7A44`, `0x800A7B14` and `0x800D241C`: other cases, not traced
  yet.

## Spot check: `func_8008F4AC`
`(type, x, z, heading, layer, check_ground, alt, ignore)`. Returns nonzero
when the tank fits: on the map, on the ground, clear of others. The generators
call it with `check_ground = 1`, `alt = 0`.

## Gangs
`0x250` bytes each from `0x80235F00`. `+0x0A` flags, `+0x0B` own index,
`+0x1B8` lists of its tanks by control, `+0x1C8` counts by control,
`+0x1D0` / `+0x1D4` health multipliers, `+0x1F4` / `+0x1F8` AI weapons.

## Tank types
Table at `0x80122E30`, `0xD0` bytes each. `+0x00` part count, `+0x04` parts
list (pairs: part, model id), `+0x08` name.

0 M1A1, 1 Mototank, 2 Goliath, 3 FLP-E Tank, 4 Hovertank, 5 Rattler,
6 Rhino, 7 M-2 Hydra, 8 Inferno, 9 Marksman, 10 Hornet, 11 Attack Boat,
12-14 TANK_BOSS_A-C.

Loaded models are at `0x803A53A0` by model id (NULL = not loaded,
`func_80095FE0` walks them the same way).

## Levels
- 19 campaign level records, `0x70` bytes from `0x80124B90`. `+0x46` the
  tanks you can pick, `+0x4A` the tank types the level loads. (The
  *All Tanks All Levels* GameShark code writes both.)
- Game mode at `0x80117EB4` (`func_800C041C` names them): 1 Campaign,
  2 Deathmatch, 3 Battlelord, 4 Frenzy, 5 Hold 'Em, 6 Family Mode,
  7 Tank Wars, 8 Convoy.
- Level clock at `0x8021945C`, 30 per second, set to 0 at a level start
  (`func_80099FE8`).

## AI states
Tank `+0x168`. Entered through `func_80086208` (table at `0x800713F0`), run
each frame through `func_80086560` (table at `0x80071470`).
- 4 (`func_80082F98` / nothing): stop and stay put. Human tanks sit here.
- 5 (`func_80082FE8` / `func_80083028`): follow the gang's leader (the first
  tank in the gang's human list, `gang +0x1B8`) within 400 units, or keep on
  a target it has. Seen in the game: tanks in this state barely move or fight
  (their `+0x170` stays 1 from the drive-out). The game's own allies don't
  use it.
- 6 (`func_800830B4` / `func_800830F8`): every frame, pick a target with
  `func_80088854` and attack it; 0x13 or 0x15 (retreat) when hurt. The target
  is kept only if it's a human tank (`+0x1E0` bit 2) of another team;
  otherwise it takes the first live tank of the first gang (of 5) with another
  team (`func_800A9928` walks the gang's lists in order), wherever it is on
  the map. This is what the missions' own allies run (SF Airport: three M1A1s
  in your gang, control 2, state 6).
- 25 (`func_80085C30` / `func_80085D58`): a new tank driving clear of its
  spot, then the state in `+0x18C`.

## Radar arrows
`func_800C8B74` (from `0x80092468`) runs only while the viewing tank has
radar (`+0x1E0` flag `0x200`), and lists every tank object (generic object
type 4) on the viewer's layer (`+0x94`).

## AI targets
- `+0xC4` attack (1) or just go there (0), `+0xC5` target kind (1 a tank in
  `+0xC8`; 2 a point in `+0xC8`/`+0xCC`; 3 a waypoint object), `+0xB4` how
  close to get (f32), `+0xC0` give-up time.
- `func_80088ABC(tank, target, range, attack)` sets a tank target (kind 1).
  `func_80088B6C(tank, point, range, attack)` sets a point.
- The driving code (`func_8007E64C`) heads for `func_8008865C`'s goal: for a
  tank target, that tank's `+0x150`/`+0x154` (its position, kept up to date
  for human and AI tanks alike).
- The mod's Escort hooks `func_80088854` (before, to note the tank; after, to
  replace its pick): the nearest enemy within 1600 of you (or of the escort,
  if within 2600 of you), attacked at the tank's `+0x1C0` range; with none,
  you, at 400, not attacked.

## Watched in the game (SF Airport and SF Breakout)
- Mission 1 can start your tank without radar (`+0x1E0` 0x003) or with it
  (0x203), so the arrows aren't always there.
- With Escort, the allies go out to enemies that come near, kill them, and
  come back to you.

## Levels and goals
The level being played is the word at `0x8021949C` (26 in SF Airport, 0 in
SF Breakout, checked in the game). `func_800E8C88(level)` gives its name,
`func_800E8E34(level)` its goal:

| Id | Level | Goal |
|---|---|---|
| 26 | SF Airport | Destroy all invading tanks |
| 0 | SF Breakout | Get to the escape ship |
| 1 | Truck Stop | Destroy all tanks and bunkers |
| 2 | Texas Slave Fortress | Rescue all prisoners |
| 3 | Drive In | Destroy both projector buildings |
| 4 | DC Mall | Capture all data and escape |
| 5 | White House | Destroy all enemies |
| 6 | Houses of Parliament | Rescue all Iron Maidens and escape |
| 7 | Tower Bridge | Get across the bridge |
| 8 | Tower of London | Destroy all tanks and boats |
| 9 | Bistro | Get all four scientists in your base |
| 10 | Champs Elysee | Destroy all tanks |
| 11 | Eiffel Tower | Destroy the Eiffel Tower reactor |
| 12 | Berlin War Zone | Rescue all prisoners and escape |
| 13 | Brandenburg Gate | Escort the convoy into Berlin |
| 14 | Escape from Berlin | Protect the convoy |
| 15 | Assault on SF | Destroy all tanks and boats |
| 16 | Alcatraz | Find and destroy the Annihilator |
| 24 | Shore Patrol | Destroy all tanks and boats |

17-21 are the multiplayer maps (Lakepark, Panhandle, Railyard, SFO,
Crossfire). Ally Orders' Auto hunts in the "destroy all" levels (26, 1, 5, 8,
10, 15, 24) and escorts in the rest.

## Objects and trains
- Generic objects: 0x44-byte entries from `0x80224EF0`. `+0x00` serial (0 =
  free), `+0x04` type, `+0x08` next index in the type's list (s16, -1 ends),
  then the object's data. List heads by type at `0x80224E68` (s16 each), free
  list head at `0x80224B50`. `func_800A18D0(type, size)` makes one,
  `func_800A1A28(prev, type)` walks a type.
- Per-type callbacks, 12 bytes per type from `0x80224B58`: `+0` update (run by
  the loop at `0x800A1DDC`; an update that sets its result to 1 has its object
  freed at `0x800A1E94`), `+4` and `+8` used elsewhere (`0x80090BBC`,
  `0x800A20B0`).
- Type 1 is a train (update `func_800EED90`; `+0x2C` distance along the track,
  `+0x30` first car, `+0x40` engine sound handle, stopped with
  `func_80097BA4(handle, 0)`). Type 2 is a train car (`+0x10` its train,
  `+0x18` its distance), made by the code at `0x800EF400`; cars have no update
  of their own. Truck Stop has two trains with 12 cars.
- The campaign is a list of steps at `0x801254C0` (cutscenes and levels;
  level steps have the level id at `+0x04`), the current one at `0x803A8310`.
  Setting it (and `0x8021949C`) then choosing Restart in the pause menu loads
  that level: SF Airport is step 3, SF Breakout 5, Truck Stop 7.

## Ally spawning and catch-up (watched in Truck Stop)
- Spawned around the player, some allies landed behind the road barriers and
  never moved. A gang's base spots (`gang +0x1E8`: 5 of 12 bytes, x, z,
  heading, layer; `func_800A9080` uses them for the gang's own tanks) are
  always drivable, so allies now come in there.
- At full speed the player outruns the AI. Escorts more than 3000 away are
  put on a free spot (checked with `func_8008F4AC`) 500-900 behind the player,
  along the way the player has driven in the last second, with `+0x08..0x1C`,
  `+0x110/0x114` and `+0x150/0x154` set to the new place.

## Train tracks
Each train runs on a straight track: from (`+0x0C`, `+0x10`) along the angle
at `+0x24` (u16) for `+0x14` units; a car's place is that line at its
distance (`func_800EEADC`, with `func_8009D4B0` = sin and `func_8009D510` =
cos of a 16-bit angle). Truck Stop: z = 72 (angle 0x4000) and z = 216
(0xC000); the road is about 230 from the nearer one, the player gang's base
spots 170-180. The mod refuses spawn spots within 150 of a track.

## Mission allies vs the mod's (SF Airport, compared in the game)
The whole 0x4CC-byte tank of a mission ally (M1A1 from a generator) and one
of the mod's (M1A1, Hunt) differ only in what they're doing at the moment:
`+0x30`/`+0x5C` (motion), `+0xAC` (who it can see), `+0xE8..0xEC` (an attack
run, `func_80089040`), `+0x140`/`+0x14C` (driving), `+0x230`/`+0x238` (last
hit). Their setup is the same: same gang, control, state, skill, weapons.

## AI skill and firing
- Skill table at `0x80125C30`, 6 floats per level (10 levels), the gang's
  level at `gang +0x1F0`. `func_80082A90` copies a level into the tank:
  `+0x1B8` = [0] / `gang +0x1DC` (shot delay), `+0x1BC` = [1] x the class's
  reach (`type +0xB8`, u16: M1A1 3500), sight; `+0x1C0` = [2] x reach, attack
  range; `+0x1C4` = [3]; `+0x1C8` = [4] x health x `type +0xBC`, the health
  it retreats at; `+0x1CC` = [5], the chance of using a special weapon.
- Firing (`0x800873FC`): when the level clock passes `+0xD8` and `+0xDC`,
  pick a weapon (`func_80089200`: a special weapon if the target is within
  sight and a roll is under `+0x1CC`), fire (`func_80087DD4`), then wait
  `(75 + rand 15) * +0x1B8 * 0.25` frames. Sight (`+0x1BC`) also decides
  which tanks see each other (`0x80089984`, the smaller of the two).
- The player's gang in SF Airport / Truck Stop is level 1: shot delay 2.5
  (/1.1), sight 0.4, range 0.2, special weapons 0. Level 8 is the best: 1.0,
  0.99, 0.4.

## Full missions (Champs Elysee)
Champs Elysee (level 10, campaign step 20) has 23 of the 24 tank slots in use
a few seconds in. `func_8008AE04` makes room for a human tank (control 0) by
taking out an AI tank with `func_8008B82C(tank, 0x7F, 0)`: no kill credited,
its generator's alive count (`+0x2B`) goes down, and the slot is free at once.
The mod does the same for its allies (an enemy 3500+ from the player, not a
boat or boss) and adds one back to that generator's tanks left (`+0x18`).
