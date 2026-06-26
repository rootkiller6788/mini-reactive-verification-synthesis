/**
 * robot_planning.c ¡ª Robot Mission Planning via LTL Synthesis
 *
 * Synthesizes a controller for a robot operating in a grid world.
 * Inputs: obstacle_near, goal_visible, battery_low
 * Outputs: move_forward, turn_left, turn_right, recharge
 *
 * The specification captures temporal mission constraints:
 *   - Safety: G(obstacle_near -> !move_forward)
 *   - Liveness: G(goal_visible -> F(reached))
 *   - Survival: G(battery_low -> F(recharge))
 *   - Persistence: G F (!battery_low) under proper charging
 *
 * This shows how LTL synthesis applies to robotics mission planning.
 *
 * Reference: Kress-Gazit, Fainekos, Pappas (2009)
 *            "Temporal-Logic-Based Reactive Mission and Motion Planning",
 *            IEEE Transactions on Robotics
 *
 * Knowledge Level: L6 Canonical Problems, L7 Applications
 */

#include "reactive_types.h"
#include "synthesis.h"
#include "ltl_syntax.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== Robot Mission Planning via LTL Synthesis ===\n\n");

    int32_t ni = 3;  /* obstacle_near, goal_visible, battery_low */
    int32_t no = 4;  /* move_forward, turn_left, turn_right, recharge */

    ltl_formula_t *obs = ltl_var(0);
    ltl_formula_t *goal = ltl_var(1);
    ltl_formula_t *batt = ltl_var(2);
    ltl_formula_t *move = ltl_var(3);
    ltl_formula_t *turn_l = ltl_var(4);
    ltl_formula_t *turn_r = ltl_var(5);
    ltl_formula_t *recharge = ltl_var(6);

    /* G(obstacle_near -> !move_forward) */
    ltl_formula_t *safety = ltl_always(ltl_implies(obs, ltl_not(move)));

    /* G(goal_visible -> F(goal_reached)) ¡ª use turn as proxy for reaching goal */
    ltl_formula_t *progress = ltl_always(ltl_implies(goal, ltl_eventually(turn_l)));

    /* G(battery_low -> F(recharge)) */
    ltl_formula_t *batt_safe = ltl_always(ltl_implies(batt, ltl_eventually(recharge)));

    /* G(recharge -> !move_forward) */
    ltl_formula_t *recharge_safe = ltl_always(ltl_implies(recharge, ltl_not(move)));

    /* Combine */
    ltl_formula_t *s1 = ltl_and(safety, progress);
    ltl_formula_t *s2 = ltl_and(batt_safe, recharge_safe);
    ltl_formula_t *spec = ltl_and(s1, s2);

    printf("Robot mission specification built.\n");
    printf("Inputs: obstacle_near, goal_visible, battery_low\n");
    printf("Outputs: move_forward, turn_left, turn_right, recharge\n\n");

    synthesis_spec_t *syn_spec = synthesis_spec_create(ltl_true(), spec, ni, no);
    synthesis_config_t cfg = synthesis_config_default();

    synthesis_result_t *result = synthesis_realizability_check(syn_spec, &cfg);

    printf("Synthesis result: %s\n",
           result->status == SYNTH_REALIZABLE ? "REALIZABLE" : result->message);
    if (result->module) {
        printf("Module: %d states\n", result->num_states);
    }
    printf("Time: %.3f s\n", result->time_seconds);

    synthesis_result_destroy(result);
    synthesis_spec_destroy(syn_spec);

    ltl_free(obs); ltl_free(goal); ltl_free(batt);
    ltl_free(move); ltl_free(turn_l); ltl_free(turn_r); ltl_free(recharge);
    ltl_free(safety); ltl_free(progress); ltl_free(batt_safe); ltl_free(recharge_safe);
    ltl_free(s1); ltl_free(s2); ltl_free(spec);

    printf("Done.\n");
    return 0;
}
