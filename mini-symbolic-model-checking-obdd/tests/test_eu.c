#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "obdd.h"
#include "model.h"
#include "ctlsmc.h"
int main(void) {
    setbuf(stdout, NULL);
    printf("Step1: kripke\n");
    kripke_t *K = kripke_create(1, 2);
    kripke_add_initial_state(K, 0);
    kripke_add_transition(K, 0, 1);
    kripke_add_transition(K, 1, 0);
    kripke_set_label_mask(K, 0, "p", 0x1, 2);
    kripke_set_label_mask(K, 1, "q", 0x2, 2);
    printf("Step2: EF\n");
    ctl_formula_t *p = ctl_atom(0);
    ctl_formula_t *ef_p = ctl_ef(p);
    printf("Step3: model check EF p\n");
    obdd_ref_t res = ctl_model_check(K, ef_p);
    printf("Step4: sat=%d\n", obdd_is_satisfiable(K->mgr, res));
    ctl_free(ef_p); ctl_free(p);
    kripke_destroy(K);
    printf("Done\n");
    return 0;
}
