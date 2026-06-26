/**
 * test_all.c - Comprehensive tests for symbolic model checking with OBDDs
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include "obdd.h"
#include "model.h"
#include "ctlsmc.h"
#include "fixpoint.h"
#include "params.h"

static int g_pass = 0, g_fail = 0;
#define T(n) printf("  %-45s ... ", n)
#define P()  do { printf("PASS\n"); g_pass++; } while(0)
#define F(m) do { printf("FAIL: %s\n", m); g_fail++; return; } while(0)
#define C(c) do { if (!(c)) { F(#c); return; } } while(0)

/* --- OBDD Tests --- */
static void t1_mgr(void) {
    T("obdd manager create/destroy");
    obdd_manager_t *m = obdd_mgr_create(8);
    C(m != NULL);
    C(obdd_get_node_count(m) >= 2);
    obdd_mgr_destroy(m);
    P();
}
static void t2_var(void) {
    T("obdd variable literals");
    obdd_manager_t *m = obdd_mgr_create(4);
    C(m != NULL);
    obdd_ref_t x0 = obdd_var(m, 0);
    C(obdd_is_satisfiable(m, x0));
    C(!obdd_is_tautology(m, x0));
    obdd_ref_t nx0 = obdd_not(m, x0);
    C(!obdd_is_satisfiable(m, obdd_and(m, x0, nx0)));
    C(obdd_is_tautology(m, obdd_or(m, x0, nx0)));
    obdd_mgr_destroy(m);
    P();
}
static void t3_bool(void) {
    T("obdd boolean operations");
    obdd_manager_t *m = obdd_mgr_create(4);
    C(m != NULL);
    obdd_ref_t x0 = obdd_var(m, 0), x1 = obdd_var(m, 1);
    C(obdd_sat_count(m, obdd_and(m, x0, x1), 4) == 4);
    C(obdd_sat_count(m, obdd_or(m, x0, x1), 4) == 12);
    C(obdd_sat_count(m, obdd_xor(m, x0, x1), 4) == 8);
    C(obdd_sat_count(m, obdd_imp(m, x0, x1), 4) == 12);
    obdd_mgr_destroy(m);
    P();
}
static void t4_restr(void) {
    T("obdd restrict and compose");
    obdd_manager_t *m = obdd_mgr_create(4);
    C(m != NULL);
    obdd_ref_t x0 = obdd_var(m, 0), x1 = obdd_var(m, 1);
    obdd_ref_t f = obdd_and(m, x0, x1);
    C(obdd_equals(m, obdd_restrict(m, f, 0, true), x1));
    C(!obdd_is_satisfiable(m, obdd_restrict(m, f, 0, false)));
    C(obdd_equals(m, obdd_compose(m, f, 0, x1), x1));
    obdd_mgr_destroy(m);
    P();
}
static void t5_quant(void) {
    T("obdd quantification");
    obdd_manager_t *m = obdd_mgr_create(4);
    C(m != NULL);
    obdd_ref_t x0 = obdd_var(m, 0), x1 = obdd_var(m, 1);
    C(obdd_equals(m, obdd_exists(m, obdd_and(m, x0, x1), 0), x1));
    C(obdd_equals(m, obdd_forall(m, obdd_or(m, x0, x1), 0), x1));
    obdd_mgr_destroy(m);
    P();
}
static void t6_sat(void) {
    T("obdd sat count and any sat");
    obdd_manager_t *m = obdd_mgr_create(3);
    C(m != NULL);
    C(obdd_sat_count(m, OBDD_TRUE, 3) == 8);
    C(obdd_sat_count(m, OBDD_FALSE, 3) == 0);
    obdd_ref_t x0 = obdd_var(m, 0), x1 = obdd_var(m, 1), x2 = obdd_var(m, 2);
    obdd_ref_t all3 = obdd_and(m, obdd_and(m, x0, x1), x2);
    C(obdd_sat_count(m, all3, 3) == 1);
    uint64_t asgn = 0;
    C(obdd_any_sat(m, all3, 3, &asgn));
    C(asgn == 7);
    obdd_mgr_destroy(m);
    P();
}
static void t7_eval(void) {
    T("obdd evaluate");
    obdd_manager_t *m = obdd_mgr_create(3);
    C(m != NULL);
    obdd_ref_t f = obdd_and(m, obdd_var(m, 0), obdd_var(m, 1));
    C(obdd_evaluate(m, f, 3, 0x7));
    C(obdd_evaluate(m, f, 3, 0x6));
    C(!obdd_evaluate(m, f, 3, 0x4));
    obdd_mgr_destroy(m);
    P();
}
static void t8_kripke(void) {
    T("kripke structure");
    kripke_t *K = kripke_create(3, 2);
    C(K != NULL);
    kripke_add_initial_state(K, 0);
    kripke_add_transition(K, 0, 1);
    kripke_add_transition(K, 1, 2);
    kripke_set_label(K, 0, "p", kripke_encode_state(K, 0));
    C(K->num_state_bits == 3);
    kripke_destroy(K);
    P();
}
static void t9_ctl(void) {
    T("ctl formula construction");
    /* Create separate atoms for each formula to avoid shared-child double-free */
    ctl_formula_t *ex = ctl_ex(ctl_atom(0));
    ctl_formula_t *eg = ctl_eg(ctl_atom(0));
    ctl_formula_t *eu = ctl_eu(ctl_atom(0), ctl_atom(1));
    ctl_formula_t *ag = ctl_ag(ctl_atom(0));
    C(ex != NULL); C(eg != NULL); C(eu != NULL); C(ag != NULL);
    ctl_free(ex); ctl_free(eg); ctl_free(eu); ctl_free(ag);
    P();
}
static void t10_ctlmc(void) {
    T("ctl model checking AG");
    kripke_t *K = kripke_create(1, 2);
    kripke_add_initial_state(K, 0);
    kripke_add_transition(K, 0, 1);
    kripke_add_transition(K, 1, 0);
    kripke_set_label_mask(K, 0, "p", 0x1, 2);
    kripke_set_label_mask(K, 1, "q", 0x2, 2);
    ctl_formula_t *p = ctl_atom(0), *q = ctl_atom(1);
    ctl_formula_t *ag_porq = ctl_ag(ctl_or(p, q));
    C(ctl_check_initial(K, ag_porq));
    ctl_free(ag_porq);
    kripke_destroy(K);
    P();
}
static obdd_ref_t tau_s(const kripke_t *K0, obdd_ref_t Z, void *ctx) {
    obdd_ref_t *init = (obdd_ref_t *)ctx;
    return obdd_or(K0->mgr, Z, *init);
}
static void t11_fix(void) {
    T("fixpoint computation");
    kripke_t *K = kripke_create(2, 1);
    obdd_ref_t init_set = OBDD_FALSE;
    int32_t iters = 0;
    obdd_ref_t lfp = fixpoint_least(K, tau_s, &init_set, 10, &iters);
    C(iters <= 3);
    C(lfp == OBDD_FALSE);
    kripke_destroy(K);
    P();
}
static void t12_cfg(void) {
    T("configuration");
    smc_config_t cfg = smc_config_default();
    C(cfg.max_vars == 64);
    C(cfg.max_nodes == 65536);
    C(smc_config_validate(&cfg) == 1);
    P();
}
static void t13_order(void) {
    T("variable ordering");
    obdd_manager_t *m = obdd_mgr_create(6);
    int32_t order[] = {5,4,3,2,1,0};
    obdd_set_var_order(m, order);
    C(obdd_is_satisfiable(m, obdd_var(m, 5)));
    obdd_sift(m, 0);
    obdd_reorder(m, 2);
    obdd_mgr_destroy(m);
    P();
}
int main(void) {
    printf("=== OBDD Symbolic Model Checking Tests ===\n\n");
    t1_mgr(); t2_var(); t3_bool(); t4_restr(); t5_quant();
    t6_sat(); t7_eval(); t8_kripke(); t9_ctl(); t10_ctlmc();
    t11_fix(); t12_cfg(); t13_order();
    printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
