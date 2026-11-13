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
| `0x18` | first AI state. 7, 8, 14, 15, 16 start that state (`func_80086208`), anything else the normal one (state 0x19, `func_8008616C`) | |
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
