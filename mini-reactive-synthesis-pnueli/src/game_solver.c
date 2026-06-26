/**
 * game_solver.c -- Game Solving Algorithms for Reactive Synthesis
 *
 * Implements safety game solving (greatest fixed point), reachability
 * game solving (attractor computation), parity game solving
 * (Zielonka's recursive algorithm and Jurdzinski's progress measures),
 * and strategy extraction from winning regions.
 *
 * Reference: Zielonka (1998) TCS; Jurdzinski (2000) STACS;
 *            Pnueli & Rosner (1989) FOCS
 *
 * Knowledge Level: L5 Algorithms, L4 Fundamental Laws
 */

#include "game_structure.h"
#include "reactive_types.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ====================================================================
 * Game Arena Construction
 * ==================================================================== */

game_arena_t *game_arena_create(int32_t num_vertices) {
    assert(num_vertices > 0);
    game_arena_t *arena = (game_arena_t *)calloc(1, sizeof(game_arena_t));
    assert(arena != NULL);
    arena->num_vertices = num_vertices;
    arena->owner = (player_t *)calloc((size_t)num_vertices, sizeof(player_t));
    arena->outdegree = (int32_t *)calloc((size_t)num_vertices, sizeof(int32_t));
    arena->is_initial = (bool *)calloc((size_t)num_vertices, sizeof(bool));
    arena->edge_start = (int32_t *)calloc((size_t)(num_vertices + 1), sizeof(int32_t));
    arena->edges = NULL;
    arena->num_edges = 0;
    assert(arena->owner && arena->outdegree && arena->is_initial && arena->edge_start);
    return arena;
}

void game_arena_destroy(game_arena_t *arena) {
    if (!arena) return;
    free(arena->owner); free(arena->outdegree);
    free(arena->edges); free(arena->edge_start);
    free(arena->is_initial); free(arena);
}

void game_arena_add_edge(game_arena_t *arena, int32_t src, int32_t dst) {
    assert(arena && src >= 0 && src < arena->num_vertices);
    assert(dst >= 0 && dst < arena->num_vertices);
    int32_t cap = arena->edge_start[arena->num_vertices];
    if (arena->num_edges >= cap) {
        int32_t nc = (cap == 0) ? 64 : cap * 2;
        arena->edges = (int32_t *)realloc(arena->edges, (size_t)nc * sizeof(int32_t));
        assert(arena->edges);
        arena->edge_start[arena->num_vertices] = nc;
    }
    arena->edges[arena->num_edges++] = dst;
    arena->outdegree[src]++;
}

void game_arena_set_owner(game_arena_t *arena, int32_t v, player_t o) {
    assert(arena && v >= 0 && v < arena->num_vertices);
    arena->owner[v] = o;
}

void game_arena_set_initial(game_arena_t *arena, int32_t v, bool init) {
    assert(arena && v >= 0 && v < arena->num_vertices);
    arena->is_initial[v] = init;
}

void game_arena_get_successors(const game_arena_t *arena, int32_t v,
                                int32_t **succ, int32_t *cnt) {
    assert(arena && v >= 0 && v < arena->num_vertices);
    int32_t accum = 0;
    int32_t start = 0;
    for (int32_t i = 0; i < v; i++) accum += arena->outdegree[i];
    start = accum;
    *cnt = arena->outdegree[v];
    *succ = (int32_t *)malloc((size_t)(*cnt) * sizeof(int32_t));
    assert(*succ);
    for (int32_t e = 0; e < *cnt; e++)
        (*succ)[e] = arena->edges[start + e];
}

void game_arena_print_dot(const game_arena_t *arena) {
    if (!arena) return;
    printf("digraph G {\n  rankdir=LR;\n");
    for (int32_t v = 0; v < arena->num_vertices; v++) {
        const char *s = (arena->owner[v] == PLAYER_ENV) ? "box" : "diamond";
        printf("  v%d [label=\"v%d\", shape=%s];\n", v, v, s);
    }
    int32_t acc = 0;
    for (int32_t v = 0; v < arena->num_vertices; v++) {
        int32_t st = acc; acc += arena->outdegree[v];
        for (int32_t e = st; e < acc && e < arena->num_edges; e++)
            printf("  v%d -> v%d;\n", v, arena->edges[e]);
    }
    printf("}\n");
}

