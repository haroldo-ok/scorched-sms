/* ---------------------------------------------------------------------------
 * SCORCHED SMS - a Scorched Earth style artillery game for the
 * Sega Master System.  devkitSMS / SDCC.
 * ------------------------------------------------------------------------- */
#include "game.h"

tank_t tanks[4];
u8 game_state;
u8 num_players = 2, num_rounds = 5, cur_round, cur_player;
u8 setup_players = 2;
u8 ctrl_cfg[4] = {0, 2, 2, 2};
u8 opt_falling = 1, opt_talk = 1, opt_wind = 1, opt_cash = 2;
u8 opt_sky = SKY_RANDOM, opt_autoend = 1;
u8 cur_sky = SKY_SHADED;
u8 in_round, round_abort, sys_open;
extern u8 star_seed;
s8 wind;
long wind_acc;
u8 held, pressed;
u16 frame_count;
u16 rng = 0xACE1;
u8 shots_fired, turn_count;
u8 trail_tile = S_SMOKE;

u8 nz_lvl, nz_rate, nz_cnt, bl_t;

const u8 shield_max[NI] = {0, 0, 25, 55, 100, 150, 60, 0, 0, 0, 0, 0, 0, 0, 0};

const u8 bg_pal[16] = {
    0x00, 0x10, 0x20, 0x24, 0x34, 0x35, 0x39, 0x3A, 0x3E,   /* black + sky */
    0x08, 0x0E, 0x04, 0x15, 0x00, 0x0F, 0x3F };
const u8 spr_pal[16] = {
    0x3F,  /* 0: transparent for sprites; speech-bubble paper for BG text */
    0x3F, 0x03, 0x0F, 0x3C, 0x33, 0x2A, 0x0B, 0x02, 0x15,
    0x3D, 0x37, 0x2F, 0x0C, 0x3E,
    0x00 /* 15: speech-bubble ink */ };

/* sky palette entries 1..8 (top band to bottom band) for each sky style */
const u8 sky_pals[NSKY][8] = {
    {0},                                                    /* random: unused */
    {0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34},       /* plain */
    {0x10, 0x20, 0x24, 0x34, 0x35, 0x39, 0x3A, 0x3E},       /* shaded */
    {0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x20, 0x20},       /* stars */
    {0x15, 0x15, 0x15, 0x25, 0x25, 0x26, 0x2A, 0x2A},       /* stormy */
    {0x11, 0x21, 0x22, 0x23, 0x13, 0x07, 0x0B, 0x0F},       /* sunset */
};

/* storm state */
u16 storm_cd = 300;
u8 bolt_t, bolt_x, bolt_y, bolt_zz, flash;

const u8 band[24] = {0,0,0,0,0, 1,1,1, 2,2,2, 3,3,3, 4,4,4, 5,5,5, 6,6, 7,7};

/* ------------------------------------------------------------------ util */
u8 rnd(void) {
    rng ^= rng << 7;
    rng ^= rng >> 9;
    rng ^= rng << 8;
    return (u8)(rng >> 3);
}

s16 iabs(s16 v) { return v < 0 ? -v : v; }

static void sfx_tick(void) {
    if (nz_lvl) {
        if (++nz_cnt >= nz_rate) { nz_cnt = 0; nz_lvl--; }
        PSG = 0xF0 | (15 - nz_lvl);
    }
    if (bl_t) { if (!--bl_t) PSG = 0xBF; }
}
void sfx_boom(u8 rate) { PSG = 0xE6; nz_lvl = 15; nz_rate = rate; nz_cnt = 0; PSG = 0xF0; }
void sfx_fire(void)    { PSG = 0xE4; nz_lvl = 12; nz_rate = 1; nz_cnt = 0; PSG = 0xF3; }
void sfx_crackle(void) { PSG = 0xE5; nz_lvl = 9; nz_rate = 3; nz_cnt = 0; PSG = 0xF6; }
void sfx_blip(u8 period) {
    PSG = 0xA0 | (period & 15); PSG = (period >> 4) & 0x3F; PSG = 0xB4; bl_t = 3;
}
void sfx_whistle(u16 period) {
    PSG = 0x80 | (period & 15); PSG = (period >> 4) & 0x3F; PSG = 0x98;
}
void sfx_quiet(void) { PSG = 0x9F; }

