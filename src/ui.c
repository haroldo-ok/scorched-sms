/* ---------------------------------------------------------------------------
 * Text, HUD, speech bubbles, the in-game arsenal menu, tank movement,
 * target selection, the arms dealer, title / setup / results screens.
 * ------------------------------------------------------------------------- */
#include "game.h"

extern u8 sol_a, sol_p;
u8 font_bubble;

/* ------------------------------------------------------------------ text */
void print_at(u8 x, u8 y, const char *s) {
    u16 fl = font_bubble ? BUBBLE : 0;
    SMS_setNextTileatXY(x, y);
    while (*s) { SMS_setTile((T_FONT - 32 + (u8)*s) | fl); s++; }
}

void print_num(u8 x, u8 y, u16 v, u8 digits) {
    u8 buf[5], i = digits;
    u16 fl = font_bubble ? BUBBLE : 0;
    while (i) { buf[--i] = v % 10; v /= 10; }
    SMS_setNextTileatXY(x, y);
    for (i = 0; i < digits; i++) SMS_setTile((T_FONT + 16 + buf[i]) | fl);
}

static void print_digit(u8 x, u8 y, u8 d) {
    SMS_setTileatXY(x, y, (T_FONT + 16 + d) | (font_bubble ? BUBBLE : 0));
}

void clear_row(u8 y) {
    u8 i;
    SMS_setNextTileatXY(0, y);
    for (i = 0; i < 32; i++) SMS_setTile(T_FONT);
}

void text_screen(void) {
    u8 y;
    SMS_initSprites(); SMS_finalizeSprites();
    SMS_waitForVBlank(); SMS_copySpritestoSAT();
    SMS_displayOff();
    for (y = 0; y < 24; y++) clear_row(y);
    SMS_displayOn();
    SMS_initSprites();
}

void wait_button(void) {
    do { frame(); } while (!(pressed & (K_1 | K_2)));
    sfx_blip(100);
}

/* ------------------------------------------------------------------- HUD */
static const char * const guide_short[5] = {"HEAT ", "BALL ", "HORZ ", "VERT ", "     "};

void draw_hud(void) {
    tank_t *t = &tanks[cur_player];
    u8 w = t->weapon;
    font_bubble = 0;
    print_at(0, 0, "P  ANG:    PWR:    HP:    ");
    print_digit(1, 0, cur_player + 1);
    print_num(7, 0, t->angle, 3);
    print_num(15, 0, t->power, 3);
    print_num(22, 0, t->hp > 0 ? t->hp : 0, 3);
    if (t->shield) { print_at(26, 0, "SH:"); print_num(29, 0, t->shp, 3); }
    else print_at(26, 0, "      ");
    print_at(0, 1, "             ");
    print_at(0, 1, weapons[w].name);
    if (w) { print_at(12, 1, " X"); print_num(14, 1, t->ammo[w], 2); }
    else print_at(12, 1, "    ");
    print_at(16, 1, " ");
    print_at(17, 1, guide_short[t->guide ? t->guide - I_HEAT : 4]);
    print_at(22, 1, "WIND:     ");
    if (wind < 0) { print_at(27, 1, "<"); print_num(28, 1, -wind, 2); }
    else { print_num(27, 1, wind, 2); if (wind) print_at(29, 1, ">"); }
}

/* ------------------------------------------------------------ speech */
static const char * const attack_lines[16] = {
    "EAT THIS!", "INCOMING!", "SPECIAL DELIVERY", "DUCK!", "BOOM TIME",
    "NOTHING PERSONAL", "TAKE COVER!", "FIRE IN THE HOLE", "SURPRISE!",
    "READY OR NOT", "CATCH!", "SAY CHEESE", "HOLD STILL", "THIS ONE'S FOR YOU",
    "KNOCK KNOCK", "MAIL CALL!" };
static const char * const death_lines[16] = {
    "TELL MOM I TRIED", "NOT LIKE THIS!", "WORTH IT", "OUCH.", "I REGRET NOTHING",
    "MEDIC!", "SEE YOU IN SPACE", "BLAME THE WIND!", "LAG!", "UNFAIR!",
    "GOODBYE CRUEL DIRT", "I WAS AIMING!", "TOO YOUNG...", "TILT!",
    "MY WARRANTY...", "BAD AIM. YOURS." };

