# Generates tests/playtest.txt (two humans, every weapon class + items) and tests/soak.txt
TS = 70                      # sizeof(tank_t)
OFF = dict(x=0, y=1, hp=2, alive=4, angle=5, power=6, weapon=7, ctrl=8, shield=11, shp=12,
           guide=13, trigger=14, chute=15, cash=18, score=20, ammo=22, items=55)
I = dict(CHUTE=0, BATTERY=1, MAG=2, SHIELD=3, FORCE=4, HEAVY=5, SUPERMAG=6, AUTODEF=7,
         FUEL=8, TRIGGER=9, HEAT=10, BALLISTIC=11, HORZ=12, VERT=13, LAZY=14)
def f(p, name, extra=0): return "tanks+%d" % (p * TS + OFF[name] + extra)

L = []
def a(*s): L.extend(s)
def taps(btn, n, gap=4):
    for _ in range(n): a("tap " + btn, "run %d" % gap)

a("symbols scorched.noi",
  "echo boot -> title", "waituntil game_state == 0 300 w1", "run 90",
  "expectvdp display 1", "expectcolors 8", "screenshot shots/00_title.ppm",
  "tap 1", "waituntil game_state == 1 60 w1", "run 20")
a("echo setup: P2 human, talking off")
taps("down", 2); taps("left", 2); taps("down", 4); taps("right", 1); taps("down", 4)
a("screenshot shots/01_setup.ppm", "tap 1",
  "waituntil game_state == 6 100 w1", "run 40")
a("echo shop: buy a missile pack, browse pages")
taps("1", 1, 6)
a("expect %s == 5 w1" % f(0, "ammo", 1), "expect %s == 4810" % f(0, "cash"))
taps("right", 2, 8); taps("down", 3, 6)
a("screenshot shots/02_shop.ppm")
taps("2", 1, 4)
a("waituntil game_state == 6 100 w1", "run 40")
taps("2", 1, 4)
a("waituntil game_state == 2 400 w1", "run 10",
  "expect num_players == 2 w1", "screenshot shots/03_aim.ppm")

a("echo aim responds to the pad",
  "hold left", "run 30", "hold none", "run 2",
  "expect %s > 60 w1" % f(0, "angle"))

cur = 0
oth = 1
a("echo item: shield from the arsenal menu",
  "poke %s 2 w1" % f(cur, "items", I["SHIELD"]),
  "tap 2", "run 6", "expect game_state == 9 w1")
taps("down", 2)
taps("1", 1, 6)
a("expect %s == 3 w1" % f(cur, "shield"), "expect %s == 55 w1" % f(cur, "shp"),
  "expect %s == 1 w1" % f(cur, "items", I["SHIELD"]),
  "screenshot shots/11_menu.ppm", "tap 2", "run 8", "expect game_state == 2 w1")
a("echo item: battery",
  "poke %s 50" % f(cur, "hp"), "poke %s 3 w1" % f(cur, "items", I["BATTERY"]),
  "tap 2", "run 6")
taps("down", 3); taps("1", 1, 6)
a("expect %s == 60" % f(cur, "hp"), "tap 2", "run 8")
a("echo item: fuel / tank movement",
  "poke %s 40 w1" % f(cur, "items", I["FUEL"]), "print %s w1" % f(cur, "x"),
  "tap 2", "run 6")
taps("down", 6); taps("1", 1, 6)
a("expect game_state == 10 w1",
  "hold left", "run 50", "hold none", "run 4", "screenshot shots/12_move.ppm",
  "print %s w1" % f(cur, "x"),
  "expect %s < 40 w1" % f(cur, "items", I["FUEL"]),
  "tap 1", "run 6", "expect game_state == 2 w1")
a("echo item: lazy boy auto-aim",
  "poke %s 1 w1" % f(cur, "items", I["LAZY"]),
  "poke %s 1 w1" % f(cur, "weapon"), "poke %s 5 w1" % f(cur, "ammo", 1),
  "tap 2", "run 6")
