/* ---------------------------------------------------------------------------
 * Weapons: tables, projectile physics, guidance, shields and every weapon
 * behaviour (explosives, MIRVs, napalm, rollers, diggers, dirt, riot, laser).
 * ------------------------------------------------------------------------- */
#include "game.h"
#include <string.h>

/*  name            price bun class       rad dmg  param */
const weapon_t weapons[NWT] = {
    {"BABY MISSILE",     0,  0, C_EXPL,      8,  40,  0},
    {"MISSILE",        190,  5, C_EXPL,     14,  65,  0},
    {"BABY NUKE",     1000,  3, C_EXPL,     22,  95,  0},
    {"NUKE",          1200,  1, C_EXPL,     32, 130,  0},
    {"LEAPFROG",      1000,  2, C_LEAP,     16,  55,  0},
    {"FUNKY BOMB",     700,  2, C_FUNKY,    20,  60,  6},
    {"MIRV",          1000,  3, C_MIRV,      0,   0,  5},
    {"DEATH'S HEAD",  2000,  1, C_MIRV,      0,   0,  9},
    {"NAPALM",        1000, 10, C_NAPALM,    0,   3, 10},
    {"HOT NAPALM",    2000,  2, C_NAPALM,    0,   5, 16},
    {"TRACER",          10, 20, C_TRACER,    0,   0,  0},
    {"SMOKE TRACER",    50, 10, C_TRACER,    0,   0,  1},
    {"BABY ROLLER",    500, 10, C_ROLLER,   10,  45,  0},
    {"ROLLER",         600,  5, C_ROLLER,   16,  65,  0},
    {"HEAVY ROLLER",   675,  2, C_ROLLER,   24,  95,  0},
    {"RIOT CHARGE",    200, 10, C_RIOTCHG,  36,   0,  0},
    {"RIOT BLAST",     500,  5, C_RIOTCHG,  60,   0,  0},
    {"RIOT BOMB",      500,  5, C_RIOTBOMB, 26,   0,  0},
    {"HVY RIOTBOMB",   475,  2, C_RIOTBOMB, 40,   0,  0},
    {"BABY DIGGER",    300, 10, C_DIGGER,   20,   2,  3},
    {"DIGGER",         250,  5, C_DIGGER,   30,   3,  5},
    {"HEAVY DIGGER",   675,  2, C_DIGGER,   40,   4,  7},
    {"BABY SANDHOG",  1000, 10, C_SANDHOG,  10,  40,  3},
    {"SANDHOG",       1675,  5, C_SANDHOG,  14,  55,  5},
    {"HVY SANDHOG",   2500,  2, C_SANDHOG,  20,  75,  7},
    {"DIRT CLOD",      500, 10, C_DIRT,     10,   0,  0},
    {"DIRT BALL",      500,  5, C_DIRT,     20,   0,  0},
    {"TON OF DIRT",    675,  2, C_DIRT,     34,   0,  0},
    {"LIQUID DIRT",    500, 10, C_LIQUID,    0,   0,  70},
    {"DIRT CHARGE",    500,  5, C_DIRTCHG,  40,   0,  0},
    {"DISRUPTER",      500, 10, C_DISRUPT,  64,   0,  0},
    {"PLASMA BLAST",   900,  5, C_PLASMA,    0,  90,  0},
    {"LASER",          500,  5, C_LASER,     0,  60,  0},
    /* hidden sub-munitions */
    {"",                 0,  0, C_EXPL,     14,  60,  0},   /* MIRV warhead */
    {"",                 0,  0, C_EXPL,     22,  90,  0},   /* Death's Head warhead */
    {"",                 0,  0, C_EXPL,     12,  45,  0},   /* funky bomblet */
    {"",                 0,  0, C_EXPL,     12,  40,  0},   /* tank death */
};