/* ====================================================================
 * Parity Game
 * ==================================================================== */

parity_game_t *parity_game_create(int32_t nv, int32_t mp) {
    assert(nv > 0 && mp >= 0);
    parity_game_t *g = (parity_game_t *)calloc(1, sizeof(parity_game_t));
    assert(g);
    g->arena = *game_arena_create(nv);
    g->priority = (int32_t *)calloc((size_t)nv, sizeof(int32_t));
    g->max_priority = mp;
    assert(g->priority);
    return g;
}

void parity_game_destroy(parity_game_t *g) {
    if (!g) return;
    free(g->priority);
    free(g->arena.owner); free(g->arena.outdegree);
    free(g->arena.edges); free(g->arena.edge_start);
    free(g->arena.is_initial); free(g);
}

void parity_game_set_priority(parity_game_t *g, int32_t v, int32_t p) {
    assert(g && v >= 0 && v < g->arena.num_vertices);
    assert(p >= 0 && p <= g->max_priority);
    g->priority[v] = p;
}

void parity_game_solution_destroy(parity_game_solution_t *s) {
    if (!s) return;
    free(s->winning_region[0]); free(s->winning_region[1]);
    free(s->strategy); free(s);
}

/* ====================================================================
 * Attractor Computation
 *
 * attr_p(T) = least fixed point containing T and closed under:
 *   v in V_p and E(v) intersects attr_p(T) => v in attr_p(T)
 *   v in V_{1-p} and E(v) subset of attr_p(T) => v in attr_p(T)
 *
 * Reference: Thomas (1995); Zielonka (1998)
 * ==================================================================== */

static void attractor_compute(const game_arena_t *arena, const bool *target,
                               player_t player, bool *result) {
    int32_t n = arena->num_vertices;
    memcpy(result, target, (size_t)n * sizeof(bool));
    int32_t *remaining = (int32_t *)malloc((size_t)n * sizeof(int32_t));
    int32_t *queue = (int32_t *)malloc((size_t)n * sizeof(int32_t));
    int32_t *pred_list = (int32_t *)malloc((size_t)(arena->num_edges + n) * sizeof(int32_t));
    assert(remaining && queue && pred_list);

    int32_t acc = 0;
    for (int32_t v = 0; v < n; v++) {
        remaining[v] = arena->outdegree[v];
    }

    /* Build reverse adjacency: for each vertex, list of predecessors */
    int32_t *pred_start = (int32_t *)calloc((size_t)(n + 1), sizeof(int32_t));
    int32_t *pred_count = (int32_t *)calloc((size_t)n, sizeof(int32_t));
    /* Count predecessors */
    acc = 0;
    for (int32_t v = 0; v < n; v++) {
        int32_t st = acc; acc += arena->outdegree[v];
        for (int32_t e = st; e < acc && e < arena->num_edges; e++) {
            int32_t u = arena->edges[e];
            pred_count[u]++;
        }
    }
    /* Build prefix sums */
    int32_t total_preds = 0;
    for (int32_t v = 0; v < n; v++) {
        pred_start[v] = total_preds;
        total_preds += pred_count[v];
        pred_count[v] = 0; /* reset for filling */
    }
    pred_start[n] = total_preds;
    /* Fill predecessor lists */
    acc = 0;
    for (int32_t v = 0; v < n; v++) {
        int32_t st = acc; acc += arena->outdegree[v];
        for (int32_t e = st; e < acc && e < arena->num_edges; e++) {
            int32_t u = arena->edges[e];
            int32_t pos = pred_start[u] + pred_count[u];
            pred_list[pos] = v;
            pred_count[u]++;
        }
    }

    /* BFS from target set */
    int32_t qh = 0, qt = 0;
    for (int32_t v = 0; v < n; v++)
        if (target[v]) queue[qt++] = v;

    while (qh < qt) {
        int32_t u = queue[qh++];
        /* For each predecessor v of u */
        for (int32_t p = pred_start[u]; p < pred_start[u + 1]; p++) {
            int32_t v = pred_list[p];
            if (result[v]) continue;
            if (arena->owner[v] == player) {
                result[v] = true;
                queue[qt++] = v;
            } else {
                remaining[v]--;
                if (remaining[v] == 0) {
                    result[v] = true;
                    queue[qt++] = v;
                }
            }
        }
    }

    free(remaining); free(queue); free(pred_list);
    free(pred_start); free(pred_count);
}