taps("down", 4); taps("1", 1, 6)
a("expect game_state == 11 w1", "run 10", "screenshot shots/13_target.ppm",
  "tap 1", "waituntil game_state == 2 600 w1",
  "expect %s == 0 w1" % f(cur, "items", I["LAZY"]),
  "print %s w1" % f(cur, "angle"), "print %s w1" % f(cur, "power"))

W = [("missile", 1, 14), ("nuke", 3, 16), ("leapfrog", 4, 40), ("funky", 5, 30),
     ("mirv", 6, 20), ("deathhead", 7, 30), ("napalm", 8, 60), ("smoke", 11, 0),
     ("roller", 13, 8), ("riotcharge", 15, 8), ("riotbomb", 17, 10), ("digger", 20, 20),
     ("sandhog", 23, 30), ("dirtball", 26, 12), ("liquid", 28, 25), ("dirtcharge", 29, 8),
     ("disrupter", 30, 6), ("plasma", 31, 14), ("laser", 32, 12), ("hvyroller", 14, 20)]
shots = 0
for name, w, delay in W:
    cur = shots % 2
    oth = 1 - cur
    a("echo weapon: " + name,
      "poke wind 0 w1", "poke wind_acc 0", "poke wind_acc+2 0")
    for p in (0, 1):
        a("poke %s 300" % f(p, "hp"),
          "poke %s %d w1" % (f(p, "weapon"), w),
          "poke %s 3 w1" % f(p, "ammo", w),
          )
    proj_w = name not in ("riotcharge", "dirtcharge", "plasma", "laser")
    if proj_w:         # aim at the other tank with a Lazy Boy (also tests the item)
        a("poke %s 1 w1" % f(cur, "items", I["LAZY"]), "tap 2", "run 6")
        taps("down", 4); taps("1", 1, 6)
        a("expect game_state == 11 w1", "tap 1", "waituntil game_state == 2 600 w1",
          "expect %s == 0 w1" % f(cur, "items", I["LAZY"]))
    if shots == 0:     # overshoot on purpose; vertical guidance drops it on the target
        a("echo   + vertical guidance",
          "poke %s 1 w1" % f(cur, "items", I["VERT"]), "poke %s 100 w1" % f(cur, "power"),
          "tap 2", "run 6")
        taps("down", 1); taps("right", 1)
        a("expect %s == 13 w1" % f(cur, "guide"), "tap 2", "run 8")
    if shots == 1:     # lift the other tank; it must parachute down without damage
        a("echo   + parachute on the other tank",
          "poke %s 2 w1" % f(oth, "items", I["CHUTE"]), "poke %s 40 w1" % f(oth, "y"))
    a("run 2", "tap 1")
    if shots == 0:
        a("expect game_state == 11 w1", "tap 1",
          "waituntil proj+19 == 2 600 w1", "run 4", "screenshot shots/04_vert.ppm")
    if shots == 1:
        a("waituntil %s == 1 900 w1" % f(oth, "chute"), "run 30", "screenshot shots/05_chute.ppm")
    shots += 1
    if name == "smoke" or shots == 2:
        a("waituntil game_state == 2 3000 w1", "run 4", "screenshot shots/w_%s.ppm" % name)
    else:
        a("waituntil game_state == 4 900 w1", "run %d" % delay, "screenshot shots/w_%s.ppm" % name,
          "waituntil game_state == 2 4000 w1")
    if shots == 2:
        a("expect %s == 1 w1" % f(0, "items", I["CHUTE"]), "expect %s > 200" % f(0, "hp"))
    a("expect shots_fired == %d w1" % shots,
      "expect %s == 2 w1" % f(cur, "ammo", w))
a("run 5", "screenshot shots/10_after_weapons.ppm", "print pool_free w1")
shots_end = shots

shots += 1

