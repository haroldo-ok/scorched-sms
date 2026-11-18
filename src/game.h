#ifndef GAME_H
#define GAME_H
#include "SMSlib.h"
#include "data.h"

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned int u16;
typedef signed int s16;

__sfr __at 0x7F PSG;

/* ---- VRAM layout ---- */
#define T_SKY     0      /* 8 gradient tiles */
#define T_GROUND  8      /* solid ground (generated at boot) */
#define T_BEDROCK 9
#define T_FONT    10     /* 64 glyphs, char c -> T_FONT + c - 32 */
#define T_POOL1   74     /* dynamic terrain tiles */
#define N_POOL1   182
#define T_SPR     256
#define NSPR      42
#define T_POOL2   (T_SPR + NSPR)
#define N_POOL    254
#define T_STAR    380    /* 16 star tiles: band*2 + variant */
#define BUBBLE    TILE_USE_SPRITE_PALETTE

/* sprite tiles (relative to 256) */
#define S_TANK   0
#define S_TURRET 4
#define S_SHOT   23
#define S_MARK   24
#define S_FIRE   25
#define S_ARROW  27
#define S_SHIELD 28
#define S_MAG    30
#define S_CHUTE  32
#define S_FLAME  34
#define S_SMOKE  36
#define S_CROSS  37
#define S_LASER  38
#define S_BALL   39
#define S_BOLT   40

#define K_UP 1
#define K_DOWN 2
#define K_LEFT 4
#define K_RIGHT 8
#define K_1 16
#define K_2 32

/* ---- terrain: bitmap of screen rows 16..183 (cell rows 2..22) ---- */
#define TOPY  16
#define BOTY  184
#define TH    168
#define CROWS 21

#define GRAV 1100L
#define MAXP 9
#define TRAIL 32
#define NFL 16

enum { ST_TITLE, ST_SETUP, ST_AIM, ST_FLIGHT, ST_EXPLODE, ST_ROUNDEND, ST_SHOP,
       ST_GAMEOVER, ST_THINK, ST_MENU, ST_MOVE, ST_TARGET, ST_TALK, ST_SYSMENU };

enum { SKY_RANDOM, SKY_PLAIN, SKY_SHADED, SKY_STARS, SKY_STORMY, SKY_SUNSET, NSKY };
/* round_abort / end reasons */
enum { END_NORMAL, END_MENU, END_MASSKILL, END_NOHUMANS };

/* weapon classes */
enum { C_EXPL, C_LEAP, C_FUNKY, C_MIRV, C_NAPALM, C_TRACER, C_ROLLER, C_RIOTCHG,
       C_RIOTBOMB, C_DIGGER, C_SANDHOG, C_DIRT, C_LIQUID, C_DIRTCHG, C_DISRUPT,
       C_PLASMA, C_LASER };

enum { W_BABYMISSILE, W_MISSILE, W_BABYNUKE, W_NUKE, W_LEAPFROG, W_FUNKY, W_MIRV,
       W_DEATHHEAD, W_NAPALM, W_HOTNAPALM, W_TRACER, W_SMOKE, W_BABYROLLER,
       W_ROLLER, W_HVYROLLER, W_RIOTCHARGE, W_RIOTBLAST, W_RIOTBOMB, W_HVYRIOT,
       W_BABYDIGGER, W_DIGGER, W_HVYDIGGER, W_BABYHOG, W_SANDHOG, W_HVYHOG,
       W_DIRTCLOD, W_DIRTBALL, W_TONDIRT, W_LIQUID, W_DIRTCHARGE, W_DISRUPTER,
       W_PLASMA, W_LASER,
       NW,                                   /* 33 buyable weapons */
       W_MIRVSUB = NW, W_DHSUB, W_FUNKYSUB, W_DEATH, NWT };

enum { I_CHUTE, I_BATTERY, I_MAG, I_SHIELD, I_FORCE, I_HEAVY, I_SUPERMAG,
       I_AUTODEF, I_FUEL, I_TRIGGER, I_HEAT, I_BALLISTIC, I_HORZ, I_VERT, I_LAZY, NI };

typedef struct {
    char name[13];
    u16 price;
    u8 bundle;
    u8 cls;
    u8 radius;
    u8 damage;
    u8 param;
} weapon_t;

typedef struct {
    char name[13];
    u16 price;
    u8 bundle;
} item_t;