/* ====================================================================
 * Zielonka's Recursive Parity Game Solver
 *
 * solve(G):
 *   if V empty: return (empty, empty)
 *   p = max priority in G, alpha = p mod 2
 *   U = vertices with priority p
 *   A = attractor_alpha(U) within G
 *   (W0', W1') = solve(G \ A)
 *   if W_{1-alpha}' empty:
 *     W_alpha = W_alpha' union A, W_{1-alpha} = W_{1-alpha}'
 *   else:
 *     B = attractor_{1-alpha}(W_{1-alpha}') within G
 *     (W0'', W1'') = solve(G \ B)
 *     W_alpha = W_alpha'', W_{1-alpha} = W_{1-alpha}'' union B
 *
 * Reference: Zielonka (1998) TCS
 * ==================================================================== */

static int32_t find_max_priority_in_set(const parity_game_t *game,
                                         const bool *in_set) {
    int32_t mp = -1;
    for (int32_t v = 0; v < game->arena.num_vertices; v++)
        if (in_set[v] && game->priority[v] > mp) mp = game->priority[v];
    return mp;
}

static void zielonka_rec(const parity_game_t *game, const bool *V_set,
                          bool *W0, bool *W1, int32_t depth) {
    int32_t n = game->arena.num_vertices;
    if (depth > 1000) return;

    int32_t max_p = find_max_priority_in_set(game, V_set);
    if (max_p < 0) return;

    player_t alpha = (max_p % 2 == 0) ? PLAYER_ENV : PLAYER_SYS;

    /* U = {v in V_set | priority[v] == max_p} */
    bool *U = (bool *)calloc((size_t)n, sizeof(bool));
    for (int32_t v = 0; v < n; v++)
        if (V_set[v] && game->priority[v] == max_p) U[v] = true;

    /* A = attractor_{alpha}(U) */
    bool *A = (bool *)calloc((size_t)n, sizeof(bool));
    attractor_compute(&game->arena, U, alpha, A);

    /* V_prime = V_set \ A */
    bool *Vp = (bool *)calloc((size_t)n, sizeof(bool));
    bool nonempty = false;
    for (int32_t v = 0; v < n; v++)
        if (V_set[v] && !A[v]) { Vp[v] = true; nonempty = true; }

    bool *W0p = (bool *)calloc((size_t)n, sizeof(bool));
    bool *W1p = (bool *)calloc((size_t)n, sizeof(bool));

    if (nonempty) zielonka_rec(game, Vp, W0p, W1p, depth + 1);

    player_t other = (alpha == PLAYER_ENV) ? PLAYER_SYS : PLAYER_ENV;
    bool *W_other = (other == PLAYER_ENV) ? W0p : W1p;

    bool other_empty = true;
    for (int32_t v = 0; v < n; v++)
        if (Vp[v] && W_other[v]) { other_empty = false; break; }

    if (other_empty) {
        bool *W_alpha = (alpha == PLAYER_ENV) ? W0 : W1;
        for (int32_t v = 0; v < n; v++) {
            if (W0p[v]) W0[v] = true;
            if (W1p[v]) W1[v] = true;
            if (A[v]) W_alpha[v] = true;
        }
    } else {
        bool *B = (bool *)calloc((size_t)n, sizeof(bool));
        attractor_compute(&game->arena, W_other, other, B);

        bool *Vdp = (bool *)calloc((size_t)n, sizeof(bool));
        bool ne2 = false;
        for (int32_t v = 0; v < n; v++)
            if (V_set[v] && !B[v]) { Vdp[v] = true; ne2 = true; }

        bool *W0dp = (bool *)calloc((size_t)n, sizeof(bool));
        bool *W1dp = (bool *)calloc((size_t)n, sizeof(bool));
        if (ne2) zielonka_rec(game, Vdp, W0dp, W1dp, depth + 1);

        for (int32_t v = 0; v < n; v++) {
            if (W0dp[v]) W0[v] = true;
            if (W1dp[v]) W1[v] = true;
            if (B[v]) {
                if (other == PLAYER_ENV) W0[v] = true;
                else W1[v] = true;
            }
        }
        free(B); free(Vdp); free(W0dp); free(W1dp);
    }
    free(U); free(A); free(Vp); free(W0p); free(W1p);
}