shots = shots_end - 1
a("echo end the round: kill the other tank",
  "poke %s 0" % f(1 - shots % 2, "hp"),
  "poke %s 0 w1" % f(shots % 2, "weapon"),
  "tap 1", "waituntil game_state == 5 2000 w1", "run 20",
  "screenshot shots/16_roundend.ppm", "expect cur_round == 0 w1",
  "tap 1", "waituntil game_state == 6 200 w1", "run 40", "tap 2",
  "waituntil game_state == 6 200 w1", "run 40", "tap 2",
  "waituntil game_state == 2 600 w1", "expect cur_round == 1 w1",
  "echo last round -> game over -> title",
  "poke cur_round 4 w1",
  "poke %s 0" % f(1 - (shots + 1) % 2, "hp"),
  "poke %s 0 w1" % f(0, "weapon"), "poke %s 0 w1" % f(1, "weapon"),
  "tap 1", "waituntil game_state == 5 3000 w1", "run 10", "tap 1",
  "waituntil game_state == 7 200 w1", "run 10", "screenshot shots/17_gameover.ppm",
  "tap 1", "waituntil game_state == 0 600 w1",
  "expectpace 0", "spmin")
open("tests/playtest.txt", "w").write("\n".join(L) + "\n")

# ---- soak: 4 CPUs of every skill, talking on, changing wind, 3 rounds
L = []
a("symbols scorched.noi", "waituntil game_state == 0 300 w1", "run 100", "tap 1",
  "waituntil game_state == 1 60 w1", "run 20")
taps("right", 2)                         # 4 players
taps("down", 1); taps("right", 1)        # P1 MORON
taps("down", 1); taps("right", 0)        # P2 SHOOTER (default)
taps("down", 1); taps("right", 1)        # P3 POOLSHARK
taps("down", 1); taps("right", 2)        # P4 CYBORG
taps("down", 1); taps("left", 1)         # rounds 3
taps("down", 1); taps("right", 1)        # cash 10000
taps("down", 3); taps("right", 1)        # wind changing
taps("down", 3)                          # past SKY / AUTO END to START
a("screenshot shots/s0_setup.ppm", "tap 1",
  "waituntil game_state == 8 600 w1",
  "expect num_players == 4 w1", "expect num_rounds == 3 w1",
  "expect tanks+8 == 1 w1", "expect tanks+218 == 4 w1",
  "shots shots/s_r1_ 6 400")
for r in range(3):
    a("waituntil game_state == 5 120000 w1", "frames", "run 30",
      "screenshot shots/s%d_round.ppm" % (r + 1), "tap 1")
    if r < 2:
        a("waituntil game_state == 8 600 w1", "shots shots/s_r%d_ 3 500" % (r + 2))
a("waituntil game_state == 7 300 w1", "run 10", "screenshot shots/s9_gameover.ppm",
  "print shots_fired w1", "tap 1", "waituntil game_state == 0 600 w1",
  "expectpace 0", "spmin")
open("tests/soak.txt", "w").write("\n".join(L) + "\n")

# ---- items2: guidance, force shield, contact trigger, super mag, auto defense
L = []
a("symbols scorched.noi",
  "waituntil game_state == 0 300 w1", "run 90", "tap 1", "waituntil game_state == 1 60 w1", "run 20")
taps("down", 2); taps("left", 2); taps("down", 4); taps("right", 1); taps("down", 4)
a("tap 1", "waituntil game_state == 6 100 w1", "run 40", "tap 2",
  "waituntil game_state == 6 100 w1", "run 40", "tap 2", "waituntil game_state == 2 400 w1", "run 10")
def lazy(cur):
    a("poke %s 1 w1" % f(cur, "items", I["LAZY"]), "tap 2", "run 6")
    taps("down", 4); taps("1", 1, 6)
    a("tap 1", "waituntil game_state == 2 600 w1", "run 3")
