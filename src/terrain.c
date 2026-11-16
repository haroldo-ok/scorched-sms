/* ---------------------------------------------------------------------------
 * Destructible bitmap terrain on a tile-only VDP.
 *
 * bm[] holds one bit per pixel for screen rows 16..183 (row-major, 32 bytes
 * per row, MSB = leftmost pixel, the same layout as a tile bit-plane).
 * Every 8x8 cell is either all sky (shared tile), all ground (shared tile) or
 * "mixed": mixed cells get a private tile from a pool of 254 VRAM slots.
 * Edits mark cells dirty; render_dirty() rebuilds and uploads them.
 * ------------------------------------------------------------------------- */
#include "game.h"
#include <string.h>

u8 bm[TH * 32];
u8 cellmap[CROWS * 32];      /* 0 sky, 1 ground, 2.. pool slot + 2 */
u8 dirty[CROWS * 4];
u8 any_dirty;
u8 pool_used[32];
u8 pool_free;
u8 height[256];

u8 tilebuf[32];
const u8 bitm[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
const u8 tex[8] = {0x40, 0x04, 0x20, 0x00, 0x11, 0x00, 0x82, 0x08};

static u16 pool_tile(u8 s) {
    return s < N_POOL1 ? T_POOL1 + s : T_POOL2 + (s - N_POOL1);
}

static u8 pool_alloc(void) {
    u8 i, j, b;
    if (!pool_free) return 255;
    for (i = 0; i < 32; i++) {
        b = pool_used[i];
        if (b == 0xFF) continue;
        for (j = 0; j < 8; j++) {
            if (!(b & bitm[j])) {
                pool_used[i] = b | bitm[j];
                pool_free--;
                return (i << 3) + j;
            }
        }
    }
    return 255;
}

static void pool_release(u8 s) {
    pool_used[s >> 3] &= ~bitm[s & 7];
    pool_free++;
}

static u16 code_tile(u8 c, u8 cy) {
    if (c == 0) return T_SKY + band[cy + 2];
    if (c == 1) return T_GROUND;
    return pool_tile(c - 2);
}

u16 cell_tile(u8 cx, u8 cy) {
    return code_tile(cellmap[((u16)cy << 5) + cx], cy);
}

static void markc(u8 cy, u8 cx) {
    dirty[(cy << 2) + (cx >> 3)] |= bitm[cx & 7];
    any_dirty = 1;
}

/* ---- hand-written inner loops (SDCC keeps C locals in IX slots: ~6x slower) */
const u8 *sc_ptr;              /* first bitmap byte of the cell */
u8 sc_above;                   /* bitmap row above the cell */
u8 sc_all, sc_any, sc_anyt, sc_nfull, sc_cnt;
u8 sm[4];                      /* sky colour bit masks: 0xFF if bit set */

/* sc_all = AND of rows, sc_any = OR, sc_anyt != 0 if any top edge, sc_nfull */
static void scan_cell(void) __naked {
    __asm
    ld  hl, (_sc_ptr)
    ld  a, (_sc_above)
    ld  c, a
    ld  de, #32
    ld  b, #8
    xor a
    ld  (_sc_any), a
    ld  (_sc_anyt), a
    ld  (_sc_nfull), a
    dec a
    ld  (_sc_all), a
00001$:
    ld  a, c
    cpl
    and a, (hl)
    jr  z, 00002$
    ld  (_sc_anyt), a
00002$:
    ld  c, (hl)
    ld  a, (_sc_all)
    and a, c
    ld  (_sc_all), a
    ld  a, (_sc_any)
    or  a, c
    ld  (_sc_any), a
    ld  a, c
    inc a
    jr  nz, 00003$
    ld  a, (_sc_nfull)
    inc a
    ld  (_sc_nfull), a
00003$:
    add hl, de
    djnz 00001$
    ret
    __endasm;
}

/* tilebuf <- 4bpp tile for the cell at sc_ptr (sky colour in sm[]) */
static void make_tile(void) __naked {
    __asm
    di
    push ix
    push iy
    exx
    ld  hl, #_sm
    ld  b, (hl)
    inc hl
    ld  c, (hl)
    inc hl
    ld  d, (hl)
    inc hl
    ld  e, (hl)
    exx
    ld  hl, (_sc_ptr)
    ld  de, #_tilebuf
    ld  iy, #_tex
    ld  a, (_sc_above)
    ld  c, a
    ld  a, #8
    ld  (_sc_cnt), a
00010$:
    ld  b, (hl)            ; b = m
    ld  a, c
    cpl
    and a, b
    ld  c, a               ; c = t (top edge)
    ; plane 0 = (m & ~t) | (~m & s1)
    cpl
    and a, b
    ld  (de), a
    ld  a, b
    cpl
    exx
    and a, b
    exx
    ex  de, hl
    or  a, (hl)
    ld  (hl), a
    inc hl
    ex  de, hl
    ; plane 1 = t | (tex & m & ~t) | (~m & s2)
    ld  a, c
    cpl
    and a, b
    and a, 0 (iy)
    or  a, c
    ld  (de), a
    ld  a, b
    cpl
    exx
    and a, c
    exx
    ex  de, hl
    or  a, (hl)
    ld  (hl), a
    inc hl
    ex  de, hl
    ; plane 2 = ~m & s4
    ld  a, b
    cpl
    exx
    and a, d
    exx
    ld  (de), a
    inc de
    ; plane 3 = m | (~m & s8)
    ld  a, b
    cpl
    exx
    and a, e
    exx
    or  a, b
    ld  (de), a
    inc de
    ; next row
    ld  c, b
    push de
    ld  de, #32
    add hl, de
    pop de
    inc iy
    ld  a, (_sc_cnt)
    dec a
    ld  (_sc_cnt), a
    jr  nz, 00010$
    pop iy
    pop ix
    ei
    ret
    __endasm;
}

static void set_sky(u8 sky) {
    sm[0] = (sky & 1) ? 0xFF : 0;
    sm[1] = (sky & 2) ? 0xFF : 0;
    sm[2] = (sky & 4) ? 0xFF : 0;
    sm[3] = (sky & 8) ? 0xFF : 0;
}

void build_ground_tile(void) {
    u8 r;
    for (r = 0; r < 8; r++) {           /* same as make_tile() for a full cell */
        tilebuf[(r << 2) + 0] = 0xFF;
        tilebuf[(r << 2) + 1] = tex[r];
        tilebuf[(r << 2) + 2] = 0;
        tilebuf[(r << 2) + 3] = 0xFF;
    }
    SMS_loadTiles(tilebuf, T_GROUND, 32);
}

static void build_cell(u8 cx, u8 cy) {
    u16 idx = ((u16)cy << 5) + cx;
    const u8 *p = &bm[((u16)cy << 8) + cx];
    u8 old = cellmap[idx], nc, s;
    sc_ptr = p;
    sc_above = cy ? p[-32] : 0;
    scan_cell();
    if (!sc_any) nc = 0;
    else if (sc_all == 0xFF && !sc_anyt) nc = 1;
    else {
        if (old >= 2) nc = old;
        else {
            s = pool_alloc();
            nc = (s == 255) ? (sc_nfull >= 4 ? 1 : 0) : s + 2;
        }
        if (nc >= 2) {
            set_sky(band[cy + 2] + 1);
            make_tile();
            SMS_loadTiles(tilebuf, pool_tile(nc - 2), 32);
        }
    }
    if (nc < 2 && old >= 2) pool_release(old - 2);
    if (nc != old) {
        cellmap[idx] = nc;
        SMS_setTileatXY(cx, cy + 2, code_tile(nc, cy));
    }
}

/* rebuild up to budget dirty cells; returns 1 if work remains */
u8 render_dirty(u8 budget) {
    u8 cy, b, j, v;
    u8 *d = dirty;
    if (!any_dirty) return 0;
    for (cy = 0; cy < CROWS; cy++) {
        for (b = 0; b < 4; b++, d++) {
            v = *d;
            if (!v) continue;
            for (j = 0; j < 8; j++) {
                if (v & bitm[j]) {
                    v &= ~bitm[j];
                    *d = v;
                    build_cell((b << 3) + j, cy);
                    if (!--budget) return 1;
                }
            }
        }
    }
    any_dirty = 0;
    return 0;
}

void render_all(void) {
    while (render_dirty(255));
}

void restore_rows(u8 r0, u8 r1) {
    u8 r, cx;
    for (r = r0; r <= r1; r++) {
        SMS_setNextTileatXY(0, r);
        for (cx = 0; cx < 32; cx++) {
            if (r < 2) SMS_setTile(T_FONT);
            else if (r >= 23) SMS_setTile(T_BEDROCK);
            else SMS_setTile(cell_tile(cx, r - 2));
        }
    }
}

void terrain_reset(void) {
    u16 i;
    for (i = 0; i < CROWS * 32; i++) cellmap[i] = 0;
    for (i = 0; i < CROWS * 4; i++) dirty[i] = 0;
    for (i = 0; i < 32; i++) pool_used[i] = 0;
    pool_used[31] = 0x03;          /* slots 254, 255 never used */
    pool_free = N_POOL;
    any_dirty = 0;
}

void gen_terrain(void) {
    u8 p1 = rnd(), p2 = rnd(), p3 = rnd();
    u8 f3 = 3 + (rnd() & 3);
    u8 a1 = 24 + (rnd() & 15), a2 = 8 + (rnd() & 15), a3 = 2 + (rnd() & 3);
    u8 base = 112 + (rnd() & 15);
    u8 x = 0;
    int v;
    do {
        v = (int)sin256[(u8)((x >> 1) + p1)] * a1
          + (int)sin256[(u8)(x * 2 + p2)] * a2
          + (int)sin256[(u8)(x * f3 + p3)] * a3;
        v = base - (v >> 7);
        if (v < 56) v = 56;
        if (v > 172) v = 172;
        height[x] = (u8)v;
    } while (++x);
}

void terrain_from_heights(void) {
    u8 c, i, lo, hi, h, y, m, x;
    u8 *p;
    memset(bm, 0, TH * 32);
    for (c = 0; c < 32; c++) {
        lo = 255; hi = 0;
        x = c << 3;
        for (i = 0; i < 8; i++) {
            h = height[x + i];
            if (h < TOPY) h = TOPY;
            if (h < lo) lo = h;
            if (h > hi) hi = h;
        }
        /* rows where all 8 columns are solid */
        p = &bm[((u16)(hi - TOPY) << 5) + c];
        for (y = hi; y < BOTY; y++) { *p = 0xFF; p += 32; }
        /* partial rows */
        p = &bm[((u16)(lo - TOPY) << 5) + c];
        for (y = lo; y < hi; y++) {
            m = 0;
            for (i = 0; i < 8; i++) if (height[x + i] <= y) m |= bitm[i];
            *p = m; p += 32;
        }
    }
}

/* rebuild the whole screen (display should be off) */
void draw_playfield(void) {
    u8 r, cx;
    terrain_reset();
    for (r = 0; r < 2; r++) clear_row(r);
    for (r = 2; r < 23; r++) {
        SMS_setNextTileatXY(0, r);
        for (cx = 0; cx < 32; cx++) SMS_setTile(T_SKY + band[r]);
    }
    SMS_setNextTileatXY(0, 23);
    for (cx = 0; cx < 32; cx++) SMS_setTile(T_BEDROCK);
    for (r = 0; r < CROWS * 4; r++) dirty[r] = 0xFF;
    any_dirty = 1;
    render_all();
}

/* set/clear bits x0..x1 on terrain row r */
void span(u8 r, int x0, int x1, u8 set) {
    u8 c0, c1, c, lm, rm, m, o, n, cy = r >> 3;
    u8 edge = ((r & 7) == 7) && (r < TH - 1);
    u8 *p;
    if (x0 < 0) x0 = 0;
    if (x1 > 255) x1 = 255;
    if (x0 > x1) return;
    c0 = (u8)x0 >> 3; c1 = (u8)x1 >> 3;
    lm = 0xFF >> ((u8)x0 & 7);
    rm = 0xFF << (7 - ((u8)x1 & 7));
    p = &bm[((u16)r << 5) + c0];
    c = c0;
    for (;;) {
        m = 0xFF;
        if (c == c0) m &= lm;
        if (c == c1) m &= rm;
        o = *p;
        n = set ? (o | m) : (o & ~m);
        if (n != o) {
            *p = n;
            markc(cy, c);
            if (edge) markc(cy + 1, c);
        }
        if (c == c1) break;
        c++; p++;
    }
}

void circle(int cx, int cy, u8 r, u8 set) {
    int rr = (int)r * r, dy, y;
    int hw = r;
    for (dy = 0; dy <= r; dy++) {
        while (hw * hw + dy * dy > rr) hw--;
        y = cy - dy - TOPY;
        if (y >= 0 && y < TH) span((u8)y, cx - hw, cx + hw, set);
        if (dy) {
            y = cy + dy - TOPY;
            if (y >= 0 && y < TH) span((u8)y, cx - hw, cx + hw, set);
        }
    }
}

void put_pixel(u8 x, u8 y) {
    if (y >= TOPY && y < BOTY) span(y - TOPY, x, x, 1);
}

u8 solid(int x, int y) {
    if (x < 0 || x > 255) return 0;
    if (y >= BOTY) return 1;
    if (y < TOPY) return 0;
    return bm[((u16)(y - TOPY) << 5) + ((u8)x >> 3)] & bitm[x & 7];
}

/* first solid y at or below y in column x */
u8 surface_at(int x, int y) {
    if (y < TOPY) y = TOPY;
    while (y < BOTY && !solid(x, y)) y++;
    return (u8)y;
}

/* 1 if rectangle (screen coords, inclusive) has no dirt */
u8 box_clear(int x0, int x1, int y0, int y1) {
    u8 c0, c1, c, lm, rm, m;
    u8 *p;
    int y;
    if (x0 < 0) x0 = 0;
    if (x1 > 255) x1 = 255;
    if (y1 >= BOTY) return 0;
    c0 = (u8)x0 >> 3; c1 = (u8)x1 >> 3;
    lm = 0xFF >> ((u8)x0 & 7);
    rm = 0xFF << (7 - ((u8)x1 & 7));
    for (y = y0; y <= y1; y++) {
        if (y < TOPY) continue;
        p = &bm[((u16)(y - TOPY) << 5) + c0];
        c = c0;
        for (;;) {
            m = 0xFF;
            if (c == c0) m &= lm;
            if (c == c1) m &= rm;
            if (*p & m) return 0;
            if (c == c1) break;
            c++; p++;
        }
    }
    return 1;
}

/* ---- falling dirt: one pass moves every unsupported pixel of a byte column
 * down one row, rows st_n..1 counting up from st_p (assembly: ~60 cycles/row) */
u8 *st_p;
u8 st_n, st_moved, st_bfirst, st_blast;

static void settle_col(void) __naked {
    __asm
    push ix
    ld  ix, (_st_p)
    ld  a, (_st_n)
    ld  b, a
    xor a
    ld  (_st_moved), a
    ld  de, #-32
00020$:
    ld  a, 0 (ix)
    or  a, a
    jr  z, 00022$
    ld  c, a
    ld  a, 32 (ix)
    cpl
    and a, c
    jr  z, 00022$
    ld  h, a               ; h = falling bits
    or  a, 32 (ix)
    ld  32 (ix), a
    ld  a, h
    cpl
    and a, c
    ld  0 (ix), a
    ld  a, (_st_moved)
    or  a, a
    jr  nz, 00021$
    inc a
    ld  (_st_moved), a
    ld  a, b
    ld  (_st_bfirst), a
00021$:
    ld  a, b
    ld  (_st_blast), a
00022$:
    add ix, de
    djnz 00020$
    pop ix
    ret
    __endasm;
}

/* drop unsupported dirt in x0..x1 down to terrain row ybot, animated */
void settle_range(int x0, int x1, u8 ybot) {
    u8 c0, c1, c, ytop, any, pass = 0, n, ylo, yhi, cy;
    u8 *p;
    if (x0 < 0) x0 = 0;
    if (x1 > 255) x1 = 255;
    if (x0 > x1) return;
    if (ybot > (u8)(TH - 2)) ybot = (u8)(TH - 2);
    c0 = (u8)x0 >> 3; c1 = (u8)x1 >> 3;
    /* first row holding any dirt in the range */
    for (ytop = 0; ytop < ybot; ytop++) {
        p = &bm[((u16)ytop << 5) + c0];
        for (c = c0; c <= c1; c++, p++) if (*p) goto found;
    }
    return;
found:
    n = ybot - ytop + 1;
    do {
        any = 0;
        for (c = c0; c <= c1; c++) {
            st_p = &bm[((u16)ybot << 5) + c];
            st_n = n;
            settle_col();
            if (!st_moved) continue;
            any = 1;
            yhi = ybot - (n - st_bfirst) + 2;          /* rows y+1 and y+2 change look */
            ylo = ybot - (n - st_blast);
            if (yhi > TH - 1) yhi = TH - 1;
            for (cy = ylo >> 3; cy <= (yhi >> 3); cy++) markc(cy, c);
        }
        if (any && (++pass & 1) == 0) {
            render_all();
            draw_world();
            frame();
        }
    } while (any);
    render_all();
}
