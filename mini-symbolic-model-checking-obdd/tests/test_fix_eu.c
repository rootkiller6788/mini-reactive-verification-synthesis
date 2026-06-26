#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "obdd.h"
#include "model.h"
#include "fixpoint.h"

typedef struct {
    const kripke_t *K;
    obdd_ref_t phi_set;
    obdd_ref_t psi_set;
} eu_ctx_t;

static obdd_ref_t tau(const kripke_t *K0, obdd_ref_t Z, void *ctx) {
    eu_ctx_t *c = (eu_ctx_t *)ctx;
    (void)K0;
    printf("  tau: preimage...\n"); fflush(stdout);
    obdd_ref_t exz = kripke_preimage(c->K, Z);
    printf("  tau: AND...\n"); fflush(stdout);
    obdd_ref_t t1 = obdd_and(c->K->mgr, c->phi_set, exz);
    printf("  tau: OR...\n"); fflush(stdout);
    obdd_ref_t t2 = obdd_or(c->K->mgr, c->psi_set, t1);
    printf("  tau: done\n"); fflush(stdout);
    return t2;
}

int main(void) {
    setbuf(stdout, NULL);
    printf("t1: kripke\n");
    kripke_t *K = kripke_create(1, 2);
    kripke_add_initial_state(K, 0);
    kripke_add_transition(K, 0, 1);
    kripke_add_transition(K, 1, 0);
    kripke_set_label_mask(K, 0, "p", 0x1, 2);
    kripke_set_label_mask(K, 1, "q", 0x2, 2);

    printf("t2: setup context\n");
    eu_ctx_t ctx;
    ctx.K = K;
    ctx.phi_set = OBDD_TRUE;   /* TRUE for EF */
    ctx.psi_set = K->labels[0]; /* label for p */

    printf("t3: fixpoint_least\n");
    int32_t iters = 0;
    obdd_ref_t result = fixpoint_least(K, tau, &ctx, 100, &iters);
    printf("t4: result iters=%d sat=%d\n", iters,
           obdd_is_satisfiable(K->mgr, result));

    kripke_destroy(K);
    printf("Done\n");
    return 0;
}
