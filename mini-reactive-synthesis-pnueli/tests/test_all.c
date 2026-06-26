/**
 * test_all.c �� Unified Test Suite
 */
#include "reactive_types.h"
#include "ltl_syntax.h"
#include "game_structure.h"
#include "automaton.h"
#include "synthesis.h"
#include "gr1_synthesis.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int g_passed = 0, g_failed = 0;
#define TEST(n) do { printf("  TEST: %s ... ", n); } while(0)
#define PASS() do { printf("PASSED\n"); g_passed++; } while(0)
#define FAIL(m) do { printf("FAILED: %s\n", m); g_failed++; } while(0)
#define CHECK(c) do { if (!(c)) { FAIL(#c); return; } } while(0)

static void t_ltl_constants(void) {
    TEST("LTL constants");
    ltl_formula_t *t = ltl_true(), *f = ltl_false();
    CHECK(t && f && t->type == LTL_TRUE && f->type == LTL_FALSE);
    PASS();
}
static void t_ltl_var(void) {
    TEST("LTL variable");
    ltl_formula_t *p = ltl_var(0);
    CHECK(p && p->type == LTL_VAR && p->var_id == 0);
    ltl_free(p);
    PASS();
}
static void t_ltl_dneg(void) {
    TEST("LTL double negation");
    ltl_formula_t *p = ltl_var(0), *np = ltl_not(p), *nnp = ltl_not(np);
    CHECK(nnp->type == LTL_VAR);
    ltl_free(p); ltl_free(np);
    PASS();
}
static void t_ltl_and_simp(void) {
    TEST("LTL AND simplification");
    ltl_formula_t *p = ltl_var(0), *pt = ltl_and(p, ltl_true());
    CHECK(pt->type == LTL_VAR);
    ltl_free(p); ltl_free(pt);
    PASS();
}
static void t_ltl_size(void) {
    TEST("LTL size");
    ltl_formula_t *p = ltl_var(0), *xp = ltl_next(p);
    CHECK(ltl_size(p) == 1 && ltl_size(xp) == 2);
    ltl_free(p); ltl_free(xp);
    PASS();
}
static void t_ltl_depth(void) {
    TEST("LTL temporal depth");
    ltl_formula_t *p = ltl_var(0), *gp = ltl_always(p);
    CHECK(ltl_temporal_depth(p) == 0 && ltl_temporal_depth(gp) == 1);
    ltl_free(p); ltl_free(gp);
    PASS();
}
static void t_ltl_collect(void) {
    TEST("LTL collect props");
    ltl_formula_t *p0 = ltl_var(0), *p3 = ltl_var(3), *ap = ltl_and(p0, p3);
    valuation_t props = ltl_collect_propositions(ap);
    CHECK(valuation_get(props, 0) && valuation_get(props, 3));
    ltl_free(p0); ltl_free(p3); ltl_free(ap);
    PASS();
}
static void t_ltl_parse(void) {
    TEST("LTL parse");
    int32_t vars;
    ltl_formula_t *f = ltl_parse("p0 & p1", &vars);
    CHECK(f != NULL);
    ltl_free(f);
    PASS();
}
static void t_ltl_nnf(void) {
    TEST("LTL NNF");
    ltl_formula_t *p = ltl_var(0), *q = ltl_var(1);
    ltl_formula_t *imp = ltl_implies(p, q), *nnf = ltl_to_nnf(imp);
    CHECK(nnf != NULL);
    ltl_free(p); ltl_free(q); ltl_free(imp); ltl_free(nnf);
    PASS();
}
static void t_ltl_eval(void) {
    TEST("LTL bounded eval");
    ltl_formula_t *p = ltl_var(0);
    valuation_t tr[3]; tr[0]=VALUATION_EMPTY; tr[1]=valuation_singleton(0); tr[2]=valuation_singleton(0);
    CHECK(!ltl_evaluate_bounded(p, tr, 3, 0) && ltl_evaluate_bounded(p, tr, 3, 1));
    ltl_free(p);
    PASS();
}
static void t_valuation_basic(void) {
    TEST("Valuation basic");
    valuation_t v = VALUATION_EMPTY;
    v = valuation_set(v, 0, true); CHECK(valuation_get(v, 0));
    v = valuation_set(v, 0, false); CHECK(!valuation_get(v, 0));
    PASS();
}
static void t_valuation_single(void) {
    TEST("Valuation singleton");
    valuation_t v = valuation_singleton(7);
    CHECK(valuation_get(v, 7) && !valuation_get(v, 0));
    PASS();
}
static void t_valuation_logic(void) {
    TEST("Valuation AND/OR/NOT");
    valuation_t a = valuation_singleton(0);
    a = valuation_set(a, 1, true);
    valuation_t b = valuation_singleton(0);
    b = valuation_set(b, 2, true);
    /* a={0,1}, b={0,2}. AND = {0}, OR = {0,1,2} */
    CHECK(valuation_get(valuation_and(a,b),0));
    CHECK(!valuation_get(valuation_and(a,b),1));
    CHECK(!valuation_get(valuation_and(a,b),2));
    CHECK(valuation_get(valuation_or(a,b),0));
    CHECK(valuation_get(valuation_or(a,b),1));
    CHECK(valuation_get(valuation_or(a,b),2));
    CHECK(!valuation_get(valuation_not(a,4),0));
    CHECK(valuation_get(valuation_not(a,4),2));
    PASS();
}
static void t_valuation_matches(void) {
    TEST("Valuation matches");
    valuation_t g = valuation_singleton(0), m = valuation_singleton(0);
    CHECK(valuation_matches(valuation_singleton(0), g, m));
    CHECK(!valuation_matches(VALUATION_EMPTY, g, m));
    PASS();
}
static void t_module_create(void) {
    TEST("Module create");
    reactive_module_t *m = reactive_module_create(4, 2, 2);
    CHECK(m && m->num_states == 4);
    reactive_module_destroy(m);
    PASS();
}
static void t_module_step(void) {
    TEST("Module step");
    reactive_module_t *m = reactive_module_create(2, 1, 1);
    valuation_t g = VALUATION_EMPTY;
    reactive_module_add_transition(m, 0, 1, g, g);
    CHECK(reactive_module_step(m, 0, g) == 1);
    reactive_module_destroy(m);
    PASS();
}
static void t_module_output(void) {
    TEST("Module output");
    reactive_module_t *m = reactive_module_create(2, 1, 2);
    reactive_module_set_output(m, 0, valuation_singleton(0));
    reactive_module_set_output(m, 1, valuation_singleton(1));
    CHECK(valuation_get(reactive_module_get_output(m,0),0));
    CHECK(valuation_get(reactive_module_get_output(m,1),1));
    reactive_module_destroy(m);
    PASS();
}
static void t_arena_create(void) {
    TEST("Arena create");
    game_arena_t *a = game_arena_create(5);
    CHECK(a && a->num_vertices == 5);
    game_arena_destroy(a);
    PASS();
}
static void t_arena_edges(void) {
    TEST("Arena edges");
    game_arena_t *a = game_arena_create(3);
    game_arena_add_edge(a, 0, 1); game_arena_add_edge(a, 0, 2);
    int32_t *succ, cnt;
    game_arena_get_successors(a, 0, &succ, &cnt);
    CHECK(cnt == 2); free(succ);
    game_arena_destroy(a);
    PASS();
}
static void t_safety_game(void) {
    TEST("Safety game");
    safety_game_t *g = safety_game_create(3);
    game_arena_add_edge(&g->arena,0,1); game_arena_add_edge(&g->arena,0,2);
    game_arena_add_edge(&g->arena,1,1); game_arena_add_edge(&g->arena,2,2);
    game_arena_set_owner(&g->arena, 0, PLAYER_SYS);
    safety_game_set_safe(g,0,true); safety_game_set_safe(g,1,true); safety_game_set_safe(g,2,false);
    bool *W = safety_game_solve(g);
    CHECK(W && W[1]); free(W);
    safety_game_destroy(g);
    PASS();
}
static void t_reach_game(void) {
    TEST("Reachability game");
    reachability_game_t *g = reachability_game_create(3);
    game_arena_add_edge(&g->arena,0,1); game_arena_add_edge(&g->arena,1,2); game_arena_add_edge(&g->arena,2,2);
    game_arena_set_owner(&g->arena,0,PLAYER_SYS);
    reachability_game_set_target(g,2,true);
    bool *R = reachability_game_solve(g);
    CHECK(R); free(R);
    reachability_game_destroy(g);
    PASS();
}
static void t_parity_game(void) {
    TEST("Parity game Zielonka");
    parity_game_t *g = parity_game_create(4,2);
    game_arena_add_edge(&g->arena,0,1); game_arena_add_edge(&g->arena,1,0);
    game_arena_add_edge(&g->arena,2,3); game_arena_add_edge(&g->arena,3,2);
    parity_game_set_priority(g,0,0); parity_game_set_priority(g,1,1);
    parity_game_set_priority(g,2,0); parity_game_set_priority(g,3,1);
    parity_game_solution_t *s = parity_game_solve_zielonka(g);
    CHECK(s); parity_game_solution_destroy(s);
    parity_game_destroy(g);
    PASS();
}
static void t_nba_create(void) {
    TEST("NBA create");
    nba_t *n = nba_create(3,2);
    CHECK(n && n->num_states==3);
    nba_destroy(n);
    PASS();
}
static void t_nba_trans(void) {
    TEST("NBA transitions");
    nba_t *n = nba_create(2,1);
    nba_add_transition(n,0,0,1);
    CHECK(nba_get_successors(n,0,0) & 2);
    nba_destroy(n);
    PASS();
}
static void t_nba_empty(void) {
    TEST("NBA emptiness");
    nba_t *n = nba_create(2,1);
    nba_add_transition(n,0,0,1); nba_add_transition(n,1,0,1);
    n->is_accepting[1]=true;
    CHECK(!nba_is_empty(n));
    nba_destroy(n);
    PASS();
}
static void t_nba_product(void) {
    TEST("NBA product");
    nba_t *a1=nba_create(2,1), *a2=nba_create(2,1);
    nba_add_transition(a1,0,0,1); nba_add_transition(a2,0,0,1);
    nba_t *p=nba_product(a1,a2);
    CHECK(p && p->num_states==4);
    nba_destroy(a1); nba_destroy(a2); nba_destroy(p);
    PASS();
}
static void t_dpa(void) {
    TEST("DPA");
    dpa_t *d = dpa_create(3,2,1);
    dpa_set_transition(d,0,0,1);
    CHECK(dpa_get_successor(d,0,0)==1);
    dpa_destroy(d);
    PASS();
}
static void t_synth_validate(void) {
    TEST("Synthesis validate");
    synthesis_spec_t *s = synthesis_spec_create(ltl_true(), ltl_var(0), 2, 2);
    const char *e=NULL;
    CHECK(synthesis_validate_spec(s,&e));
    synthesis_spec_destroy(s);
    PASS();
}
static void t_gr1_spec(void) {
    TEST("GR(1) spec");
    gr1_spec_t *s = gr1_spec_create(ltl_true(),ltl_true(),ltl_true(),ltl_true(),NULL,0,NULL,0);
    CHECK(s); gr1_spec_destroy(s);
    PASS();
}
static void t_gr1_ss(void) {
    TEST("GR(1) state space");
    gr1_state_space_t *sp = gr1_state_space_create(2,2);
    CHECK(sp && sp->num_states==16);
    gr1_state_space_destroy(sp);
    PASS();
}
static void t_sym_region(void) {
    TEST("Symbolic region");
    symbolic_region_t *r = symbolic_region_create(100);
    CHECK(!symbolic_region_contains(r,5));
    symbolic_region_add(r,5); CHECK(symbolic_region_contains(r,5));
    symbolic_region_remove(r,5); CHECK(!symbolic_region_contains(r,5));
    symbolic_region_destroy(r);
    PASS();
}

int main(void) {
    printf("=== mini-reactive-synthesis-pnueli Test Suite ===\n");
    printf("--- LTL ---\n");
    t_ltl_constants(); t_ltl_var(); t_ltl_dneg(); t_ltl_and_simp();
    t_ltl_size(); t_ltl_depth(); t_ltl_collect(); t_ltl_parse();
    t_ltl_nnf(); t_ltl_eval();
    printf("--- Valuations ---\n");
    t_valuation_basic(); t_valuation_single(); t_valuation_logic(); t_valuation_matches();
    printf("--- Modules ---\n");
    t_module_create(); t_module_step(); t_module_output();
    printf("--- Games ---\n");
    t_arena_create(); t_arena_edges(); t_safety_game(); t_reach_game(); t_parity_game();
    printf("--- Automata ---\n");
    t_nba_create(); t_nba_trans(); t_nba_empty(); t_nba_product(); t_dpa();
    printf("--- Synthesis ---\n");
    t_synth_validate(); t_gr1_spec(); t_gr1_ss(); t_sym_region();
    printf("\n=== %d passed, %d failed ===\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
