/**
 * arbiter_synthesis.c ¡ª Bus Arbiter Synthesis Example
 *
 * Synthesizes an arbiter for a shared bus with two request lines
 * (r0, r1) and two grant lines (g0, g1). The specification is:
 *   - Mutual exclusion: G(!(g0 & g1))
 *   - Starvation freedom: G(r0 -> F(g0)) & G(r1 -> F(g1))
 *   - No spurious grants: G(g0 -> r0) & G(g1 -> r1)
 *
 * This is a classic reactive synthesis benchmark.
 * Reference: Pnueli (1977), Bloem et al. (2007) CAV
 *
 * Knowledge Level: L6 Canonical Problems, L7 Applications
 */

#include "reactive_types.h"
#include "synthesis.h"
#include "ltl_syntax.h"
#include "game_structure.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== Bus Arbiter Synthesis ===\n\n");

    int32_t num_inputs = 2;   /* r0, r1 */
    int32_t num_outputs = 2;  /* g0, g1 */

    /* Build specification formula */
    ltl_formula_t *r0 = ltl_var(0);
    ltl_formula_t *r1 = ltl_var(1);
    ltl_formula_t *g0 = ltl_var(2);
    ltl_formula_t *g1 = ltl_var(3);

    /* Mutual exclusion: G !(g0 & g1) */
    ltl_formula_t *g0_and_g1 = ltl_and(g0, g1);
    ltl_formula_t *not_both = ltl_not(g0_and_g1);
    ltl_formula_t *mutex = ltl_always(not_both);
    ltl_free(g0_and_g1); ltl_free(not_both);

    /* Starvation freedom: G(r0 -> F(g0)) */
    ltl_formula_t *fg0 = ltl_eventually(g0);
    ltl_formula_t *r0_imp_fg0 = ltl_implies(r0, fg0);
    ltl_formula_t *starv0 = ltl_always(r0_imp_fg0);
    ltl_free(fg0); ltl_free(r0_imp_fg0);

    /* G(r1 -> F(g1)) */
    ltl_formula_t *fg1 = ltl_eventually(g1);
    ltl_formula_t *r1_imp_fg1 = ltl_implies(r1, fg1);
    ltl_formula_t *starv1 = ltl_always(r1_imp_fg1);
    ltl_free(fg1); ltl_free(r1_imp_fg1);

    /* No spurious grants: G(g0 -> r0) */
    ltl_formula_t *g0_imp_r0 = ltl_implies(g0, r0);
    ltl_formula_t *legit0 = ltl_always(g0_imp_r0);
    ltl_free(g0_imp_r0);

    /* G(g1 -> r1) */
    ltl_formula_t *g1_imp_r1 = ltl_implies(g1, r1);
    ltl_formula_t *legit1 = ltl_always(g1_imp_r1);
    ltl_free(g1_imp_r1);

    /* Combine all guarantees */
    ltl_formula_t *g12 = ltl_and(mutex, starv0);
    ltl_formula_t *g34 = ltl_and(starv1, legit0);
    ltl_formula_t *g1234 = ltl_and(g12, g34);
    ltl_formula_t *spec = ltl_and(g1234, legit1);
    ltl_free(g12); ltl_free(g34); ltl_free(g1234);

    /* Assumption: environment is free (TRUE) */
    ltl_formula_t *assumption = ltl_true();

    /* Build specification */
    synthesis_spec_t *syn_spec = synthesis_spec_create(assumption, spec, num_inputs, num_outputs);

    printf("Specification formula: ");
    ltl_print(spec);
    printf("\n\n");

    /* Attempt synthesis */
    synthesis_config_t cfg = synthesis_config_default();
    cfg.verbose = true;
    cfg.timeout_seconds = 30.0;

    synthesis_result_t *result = synthesis_realizability_check(syn_spec, &cfg);

    printf("Synthesis status: ");
    switch (result->status) {
    case SYNTH_REALIZABLE:
        printf("REALIZABLE\n");
        printf("Synthesized module: %d states\n", result->num_states);
        if (result->module) {
            printf("\nModule:\n");
            reactive_module_print(result->module);
        }
        break;
    case SYNTH_UNREALIZABLE:
        printf("UNREALIZABLE\n");
        break;
    case SYNTH_TIMEOUT:
        printf("TIMEOUT\n");
        break;
    default:
        printf("UNKNOWN/ERROR: %s\n", result->message);
        break;
    }
    printf("Time: %.3f seconds\n", result->time_seconds);

    synthesis_result_destroy(result);
    synthesis_spec_destroy(syn_spec);

    ltl_free(r0); ltl_free(r1); ltl_free(g0); ltl_free(g1);
    ltl_free(mutex); ltl_free(starv0); ltl_free(starv1);
    ltl_free(legit0); ltl_free(legit1); ltl_free(spec);

    printf("\nDone.\n");
    return 0;
}