def prep():
    a("poke wind 0 w1", "poke wind_acc 0", "poke wind_acc+2 0")
    for p in (0, 1):
        a("poke %s 300" % f(p, "hp"), "poke %s 1 w1" % f(p, "weapon"), "poke %s 5 w1" % f(p, "ammo", 1))
a("echo horizontal guidance (P1)"); prep(); lazy(0)
a("poke %s 1 w1" % f(0, "items", I["HORZ"]), "poke %s 100 w1" % f(0, "power"), "tap 2", "run 6")
taps("down", 1); taps("right", 1)
a("expect %s == 12 w1" % f(0, "guide"), "tap 2", "run 8", "tap 1", "run 6", "tap 1",
  "waituntil proj+19 == 1 600 w1", "run 4", "screenshot shots/g_horz.ppm",
  "waituntil game_state == 2 3000 w1")
a("echo ballistic guidance fixes a bad power setting (P2)"); prep(); lazy(1)
a("poke %s 1 w1" % f(1, "items", I["BALLISTIC"]), "poke %s 15 w1" % f(1, "power"), "tap 2", "run 6")
taps("down", 1); taps("right", 1)
a("expect %s == 11 w1" % f(1, "guide"), "tap 2", "run 8", "tap 1", "run 6", "tap 1",
  "waituntil game_state == 4 900 w1", "expect %s > 25 w1" % f(1, "power"), "run 6", "screenshot shots/g_ballistic.ppm",
  "waituntil game_state == 2 3000 w1", "expect %s < 300" % f(0, "hp"))
a("echo force shield bounces a shell (P1 fires at shielded P2)"); prep(); lazy(0)
a("poke %s 4 w1" % f(1, "shield"), "poke %s 100 w1" % f(1, "shp"), "tap 1",
  "waituntil game_state == 2 3000 w1", "expect %s == 300" % f(1, "hp"))
a("echo contact trigger: roller detonates on impact (P2)"); prep(); lazy(1)
a("poke %s 13 w1" % f(1, "weapon"), "poke %s 3 w1" % f(1, "ammo", 13),
  "poke %s 5 w1" % f(1, "items", I["TRIGGER"]), "tap 2", "run 6")
taps("down", 5); taps("right", 1)
a("expect %s == 1 w1" % f(1, "trigger"), "tap 2", "run 8", "tap 1",
  "waituntil game_state == 2 3000 w1", "expect %s == 4 w1" % f(1, "items", I["TRIGGER"]),
  "expect trig_armed == 1 w1")
a("echo super mag deflects (P1 fires at P2)"); prep(); lazy(0)
a("poke %s 6 w1" % f(1, "shield"), "poke %s 60 w1" % f(1, "shp"), "tap 1",
  "waituntil game_state == 3 300 w1", "run 50", "screenshot shots/g_mag.ppm",
  "waituntil game_state == 2 3000 w1")
a("echo auto defense raises the best shield at the next round",
  "poke %s 1 w1" % f(0, "items", I["AUTODEF"]), "poke %s 1 w1" % f(0, "items", I["HEAVY"]),
  "poke %s 0" % f(0, "hp"), "poke %s 0 w1" % f(1, "weapon"), "tap 1",
  "waituntil game_state == 5 3000 w1", "run 10", "tap 1",
  "waituntil game_state == 6 200 w1", "run 40", "tap 2", "waituntil game_state == 6 200 w1", "run 40", "tap 2",
  "waituntil game_state == 2 600 w1",
  "expect %s == 5 w1" % f(0, "shield"), "expect %s == 150 w1" % f(0, "shp"),
  "expect %s == 0 w1" % f(0, "items", I["AUTODEF"]), "run 4", "screenshot shots/g_autodef.ppm",
  "expectpace 0")
open("tests/items2.txt", "w").write("\n".join(L) + "\n")

# ---- features: auto end (no humans left), PAUSE system menu, lightning
L = []
a("symbols scorched.noi",
  "waituntil game_state == 0 300 w1", "run 90", "tap 1", "waituntil game_state == 1 60 w1", "run 20")
