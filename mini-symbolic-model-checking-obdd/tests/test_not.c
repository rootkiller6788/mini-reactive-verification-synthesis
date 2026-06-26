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
    printf("NOT check...\n");
    ctl_formula_t *p = ctl_atom(0);
    ctl_formula_t *np = ctl_not(p);
    obdd_ref_t res = ctl_model_check(K, np);
    printf("sat=%d\n", obdd_is_satisfiable(K->mgr, res));
    ctl_free(np); ctl_free(p);
    kripke_destroy(K);
    printf("Done.\n");
    return 0;
}