parity_game_solution_t *parity_game_solve_zielonka(const parity_game_t *game) {
    assert(game);
    int32_t n = game->arena.num_vertices;
    parity_game_solution_t *sol = (parity_game_solution_t *)calloc(1, sizeof(parity_game_solution_t));
    assert(sol);
    sol->winning_region[0] = (bool *)calloc((size_t)n, sizeof(bool));
    sol->winning_region[1] = (bool *)calloc((size_t)n, sizeof(bool));
    sol->strategy = (int32_t *)malloc((size_t)n * sizeof(int32_t));
    assert(sol->winning_region[0] && sol->winning_region[1] && sol->strategy);
    for (int32_t i = 0; i < n; i++) sol->strategy[i] = -1;

    bool *all = (bool *)malloc((size_t)n * sizeof(bool));
    for (int32_t i = 0; i < n; i++) all[i] = true;
    zielonka_rec(game, all, sol->winning_region[0], sol->winning_region[1], 0);
    free(all);

    for (int32_t v = 0; v < n; v++) {
        if (sol->winning_region[0][v]) sol->total_win_size[0]++;
        if (sol->winning_region[1][v]) sol->total_win_size[1]++;
    }

    /* Extract positional strategies */
    int32_t acc = 0;
    for (int32_t v = 0; v < n; v++) {
        int32_t st = acc; acc += game->arena.outdegree[v];
        for (int32_t p = 0; p < 2; p++) {
            if (sol->winning_region[p][v] && game->arena.owner[v] == (player_t)p) {
                for (int32_t e = st; e < acc && e < game->arena.num_edges; e++) {
                    if (sol->winning_region[p][game->arena.edges[e]]) {
                        sol->strategy[v] = game->arena.edges[e]; break;
                    }
                }
            }
        }
    }
    return sol;
}

/* ====================================================================
 * Jurdzinski Progress Measures Solver
 *
 * Reference: Jurdzinski (2000) STACS
 * ==================================================================== */

typedef struct { int32_t *M; int32_t mp; } pm_t;

static pm_t *pm_create(int32_t n, int32_t mp) {
    pm_t *p = (pm_t *)malloc(sizeof(pm_t));
    p->mp = mp;
    p->M = (int32_t *)calloc((size_t)(n * (mp + 1)), sizeof(int32_t));
    return p;
}
static void pm_free(pm_t *p) { if (p) { free(p->M); free(p); } }

static bool pm_less(const pm_t *p, int32_t v, int32_t w) {
    int32_t bv = v * (p->mp + 1), bw = w * (p->mp + 1);
    for (int32_t i = 0; i <= p->mp; i++) {
        if (p->M[bv + i] < p->M[bw + i]) return true;
        if (p->M[bv + i] > p->M[bw + i]) return false;
    }
    return false;
}

static bool pm_lift(pm_t *pm, const parity_game_t *game, int32_t v, int32_t u) {
    int32_t bv = v * (pm->mp + 1), bu = u * (pm->mp + 1);
    int32_t pv = game->priority[v];
    bool changed = false;
    for (int32_t p = 0; p <= pm->mp; p++) {
        int32_t nv;
        if (p < pv) nv = 0;
        else if (p == pv) nv = pm->M[bu + p] + ((pv % 2 == 0) ? 0 : 1);
        else nv = pm->M[bu + p];
        if (nv > pm->M[bv + p]) { pm->M[bv + p] = nv; changed = true; }
    }
    return changed;
}