const item_t items_tbl[NI] = {
    {"PARACHUTE",    1000,  8},
    {"BATTERY",       500, 10},
    {"MAG DEFLECT",  1000,  2},
    {"SHIELD",       2000,  3},
    {"FORCE SHIELD", 2500,  3},
    {"HEAVY SHIELD", 3000,  2},
    {"SUPER MAG",    4000,  2},
    {"AUTO DEFENSE",  150,  1},
    {"FUEL TANK",    1000, 40},
    {"CONTACT TRIG",  100, 25},
    {"HEAT GUIDE",   1000,  6},
    {"BALLISTIC",    1000,  2},
    {"HORZ GUIDE",   1500,  5},
    {"VERT GUIDE",   2000,  5},
    {"LAZY BOY",     2000,  2},
};

#define EX_NORMAL 0
#define EX_RIOT   1
#define EX_DIRT   2
#define EX_ENERGY 3

extern const u8 bitm[8];
extern u8 trail_tile;
void award(u16 a);

proj_t proj[MAXP];
proj_t simp;
int hit_x;
u8 hit_y;
u8 trail_x[TRAIL], trail_y[TRAIL], trail_n;
u8 guide_target;
u8 guide_on;
int ex0, ex1;
u8 eybot;                      /* lowest terrain row that may have a hole */
u8 trig_armed;
static s8 vert_side;
u8 fl_x[NFL], fl_y[NFL];
static int wx[7], wy[7];
static u8 wa[7], wlive[7];

static void track(int x0, int x1, int ybot) {
    if (x0 < ex0) ex0 = x0;
    if (x1 > ex1) ex1 = x1;
    ybot -= TOPY;
    if (ybot > TH - 2) ybot = TH - 2;
    if (ybot > (int)eybot) eybot = (u8)ybot;
}

void launch(proj_t *p, tank_t *t, u8 a, u8 pw) {
    long c = cosd(a), s = sind(a), v;
    v = t->x;
    p->x = (v << 16) + c * 1536L;
    v = t->y - 6;
    p->y = (v << 16) - s * 1536L;
    v = pw * 6;
    p->vx = c * v;
    p->vy = -(s * v);
    p->active = 1;
    p->kind = 0;
    p->gen = 0;
    p->mode = M_FREE;
}

/* Physics runs on globals: SDCC turns long ops on struct fields into ldir copies */
long gs[4];                    /* x, y, vx, vy (16.16) - same layout as proj_t */
u8 gmode;
u8 sim_mode;                   /* AI simulation: shields count as hits */
#define GX  gs[0]
#define GY  gs[1]
#define GVX gs[2]
#define GVY gs[3]
#define GX_HI (((int *)&gs[0])[1])
#define GY_HI (((int *)&gs[1])[1])

int tank_ytop;                 /* shells above this row cannot touch any tank */
u8 near_mask[32];              /* per 8-px column: tanks a shell there could touch */

void prep_tanks(void) {
    u8 i, R, sh;
    int c, c0, c1;
    tank_t *t = tanks;
    tank_ytop = 999;
    memset(near_mask, 0, 32);
    for (i = 0; i < num_players; i++, t++) {
        if (!t->alive) continue;
        sh = (i == cur_player) ? 0 : t->shield;
        R = sh == I_SUPERMAG ? 42 : (sh == I_MAG ? 26 : (sh ? 13 : 6));
        if ((int)t->y - 4 - R < tank_ytop) tank_ytop = (int)t->y - 4 - R;
        c0 = ((int)t->x - R) >> 3; if (c0 < 0) c0 = 0;
        c1 = ((int)t->x + R) >> 3; if (c1 > 31) c1 = 31;
        for (c = c0; c <= c1; c++) near_mask[c] |= bitm[i];
    }
}

/* gvy += gravity, gvx += wind (free flight); gx += gvx; gy += gvy */
static void integrate(void) __naked {
    __asm
    ld  a, (_gmode)
    or  a, a
    jr  nz, 00031$
    ld  hl, #_gs + 12
    ld  a, (hl)
    add a, #0x4C
    ld  (hl), a
    inc hl
    ld  a, (hl)
    adc a, #0x04
    ld  (hl), a
    inc hl
    ld  a, (hl)
    adc a, #0x00
    ld  (hl), a
    inc hl
    ld  a, (hl)
    adc a, #0x00
    ld  (hl), a
    ld  hl, #_gs + 8
    ld  de, #_wind_acc
    call 00032$
00031$:
    ld  hl, #_gs
    ld  de, #_gs + 8
    call 00032$
    ld  hl, #_gs + 4
    ld  de, #_gs + 12
00032$:
    ld  a, (de)
    add a, (hl)
    ld  (hl), a
    inc hl
    inc de
    ld  a, (de)
    adc a, (hl)
    ld  (hl), a
    inc hl
    inc de
    ld  a, (de)
    adc a, (hl)
    ld  (hl), a
    inc hl
    inc de
    ld  a, (de)
    adc a, (hl)
    ld  (hl), a
    ret
    __endasm;
}

