#!/usr/bin/env python3
# Generates src/data.c + src/data.h : font, fixed tiles, sprite tiles, LUTs
import math
F5x7 = """00 00 00 00 00|00 00 5F 00 00|00 07 00 07 00|14 7F 14 7F 14|24 2A 7F 2A 12|23 13 08 64 62|36 49 55 22 50|00 05 03 00 00|00 1C 22 41 00|00 41 22 1C 00|14 08 3E 08 14|08 08 3E 08 08|00 50 30 00 00|08 08 08 08 08|00 60 60 00 00|20 10 08 04 02|3E 51 49 45 3E|00 42 7F 40 00|42 61 51 49 46|21 41 45 4B 31|18 14 12 7F 10|27 45 45 45 39|3C 4A 49 49 30|01 71 09 05 03|36 49 49 49 36|06 49 49 29 1E|00 36 36 00 00|00 56 36 00 00|08 14 22 41 00|14 14 14 14 14|00 41 22 14 08|02 01 51 09 06|32 49 79 41 3E|7E 11 11 11 7E|7F 49 49 49 36|3E 41 41 41 22|7F 41 41 22 1C|7F 49 49 49 41|7F 09 09 09 01|3E 41 49 49 7A|7F 08 08 08 7F|00 41 7F 41 00|20 40 41 3F 01|7F 08 14 22 41|7F 40 40 40 40|7F 02 0C 02 7F|7F 04 08 10 7F|3E 41 41 41 3E|7F 09 09 09 06|3E 41 51 21 5E|7F 09 19 29 46|46 49 49 49 31|01 01 7F 01 01|3F 40 40 40 3F|1F 20 40 20 1F|3F 40 38 40 3F|63 14 08 14 63|07 08 70 08 07|61 51 49 45 43|00 7F 41 41 00|02 04 08 10 20|00 41 41 7F 00|04 02 01 02 04|40 40 40 40 40"""
font = []
for ch in F5x7.split('|'):
    cols = [int(v, 16) for v in ch.split()]
    for y in range(8):
        b = 0
        for x in range(5):
            if cols[x] >> y & 1:
                b |= 0x80 >> (x + 1)
        font.append(b)
assert len(font) == 512

def tile(rows):
    out = []
    for r in rows:
        for p in range(4):
            b = 0
            for x in range(8):
                if r[x] >> p & 1: b |= 0x80 >> x
            out.append(b)
    return out

def pic(s, cmap):
    rows = [l for l in s.strip().split('\n')]
    assert len(rows) == 8 and all(len(r) == 8 for r in rows), s
    return tile([[cmap.get(c, 0) for c in r] for r in rows])

def pic16(s, cmap):
    rows = [l for l in s.strip().split('\n')]
    assert len(rows) == 8 and all(len(r) == 16 for r in rows), s
    return pic('\n'.join(r[:8] for r in rows), cmap) + pic('\n'.join(r[8:] for r in rows), cmap)

# ---- fixed BG tiles: 8 sky bands, bedrock
bg = []
for c in range(1, 9): bg += tile([[c]*8]*8)
g = [[12]*8 for _ in range(8)]
for (x, y) in [(1,1),(5,2),(3,4),(7,5),(0,6),(4,7),(2,2),(6,6)]: g[y][x] = 0
bg += tile(g)

# ---- sprite tiles (order must match S_* in game.h)
spr = []
TANK = """
........
........
........
..XXXX..
.XXXXXX.
XXXXXXXX
KXKXXKXK
.KKKKKK.
"""
for col in (2, 3, 4, 5):
    spr += pic(TANK, {'X': col, 'K': 9})
for f in range(19):   # turret frames, 10 degrees each
    a = math.radians(f * 10)
    g = [[0]*8 for _ in range(8)]
    for i in range(0, 21):
        t = i * 0.25
        x = int(math.floor(4.0 + t * math.cos(a))) if f <= 9 else int(math.ceil(3.0 + t * math.cos(a)))
        y = int(round(7 - t * math.sin(a)))
        if 0 <= x < 8 and 0 <= y < 8: g[y][x] = 6
    spr += tile(g)
