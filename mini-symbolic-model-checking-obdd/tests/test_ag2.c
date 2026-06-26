#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "obdd.h"
#include "model.h"
#include "ctlsmc.h"
int main(void) {
    setbuf(stdout, NULL);
    kripke_t *K = kripke_create(1, 2);
    kripke_add_initial_state(K, 0);
    kripke_add_transition(K, 0, 1);
    kripke_add_transition(K, 1, 0);
    kripke_set_label_mask(K, 0, "p", 0x1, 2);
    kripke_set_label_mask(K, 1, "q", 0x2, 2);
    /* Build formula tree */
    ctl_formula_t *p = ctl_atom(0);
    ctl_formula_t *q = ctl_atom(1);
    ctl_formula_t *porq = ctl_or(p, q);
    ctl_formula_t *ag = ctl_ag(porq);
    /* Model check */
    obdd_ref_t result = ctl_model_check(K, ag);
    printf("sat=%d\n", obdd_is_satisfiable(K->mgr, result));
    /* Free ONLY the root; children are freed recursively */
    ctl_free(ag);
    /* p, q, porq are already freed via ag's recursive free */
    kripke_destroy(K);
    printf("Done.\n");
    return 0;
}
