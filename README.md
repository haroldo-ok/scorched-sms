# Scorched SMS

An artillery game for the **Sega Master System** in the spirit of the classic
MS-DOS game *Scorched Earth*. Written in C with
[devkitSMS](https://github.com/sverx/devkitSMS) (SMSlib + PSGlib) and SDCC.

2–4 tanks, human or computer, take turns lobbing shells across destructible
terrain. Earn cash for damage and kills, then spend it at the arms dealer
between rounds.

## Features

- **Destructible per-pixel terrain** with overhangs and tunnels, and an
  optional *falling dirt* mode where unsupported ground collapses.
- **33 weapons**:
  - **Missiles and nukes:** Baby Missile, Missile, Baby Nuke, Nuke
  - **Multi-hit and split shells:** LeapFrog, Funky Bomb, MIRV, Death's Head
  - **Fire:** Napalm, Hot Napalm
  - **Tracers:** Tracer, Smoke Tracer
  - **Rollers:** Baby, standard and Heavy
  - **Riot weapons:** Riot Charge, Riot Blast, Riot Bomb, Heavy Riot Bomb
  - **Diggers and Sandhogs:** three of each
  - **Dirt:** Dirt Clod, Dirt Ball, Ton of Dirt, Liquid Dirt, Dirt Charge,
    Earth Disrupter
  - **Energy:** Plasma Blast, Laser
- **15 accessories**:
  - **Shields:** Mag Deflector, Shield, Force Shield, Heavy Shield, Super Mag
  - **Survival:** Parachutes, Batteries, Auto Defense
  - **Fuel Tanks:** let your tank drive
  - **Contact Trigger**
  - **Guidance:** Heat, Ballistic, Horizontal and Vertical guidance, plus the
    Lazy Boy auto-aim
- **Tank movement.** Buy Fuel Tanks, then pick *MOVE TANK* from the in-game
  menu:
  - Moving 1 pixel costs 1 fuel, and climbing costs extra.
  - A step of more than 3 pixels blocks you.
  - Driving off a ledge makes you fall, with fall damage unless a parachute
    opens.
- **Talking tanks.** Speech bubbles appear when a tank fires and when it dies.
- **Four CPU skill levels:** Moron, Shooter, Poolshark and Cyborg. The CPU aims
  by simulating the real shot, including wind and shields. Higher levels aim
  better, shop smarter and use shields and batteries.
- **Wind:** off, normal, or changing between turns.
- **Sky styles,** chosen in setup like the original's Sky option:
  - **Random** (default): a different style each round.
  - **Plain:** a solid blue sky.
  - **Shaded:** a vertical blue gradient.
  - **Stars:** a black night sky dotted with stars.
  - **Stormy:** a dark, overcast sky with lightning bolts, a white flash and
    thunder every few seconds. The lightning is scenery only and does no
    damage.
  - **Sunset:** a purple-to-orange dusk gradient.
- **Auto end round (optional, on by default).** If every human player has been
  destroyed, the round ends at once instead of making you watch the CPUs finish
  it. There is no round winner, and play moves on to the shop. It does nothing
  in an all-CPU game.
- **System menu (PAUSE button).** Press the console's PAUSE button at any time
  during a round, including a CPU turn or a shell in flight:
  - **Resume:** carry on playing.
  - **End Round:** stop now with no winner.
  - **Mass Kill:** every remaining tank explodes.

  After End Round or Mass Kill, any shot in progress is cut short and the game
  goes to the round summary, then the shop.
- **Setup:** 3, 5 or 10 rounds, and a choice of starting cash.
- **Sound:** PSG sound effects for firing, the falling-shell whistle, explosions
  and napalm crackle.

## Controls

| Input | Action |
|---|---|
| D-pad Left / Right | Turret angle (in move mode: drive) |
| D-pad Up / Down | Shot power |
| Button 1 | Fire / confirm |
| Button 2 | In-game arsenal menu / back |
| PAUSE (console) | System menu: Resume, End Round, Mass Kill |

Pad 2 works too, for hot-seat play. The Master System has no keyboard, so the
console's PAUSE button takes the place of the DOS game's F9/Esc system-menu
keys. In an emulator, PAUSE is usually mapped to Enter or Start.

**In-game menu (Button 2):**
- **WEAPON:** cycle through the weapons you own (Left/Right).
- **GUIDANCE:** arm a guidance system for the next shot.
- **SHIELD:** choose a shield type, then activate it with Button 1.
- **BATTERY:** Button 1 restores 10 HP.
- **LAZY BOY:** pick a target and the turret aims itself.
- **TRIGGER:** toggle the Contact Trigger.
- **MOVE TANK:** enter drive mode.

Ballistic, Horizontal and Vertical guidance ask you to choose a target when you
fire.

## Building

Requirements: SDCC ≥ 4.2, devkitSMS (installed in `~/.devkitsms` with
`ihx2sms` on `PATH`), and Python 3.

```sh
make            # generates src/data.c from gen.py, builds scorched.sms
make clean
```

`checkmap.py` runs after linking and fails the build if the fixed code reaches
the ROM header at `0x7FE0`, bank 2 overflows, or RAM use grows too large.

### ROM layout

The ROM is 48 KB and does no bank switching.
- **Fixed area (`0x0000`–`0x7FFF`):** `main.c`, `terrain.c` and `weapons.c`,
  plus SMSlib.
- **Bank 2 (`0x8000`–`0xBFFF`):** `ui.c`, `ai.c` and the asset data, compiled
  with `--codeseg/--constseg BANK2`. Bank 2 stays mapped in that slot for the
  whole game. It is nearly full (about 700 bytes left), so more UI code will
  need another bank or a move into the fixed area, which has room to spare.

### Source map

| File | Contents |
|---|---|
| `src/game.h` | Shared types, constants, VRAM layout, weapon/item enums |
| `src/main.c` | Main loop, rounds, tanks, damage, shields, sound, frame loop |
| `src/terrain.c` | Bitmap terrain, dynamic tile pool, falling dirt (Z80 asm inner loops) |
| `src/weapons.c` | Weapon and item tables, projectile physics, guidance, every weapon behaviour |
| `src/ai.c` | CPU aiming (simulated shots), weapon choice, shopping |
| `src/ui.c` | HUD, menus, shop, setup, speech bubbles, movement, target selection |
| `gen.py` | Generates `src/data.c` / `data.h`: font, sprites, fixed tiles, star tiles, sine tables |

## How the terrain works

The Master System has no framebuffer, only 8×8 tiles. The terrain is therefore
kept as a 1-bit-per-pixel bitmap in RAM (256×168 pixels, 5,376 bytes).

Each 8×8 screen cell falls into one of three cases:
- **All sky:** uses a shared tile.
- **All ground:** uses a shared tile.
- **Mixed:** gets a private tile from a pool of 254 VRAM slots.

When a weapon changes the terrain, the affected cells are marked dirty, then
rebuilt and uploaded a few per frame. Building a tile and the falling-dirt pass
are written in Z80 assembly because SDCC's code for those loops was about 6×
slower. If the pool ever runs out, a cell is shown as plain sky or plain
ground, whichever is closer. Physics always uses the exact bitmap.

Sky styles cost almost nothing. Sky pixels, both in all-sky cells and in the
sky part of mixed cells, use background palette entries 1–8, one per
horizontal band. A style is just a different set of those eight colours, and
the lightning flash briefly turns all eight white. The starry sky also swaps
some all-sky cells for 16 star tiles: one star tile and one twinkle tile per
band, placed by a hash of the cell position.

## Testing

The game is tested headlessly with the `smstest` emulator from the
devkitSMS skill. The scripts are generated by `tests/gen_tests.py`:

```sh
python3 tests/gen_tests.py
smstest scorched.sms tests/playtest.txt   # 2 humans: every weapon class + items
smstest scorched.sms tests/items2.txt     # guidance, force shield, trigger, super mag, auto defense
smstest scorched.sms tests/soak.txt       # 4 CPUs (all skills), talking, changing wind, 3 rounds
smstest scorched.sms tests/features.txt   # auto end, PAUSE menu (End Round, Mass Kill), lightning
smstest scorched.sms tests/sky_stars.txt  # one per sky style: sky_plain/_shaded/_stars/_stormy/_sunset
```

All nine scripts pass. Each one drives the game with scripted input (including the console PAUSE
button), checks
game variables (state, ammo, HP, shields, fuel, guidance mode) and saves
screenshots to `shots/`.

## Known limitations

- **Turn length:** a CPU turn takes about 5 seconds. With large starting cash,
  CPUs stack shields and batteries, so rounds run long.
- **Collapsing terrain:** falling dirt drops straight down, column by column,
  so collapsed ground can look jagged.
- **Digger tunnels:** tunnels from diggers stay up until a later explosion
  nearby makes them collapse.
- **Round limit:** a round is called a draw after 200 turns.
- **Flicker:** with four tanks plus shields and parachutes on the same screen
  line, sprites flicker. That's a Master System hardware limit of 8 sprites
  per line.
- **Simulated force shields:** when the CPU (or the Lazy Boy) plans a shot, it
  treats a force shield as a solid hit, so it can be fooled into firing at a
  target the shell will bounce off.
- **Hardware:** the game has only been run in an emulator, not on a real
  console.

## Credits

Inspired by *Scorched Earth* (Wendell Hicken, 1991). All code, graphics and
tank quips in this project are original. The 5×7 font is a common public-domain
bitmap font.