taps("right", 1)                          # 3 players: P1 human, P2/P3 shooter
taps("down", 4); taps("left", 1)          # rounds 5 -> 3
taps("down", 3); taps("right", 1)         # talking off
taps("down", 2); taps("right", 4)         # sky RANDOM -> STORMY
a("expect opt_sky == 4 w1", "expect opt_autoend == 1 w1", "screenshot shots/f0_setup.ppm")
taps("down", 2)
a("tap 1", "waituntil game_state == 6 100 w1", "run 40", "tap 2",
  "waituntil game_state == 2 900 w1", "expect cur_sky == 4 w1",
  "echo lightning", "waituntil bolt_t == 8 900 w1", "screenshot shots/f1_lightning.ppm")
a("echo auto end: the only human dies, two CPUs still alive",
  "poke %s 0" % f(0, "hp"), "poke %s 0 w1" % f(0, "weapon"), "poke %s 90 w1" % f(0, "angle"),
  "poke %s 20 w1" % f(0, "power"), "run 3", "tap 1",
  "waituntil game_state == 5 1500 w1",
  "expect %s == 1 w1" % f(1, "alive"), "expect %s == 1 w1" % f(2, "alive"),
  "expect cur_round == 0 w1", "run 10", "screenshot shots/f2_autoend.ppm",
  "tap 1", "waituntil game_state == 6 200 w1", "run 40", "tap 2")
a("echo PAUSE -> END ROUND during a CPU turn",
  "waituntil game_state == 8 2000 w1", "run 10", "pause", "run 4",
  "expect game_state == 13 w1", "screenshot shots/f3_sysmenu.ppm",
  "tap down", "run 4", "tap 1", "waituntil game_state == 5 300 w1",
  "expect cur_round == 1 w1", "expect %s == 1 w1" % f(1, "alive"),
  "run 10", "screenshot shots/f4_endround.ppm",
  "tap 1", "waituntil game_state == 6 200 w1", "run 40", "tap 2")
a("echo PAUSE -> MASS KILL",
  "waituntil game_state == 2 2000 w1", "run 10", "pause", "run 4",
  "expect game_state == 13 w1", "tap down", "run 4", "tap down", "run 4", "tap 1",
  "waituntil game_state == 4 200 w1", "run 6", "screenshot shots/f5_masskill.ppm",
  "waituntil game_state == 5 600 w1",
  "expect %s == 0 w1" % f(0, "alive"), "expect %s == 0 w1" % f(1, "alive"), "expect %s == 0 w1" % f(2, "alive"),
  "run 10", "screenshot shots/f6_masskill_summary.ppm",
  "tap 1", "waituntil game_state == 7 300 w1", "run 20", "tap 1", "waituntil game_state == 0 600 w1",
  "echo PAUSE is ignored outside a round", "pause", "run 4", "expect game_state == 0 w1",
  "expectpace 0", "spmin")
open("tests/features.txt", "w").write("\n".join(L) + "\n")

# ---- one short script per sky style (screenshot of the first turn)
for sky, name in [(1, "plain"), (2, "shaded"), (3, "stars"), (4, "stormy"), (5, "sunset")]:
    L = []
    a("symbols scorched.noi",
      "waituntil game_state == 0 300 w1", "run 90", "tap 1", "waituntil game_state == 1 60 w1", "run 20")
    taps("down", 8); taps("right", sky)
    a("expect opt_sky == %d w1" % sky)
    taps("down", 2)
    a("tap 1", "waituntil game_state == 6 100 w1", "run 40", "tap 2",
      "waituntil game_state == 2 900 w1", "expect cur_sky == %d w1" % sky,
      "run 20", "screenshot shots/sky_%d_%s.ppm" % (sky, name))
    open("tests/sky_%s.txt" % name, "w").write("\n".join(L) + "\n")