/* 0 flying, 1 hit (hit_x, hit_y), 2 gone */
u8 step_g(void) {
    int ix, iy, dx, dy;
    u8 i, sh, R, m;
    tank_t *t;
    integrate();
    ix = GX_HI;
    iy = GY_HI;
    if ((u16)ix > 255) {                     /* off the sides */
        if (ix < -200 || ix > 455 || iy >= BOTY) return 2;
        return 0;
    }
    if (iy >= BOTY) {
        hit_x = ix; hit_y = (u8)(BOTY - 1);
        return 1;
    }
    if (iy >= TOPY && (bm[((u16)(iy - TOPY) << 5) + ((u8)ix >> 3)] & bitm[ix & 7])) {
        hit_x = ix; hit_y = (u8)iy;
        return 1;
    }
    if (iy < tank_ytop) return 0;
    m = near_mask[(u8)ix >> 3];
    if (!m) return 0;
    t = tanks;
    for (i = 0; i < num_players; i++, t++) {
        if (!(m & bitm[i])) continue;
        dx = ix - (int)t->x;
        if (dx < -40 || dx > 40) continue;
        dy = iy - ((int)t->y - 4);
        if (dy < -40 || dy > 40) continue;
        sh = t->shield;
        if (sh && i != cur_player) {
            if (sh == I_MAG || sh == I_SUPERMAG) {
                R = (sh == I_MAG) ? 24 : 40;
                if (dx > -R && dx < R && dy > -R && dy < R) {
                    if (sh == I_MAG) {
                        if (dx < 0) GVX -= 2600; else GVX += 2600;
                        if (dy < 0) GVY -= 2600; else GVY += 2600;
                    } else {
                        if (dx < 0) GVX -= 5200; else GVX += 5200;
                        if (dy < 0) GVY -= 5200; else GVY += 5200;
                    }
                }
            } else if (dx > -11 && dx < 11 && dy > -11 && dy < 9) {
                if (sh == I_FORCE && !sim_mode) {
                    if ((dx < 0) == (GVX > 0)) GVX = -GVX;
                    if ((dy < 0) == (GVY > 0)) GVY = -GVY;
                } else {
                    hit_x = ix; hit_y = (u8)iy;
                    return 1;
                }
            }
        }
        if (dx >= -4 && dx <= 3 && dy < 4 && dy >= -3) {
            hit_x = ix; hit_y = (u8)iy;
            return 1;
        }
    }
    return 0;
}

u8 step(proj_t *p) {
    u8 r;
    memcpy(gs, p, 16);
    gmode = p->mode;
    r = step_g();
    memcpy(p, gs, 16);
    return r;
}

static void guidance(proj_t *p) {
    int ix = (int)(p->x >> 16), iy = (int)(p->y >> 16), dx, dy, d, best = 999, bx = 0, by = 0;
    u8 i;
    s8 side;
    tank_t *t;
    if (guide_on == I_HEAT) {
        t = tanks;
        for (i = 0; i < num_players; i++, t++) {
            if (i == cur_player || !t->alive) continue;
            dx = (int)t->x - ix; dy = (int)t->y - 4 - iy;
            d = iabs(dx) + iabs(dy);
            if (d < best) { best = d; bx = dx; by = dy; }
        }
        if (best < 64) {
            p->vx += bx > 0 ? 3600L : -3600L;
            p->vy += by > 0 ? 3600L : -3600L;
        }
        return;
    }
    t = &tanks[guide_target];
    if (guide_on == I_HORZ) {
        if (p->mode == M_FREE && p->vy > 0 && iy >= (int)t->y - 12) {
            p->mode = M_HORZ;
            p->vy = 0;
            p->vx = ix < (int)t->x ? 98304L : -98304L;
            sfx_blip(40);
        }
    } else if (guide_on == I_VERT) {
        if (p->mode == M_FREE) {
            side = ix < (int)t->x ? -1 : 1;
            if (vert_side && side != vert_side && iy < (int)t->y) {
                p->mode = M_VERT;
                p->vx = 0;
                p->vy = 98304L;
                p->x = (long)t->x << 16;
                sfx_blip(40);
            }
            vert_side = side;
        }
    }
}

