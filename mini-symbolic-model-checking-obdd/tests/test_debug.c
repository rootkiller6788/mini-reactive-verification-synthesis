#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "obdd.h"
#include "model.h"
#include "ctlsmc.h"
int main(void) {
    printf("Step 1: kripke_create\n"); fflush(stdout);
    kripke_t *K = kripke_create(2, 2);
    if (!K) { printf("FAIL at kripke_create\n"); return 1; }

    printf("Step 2: add init+trans\n"); fflush(stdout);
    kripke_add_initial_state(K, 0);
    kripke_add_transition(K, 0, 1);
    kripke_add_transition(K, 1, 0);

    printf("Step 3: set labels\n"); fflush(stdout);
    kripke_set_label_mask(K, 0, "p", 0x1, 4);
    kripke_set_label_mask(K, 1, "q", 0x2, 4);

    printf("Step 4: ctl_atom p\n"); fflush(stdout);
    ctl_formula_t *p = ctl_atom(0);

    printf("Step 5: ctl_atom q\n"); fflush(stdout);
    ctl_formula_t *q = ctl_atom(1);

    printf("Step 6: ctl_or(p,q)\n"); fflush(stdout);
    ctl_formula_t *porq = ctl_or(p, q);

    printf("Step 7: ctl_ag\n"); fflush(stdout);
    ctl_formula_t *ag = ctl_ag(porq);

    printf("Step 8: model check\n"); fflush(stdout);
    obdd_ref_t res = ctl_model_check(K, ag);
    printf("  result satisfiable=%d\n", obdd_is_satisfiable(K->mgr, res));
    fflush(stdout);

    printf("Step 9: check initial\n"); fflush(stdout);
    int hold = ctl_check_initial(K, ag);
    printf("  holds=%d\n", hold);
    fflush(stdout);

    printf("Step 10: cleanup\n"); fflush(stdout);
    ctl_free(ag); ctl_free(porq); ctl_free(p); ctl_free(q);
    kripke_destroy(K);
    printf("Done.\n");
    return 0;
}