void say(tank_t *t, const char *s) {
    u8 len = 0, row, col, i, st = game_state;
    int y;
    const char *q = s;
    while (*q++) len++;
    len += 2;
    y = (int)t->y - 28;
    if (y < 16) y = 16;
    row = (u8)y >> 3;
    col = t->x >> 3;
    col = col > (len >> 1) ? col - (len >> 1) : 0;
    if (col + len > 32) col = 32 - len;
    game_state = ST_TALK;
    font_bubble = 1;
    print_at(col, row, " ");
    print_at(col + 1, row, s);
    print_at(col + len - 1, row, " ");
    font_bubble = 0;
    sfx_blip(90);
    for (i = 0; i < 80; i++) { draw_world(); frame(); }
    restore_rows(row, row);
    game_state = st;
}

void talk_attack(tank_t *t) {
    if (opt_talk && (rnd() % 3) == 0) say(t, attack_lines[rnd() & 15]);
}

void talk_death(tank_t *t) {
    if (opt_talk) say(t, death_lines[rnd() & 15]);
}

/* ------------------------------------------------------- target select */
static u8 next_enemy(u8 from, s8 dir) {
    u8 k, i = from;
    for (k = 0; k < 4; k++) {
        i = (u8)(i + num_players + dir) % num_players;
        if (i != cur_player && tanks[i].alive) return i;
    }
    return 255;
}

u8 select_target(void) {
    u8 tg, ok = 0, st = game_state;
    tank_t *o;
    tg = next_enemy(cur_player, 1);
    if (tg == 255) return 0;
    game_state = ST_TARGET;
    clear_row(1);
    print_at(0, 1, "TARGET:P   LR:NEXT 1:OK 2:BACK");
    for (;;) {
        print_digit(8, 1, tg + 1);
        o = &tanks[tg];
        if (frame_count & 8) SMS_addSprite(o->x - 4, o->y - 26, S_CROSS);
        draw_world();
        frame();
        if (pressed & K_LEFT) { tg = next_enemy(tg, -1); sfx_blip(150); }
        if (pressed & K_RIGHT) { tg = next_enemy(tg, 1); sfx_blip(150); }
        if (pressed & K_1) { ok = 1; guide_target = tg; break; }
        if (pressed & K_2) break;
    }
    game_state = st;
    draw_hud();
    return ok;
}

/* -------------------------------------------------------------- movement */
static u8 try_move(tank_t *t, s8 dir) {
    int nx = (int)t->x + dir, ny;
    u8 up, j;
    tank_t *o = tanks;
    if (nx < 8 || nx > 247) return 0;
    for (j = 0; j < num_players; j++, o++) {
        if (o == t || !o->alive) continue;
        if (iabs(nx - (int)o->x) < 8 && iabs((int)o->y - (int)t->y) < 10) return 0;
    }
    for (up = 0; up < 4; up++) {
        ny = (int)t->y - up;
        if (box_clear(nx - 3, nx + 3, ny - 7, ny - 1)) {
            if (t->items[I_FUEL] < 1 + up) return 0;
            t->items[I_FUEL] -= 1 + up;
            t->x = (u8)nx;
            t->y = (u8)ny;
            settle_tanks();
            return 1;
        }
    }
    return 0;
}

static void move_mode(void) {
    tank_t *t = &tanks[cur_player];
    u8 rep = 0;
    game_state = ST_MOVE;
    clear_row(1);
    print_at(0, 1, "MOVE:LEFT/RIGHT  FUEL:     1:OK");
    for (;;) {
        print_num(22, 1, t->items[I_FUEL], 3);
        draw_world(); draw_arrow(); frame();
        if (held & (K_LEFT | K_RIGHT)) {
            if (!(rep++ & 3)) {
                if (try_move(t, (held & K_LEFT) ? -1 : 1)) {
                    PSG = 0xE5; PSG = 0xF8;
                } else PSG = 0xFF;
            }
        } else { rep = 0; PSG = 0xFF; }
        if (pressed & (K_1 | K_2)) break;
        if (!t->alive || t->hp <= 0) break;
    }
    PSG = 0xFF;
    game_state = ST_AIM;
    draw_hud();
}