/* ------------------------------------------------------------ explosions */
static void draw_ring(int cx, int cy, u8 rr, u8 n) {
    u8 i, a = (u8)frame_count << 2, st = n == 8 ? 32 : (n == 12 ? 21 : 16);
    int x, y;
    for (i = 0; i < n; i++) {
        x = cx - 4 + (((int)sin256[(u8)(a + 64)] * rr) >> 7);
        y = cy - 4 + (((int)sin256[a] * rr) >> 7);
        a += st;
        if (x < 0 || x > 248 || y < 0 || y > 191) continue;
        SMS_addSprite((u8)x, (u8)y, S_FIRE + ((i + (u8)frame_count) & 1));
    }
    if (cx >= 4 && cx <= 251 && cy >= 4 && cy < 192)
        SMS_addSprite((u8)(cx - 4), (u8)(cy - 4), S_FIRE + ((u8)frame_count & 1));
}

static void blast_damage(int cx, int cy, u8 r, u8 dmg, u8 skip) {
    u8 i, reach = r + 6;
    int dx, dy, d;
    tank_t *t = tanks;
    for (i = 0; i < num_players; i++, t++) {
        if (!t->alive || i == skip) continue;
        dx = iabs(cx - (int)t->x);
        dy = iabs(cy - ((int)t->y - 4));
        d = dx > dy ? dx + (dy >> 1) : dy + (dx >> 1);
        if (d >= reach) continue;
        hurt(i, (u16)dmg * (u8)(reach - d) / reach);
    }
}

static void explode(int cx, int cy, u8 r, u8 dmg, u8 mode) {
    u8 n = r <= 10 ? 8 : (r <= 18 ? 12 : 16), rr, i, st = r > 24 ? 3 : 2;
    game_state = ST_EXPLODE;
    sfx_boom((r >> 2) ? (r >> 2) : 1);
    for (rr = 2; rr <= r; rr += st) { draw_ring(cx, cy, rr, n); draw_world(); frame(); }
    if (mode != EX_ENERGY) {
        circle(cx, cy, r, mode == EX_DIRT);
        track(cx - r, cx + r, mode == EX_DIRT ? BOTY : cy + r + 1);
    }
    while (render_dirty(10)) { draw_ring(cx, cy, r, n); draw_world(); frame(); }
    for (i = 0; i < 6; i++) {
        if (i & 1) draw_ring(cx, cy, r, n);
        draw_world(); frame();
    }
    if (dmg && (mode == EX_NORMAL || mode == EX_ENERGY))
        blast_damage(cx, cy, r, dmg, mode == EX_ENERGY ? cur_player : 255);
}

static void spawn(long x, long y, long vx, long vy, u8 kind, u8 gen) {
    u8 i;
    proj_t *p = proj;
    for (i = 0; i < MAXP; i++, p++) {
        if (p->active) continue;
        p->x = x; p->y = y; p->vx = vx; p->vy = vy;
        p->kind = kind; p->gen = gen; p->mode = M_FREE; p->active = 1;
        return;
    }
}

/* ---------------------------------------------------------------- napalm */
/* surface near y in column x; 255 if blocked by a wall */
static u8 col_surface(int x, u8 y) {
    u8 k;
    if (x < 0 || x > 255) return 255;
    if (solid(x, (int)y - 1)) {
        for (k = 0; k < 3; k++) {
            y--;
            if (!solid(x, (int)y - 1)) return y;
        }
        return 255;
    }
    while (y < BOTY && !solid(x, y)) y++;
    return y;
}

