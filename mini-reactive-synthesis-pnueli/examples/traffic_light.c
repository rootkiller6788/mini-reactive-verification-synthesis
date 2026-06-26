/**
 * traffic_light.c ¡ª Traffic Light Controller Synthesis
 *
 * Synthesizes a traffic light controller for a 4-way intersection.
 * Inputs: pedestrian_button, emergency_vehicle
 * Outputs: ns_green, ns_yellow, ns_red, ew_green, ew_yellow, ew_red
 *
 * Specification:
 *   - Safety: G !(ns_green & ew_green)
 *   - Liveness: G(ns_green -> F(ns_red))
 *   - Emergency: G(emergency_vehicle -> F(ns_red & ew_red))
 *   - Response: G(pedestrian_button -> F(ns_red & ew_red))
 *
 * Knowledge Level: L6 Canonical Problems, L7 Applications
 */

#include "reactive_types.h"
#include "synthesis.h"
#include "ltl_syntax.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== Traffic Light Controller Synthesis ===\n\n");

    int32_t num_inputs = 2;
    int32_t num_outputs = 6;

    ltl_formula_t *ped = ltl_var(0);
    ltl_formula_t *emv = ltl_var(1);
    ltl_formula_t *ns_g = ltl_var(2);
    ltl_formula_t *ns_y = ltl_var(3);
    ltl_formula_t *ns_r = ltl_var(4);
    ltl_formula_t *ew_g = ltl_var(5);
    ltl_formula_t *ew_y = ltl_var(6);
    ltl_formula_t *ew_r = ltl_var(7);

    /* Safety: G !(ns_g & ew_g) */
    ltl_formula_t *no_conflict = ltl_always(ltl_not(ltl_and(ns_g, ew_g)));

    /* Liveness: G(ns_g -> F(ns_r)) */
    ltl_formula_t *ns_progress = ltl_always(ltl_implies(ns_g, ltl_eventually(ns_r)));

    /* Emergency response: G(emv -> F(ns_r & ew_r)) */
    ltl_formula_t *both_red = ltl_and(ns_r, ew_r);
    ltl_formula_t *emergency_resp = ltl_always(ltl_implies(emv, ltl_eventually(both_red)));
    ltl_free(both_red);

    /* Pedestrian response: G(ped -> F(ns_r & ew_r)) */
    ltl_formula_t *both_red2 = ltl_and(ns_r, ew_r);
    ltl_formula_t *ped_resp = ltl_always(ltl_implies(ped, ltl_eventually(both_red2)));
    ltl_free(both_red2);

    /* Combine guarantees */
    ltl_formula_t *spec1 = ltl_and(no_conflict, ns_progress);
    ltl_formula_t *spec2 = ltl_and(emergency_resp, ped_resp);
    ltl_formula_t *spec = ltl_and(spec1, spec2);

    synthesis_spec_t *syn_spec = synthesis_spec_create(ltl_true(), spec, num_inputs, num_outputs);

    printf("Traffic light specification built.\n");
    printf("Inputs: ped, emv  Outputs: ns_g, ns_y, ns_r, ew_g, ew_y, ew_r\n\n");

    synthesis_config_t cfg = synthesis_config_default();

    synthesis_result_t *result = synthesis_realizability_check(syn_spec, &cfg);

    printf("Result: ");
    if (result->status == SYNTH_REALIZABLE) {
        printf("REALIZABLE (%d states)\n", result->num_states);
    } else {
        printf("%s\n", result->message);
    }
    printf("Time: %.3f s\n", result->time_seconds);

    synthesis_result_destroy(result);
    synthesis_spec_destroy(syn_spec);
    ltl_free(ped); ltl_free(emv);
    ltl_free(ns_g); ltl_free(ns_y); ltl_free(ns_r);
    ltl_free(ew_g); ltl_free(ew_y); ltl_free(ew_r);
    ltl_free(no_conflict); ltl_free(ns_progress);
    ltl_free(emergency_resp); ltl_free(ped_resp);
    ltl_free(spec1); ltl_free(spec2); ltl_free(spec);

    printf("Done.\n");
    return 0;
}
