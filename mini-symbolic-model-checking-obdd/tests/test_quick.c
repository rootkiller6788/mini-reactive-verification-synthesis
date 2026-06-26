#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "obdd.h"
#include "model.h"
#include "ctlsmc.h"
#include "fixpoint.h"
#include "params.h"

int main(void) {
    printf("=== Quick tests ===\n");
    fflush(stdout);

    /* Test 1: OBDD manager */
    printf("  t1..."); fflush(stdout);
    obdd_manager_t *m = obdd_mgr_create(4);
    if (!m) { printf("FAIL\n"); return 1; }
    printf("PASS\n"); fflush(stdout);

    /* Test 2: variable */
    printf("  t2..."); fflush(stdout);
    obdd_ref_t x0 = obdd_var(m, 0);
    if (!obdd_is_satisfiable(m, x0)) { printf("FAIL\n"); return 1; }
    printf("PASS\n"); fflush(stdout);

    /* Test 3: boolean ops */
    printf("  t3..."); fflush(stdout);
    obdd_ref_t x1 = obdd_var(m, 1);
    obdd_ref_t a = obdd_and(m, x0, x1);
    if (obdd_sat_count(m, a, 4) != 4) { printf("FAIL\n"); return 1; }
    printf("PASS\n"); fflush(stdout);

    /* Test 4: Kripke structure */
    printf("  t4..."); fflush(stdout);
    kripke_t *K = kripke_create(2, 2);
    if (!K) { printf("FAIL\n"); return 1; }
    kripke_add_initial_state(K, 0);
    kripke_add_transition(K, 0, 1);
    kripke_add_transition(K, 1, 0);
    printf("PASS\n"); fflush(stdout);

    /* Test 5: CTL */
    printf("  t5..."); fflush(stdout);
    kripke_set_label_mask(K, 0, "p", 0x1, 4);
    kripke_set_label_mask(K, 1, "q", 0x2, 4);
    ctl_formula_t *p = ctl_atom(0);
    ctl_formula_t *ex_p = ctl_ex(p);
    obdd_ref_t res = ctl_model_check(K, ex_p);
    if (!obdd_is_satisfiable(K->mgr, res)) { printf("FAIL\n"); return 1; }
    printf("PASS\n"); fflush(stdout);

    /* Test 6: Fixpoint */
    printf("  t6..."); fflush(stdout);
    ctl_formula_t *q = ctl_atom(1);
    ctl_formula_t *ag_porq = ctl_ag(ctl_or(p, q));
    if (!ctl_check_initial(K, ag_porq)) { printf("FAIL\n"); return 1; }
    printf("PASS\n"); fflush(stdout);

    /* Test 7: Config */
    printf("  t7..."); fflush(stdout);
    smc_config_t cfg = smc_config_default();
    if (cfg.max_vars != 64) { printf("FAIL\n"); return 1; }
    printf("PASS\n"); fflush(stdout);

    ctl_free(ag_porq); ctl_free(ex_p); ctl_free(p); ctl_free(q);
    kripke_destroy(K);
    obdd_mgr_destroy(m);

    printf("All passed.\n");
    return 0;
}