void apply_sky(u8 sky) {
    u8 i;
    cur_sky = sky;
    star_seed = rnd();
    for (i = 0; i < 8; i++) SMS_setBGPaletteColor(i + 1, sky_pals[sky][i]);
}

/* stormy sky: occasional lightning bolt with a white flash and thunder */
static void storm_tick(void) {
    u8 x, y, k, seg;
    if (cur_sky != SKY_STORMY || !in_round || sys_open) return;
    if (bolt_t) {
        bolt_t--;
        x = bolt_x; seg = 0;
        for (y = TOPY; y + 8 <= bolt_y; y += 8) {
            k = (bolt_zz >> (seg & 7)) & 1;
            SMS_addSprite(x - 4, y, S_BOLT + k);
            if (k) { if (x < 240) x += 2; } else { if (x > 12) x -= 2; }
            seg++;
        }
        return;
    }
    if (--storm_cd) return;
    storm_cd = 240 + ((u16)rnd() << 1);
    bolt_t = 12;
    bolt_x = 16 + rnd() % 224;
    bolt_y = surface_at(bolt_x, TOPY);
    bolt_zz = rnd();
    flash = 4;
    sfx_boom(6);
}

void frame(void) {
    u16 k;
    u8 i;
    storm_tick();
    SMS_finalizeSprites();
    SMS_waitForVBlank();
    SMS_copySpritestoSAT();
    if (flash) {
        flash--;
        if (flash == 3) for (i = 1; i <= 8; i++) SMS_setBGPaletteColor(i, 0x3F);
        else if (flash == 1) for (i = 0; i < 8; i++) SMS_setBGPaletteColor(i + 1, sky_pals[cur_sky][i]);
    }
    k = SMS_getKeysStatus();
    k = (k | (k >> 6)) & 0x3F;      /* pad 2 also drives the game (hot-seat) */
    pressed = (u8)k & ~held;
    held = (u8)k;
    sfx_tick();
    frame_count++;
    SMS_initSprites();
    if (SMS_queryPauseRequested()) {          /* PAUSE opens the system menu */
        SMS_resetPauseRequest();
        if (in_round && !sys_open) sys_menu();
    }
}

int cosd(u8 a) { return a <= 90 ? (int)sindeg[90 - a] : -(int)sindeg[a - 90]; }
int sind(u8 a) { return a <= 90 ? (int)sindeg[a] : (int)sindeg[180 - a]; }

void set_wind(s8 w) {
    if (w > 15) w = 15;
    if (w < -15) w = -15;
    wind = w;
    wind_acc = (long)w * 20L;
}

/* --------------------------------------------------------------- drawing */
void draw_world(void) {
    u8 i, j, n = num_players, s;
    tank_t *t;
    j = (u8)frame_count & 3;
    while (j >= n) j -= n;
    for (i = 0; i < n; i++) {               /* rotate order: flicker, not dropout */
        t = &tanks[j];
        if (t->alive) {
            SMS_addSprite(t->x - 4, t->y - 8, S_TANK + j);
            SMS_addSprite(t->x - 4, t->y - 13, S_TURRET + t->tframe);
            if (t->shield) {
                s = (t->shield == I_MAG || t->shield == I_SUPERMAG) ? S_MAG : S_SHIELD;
                SMS_addSprite(t->x - 8, t->y - 15, s);
                SMS_addSprite(t->x, t->y - 15, s + 1);
            }
            if (t->chute) {
                SMS_addSprite(t->x - 8, t->y - 22, S_CHUTE);
                SMS_addSprite(t->x, t->y - 22, S_CHUTE + 1);
            }
        }
        if (++j >= n) j = 0;
    }
    for (i = 0; i < trail_n; i++)
        SMS_addSprite(trail_x[i] - 4, trail_y[i] - 4, trail_tile);
}

void draw_arrow(void) {
    tank_t *t = &tanks[cur_player];
    SMS_addSprite(t->x - 4, t->y - 30 - ((frame_count >> 3) & 3), S_ARROW);
}

/* ----------------------------------------------------------------- tanks */
u8 alive_count(void) {
    u8 i, n = 0;
    for (i = 0; i < num_players; i++) if (tanks[i].alive) n++;
    return n;
}

u8 grounded(tank_t *t) {
    if (t->y >= BOTY) return 1;
    if (t->y < TOPY) return 0;
    return !box_clear((int)t->x - 3, (int)t->x + 3, t->y, t->y);
}

