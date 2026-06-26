#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "obdd.h"
#include "model.h"
#include "fixpoint.h"

static obdd_ref_t tau_simple(const kripke_t *K, obdd_ref_t Z, void *ctx) {
    /* tau(Z) = Z OR init. This should converge in 2 iterations. */
    obdd_ref_t *init = (obdd_ref_t *)ctx;
    obdd_ref_t r = obdd_or(K->mgr, Z, *init);
    printf("  tau: Z=0x%llx init=0x%llx -> 0x%llx equals=%d\n",
           (unsigned long long)Z, (unsigned long long)*init,
           (unsigned long long)r, (int)obdd_equals(K->mgr, r, Z));
    fflush(stdout);
    return r;
}

int main(void) {
    setbuf(stdout, NULL);
    kripke_t *K = kripke_create(2, 1);
    kripke_add_initial_state(K, 0);
    obdd_ref_t init_set = kripke_encode_state(K, 0);
    printf("init_set = 0x%llx\n", (unsigned long long)init_set);
    int32_t iters = 0;
    obdd_ref_t lfp = fixpoint_least(K, tau_simple, &init_set, 10, &iters);
    printf("lfp = 0x%llx iters=%d\n", (unsigned long long)lfp, iters);
    kripke_destroy(K);
    return 0;
}