static void napalm(int x, u8 y, u8 n, u8 dmg) {
    u8 i, j, k, life, a, b, fy;
    int fx;
    tank_t *t;
    if (n > NFL) n = NFL;
    for (i = 0; i < n; i++) {
        fx = x + ((int)i - (int)(n >> 1)) * 2;
        if (fx < 0) fx = 0;
        if (fx > 255) fx = 255;
        fl_x[i] = (u8)fx;
        fl_y[i] = surface_at(fx, (int)y - 10);
    }
    sfx_crackle();
    game_state = ST_EXPLODE;
    for (life = 0; life < 120; life++) {
        for (k = 0; k < n; k++) {
            i = k + life; while (i >= n) i -= n;
            if (life & 1) {
                fx = fl_x[i]; fy = fl_y[i];
                if (fy < BOTY && !solid(fx, fy)) fl_y[i] = fy + 1;
                else {
                    a = col_surface(fx - 1, fy);
                    b = col_surface(fx + 1, fy);
                    if (a != 255 && a > fy && (b == 255 || a >= b)) { fl_x[i] = fx - 1; fl_y[i] = a; }
                    else if (b != 255 && b > fy) { fl_x[i] = fx + 1; fl_y[i] = b; }
                }
            }
            if (fl_x[i] >= 4 && fl_x[i] <= 251 && fl_y[i] >= 8)
                SMS_addSprite(fl_x[i] - 4, fl_y[i] - 8, S_FLAME + ((i + (life >> 2)) & 1));
        }
        if (!(life & 7)) {
            t = tanks;
            for (j = 0; j < num_players; j++, t++) {
                if (!t->alive) continue;
                for (i = 0; i < n; i++) {
                    if (iabs((int)fl_x[i] - (int)t->x) <= 5 && iabs((int)fl_y[i] - (int)t->y) <= 8) {
                        hurt(j, dmg);
                        break;
                    }
                }
            }
            if (!(life & 31)) sfx_crackle();
        }
        draw_world(); frame();
    }
    sfx_quiet();
}

/* --------------------------------------------------------------- rollers */
static void roller(int x, u8 y, long vx, u8 r, u8 dmg) {
    u8 ry, s, l, rr, j, flat = 0;
    u16 n;
    s8 dir;
    int nx;
    tank_t *t;
    ry = surface_at(x, (int)y - 6);
    l = col_surface(x - 3, ry);
    rr = col_surface(x + 3, ry);
    if (l != 255 && l > ry && (rr == 255 || l >= rr)) dir = -1;
    else if (rr != 255 && rr > ry) dir = 1;
    else dir = vx < 0 ? -1 : 1;
    game_state = ST_EXPLODE;
    for (n = 0; n < 300; n++) {
        nx = x + dir;
        if (nx < 0 || nx > 255) break;
        s = col_surface(nx, ry);
        if (s == 255 || s < ry) break;
        if (s == ry) { if (++flat > 10) break; } else flat = 0;
        x = nx; ry = s;
        t = tanks;
        for (j = 0; j < num_players; j++, t++) {
            if (!t->alive || (j == cur_player && n < 12)) continue;
            if (iabs(x - (int)t->x) <= 5 && iabs((int)t->y - (int)ry) <= 10) goto boom;
        }
        if (n & 1) {
            if (x >= 4 && x <= 251) SMS_addSprite((u8)(x - 4), ry - 7, S_BALL);
            draw_world(); frame();
        }
    }
boom:
    explode(x, (int)ry - 3, r, dmg, EX_NORMAL);
}

/* --------------------------------------------------- diggers & sandhogs */
static void dig(int x, u8 y, u8 worms, u8 len, u8 tr, u8 br, u8 bd) {
    u8 i, n;
    int px, py;
    if (worms > 7) worms = 7;
    for (i = 0; i < worms; i++) {
        wx[i] = x << 6; wy[i] = (int)y << 6;
        wa[i] = 24 + rnd() % 80;
        wlive[i] = 1;
    }
    game_state = ST_EXPLODE;
    for (n = 0; n < len; n++) {
        for (i = 0; i < worms; i++) {
            if (!wlive[i]) continue;
            wa[i] += (rnd() & 15) - 8;
            if (wa[i] > 128) wa[i] = (u8)(256 - wa[i]);
            wx[i] += sin256[(u8)(wa[i] + 64)];
            wy[i] += sin256[wa[i]];
            px = wx[i] >> 6; py = wy[i] >> 6;
            if (px < 1 || px > 254 || py >= BOTY - 2) { wlive[i] = 0; continue; }
            circle(px, py, tr, 0);
        }
        if (!(n & 3)) sfx_crackle();
        render_dirty(20);
        draw_world(); frame();
    }
    render_all();                       /* tunnels are left standing */
    if (br) {
        for (i = 0; i < worms; i++)
            explode(wx[i] >> 6, wy[i] >> 6, br, bd, EX_NORMAL);
    }
}