parity_game_solution_t *parity_game_solve_jurdzinski(const parity_game_t *game) {
    assert(game);
    int32_t n = game->arena.num_vertices, d = game->max_priority;
    pm_t *pm = pm_create(n, d);
    bool changed = true;
    int32_t iter = 0;
    while (changed && iter < n * n * d + 100) {
        changed = false;
        int32_t acc = 0;
        for (int32_t v = 0; v < n; v++) {
            int32_t st = acc; acc += game->arena.outdegree[v];
            for (int32_t e = st; e < acc && e < game->arena.num_edges; e++)
                if (pm_lift(pm, game, v, game->arena.edges[e])) changed = true;
        }
        iter++;
    }
    parity_game_solution_t *sol = (parity_game_solution_t *)calloc(1, sizeof(parity_game_solution_t));
    assert(sol);
    sol->winning_region[0] = (bool *)calloc((size_t)n, sizeof(bool));
    sol->winning_region[1] = (bool *)calloc((size_t)n, sizeof(bool));
    sol->strategy = (int32_t *)malloc((size_t)n * sizeof(int32_t));
    for (int32_t v = 0; v < n; v++) {
        int32_t b = v * (d + 1);
        bool fin = (pm->M[b] < 1000000);
        sol->winning_region[0][v] = fin;
        sol->winning_region[1][v] = !fin;
        sol->strategy[v] = -1;
    }
    int32_t acc = 0;
    for (int32_t v = 0; v < n; v++) {
        int32_t st = acc; acc += game->arena.outdegree[v];
        for (int32_t e = st; e < acc && e < game->arena.num_edges; e++) {
            int32_t u = game->arena.edges[e];
            if ((sol->winning_region[0][v] && sol->winning_region[0][u]) ||
                (sol->winning_region[1][v] && sol->winning_region[1][u]))
                { sol->strategy[v] = u; break; }
        }
    }
    for (int32_t p = 0; p < 2; p++)
        for (int32_t v = 0; v < n; v++)
            if (sol->winning_region[p][v]) sol->total_win_size[p]++;
    pm_free(pm);
    return sol;
}

/* ====================================================================
 * Strategy Extraction
 * ==================================================================== */

reactive_module_t *parity_game_extract_strategy(const parity_game_t *game,
                                                  const parity_game_solution_t *sol,
                                                  int32_t ni, int32_t no) {
    assert(game && sol);
    int32_t n = game->arena.num_vertices;
    int32_t wc = 0;
    for (int32_t v = 0; v < n; v++)
        if (sol->winning_region[PLAYER_SYS][v]) wc++;
    if (wc == 0) return NULL;

    reactive_module_t *mod = reactive_module_create(wc, ni, no);
    int32_t *v2s = (int32_t *)malloc((size_t)n * sizeof(int32_t));
    int32_t ms = 0;
    for (int32_t v = 0; v < n; v++) {
        v2s[v] = -1;
        if (sol->winning_region[PLAYER_SYS][v]) {
            v2s[v] = ms;
            valuation_t out = VALUATION_EMPTY;
            out = valuation_set(out, 0, (v % 2 == 0));
            reactive_module_set_output(mod, ms, out);
            ms++;
        }
    }
    int32_t acc = 0;
    for (int32_t v = 0; v < n; v++) {
        int32_t st = acc; acc += game->arena.outdegree[v];
        if (v2s[v] < 0) continue;
        int32_t tgt = sol->strategy[v];
        if (tgt >= 0 && v2s[tgt] >= 0)
            reactive_module_add_transition(mod, v2s[v], v2s[tgt], VALUATION_EMPTY, VALUATION_EMPTY);
    }
    free(v2s);
    return mod;
}

/* ====================================================================
 * Safety Game
 * ==================================================================== */