spr += pic("""
........
........
...WW...
..WWWW..
..WWWW..
...WW...
........
........
""", {'W': 1})                                   # 23 shot
spr += pic("""
...W....
..WWW...
.WWWWW..
...W....
...W....
........
........
........
""", {'W': 1})                                   # 24 off-screen marker
FIRE = """
..OOOO..
.OYYYYO.
OYYWWYYO
OYWWWWYO
OYWWWWYO
OYYWWYYO
.OYYYYO.
..OOOO..
"""
spr += pic(FIRE, {'O': 7, 'Y': 3, 'W': 1})      # 25
spr += pic(FIRE, {'O': 8, 'Y': 7, 'W': 3})      # 26
spr += pic("""
........
.WWWWWW.
.WWWWWW.
..WWWW..
..WWWW..
...WW...
...WW...
........
""", {'W': 1})                                   # 27 arrow
DOME = """
.....XXXXXX.....
...XX......XX...
..X..........X..
.X............X.
.X............X.
X..............X
X..............X
X..............X
"""
spr += pic16(DOME, {'X': 10})                    # 28,29 shield
spr += pic16(DOME, {'X': 11})                    # 30,31 mag deflector
spr += pic16("""
...XXXXXXXXXX...
.XXXXXXXXXXXXXX.
XXXXXXXXXXXXXXXX
X.X..X.XX.X..X.X
.X............X.
..X..........X..
...X........X...
....X......X....
""", {'X': 12})                                  # 32,33 parachute
spr += pic("""
...Y....
..YO....
..OOY...
.YOWO...
.OWWOY..
OWWWWO..
OWWWWWO.
.OOOOO..
""", {'O': 7, 'Y': 3, 'W': 3})                   # 34 flame
spr += pic("""
....Y...
...OY...
..YOO...
..OWOY..
.YOWWO..
.OWWWWO.
OWWWWWO.
.OOOOO..
""", {'O': 8, 'Y': 7, 'W': 3})                   # 35 flame 2
spr += pic("""
........
........
........
...SS...
...SS...
........
........
........
""", {'S': 6})                                   # 36 smoke dot
spr += pic("""
...R....
...R....
...R....
RRR.RRR.
...R....
...R....
...R....
........
""", {'R': 1})                                   # 37 crosshair
spr += pic("""
........
........
...PP...
..PWWP..
..PWWP..
...PP...
........
........
""", {'P': 11, 'W': 1})                          # 38 laser
spr += pic("""
........
..GGG...
.GWGGG..
.GGGGG..
.GGGGG..
..GGG...
........
........
""", {'G': 6, 'W': 1})                           # 39 roller

BOLT_R = """
....WY..
....WY..
....WWY.
.....WY.
.....WY.
.....WWY
......WY
......WY
"""
spr += pic(BOLT_R, {'W': 1, 'Y': 3})            # 40 bolt segment, drifts right
spr += pic('\n'.join(r[::-1] for r in BOLT_R.strip().split('\n')), {'W': 1, 'Y': 3})  # 41 drifts left

# ---- star tiles: for each of the 8 sky bands, two variants (sky colour + white stars)
stars = []
for b in range(8):
    for v in range(2):
        g2 = [[b + 1] * 8 for _ in range(8)]
        pts = [(2, 1), (6, 5)] if v == 0 else [(5, 2)]
        for (x, y) in pts: g2[y][x] = 15
        if v == 1:
            g2[1][5] = g2[3][5] = 12; g2[2][4] = g2[2][6] = 12   # twinkle cross (grey)
        stars += tile(g2)

def carr(name, data, typ="unsigned char"):
    s = "const %s %s[%d] = {\n" % (typ, name, len(data))
    for i in range(0, len(data), 16):
        s += "  " + ",".join(str(v) for v in data[i:i+16]) + ",\n"
    return s + "};\n"

c = '#include "data.h"\n'
h = "#ifndef DATA_H\n#define DATA_H\n"
def add(name, data, typ="unsigned char"):
    global c, h
    c += carr(name, data, typ)
    h += "extern const %s %s[%d];\n" % (typ, name, len(data))
add("font_1bpp", font)
add("bg_tiles", bg)
add("spr_tiles", spr)
add("star_tiles", stars)
add("sin256", [int(round(127 * math.sin(i * 2 * math.pi / 256))) for i in range(256)], "signed char")
add("sindeg", [int(round(255 * math.sin(math.radians(i)))) for i in range(91)])
h += "#define N_SPR_TILES %d\n#endif\n" % (len(spr) // 32)
open("src/data.c", "w").write(c)
open("src/data.h", "w").write(h)
print("sprite tiles:", len(spr) // 32)