void award(u16 a) {
    tank_t *t = &tanks[cur_player];
    t->cash = (t->cash > 60000u - a) ? 60000u : t->cash + a;
    t->score = (t->score > 60000u - a) ? 60000u : t->score + a;
}

void hurt(u8 i, u16 dmg) {
    tank_t *t = &tanks[i];
    u16 done = 0;
    if (!t->alive || !dmg) return;
    if (t->shield) {
        if (dmg >= t->shp) { done = t->shp; dmg -= t->shp; t->shp = 0; t->shield = 0; }
        else { t->shp -= dmg; done = dmg; dmg = 0; }
    }
    if (t->hp < 0) t->hp = 0;
    if (dmg > (u16)t->hp) dmg = t->hp;
    t->hp -= dmg;
    done += dmg;
    if (i != cur_player) award(done * 8);
}

void use_shield(tank_t *t, u8 it) {
    if (!t->items[it]) return;
    t->items[it]--;
    t->shield = it;
    t->shp = shield_max[it];
    sfx_blip(50);
}

static const u8 shield_rank[5] = {I_HEAVY, I_FORCE, I_SUPERMAG, I_SHIELD, I_MAG};
u8 activate_best_shield(tank_t *t) {
    u8 k;
    for (k = 0; k < 5; k++) {
        if (t->items[shield_rank[k]]) { use_shield(t, shield_rank[k]); return 1; }
    }
    return 0;
}

u8 buy_entry(tank_t *t, u8 e) {
    u16 price;
    u8 bundle, cap = 99;
    u8 *slot;
    if (!e || e >= NW + NI) return 0;
    if (e < NW) { price = weapons[e].price; bundle = weapons[e].bundle; slot = &t->ammo[e]; }
    else {
        price = items_tbl[e - NW].price; bundle = items_tbl[e - NW].bundle;
        slot = &t->items[e - NW];
        if (e - NW == I_FUEL) cap = 250;
    }
    if (t->cash < price || (u16)*slot + bundle > cap) return 0;
    t->cash -= price;
    *slot += bundle;
    return 1;
}

/* drop unsupported tanks; parachutes open after 8 px */
void settle_tanks(void) {
    u8 i, moved;
    tank_t *t;
    do {
        moved = 0;
        t = tanks;
        for (i = 0; i < num_players; i++, t++) {
            if (!t->alive) continue;
            if (grounded(t)) { t->fall = 0; t->chute = 0; continue; }
            moved = 1;
            if (!t->chute && t->fall >= 8 && t->items[I_CHUTE]) {
                t->items[I_CHUTE]--;
                t->chute = 1;
                sfx_blip(70);
            }
            if (t->chute) {
                if (frame_count & 1) t->y++;
            } else {
                t->y++;
                if (t->fall < 255) t->fall++;
                if (t->fall > 8 && !(t->fall & 1)) hurt(i, 1);
                if (!grounded(t)) { t->y++; if (t->fall < 255) t->fall++; }
            }
        }
        if (moved) { draw_world(); frame(); }
    } while (moved && !round_abort);
}

void place_tanks(void) {
    u8 i, j, x, h, slot, n = num_players;
    u8 gap = 208 / (n - 1);
    tank_t *t;
    slot = rnd() % n;
    for (i = 0; i < n; i++) {
        t = &tanks[i];
        x = 24 + slot * gap + (rnd() & 7) - 4;
        if (++slot >= n) slot = 0;
        h = height[x];
        for (j = 0; j < 8; j++) height[(u8)(x - 4 + j)] = h;
        for (j = 1; j <= 8; j++) {          /* ramp the ground into the pad */
            int hh = height[(u8)(x + 3 + j)];
            height[(u8)(x + 3 + j)] = (u8)(h + ((hh - (int)h) * (int)j) / 9);
            hh = height[(u8)(x - 4 - j)];
            height[(u8)(x - 4 - j)] = (u8)(h + ((hh - (int)h) * (int)j) / 9);
        }
        t->x = x; t->y = h;
        t->hp = 100; t->alive = 1;
        t->angle = x < 128 ? 60 : 120;
        t->tframe = (t->angle + 5) / 10;
        t->power = 50;
        t->shield = 0; t->shp = 0; t->guide = 0; t->trigger = 0;
        t->chute = 0; t->fall = 0;
    }
}