safety_game_t *safety_game_create(int32_t nv) {
    assert(nv > 0);
    safety_game_t *g = (safety_game_t *)calloc(1, sizeof(safety_game_t));
    assert(g);
    g->arena = *game_arena_create(nv);
    g->safe_set = (bool *)calloc((size_t)nv, sizeof(bool));
    assert(g->safe_set);
    return g;
}

void safety_game_destroy(safety_game_t *g) {
    if (!g) return;
    free(g->safe_set);
    free(g->arena.owner); free(g->arena.outdegree);
    free(g->arena.edges); free(g->arena.edge_start);
    free(g->arena.is_initial); free(g);
}

void safety_game_set_safe(safety_game_t *g, int32_t v, bool s) {
    assert(g && v >= 0 && v < g->arena.num_vertices);
    g->safe_set[v] = s;
}

bool *safety_game_solve(const safety_game_t *game) {
    assert(game);
    int32_t n = game->arena.num_vertices;
    bool *W = (bool *)malloc((size_t)n * sizeof(bool));
    bool *Wn = (bool *)malloc((size_t)n * sizeof(bool));
    assert(W && Wn);
    memcpy(W, game->safe_set, (size_t)n * sizeof(bool));
    bool changed = true;
    while (changed) {
        changed = false;
        memcpy(Wn, W, (size_t)n * sizeof(bool));
        int32_t acc = 0;
        for (int32_t v = 0; v < n; v++) {
            int32_t st = acc; acc += game->arena.outdegree[v];
            if (!W[v]) continue;
            bool ok = true;
            if (game->arena.owner[v] == PLAYER_ENV) {
                for (int32_t e = st; e < acc && e < game->arena.num_edges; e++)
                    if (!W[game->arena.edges[e]]) { ok = false; break; }
            } else {
                ok = (game->arena.outdegree[v] == 0);
                for (int32_t e = st; e < acc && e < game->arena.num_edges; e++)
                    if (W[game->arena.edges[e]]) { ok = true; break; }
            }
            if (!ok) { Wn[v] = false; changed = true; }
        }
        memcpy(W, Wn, (size_t)n * sizeof(bool));
    }
    free(Wn);
    return W;
}

int32_t *safety_game_extract_strategy(const safety_game_t *game, const bool *WR) {
    assert(game && WR);
    int32_t n = game->arena.num_vertices;
    int32_t *s = (int32_t *)malloc((size_t)n * sizeof(int32_t));
    for (int32_t i = 0; i < n; i++) s[i] = -1;
    int32_t acc = 0;
    for (int32_t v = 0; v < n; v++) {
        int32_t st = acc; acc += game->arena.outdegree[v];
        if (!WR[v] || game->arena.owner[v] != PLAYER_SYS) continue;
        for (int32_t e = st; e < acc && e < game->arena.num_edges; e++)
            if (WR[game->arena.edges[e]]) { s[v] = game->arena.edges[e]; break; }
    }
    return s;
}

/* ====================================================================
 * Reachability Game
 * ==================================================================== */

reachability_game_t *reachability_game_create(int32_t nv) {
    assert(nv > 0);
    reachability_game_t *g = (reachability_game_t *)calloc(1, sizeof(reachability_game_t));
    assert(g);
    g->arena = *game_arena_create(nv);
    g->target_set = (bool *)calloc((size_t)nv, sizeof(bool));
    assert(g->target_set);
    return g;
}

void reachability_game_destroy(reachability_game_t *g) {
    if (!g) return;
    free(g->target_set);
    free(g->arena.owner); free(g->arena.outdegree);
    free(g->arena.edges); free(g->arena.edge_start);
    free(g->arena.is_initial); free(g);
}

void reachability_game_set_target(reachability_game_t *g, int32_t v, bool t) {
    assert(g && v >= 0 && v < g->arena.num_vertices);
    g->target_set[v] = t;
}

bool *reachability_game_solve(const reachability_game_t *game) {
    assert(game);
    int32_t n = game->arena.num_vertices;
    bool *R = (bool *)calloc((size_t)n, sizeof(bool));
    assert(R);
    attractor_compute(&game->arena, game->target_set, PLAYER_SYS, R);
    return R;
}
