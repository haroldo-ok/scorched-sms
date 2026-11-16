/* ---------------------------------------------------------------------------
 * Computer players: Moron, Shooter, Poolshark, Cyborg.
 * Aiming simulates the real physics (wind, shields) and binary-searches power.
 * ------------------------------------------------------------------------- */
#include "game.h"
#include <string.h>
extern long gs[4];
extern u8 gmode;
extern u8 sim_mode;
u8 step_g(void);
void prep_tanks(void);

static u8 is_attack(u8 w) {
    u8 c = weapons[w].cls;
    return c == C_EXPL || c == C_LEAP || c == C_FUNKY || c == C_MIRV ||
           c == C_NAPALM || c == C_ROLLER || c == C_SANDHOG;
}

u8 sol_a, sol_p;
extern u8 aim_tries;
s16 sol_err;

static void think_frame(void) {
    draw_world(); draw_arrow(); frame();
}

/* signed miss: <0 short of the target, >0 past it */
s16 simulate(u8 a, u8 pw, u8 target, u8 left) {
    u8 r;
    u16 n = 700;
    s16 d;
    tank_t *me = &tanks[cur_player];
    launch(&simp, me, a, pw);
    prep_tanks();
    memcpy(gs, &simp, 16);
    gmode = M_FREE;
    sim_mode = 1;
    do { r = step_g(); } while (!r && --n);
    sim_mode = 0;
    if (r == 0) return 600;
    if (r == 2) {
        d = ((int *)&gs[0])[1] - (s16)tanks[target].x;
        if (d > 300) d = 300;
        if (d < -300) d = -300;
        return left ? -d : d;
    }
    d = hit_x - (int)me->x;
    if (d > -18 && d < 18) return -400;           /* would land on ourselves */
    d = hit_x - (int)tanks[target].x;
    return left ? -d : d;
}

/* best power for a fixed angle (binary search) */
u8 solve_power(u8 a, u8 target, u8 show) {
    u8 lo = 5, hi = 100, pw, best_p = 50, left = tanks[target].x < tanks[cur_player].x;
    s16 err, ae;
    sol_err = 32000;
    while (lo <= hi) {
        pw = (lo + hi) >> 1;
        err = simulate(a, pw, target, left);
        ae = err < 0 ? -err : err;
        if (ae < sol_err) { sol_err = ae; best_p = pw; }
        if (ae <= 2) break;
        if (err < 0) lo = pw + 1; else { if (!pw) break; hi = pw - 1; }
        if (show && (pw & 1)) think_frame();
    }
    if (sol_err > 4 && aim_tries > 2) {   /* not monotonic (pits, walls): coarse scan + refine */
        for (pw = 8; pw <= 100 && sol_err > 4; pw += 6) {
            err = simulate(a, pw, target, left);
            ae = err < 0 ? -err : err;
            if (ae < sol_err) { sol_err = ae; best_p = pw; }
            if (show) think_frame();
        }
        lo = best_p > 5 ? best_p - 5 : 0;
        hi = best_p + 5 > 100 ? 100 : best_p + 5;
        for (pw = lo; pw <= hi && sol_err > 2; pw++) {
            err = simulate(a, pw, target, left);
            ae = err < 0 ? -err : err;
            if (ae < sol_err) { sol_err = ae; best_p = pw; }
            if (show && !(pw & 1)) think_frame();
        }
    }
    return best_p;
}

static const u8 try_angles[4] = {45, 60, 75, 30};

u8 aim_tries = 4;               /* angles tried; CPU skill lowers it */

void solve_aim(u8 target, u8 show) {
    u8 i, a, p, left = tanks[target].x < tanks[cur_player].x;
    s16 best = 32000;
    sol_a = 90; sol_p = 50;
    for (i = 0; i < aim_tries && best > 3; i++) {
        a = try_angles[i];
        if (left) a = 180 - a;
        p = solve_power(a, target, show);
        if (sol_err < best) { best = sol_err; sol_a = a; sol_p = p; }
    }
    sol_err = best;
}

static const u8 err_a[5] = {0, 14, 6, 3, 1};
static const u8 err_p[5] = {0, 18, 8, 4, 1};
static const u8 attack_pref[] = {
    W_DEATHHEAD, W_NUKE, W_MIRV, W_HOTNAPALM, W_BABYNUKE, W_HVYROLLER, W_FUNKY,
    W_HVYHOG, W_LEAPFROG, W_NAPALM, W_SANDHOG, W_ROLLER, W_MISSILE, W_BABYHOG,
    W_BABYROLLER };

static u8 danger(u8 w) {
    u8 r = weapons[w].radius;
    if (weapons[w].cls == C_MIRV) return 40;
    if (weapons[w].cls == C_NAPALM) return 30;
    return r;
}

static u8 pick_weapon(tank_t *t, int dist) {
    u8 k, w;
    if (t->ctrl == 1) {
        for (k = 0; k < 8; k++) {
            w = rnd() % NW;
            if (w == 0 || t->ammo[w]) return w;
        }
        return 0;
    }
    if (dist < 44 && t->ammo[W_PLASMA]) return W_PLASMA;
    for (k = 0; k < sizeof(attack_pref); k++) {
        w = attack_pref[k];
        if (!t->ammo[w]) continue;
        if (dist < (int)danger(w) + 24) continue;
        if (t->ctrl == 4 || (rnd() & 1)) return w;
    }
    return 0;
}

