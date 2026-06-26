/**
 * mutex_synthesis.c ¡ª Mutual Exclusion Protocol Synthesis
 *
 * Synthesizes a mutual exclusion protocol for two processes.
 * Inputs: req0, req1 (process requests to enter critical section)
 * Outputs: cs0, cs1 (process is in critical section)
 *
 * Specification (Pnueli's classic example):
 *   - Mutual exclusion: G !(cs0 & cs1)
 *   - Liveness: G(req0 -> F(cs0)) & G(req1 -> F(cs1))
 *   - Grant only on request: G(cs0 -> req0) & G(cs1 -> req1)
 *
 * This example demonstrates synthesis of a coordination protocol
 * from temporal logic specification.
 *
 * Reference: Pnueli (1977) "The Temporal Logic of Programs"
 *            Manna & Pnueli (1992)
 *
 * Knowledge Level: L6 Canonical Problems
 */

#include "reactive_types.h"
#include "synthesis.h"
#include "ltl_syntax.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== Mutual Exclusion Protocol Synthesis ===\n\n");

    int32_t ni = 2, no = 2;  /* req0,req1 ; cs0,cs1 */

    ltl_formula_t *req0 = ltl_var(0), *req1 = ltl_var(1);
    ltl_formula_t *cs0 = ltl_var(2), *cs1 = ltl_var(3);

    /* G !(cs0 & cs1) */
    ltl_formula_t *mutex = ltl_always(ltl_not(ltl_and(cs0, cs1)));

    /* G(req0 -> F(cs0)) */
    ltl_formula_t *live0 = ltl_always(ltl_implies(req0, ltl_eventually(cs0)));

    /* G(req1 -> F(cs1)) */
    ltl_formula_t *live1 = ltl_always(ltl_implies(req1, ltl_eventually(cs1)));

    /* G(cs0 -> req0) */
    ltl_formula_t *grant0 = ltl_always(ltl_implies(cs0, req0));

    /* G(cs1 -> req1) */
    ltl_formula_t *grant1 = ltl_always(ltl_implies(cs1, req1));

    ltl_formula_t *s1 = ltl_and(mutex, live0);
    ltl_formula_t *s2 = ltl_and(live1, grant0);
    ltl_formula_t *s3 = ltl_and(s1, s2);
    ltl_formula_t *spec = ltl_and(s3, grant1);

    printf("Mutex specification: ");
    ltl_print(spec);
    printf("\n\n");

    synthesis_spec_t *syn_spec = synthesis_spec_create(ltl_true(), spec, ni, no);
    synthesis_config_t cfg = synthesis_config_default();

    synthesis_result_t *result = synthesis_realizability_check(syn_spec, &cfg);

    printf("Synthesis: %s\n",
           result->status == SYNTH_REALIZABLE ? "REALIZABLE" : "UNREALIZABLE");
    if (result->module) {
        printf("States: %d\n", result->num_states);
        reactive_module_print(result->module);
    }
    printf("Time: %.3f s\n", result->time_seconds);

    synthesis_result_destroy(result);
    synthesis_spec_destroy(syn_spec);

    ltl_free(req0); ltl_free(req1); ltl_free(cs0); ltl_free(cs1);
    ltl_free(mutex); ltl_free(live0); ltl_free(live1);
    ltl_free(grant0); ltl_free(grant1);
    ltl_free(s1); ltl_free(s2); ltl_free(s3); ltl_free(spec);

    printf("Done.\n");
    return 0;
}
