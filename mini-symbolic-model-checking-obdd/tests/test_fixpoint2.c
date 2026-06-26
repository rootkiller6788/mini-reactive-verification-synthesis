#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "obdd.h"
#include "model.h"
#include "fixpoint.h"
static obdd_ref_t tau_test(const kripke_t *K, obdd_ref_t Z, void *ctx) {
    printf("  tau called\n"); fflush(stdout);
    obdd_ref_t pre = kripke_preimage(K, Z);
    obdd_ref_t *phi = (obdd_ref_t *)ctx;
    return obdd_and(K->mgr, *phi, pre);
}
int main(void) {
    setbuf(stdout, NULL);
    printf("t1: create\n");
    kripke_t *K = kripke_create(2, 2);
    kripke_add_transition(K, 0, 1);
    kripke_add_transition(K, 1, 0);
    kripke_set_label_mask(K, 0, "p", 0x1, 4);
    printf("t2: get label\n");
    obdd_ref_t phi_set = K->labels[0];
    printf("t3: fixpoint_greatest\n");
    int32_t iters = 0;
    obdd_ref_t gfp = fixpoint_greatest(K, tau_test, &phi_set, 10, &iters);
    printf("t4: result iters=%d sat=%d\n", iters, obdd_is_satisfiable(K->mgr, gfp));
    kripke_destroy(K);
    return 0;
}