void ai_turn(void) {
    tank_t *t = &tanks[cur_player], *o;
    u8 sk = t->ctrl, target = 255, i, cls, ea, ep;
    u16 key, bestkey = 0xFFFF;
    int dist, a, p;

    game_state = ST_THINK;
    print_at(17, 1, "THINKING...    ");

    /* defence */
    if (sk >= 2) {
        while (t->hp <= 60 && t->items[I_BATTERY]) {
            t->items[I_BATTERY]--;
            t->hp += 10;
            if (t->hp > 100) t->hp = 100;
        }
    }
    if (!t->shield && (sk >= 2 || (rnd() & 1))) activate_best_shield(t);

    /* target */
    o = tanks;
    for (i = 0; i < num_players; i++, o++) {
        if (i == cur_player || !o->alive) continue;
        if (sk == 4) key = 0xFFFF - o->score;
        else if (sk == 3) key = iabs((int)o->x - (int)t->x);
        else key = rnd();
        if (key <= bestkey) { bestkey = key; target = i; }
    }
    if (target == 255) return;
    o = &tanks[target];
    dist = iabs((int)o->x - (int)t->x);

    /* buried? dig out */
    if (!box_clear((int)t->x - 3, (int)t->x + 3, (int)t->y - 22, (int)t->y - 9) &&
        (t->ammo[W_RIOTCHARGE] || t->ammo[W_RIOTBLAST]) && sk >= 2) {
        t->weapon = t->ammo[W_RIOTBLAST] ? W_RIOTBLAST : W_RIOTCHARGE;
        sol_a = 90; sol_p = t->power;
    } else {
        t->weapon = pick_weapon(t, dist);
        cls = weapons[t->weapon].cls;
        if (cls == C_PLASMA) {
            sol_a = t->angle;
            p = ((dist - 8) * 5) / 2;
            sol_p = p < 10 ? 10 : (p > 100 ? 100 : p);
        } else if (cls == C_RIOTCHG || cls == C_DIRTCHG || cls == C_LASER) {
            sol_a = o->x < t->x ? 160 : 20;
            sol_p = 100;
        } else {
            aim_tries = sk + 1 > 4 ? 4 : sk + 1;
            solve_aim(target, 1);
            aim_tries = 4;
        }
    }
    ea = err_a[sk]; ep = err_p[sk];
    a = (int)sol_a + (int)(rnd() % (2 * ea + 1)) - ea;
    p = (int)sol_p + (int)(rnd() % (2 * ep + 1)) - ep;
    if (a < 0) a = 0;
    if (a > 180) a = 180;
    if (p < 5) p = 5;
    if (p > 100) p = 100;
    sol_a = a;
    t->power = p;

    if (sk == 4 && t->items[I_HEAT] && is_attack(t->weapon)) t->guide = I_HEAT;

    while (t->angle != sol_a) {             /* swing the turret 3 degrees a frame */
        if (t->angle < sol_a) t->angle += (sol_a - t->angle > 3) ? 3 : sol_a - t->angle;
        else t->angle -= (t->angle - sol_a > 3) ? 3 : t->angle - sol_a;
        t->tframe = (t->angle + 5) / 10;
        if (!(t->angle & 7)) sfx_blip(200);
        think_frame();
    }
    draw_hud();
    for (i = 0; i < 12; i++) think_frame();
}

#define EI(i) (NW + (i))
static const u8 pref2[] = {W_MISSILE, W_MISSILE, W_BABYNUKE, W_BABYROLLER, W_ROLLER,
    W_NAPALM, W_LEAPFROG, EI(I_SHIELD), EI(I_BATTERY)};
static const u8 pref3[] = {W_MISSILE, W_BABYNUKE, W_NUKE, W_FUNKY, W_MIRV, W_HOTNAPALM,
    W_HVYROLLER, W_SANDHOG, EI(I_SHIELD), EI(I_FORCE), EI(I_BATTERY), EI(I_CHUTE),
    W_RIOTCHARGE, W_PLASMA};
static const u8 pref4[] = {W_NUKE, W_BABYNUKE, W_MIRV, W_DEATHHEAD, W_HOTNAPALM, W_FUNKY,
    W_HVYHOG, W_HVYROLLER, EI(I_HEAVY), EI(I_SUPERMAG), EI(I_BATTERY), EI(I_CHUTE),
    EI(I_AUTODEF), EI(I_HEAT), W_RIOTBLAST, W_PLASMA};

void ai_shop(u8 pi) {
    tank_t *t = &tanks[pi];
    u8 k, e;
    for (k = 0; k < 16; k++) {
        switch (t->ctrl) {
        case 1: e = 1 + rnd() % (NW + NI - 1); break;
        case 2: e = pref2[rnd() % sizeof(pref2)]; break;
        case 3: e = pref3[rnd() % sizeof(pref3)]; break;
        default: e = pref4[rnd() % sizeof(pref4)]; break;
        }
        buy_entry(t, e);
    }
}