/* ------------------------------------------------------------ liquid dirt */
static void liquid(int x, u16 amount) {
    int x0 = x - 60, x1 = x + 60, c;
    u16 n;
    u8 h, l, r;
    if (x0 < 0) x0 = 0;
    if (x1 > 255) x1 = 255;
    if (x < 0) x = 0;
    if (x > 255) x = 255;
    for (c = x0; c <= x1; c++) height[c] = surface_at(c, TOPY);
    game_state = ST_EXPLODE;
    for (n = 0; n < amount; n++) {
        c = x;
        for (;;) {
            h = height[c];
            l = c > x0 ? height[c - 1] : 0;
            r = c < x1 ? height[c + 1] : 0;
            if (l > h && l >= r) c--;
            else if (r > h) c++;
            else break;
        }
        if (height[c] <= TOPY + 2) break;
        height[c]--;
        put_pixel((u8)c, height[c]);
        if (!(n & 15)) { render_dirty(24); draw_world(); frame(); }
    }
    render_all();
}

/* -------------------------------------------- riot / dirt charges, plasma */
static void cone(tank_t *t, u8 reach, u8 set) {
    int c = cosd(t->angle), s = sind(t->angle), px, py;
    u8 d, r;
    sfx_boom(2);
    game_state = ST_EXPLODE;
    for (d = set ? 14 : 8; d <= reach; d += 4) {
        px = (int)t->x + ((c * (int)d) >> 8);
        py = (int)t->y - 5 - ((s * (int)d) >> 8);
        r = (d << 1) / 5 + 2;
        circle(px, py, r, set);
        track(px - r, px + r, py + r + 1);
        render_dirty(30);
        draw_ring(px, py, r, 8);
        draw_world(); frame();
    }
    render_all();
}

static void plasma(tank_t *t) {
    u8 r = 12 + ((u16)t->power * 2) / 5, rr;
    int cx = t->x, cy = (int)t->y - 4;
    sfx_boom(3);
    game_state = ST_EXPLODE;
    for (rr = 4; rr <= r; rr += 2) { draw_ring(cx, cy, rr, 16); draw_world(); frame(); }
    blast_damage(cx, cy, r, weapons[W_PLASMA].damage, cur_player);
    for (rr = 0; rr < 8; rr++) { if (rr & 1) draw_ring(cx, cy, r, 16); draw_world(); frame(); }
}

static void laser(tank_t *t) {
    long lx, ly;
    int dx = cosd(t->angle), dy = -sind(t->angle), ix = t->x, iy = t->y, xmin, xmax;
    u16 k, len = 80 + (u16)t->power * 2;
    u8 j, hit = 255;
    tank_t *o;
    lx = ((long)t->x << 8) + (long)dx * 7;
    ly = ((long)((int)t->y - 6) << 8) + (long)dy * 7;
    xmin = xmax = t->x;
    trail_n = 0;
    trail_tile = S_LASER;
    game_state = ST_EXPLODE;
    PSG = 0x80 | 6; PSG = 2; PSG = 0x92;
    for (k = 0; k < len && hit == 255; k++) {
        lx += dx; ly += dy;
        ix = (int)(lx >> 8); iy = (int)(ly >> 8);
        if (ix < 0 || ix > 255 || iy < 0 || iy >= BOTY) break;
        if (ix < xmin) xmin = ix;
        if (ix > xmax) xmax = ix;
        if (solid(ix, iy)) circle(ix, iy, 1, 0);
        o = tanks;
        for (j = 0; j < num_players; j++, o++) {
            if (j == cur_player || !o->alive) continue;
            if (ix - (int)o->x >= -4 && ix - (int)o->x <= 3 && iy < (int)o->y && iy >= (int)o->y - 7) { hit = j; break; }
        }
        if (!(k & 7) && trail_n < TRAIL && ix >= 1 && ix <= 254 && iy >= 4) {
            trail_x[trail_n] = (u8)ix; trail_y[trail_n] = (u8)iy; trail_n++;
        }
    }
    track(xmin - 2, xmax + 2, BOTY);
    for (k = 0; k < 40; k++) { render_dirty(12); draw_world(); frame(); }
    sfx_quiet();
    render_all();
    if (hit != 255) {
        hurt(hit, weapons[W_LASER].damage);
        explode(ix, iy, 6, 0, EX_ENERGY);
    }
    trail_n = 0;
    trail_tile = S_SMOKE;
}

