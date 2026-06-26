/**
 * demo_ctl.c — Interactive CTL Model Checking Demo
 *
 * Demonstrates the complete CTL workflow:
 *   1. Define a Kripke structure
 *   2. Formulate CTL properties
 *   3. Run model checking
 *   4. Report results with counterexamples
 *
 * This demo creates a simple 3-state system modeling a resource
 * manager with states: idle(0), requested(1), granted(2).
 */

#include "ctl_ast.h"
#include "ctl_kripke.h"
#include "ctl_modelcheck.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  CTL Model Checking — Interactive Demo           ║\n");
    printf("║  Resource Manager Verification                   ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    /* ─── Build Kripke Structure ─── */
    printf("Step 1: Building Kripke structure...\n");
    const char *ap_names[] = {"idle", "requested", "granted"};
    ctl_kripke *k = ctl_kripke_create(3, 3, ap_names);

    /* States: s0=idle, s1=requested, s2=granted */
    ctl_kripke_set_label(k, 0, 0, 1); /* idle */
    ctl_kripke_set_label(k, 1, 1, 1); /* requested */
    ctl_kripke_set_label(k, 2, 2, 1); /* granted */

    /* Transitions */
    ctl_kripke_add_edge(k, 0, 1); /* idle -> requested */
    ctl_kripke_add_edge(k, 1, 0); /* requested -> idle (request denied) */
    ctl_kripke_add_edge(k, 1, 2); /* requested -> granted */
    ctl_kripke_add_edge(k, 2, 0); /* granted -> idle (release) */

    ctl_state_id init = 0;
    ctl_kripke_set_initial(k, &init, 1);
    printf("  States: 3 (idle, requested, granted)\n");
    printf("  Transitions: idle->requested->granted->idle\n\n");

    /* ─── Define CTL Properties ─── */
    printf("Step 2: Formulating CTL properties...\n\n");

    /* Property 1: Safety — "It is always the case that if requested,"
     *              "eventually granted" */
    ctl_formula *req = ctl_mk_atom("requested");
    ctl_formula *gnt = ctl_mk_atom("granted");
    ctl_formula *af_grant = ctl_mk_af(ctl_clone_formula(gnt));
    ctl_formula *prop1 = ctl_mk_ag(
        ctl_mk_implies(ctl_clone_formula(req), af_grant));

    printf("Property 1: AG (requested -> AF granted)\n");
    printf("  \"Whenever a request is made, eventually "
           "the grant will be given.\"\n");

    ctl_mc_context *ctx1 = NULL;
    ctl_mc_result r1 = ctl_model_check(k, prop1, &ctx1);
    printf("  Result: %s\n",
           r1 == CTL_MC_SATISFIED ? "PASS" : "FAIL");

    /* Show SAT sets */
    printf("  States satisfying 'requested': ");
    for (int s = 0; s < 3; s++)
        if (ctl_kripke_get_label(k, (ctl_state_id)s, 1))
            printf("s%d ", s);
    printf("\n  States satisfying 'granted': ");
    for (int s = 0; s < 3; s++)
        if (ctl_kripke_get_label(k, (ctl_state_id)s, 2))
            printf("s%d ", s);
    printf("\n\n");

    ctl_free_formula(prop1);
    if (ctx1) ctl_mc_context_destroy(ctx1);

    /* Property 2: Possibility — "It is always possible to return to idle" */
    ctl_formula *idle_f = ctl_mk_atom("idle");
    ctl_formula *ef_idle = ctl_mk_ef(ctl_clone_formula(idle_f));
    ctl_formula *prop2 = ctl_mk_ag(ef_idle);

    printf("Property 2: AG EF idle\n");
    printf("  \"From any state, it is always possible "
           "to reach the idle state.\"\n");

    ctl_mc_result r2 = ctl_model_check(k, prop2, NULL);
    printf("  Result: %s\n\n",
           r2 == CTL_MC_SATISFIED ? "PASS" : "FAIL");

    ctl_free_formula(prop2);
    ctl_free_formula(idle_f);

    /* Property 3: Mutual Exclusion — "Not both idle and granted" */
    ctl_formula *idle2 = ctl_mk_atom("idle");
    ctl_formula *grant2 = ctl_mk_atom("granted");
    ctl_formula *both = ctl_mk_and(ctl_clone_formula(idle2),
                                    ctl_clone_formula(grant2));
    ctl_formula *not_both = ctl_mk_not(both);
    ctl_formula *prop3 = ctl_mk_ag(not_both);

    printf("Property 3: AG !(idle AND granted)\n");
    printf("  \"The system is never simultaneously idle and granted.\"\n");

    ctl_mc_result r3 = ctl_model_check(k, prop3, NULL);
    printf("  Result: %s\n\n",
           r3 == CTL_MC_SATISFIED ? "PASS" : "FAIL");

    ctl_free_formula(idle2); ctl_free_formula(grant2);
    ctl_free_formula(prop3);

    /* ─── Print state space ─── */
    printf("Step 3: Kripke structure visualization\n\n");
    ctl_kripke_print(k);
    printf("\n");
    ctl_kripke_print_dot(k, NULL);

    /* ─── Summary ─── */
    printf("\n╔══════════════════════════════════════════════════╗\n");
    printf("║  Verification Summary                            ║\n");
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║  Safety (request => eventually grant): %-8s ║\n",
           r1 == CTL_MC_SATISFIED ? "PASS" : "FAIL");
    printf("║  Possibility (always can go idle):    %-8s ║\n",
           r2 == CTL_MC_SATISFIED ? "PASS" : "FAIL");
    printf("║  Invariant (!(idle && granted)):      %-8s ║\n",
           r3 == CTL_MC_SATISFIED ? "PASS" : "FAIL");
    printf("╚══════════════════════════════════════════════════╝\n");

    ctl_kripke_destroy(k);
    return 0;
}
