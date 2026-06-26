#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "obdd.h"
#include "model.h"
int main(void) {
    setbuf(stdout, NULL);
    printf("t1: create kripke\n");
    kripke_t *K = kripke_create(2, 2);
    if (!K) { printf("FAIL\n"); return 1; }
    printf("t2: add transitions\n");
    kripke_add_transition(K, 0, 1);
    kripke_add_transition(K, 1, 0);
    kripke_add_initial_state(K, 0);
    printf("t3: preimage of TRUE\n");
    obdd_ref_t pre = kripke_preimage(K, OBDD_TRUE);
    printf("t4: result satisfiable=%d\n", obdd_is_satisfiable(K->mgr, pre));
    printf("t5: encode state 0\n");
    obdd_ref_t s0 = kripke_encode_state(K, 0);
    printf("t6: image of s0\n");
    obdd_ref_t img = kripke_image(K, s0);
    printf("t7: result satisfiable=%d\n", obdd_is_satisfiable(K->mgr, img));
    kripke_destroy(K);
    printf("Done.\n");
    return 0;
}