/* ---------------------------------------------------------------- rounds */
static u8 humans_in_game(void) {
    u8 i, n = 0;
    for (i = 0; i < num_players; i++) if (!tanks[i].ctrl) n++;
    return n;
}

static u8 humans_alive(void) {
    u8 i, n = 0;
    for (i = 0; i < num_players; i++) if (!tanks[i].ctrl && tanks[i].alive) n++;
    return n;
}

void play_round(void) {
    u8 i, winner = 255, reason = END_NORMAL, humans;
    tank_t *t;
    gen_terrain();
    place_tanks();
    terrain_from_heights();
    SMS_displayOff();
    apply_sky(opt_sky == SKY_RANDOM ? 1 + rnd() % (NSKY - 1) : opt_sky);
    draw_playfield();
    SMS_displayOn();
    set_wind(opt_wind ? (s8)(rnd() % 21) - 10 : 0);
    trail_n = 0;
    t = tanks;
    for (i = 0; i < num_players; i++, t++) {
        if (t->items[I_AUTODEF] && activate_best_shield(t)) t->items[I_AUTODEF]--;
    }
    cur_player = cur_round % num_players;
    turn_count = 0;
    humans = humans_in_game();
    round_abort = END_NORMAL;
    in_round = 1;
    storm_cd = 120;
    while (alive_count() > 1 && turn_count < 200 && !round_abort) {
        if (opt_autoend && humans && !humans_alive()) { round_abort = END_NOHUMANS; break; }
        t = &tanks[cur_player];
        if (t->alive) {
            if (opt_wind == 2) set_wind(wind + (s8)(rnd() % 5) - 2);
            if (t->weapon && !t->ammo[t->weapon]) t->weapon = 0;
            t->fall = 0;
            draw_hud();
            if (t->ctrl) ai_turn(); else human_turn();
            if (round_abort) break;
            draw_hud();
            talk_attack(t);
            if (round_abort) break;
            fire();
            turn_count++;
        }
        if (++cur_player >= num_players) cur_player = 0;
    }
    reason = round_abort;
    if (reason == END_MASSKILL) mass_kill();
    in_round = 0;
    round_abort = END_NORMAL;
    sfx_quiet();
    if (reason == END_NORMAL)
        for (i = 0; i < num_players; i++) if (tanks[i].alive) winner = i;
    if (alive_count() > 1) winner = 255;
    for (i = 0; i < 60; i++) { draw_world(); frame(); }
    if (winner != 255) {
        cur_player = winner;
        tanks[winner].wins++;
        award(500);
    }
    round_summary(winner, reason);
}

void main(void) {
    u8 i, k;
    tank_t *t;
    PSG = 0x9F; PSG = 0xBF; PSG = 0xDF; PSG = 0xFF;
    SMS_displayOff();
    SMS_useFirstHalfTilesforSprites(0);
    SMS_setSpriteMode(SPRITEMODE_NORMAL);
    SMS_loadBGPalette(bg_pal);
    SMS_loadSpritePalette(spr_pal);
    SMS_setBackdropColor(0);
    SMS_loadTiles(bg_tiles, T_SKY, 8 * 32);
    SMS_loadTiles(bg_tiles + 8 * 32, T_BEDROCK, 32);
    build_ground_tile();
    SMS_load1bppTiles(font_1bpp, T_FONT, 512, 0, 15);
    SMS_loadTiles(spr_tiles, T_SPR, N_SPR_TILES * 32);
    SMS_initSprites();

    SMS_loadTiles(star_tiles, T_STAR, 16 * 32);

    for (;;) {
        title();
        setup();
        t = tanks;
        for (i = 0; i < num_players; i++, t++) {
            t->score = 0; t->wins = 0; t->weapon = 0;
            for (k = 0; k < NW; k++) t->ammo[k] = 0;
            for (k = 0; k < NI; k++) t->items[k] = 0;
        }
        for (cur_round = 0; cur_round < num_rounds; cur_round++) {
            for (i = 0; i < num_players; i++) if (tanks[i].cash) shop(i);
            play_round();
        }
        game_over();
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
SMS_EMBED_SDSC_HEADER_AUTO_DATE(2, 0, "Claude", "Scorched SMS",
    "Artillery duel in the spirit of Scorched Earth. Built with devkitSMS.");