/* ---------------------------------------------------------------- impact */
static void impact(proj_t *p) {
    u8 w = p->kind, g, k;
    const weapon_t *wp = &weapons[w];
    int x = hit_x;
    u8 y = hit_y;
    long vy;
    switch (wp->cls) {
    case C_EXPL:
        explode(x, y, wp->radius, wp->damage, EX_NORMAL);
        break;
    case C_MIRV:     /* hit before splitting */
        explode(x, y, w == W_MIRV ? 14 : 22, w == W_MIRV ? 60 : 90, EX_NORMAL);
        break;
    case C_LEAP:
        g = p->gen;
        explode(x, y, wp->radius - (g << 2), wp->damage - g * 10, EX_NORMAL);
        if (g < 2) {
            vy = p->vy < 0 ? p->vy : -p->vy;
            vy = (vy * 3) >> 2;
            if (vy > -70000L) vy = -70000L;
            spawn((long)x << 16, (long)((int)y - 2) << 16, (p->vx * 3) >> 2, vy, w, g + 1);
        }
        break;
    case C_FUNKY:
        explode(x, y, wp->radius, wp->damage, EX_NORMAL);
        for (k = 0; k < wp->param; k++)
            spawn((long)x << 16, (long)((int)y - 6) << 16,
                  ((long)rnd() - 128) * 520L, -(50000L + (long)rnd() * 420L), W_FUNKYSUB, 1);
        break;
    case C_NAPALM:
        napalm(x, y, wp->param, wp->damage);
        break;
    case C_TRACER:
        sfx_blip(30);
        break;
    case C_ROLLER:
        if (trig_armed) explode(x, y, wp->radius, wp->damage, EX_NORMAL);
        else roller(x, y, p->vx, wp->radius, wp->damage);
        break;
    case C_RIOTBOMB:
        explode(x, y, wp->radius, 0, EX_RIOT);
        break;
    case C_DIGGER:
        dig(x, y, wp->param, wp->radius, wp->damage, 0, 0);
        break;
    case C_SANDHOG:
        if (trig_armed) explode(x, y, wp->radius, wp->damage, EX_NORMAL);
        else dig(x, y, wp->param, 22, 2, wp->radius, wp->damage);
        break;
    case C_DIRT:
        explode(x, y, wp->radius, 0, EX_DIRT);
        break;
    case C_LIQUID:
        liquid(x, (u16)wp->param << 2);
        break;
    case C_DISRUPT:
        explode(x, y, 10, 0, EX_ENERGY);
        settle_range(x - wp->radius, x + wp->radius, (u8)(TH - 2));
        break;
    }
}

static void do_split(u8 w) {
    u8 n = weapons[w].param, k, sub = (w == W_MIRV) ? W_MIRVSUB : W_DHSUB;
    long spread = (w == W_MIRV) ? 17000L : 12500L;
    proj_t base;
    base = proj[0];
    for (k = 0; k < n; k++) {
        proj[k] = base;
        proj[k].vx = base.vx + ((long)k - (long)(n >> 1)) * spread;
        proj[k].kind = sub;
        proj[k].gen = 1;
        proj[k].mode = M_FREE;
        proj[k].active = 1;
    }
    sfx_blip(60);
}