/* ------------------------------------------------------ in-game menu */
#define MENU_ROWS 9

static u8 next_owned_weapon(tank_t *t, u8 w, s8 dir) {
    u8 k;
    for (k = 0; k < NW; k++) {
        w = (u8)(w + NW + dir) % NW;
        if (!w || t->ammo[w]) return w;
    }
    return 0;
}

static u8 next_guide(tank_t *t, u8 g, s8 dir) {
    u8 k;
    for (k = 0; k < 5; k++) {          /* cycle 0, HEAT..VERT */
        if (dir > 0) g = (g == 0) ? I_HEAT : (g == I_VERT ? 0 : g + 1);
        else g = (g == 0) ? I_VERT : (g == I_HEAT ? 0 : g - 1);
        if (!g || t->items[g]) return g;
    }
    return 0;
}

static u8 next_shield(tank_t *t, u8 s, s8 dir) {
    u8 k;
    for (k = 0; k < 6; k++) {
        if (dir > 0) s = (s >= I_SUPERMAG || s < I_MAG) ? I_MAG : s + 1;
        else s = (s <= I_MAG) ? I_SUPERMAG : s - 1;
        if (t->items[s]) return s;
    }
    return 0;
}

static void menu_draw(tank_t *t, u8 sel, u8 shs) {
    u8 r, i;
    for (i = 0; i < MENU_ROWS; i++) {
        r = 5 + i;
        print_at(1, r, i == sel ? ">" : " ");
        print_at(2, r, "                             ");
        switch (i) {
        case 0:
            print_at(2, r, "WEAPON");
            print_at(12, r, weapons[t->weapon].name);
            if (t->weapon) { print_at(25, r, "X"); print_num(26, r, t->ammo[t->weapon], 2); }
            else print_at(25, r, "INF");
            break;
        case 1:
            print_at(2, r, "GUIDANCE");
            if (t->guide) { print_at(12, r, items_tbl[t->guide].name); print_at(25, r, "X"); print_num(26, r, t->items[t->guide], 2); }
            else print_at(12, r, "NONE");
            break;
        case 2:
            print_at(2, r, "SHIELD");
            if (shs) { print_at(12, r, items_tbl[shs].name); print_at(25, r, "X"); print_num(26, r, t->items[shs], 2); }
            else print_at(12, r, "NONE OWNED");
            break;
        case 3:
            print_at(2, r, "BATTERY   X"); print_num(13, r, t->items[I_BATTERY], 2);
            print_at(17, r, "HP"); print_num(20, r, t->hp > 0 ? t->hp : 0, 3);
            break;
        case 4:
            print_at(2, r, "LAZY BOY  X"); print_num(13, r, t->items[I_LAZY], 2);
            break;
        case 5:
            print_at(2, r, "TRIGGER   X"); print_num(13, r, t->items[I_TRIGGER], 2);
            print_at(17, r, t->trigger ? "ON" : "OFF");
            break;
        case 6:
            print_at(2, r, "MOVE TANK FUEL"); print_num(17, r, t->items[I_FUEL], 3);
            break;
        case 7:
            print_at(2, r, "PARACHUTE X"); print_num(13, r, t->items[I_CHUTE], 2);
            print_at(17, r, "(AUTO)");
            break;
        case 8:
            print_at(2, r, "DONE");
            break;
        }
    }
}

static void menu_close(void) {
    restore_rows(3, 15);
}

