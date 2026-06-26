#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "obdd.h"
#include "model.h"
#include "ctlsmc.h"
#include "fixpoint.h"
int main(void) {
    setbuf(stdout, NULL);
    printf("t1: create Kripke\n");
    kripke_t *K = kripke_create(1, 2);
    kripke_add_initial_state(K, 0);
    kripke_add_transition(K, 0, 1);
    kripke_add_transition(K, 1, 0);
    kripke_set_label_mask(K, 0, "p", 0x1, 2);
    kripke_set_label_mask(K, 1, "q", 0x2, 2);

    /* Test AG(p OR q) directly */
    printf("t2: build formula\n");
    ctl_formula_t *p = ctl_atom(0);
    ctl_formula_t *q = ctl_atom(1);
    ctl_formula_t *porq = ctl_or(p, q);
    ctl_formula_t *ag = ctl_ag(porq);

    printf("t3: model check\n");
    obdd_ref_t result = ctl_model_check(K, ag);
    printf("t4: result sat=%d\n", obdd_is_satisfiable(K->mgr, result));

    printf("t5: check initial\n");
    int holds = ctl_check_initial(K, ag);
    printf("t6: holds=%d\n", holds);

    ctl_free(ag); ctl_free(porq); ctl_free(p); ctl_free(q);
    kripke_destroy(K);
    printf("Done.\n");
    return 0;
}