static u8 is_projectile(u8 cls) {
    return !(cls == C_RIOTCHG || cls == C_DIRTCHG || cls == C_PLASMA || cls == C_LASER);
}

void fire(void) {
    tank_t *t = &tanks[cur_player];
    u8 w = t->weapon, cls = weapons[w].cls, i, s, r, any, split = 1;
    u16 timeout = 2500;
    proj_t *p;
    int ix, iy;

    if (w) t->ammo[w]--;
    shots_fired++;
    trail_n = 0;
    trail_tile = S_SMOKE;
    ex0 = 999; ex1 = -999; eybot = 0;
    guide_on = 0;
    if (t->guide && is_projectile(cls) && t->items[t->guide]) {
        t->items[t->guide]--;
        guide_on = t->guide;
    }
    t->guide = 0;
    trig_armed = 0;
    if (t->trigger && t->items[I_TRIGGER] && (cls == C_ROLLER || cls == C_SANDHOG)) {
        t->items[I_TRIGGER]--;
        trig_armed = 1;
    }
    game_state = ST_FLIGHT;
    sfx_fire();
    if (cls == C_RIOTCHG || cls == C_DIRTCHG) cone(t, weapons[w].radius, cls == C_DIRTCHG);
    else if (cls == C_PLASMA) plasma(t);
    else if (cls == C_LASER) laser(t);
    else {
        for (i = 0; i < MAXP; i++) proj[i].active = 0;
        prep_tanks();
        if (guide_on == I_BALLISTIC) t->power = solve_power(t->angle, guide_target, 0);
        launch(&proj[0], t, t->angle, t->power);
        proj[0].kind = w;
        if (cls == C_MIRV) split = 0;
        vert_side = 0;
        do {
            for (s = 0; s < 2; s++) {
                for (i = 0; i < MAXP; i++) {
                    p = &proj[i];
                    if (!p->active) continue;
                    if (i == 0 && guide_on && !p->gen) guidance(p);
                    r = step(p);
                    if (r == 2) p->active = 0;
                    else if (r == 1) {
                        p->active = 0;
                        sfx_quiet();
                        impact(p);
                        game_state = ST_FLIGHT;
                    }
                }
                if (!split && proj[0].active && proj[0].vy >= 0) { split = 1; do_split(w); }
            }
            any = 0;
            for (i = 0; i < MAXP; i++) {
                p = &proj[i];
                if (!p->active) continue;
                any = 1;
                ix = (int)(p->x >> 16);
                iy = (int)(p->y >> 16);
                if (ix < 3 || ix > 252) continue;
                if (iy < 19) SMS_addSprite((u8)(ix - 3), 16, S_MARK);
                else SMS_addSprite((u8)(ix - 3), (u8)(iy - 3), S_SHOT);
                if (i == 0) {
                    if (iy < -200) iy = -200;
                    sfx_whistle(260 + iy);
                    if (w == W_SMOKE && !(frame_count & 3) && trail_n < TRAIL && iy >= 19) {
                        trail_x[trail_n] = (u8)ix; trail_y[trail_n] = (u8)iy; trail_n++;
                    }
                }
            }
            draw_world();
            frame();
        } while (any && --timeout);
    }
    sfx_quiet();
    if (opt_falling && ex0 <= ex1) settle_range(ex0 - 2, ex1 + 2, eybot);
    ex0 = 999; ex1 = -999; eybot = 0;
    settle_tanks();
    resolve_deaths();
    if (w && !t->ammo[w]) t->weapon = 0;
    if (w != W_SMOKE) trail_n = 0;
}

void resolve_deaths(void) {
    u8 i;
    tank_t *t;
again:
    t = tanks;
    for (i = 0; i < num_players; i++, t++) {
        if (t->alive && t->hp <= 0) {
            talk_death(t);
            t->alive = 0;
            t->shield = 0;
            if (i != cur_player) award(250);
            ex0 = 999; ex1 = -999; eybot = 0;
            explode(t->x, (int)t->y - 4, weapons[W_DEATH].radius, weapons[W_DEATH].damage, EX_NORMAL);
            if (opt_falling && ex0 <= ex1) settle_range(ex0 - 2, ex1 + 2, eybot);
            settle_tanks();
            goto again;
        }
    }
}