static void ingame_menu(void) {
    tank_t *t = &tanks[cur_player];
    u8 sel = 0, redraw = 1, shs, act, r;
    s8 dir;
    game_state = ST_MENU;
    shs = next_shield(t, 0, 1);
    for (r = 3; r <= 15; r++) clear_row(r);
    print_at(11, 3, "- ARSENAL -");
    print_at(1, 15, "UD:PICK LR:CHANGE 1:USE 2:BACK");
    for (;;) {
        if (redraw) { menu_draw(t, sel, shs); draw_hud(); redraw = 0; }
        frame();
        dir = 0;
        if (pressed & K_LEFT) dir = -1;
        if (pressed & K_RIGHT) dir = 1;
        act = pressed & K_1;
        if ((pressed & K_UP) && sel) { sel--; redraw = 1; sfx_blip(150); }
        if ((pressed & K_DOWN) && sel < MENU_ROWS - 1) { sel++; redraw = 1; sfx_blip(150); }
        if (pressed & K_2) break;
        if (!dir && !act) continue;
        redraw = 1;
        sfx_blip(110);
        switch (sel) {
        case 0: if (dir) t->weapon = next_owned_weapon(t, t->weapon, dir); break;
        case 1: if (dir) t->guide = next_guide(t, t->guide, dir); break;
        case 2:
            if (dir) shs = next_shield(t, shs, dir);
            else if (shs && t->items[shs]) {
                use_shield(t, shs);
                if (!t->items[shs]) shs = next_shield(t, shs, 1);
            }
            break;
        case 3:
            if (act && t->items[I_BATTERY] && t->hp < 100) {
                t->items[I_BATTERY]--;
                t->hp += 10;
                if (t->hp > 100) t->hp = 100;
            }
            break;
        case 4:
            if (act && t->items[I_LAZY]) {
                menu_close();
                if (select_target()) {
                    t->items[I_LAZY]--;
                    game_state = ST_THINK;
                    solve_aim(guide_target, 1);
                    t->angle = sol_a; t->power = sol_p;
                    t->tframe = (t->angle + 5) / 10;
                }
                game_state = ST_AIM;
                draw_hud();
                return;
            }
            break;
        case 5:
            if (t->items[I_TRIGGER]) t->trigger ^= 1;
            break;
        case 6:
            if (act && t->items[I_FUEL]) { menu_close(); move_mode(); return; }
            break;
        case 8:
            if (act) goto done;
            break;
        }
    }
done:
    menu_close();
    game_state = ST_AIM;
    draw_hud();
}

/* ------------------------------------------------------------- human */
static u8 needs_target(u8 g, u8 w) {
    u8 c = weapons[w].cls;
    if (c == C_RIOTCHG || c == C_DIRTCHG || c == C_PLASMA || c == C_LASER) return 0;
    return g == I_BALLISTIC || g == I_HORZ || g == I_VERT;
}

void human_turn(void) {
    tank_t *t = &tanks[cur_player];
    u8 rep = 0, dirty;
    game_state = ST_AIM;
    draw_hud();
    for (;;) {
        draw_world(); draw_arrow(); frame();
        if (!t->alive) return;
        dirty = 0;
        if (held & (K_UP | K_DOWN | K_LEFT | K_RIGHT)) {
            if (rep < 255) rep++;
            if (rep == 1 || rep > 14) {
                if ((held & K_LEFT) && t->angle < 180) t->angle++;
                if ((held & K_RIGHT) && t->angle > 0) t->angle--;
                if ((held & K_UP) && t->power < 100) t->power++;
                if ((held & K_DOWN) && t->power > 0) t->power--;
                t->tframe = (t->angle + 5) / 10;
                dirty = 1;
            }
        } else rep = 0;
        if (dirty) {
            print_num(7, 0, t->angle, 3);
            print_num(15, 0, t->power, 3);
        }
        if (pressed & K_2) { ingame_menu(); continue; }
        if (pressed & K_1) {
            if (needs_target(t->guide, t->weapon) && !select_target()) continue;
            break;
        }
    }
}

/* --------------------------------------------------------------- shop */
#define SHOP_ROWS 13
#define N_ENTRIES (NW + NI - 1)

static void shop_line(tank_t *t, u8 j, u8 row, u8 cur) {
    u8 e = j + 1, own;
    u16 price;
    u8 bundle;
    const char *name;
    if (e < NW) { name = weapons[e].name; price = weapons[e].price; bundle = weapons[e].bundle; own = t->ammo[e]; }
    else { name = items_tbl[e - NW].name; price = items_tbl[e - NW].price; bundle = items_tbl[e - NW].bundle; own = t->items[e - NW]; }
    print_at(0, row, cur ? " >" : "  ");
    print_at(2, row, "              ");
    print_at(2, row, name);
    print_at(15, row, "X");
    print_num(16, row, bundle, 2);
    print_at(19, row, "$");
    print_num(20, row, price, 4);
    print_at(24, row, "  ");
    print_num(26, row, own, 3);
}