/* NOTE: playtests poke fields by offset (tanks + 70*i + off) */
typedef struct {
    u8 x, y;          /* 0,1  y = first ground row under the tank */
    s16 hp;           /* 2 */
    u8 alive;         /* 4 */
    u8 angle;         /* 5 */
    u8 power;         /* 6 */
    u8 weapon;        /* 7 */
    u8 ctrl;          /* 8  0 human, 1..4 AI skill */
    u8 tframe;        /* 9 */
    u8 wins;          /* 10 */
    u8 shield;        /* 11 active shield (item index) or 0 */
    u8 shp;           /* 12 shield points */
    u8 guide;         /* 13 armed guidance (item index) or 0 */
    u8 trigger;       /* 14 contact trigger switched on */
    u8 chute;         /* 15 parachute open */
    u8 fall;          /* 16 */
    u8 pad;           /* 17 */
    u16 cash;         /* 18 */
    u16 score;        /* 20 */
    u8 ammo[NW];      /* 22 */
    u8 items[NI];     /* 55 */
} tank_t;             /* 70 bytes */

#define M_FREE 0
#define M_HORZ 1
#define M_VERT 2
typedef struct {
    long x, y, vx, vy;
    u8 active, kind, gen, mode;
} proj_t;

/* ---- main.c ---- */
extern tank_t tanks[4];
extern u8 game_state, num_players, num_rounds, cur_round, cur_player;
extern u8 setup_players, ctrl_cfg[4];
extern u8 opt_falling, opt_talk, opt_wind, opt_cash, opt_sky, opt_autoend;
extern u8 cur_sky, in_round, round_abort, sys_open;
extern s8 wind;
extern long wind_acc;
extern u8 held, pressed;
extern u16 frame_count, rng;
extern u8 shots_fired, turn_count;
extern const weapon_t weapons[NWT];
extern const item_t items_tbl[NI];
extern const u8 band[24];
u8 rnd(void);
void frame(void);
void sfx_boom(u8 rate);
void sfx_fire(void);
void sfx_blip(u8 period);
void sfx_whistle(u16 period);
void sfx_quiet(void);
void sfx_crackle(void);
int cosd(u8 a);
int sind(u8 a);
s16 iabs(s16 v);
void draw_world(void);
void draw_arrow(void);
u8 alive_count(void);
void settle_tanks(void);
u8 grounded(tank_t *t);
void hurt(u8 i, u16 dmg);
u8 activate_best_shield(tank_t *t);
void use_shield(tank_t *t, u8 it);
void set_wind(s8 w);
void apply_sky(u8 sky);
void place_tanks(void);
void award(u16 a);
u8 buy_entry(tank_t *t, u8 e);
extern const u8 shield_max[NI];
extern u8 trail_tile;

/* ---- terrain.c ---- */
extern u8 bm[TH * 32];
extern u8 height[256];
extern u8 any_dirty;
extern u8 pool_free;
void terrain_reset(void);
void gen_terrain(void);
void terrain_from_heights(void);
void draw_playfield(void);
u8 render_dirty(u8 budget);
void render_all(void);
u16 cell_tile(u8 cx, u8 cy);
void restore_rows(u8 r0, u8 r1);
void span(u8 r, int x0, int x1, u8 set);
void circle(int cx, int cy, u8 r, u8 set);
u8 solid(int x, int y);
u8 surface_at(int x, int y);
u8 box_clear(int x0, int x1, int y0, int y1);
void settle_range(int x0, int x1, u8 ybot);
void put_pixel(u8 x, u8 y);
void build_ground_tile(void);

/* ---- weapons.c ---- */
extern proj_t proj[MAXP];
extern proj_t simp;
extern int hit_x;
extern u8 hit_y;
extern u8 trail_x[TRAIL], trail_y[TRAIL], trail_n;
extern u8 guide_target;
extern int ex0, ex1;
extern u8 trig_armed;
void launch(proj_t *p, tank_t *t, u8 a, u8 pw);
u8 step(proj_t *p);
void fire(void);
void resolve_deaths(void);
void mass_kill(void);

/* ---- ai.c ---- */
s16 simulate(u8 a, u8 pw, u8 target, u8 left);
u8 solve_power(u8 a, u8 target, u8 show);
void solve_aim(u8 target, u8 show);
void ai_turn(void);
void ai_shop(u8 pi);

/* ---- ui.c ---- */
extern u8 font_bubble;
void print_at(u8 x, u8 y, const char *s);
void print_num(u8 x, u8 y, u16 v, u8 digits);
void clear_row(u8 y);
void text_screen(void);
void draw_hud(void);
void say(tank_t *t, const char *s);
void talk_attack(tank_t *t);
void talk_death(tank_t *t);
void human_turn(void);
u8 select_target(void);
void shop(u8 pi);
void title(void);
void setup(void);
void round_summary(u8 winner, u8 reason);
void sys_menu(void);
void game_over(void);
void wait_button(void);

#endif