void shop(u8 pi) {
    tank_t *t = &tanks[pi];
    u8 sel = 0, top = 0, k, redraw = 2, rep = 0;
    if (t->ctrl) { ai_shop(pi); return; }
    game_state = ST_SHOP;
    text_screen();
    print_at(5, 1, "PLAYER   - ARMS DEALER");
    print_digit(12, 1, pi + 1);
    print_at(2, 5, "ITEM         QTY PRICE   OWN");
    print_at(1, 21, "UD:MOVE LR:PAGE 1:BUY 2:DONE");
    for (;;) {
        if (redraw) {
            print_at(5, 3, "CASH: $");
            print_num(12, 3, t->cash, 5);
            if (redraw == 2) {
                for (k = 0; k < SHOP_ROWS; k++) {
                    if (top + k < N_ENTRIES) shop_line(t, top + k, 7 + k, top + k == sel);
                    else clear_row(7 + k);
                }
            } else shop_line(t, sel, 7 + sel - top, 1);
            redraw = 0;
        }
        frame();
        if (held & (K_UP | K_DOWN)) { if (rep < 255) rep++; } else rep = 0;
        if (((pressed & K_UP) || ((held & K_UP) && rep > 15 && !(rep & 3))) && sel) {
            shop_line(t, sel, 7 + sel - top, 0);
            sel--;
            if (sel < top) { top = sel; redraw = 2; } else redraw = 1;
        }
        if (((pressed & K_DOWN) || ((held & K_DOWN) && rep > 15 && !(rep & 3))) && sel < N_ENTRIES - 1) {
            shop_line(t, sel, 7 + sel - top, 0);
            sel++;
            if (sel >= top + SHOP_ROWS) { top = sel - SHOP_ROWS + 1; redraw = 2; } else redraw = 1;
        }
        if (pressed & K_RIGHT) {
            sel = sel + SHOP_ROWS < N_ENTRIES ? sel + SHOP_ROWS : N_ENTRIES - 1;
            top = sel >= SHOP_ROWS - 1 ? sel - SHOP_ROWS + 1 : 0;
            redraw = 2;
        }
        if (pressed & K_LEFT) {
            sel = sel >= SHOP_ROWS ? sel - SHOP_ROWS : 0;
            if (sel < top) top = sel;
            redraw = 2;
        }
        if (pressed & K_2) break;
        if (pressed & K_1) {
            if (buy_entry(t, sel + 1)) sfx_blip(80); else sfx_blip(600);
            redraw = 1;
        }
    }
}

/* -------------------------------------------------------------- screens */
static const char * const ctrl_names[5] = {"HUMAN    ", "MORON    ", "SHOOTER  ", "POOLSHARK", "CYBORG   "};

void title(void) {
    u8 i;
    tank_t *t;
    game_state = ST_TITLE;
    num_players = 4;
    gen_terrain();
    place_tanks();
    terrain_from_heights();
    t = tanks;
    for (i = 0; i < 4; i++, t++) { t->angle = 30 + (rnd() % 120); t->tframe = (t->angle + 5) / 10; }
    trail_n = 0;
    SMS_displayOff();
    draw_playfield();
    font_bubble = 1;
    print_at(4, 3, "  S C O R C H E D  S M S  ");
    print_at(1, 5, " THE MOTHER OF ALL 8-BIT GAMES ");
    font_bubble = 0;
    SMS_displayOn();
    for (;;) {
        draw_world(); frame();
        if (!(frame_count & 31)) {
            if (frame_count & 32) print_at(4, 1, "PRESS BUTTON 1 TO START");
            else clear_row(1);
        }
        if (pressed & K_1) break;
    }
    rng ^= frame_count * 251u + 1;
    if (!rng) rng = 0xACE1;
    sfx_blip(100);
}

static const u16 cash_opts[5] = {0, 2000, 5000, 10000, 20000};
static const char * const wind_names[3] = {"OFF     ", "NORMAL  ", "CHANGING"};

void setup(void) {
    u8 sel = 0, i, redraw = 1, items, r, base;
    s8 dir;
    game_state = ST_SETUP;
    text_screen();
    print_at(11, 1, "GAME SETUP");
    print_at(1, 21, "UD:SELECT  LR:CHANGE  1:START");
    for (;;) {
        base = setup_players + 1;
        items = base + 6;
        if (redraw) {
            for (i = 3; i < 20; i++) clear_row(i);
            print_at(6, 3, "PLAYERS");       print_num(21, 3, setup_players, 1);
            for (i = 0; i < setup_players; i++) {
                print_at(6, 5 + i, "PLAYER");
                print_digit(13, 5 + i, i + 1);
                print_at(21, 5 + i, ctrl_names[ctrl_cfg[i]]);
            }
            print_at(6, 10, "ROUNDS");        print_num(21, 10, num_rounds, 2);
            print_at(6, 11, "START CASH");    print_num(21, 11, cash_opts[opt_cash], 5);
            print_at(6, 12, "FALLING DIRT");  print_at(21, 12, opt_falling ? "ON " : "OFF");
            print_at(6, 13, "TALKING TANKS"); print_at(21, 13, opt_talk ? "ON " : "OFF");
            print_at(6, 14, "WIND");          print_at(21, 14, wind_names[opt_wind]);
            print_at(6, 16, "START GAME");
            if (sel == 0) r = 3;
            else if (sel < base) r = 4 + sel;
            else if (sel < base + 5) r = 10 + sel - base;
            else r = 16;
            print_at(4, r, ">");
            redraw = 0;
        }
        frame();
        if ((pressed & K_UP) && sel) { sel--; redraw = 1; sfx_blip(150); }
        if ((pressed & K_DOWN) && sel < items - 1) { sel++; redraw = 1; sfx_blip(150); }
        dir = 0;
        if (pressed & K_LEFT) dir = -1;
        if (pressed & (K_RIGHT | K_1)) dir = 1;
        if (!dir) continue;
        redraw = 1;
        sfx_blip(100);
        if (sel == 0) {
            setup_players += dir;
            if (setup_players > 4) setup_players = 2;
            if (setup_players < 2) setup_players = 4;
        } else if (sel < base) {
            i = sel - 1;
            ctrl_cfg[i] = (u8)(ctrl_cfg[i] + 5 + dir) % 5;
        } else if (sel == base) {
            num_rounds = dir > 0 ? (num_rounds == 3 ? 5 : (num_rounds == 5 ? 10 : 3))
                                 : (num_rounds == 3 ? 10 : (num_rounds == 10 ? 5 : 3));
        } else if (sel == base + 1) opt_cash = (u8)(opt_cash + 5 + dir) % 5;
        else if (sel == base + 2) opt_falling ^= 1;
        else if (sel == base + 3) opt_talk ^= 1;
        else if (sel == base + 4) opt_wind = (u8)(opt_wind + 3 + dir) % 3;
        else if (pressed & K_1) break;
    }
    num_players = setup_players;
    for (i = 0; i < num_players; i++) {
        tanks[i].ctrl = ctrl_cfg[i];
        tanks[i].cash = cash_opts[opt_cash];
    }
}

static void score_table(u8 y) {
    u8 i;
    print_at(3, y, "PLAYER          SCORE  WINS");
    for (i = 0; i < num_players; i++) {
        print_at(3, y + 2 + i, "P");
        print_digit(4, y + 2 + i, i + 1);
        print_at(6, y + 2 + i, ctrl_names[tanks[i].ctrl]);
        print_num(19, y + 2 + i, tanks[i].score, 5);
        print_num(26, y + 2 + i, tanks[i].wins, 2);
    }
}

void round_summary(u8 winner) {
    game_state = ST_ROUNDEND;
    text_screen();
    print_at(10, 3, "ROUND    OVER");
    print_num(16, 3, cur_round + 1, 2);
    if (winner == 255) print_at(9, 6, "NO SURVIVORS!");
    else {
        print_at(7, 6, "PLAYER   SURVIVES!");
        print_digit(14, 6, winner + 1);
    }
    score_table(10);
    print_at(8, 20, "PRESS A BUTTON");
    wait_button();
}

void game_over(void) {
    u8 i, best = 0;
    game_state = ST_GAMEOVER;
    text_screen();
    for (i = 1; i < num_players; i++) if (tanks[i].score > tanks[best].score) best = i;
    print_at(11, 3, "GAME OVER");
    print_at(5, 6, "PLAYER   WINS THE WAR!");
    print_digit(12, 6, best + 1);
    score_table(10);
    print_at(8, 20, "PRESS A BUTTON");
    wait_button();
}
