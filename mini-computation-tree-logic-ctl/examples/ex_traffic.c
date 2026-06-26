/**
 * ex_traffic.c — Traffic Light Controller Verification with CTL
 *
 * Verifies a simple traffic light controller with CTL model checking.
 * Demonstrates safety and liveness properties in reactive systems.
 *
 * States: Red, RedYellow, Green, Yellow (for one direction)
 * The cross direction is implicitly opposite.
 *
 * Properties:
 *   1. AG (Green -> AX Yellow) — Green always followed by Yellow
 *   2. AG (Yellow -> AF Red)  — Yellow inevitably leads to Red
 *   3. AG EF Green — It is always possible to eventually see Green
 *
 * Knowledge: L6 (Canonical Problems), L7 (Applications — traffic control)
 * Reference: Alur, Dill "Theory of Timed Automata" (1994)
 */

#include "ctl_ast.h"
#include "ctl_kripke.h"
#include "ctl_modelcheck.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  CTL Model Checking — Traffic Light Controller   ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    /* Atomic propositions: red, yellow, green */
    const char *ap_names[] = {"red", "yellow", "green"};
    int nap = 3;

    /* 4 states: Red(0), RedYellow(1), Green(2), Yellow(3) */
    ctl_kripke *k = ctl_kripke_create(4, nap, ap_names);

    const char *state_names[] = {"Red", "Red+Yellow", "Green", "Yellow"};

    /* Labels */
    ctl_kripke_set_label(k, 0, 0, 1); /* Red: red */
    ctl_kripke_set_label(k, 1, 0, 1); /* RedYellow: red */
    ctl_kripke_set_label(k, 1, 1, 1); /* RedYellow: yellow */
    ctl_kripke_set_label(k, 2, 2, 1); /* Green: green */
    ctl_kripke_set_label(k, 3, 1, 1); /* Yellow: yellow */

    /* Transitions: Red -> RedYellow -> Green -> Yellow -> Red ... */
    ctl_kripke_add_edge(k, 0, 1);
    ctl_kripke_add_edge(k, 1, 2);
    ctl_kripke_add_edge(k, 2, 3);
    ctl_kripke_add_edge(k, 3, 0);

    /* Initial state: Red */
    ctl_state_id init = 0;
    ctl_kripke_set_initial(k, &init, 1);

    printf("Traffic light cycle: Red -> Red+Yellow -> Green -> Yellow -> Red\n");
    printf("States:");
    for (int s = 0; s < 4; s++) printf(" s%u=%s", (unsigned)s, state_names[s]);
    printf("\n\n");

    /* Property 1: AG (Green -> AX Yellow) */
    printf("─── Property 1: Correct Sequencing ───\n");
    ctl_formula *green = ctl_mk_atom("green");
    ctl_formula *yellow_f = ctl_mk_atom("yellow");
    ctl_formula *ax_yellow = ctl_mk_ax(yellow_f);
    ctl_formula *implies_seq = ctl_mk_implies(ctl_clone_formula(green), ax_yellow);
    ctl_formula *prop1 = ctl_mk_ag(implies_seq);

    printf("Formula: AG (green -> AX yellow)\n");
    printf("Meaning: \"Whenever green, the next state must be yellow.\"\n");

    ctl_mc_result r1 = ctl_model_check(k, prop1, NULL);
    printf("Result: %s\n\n",
           r1 == CTL_MC_SATISFIED ? "SATISFIED ✓" : "VIOLATED ✗");

    ctl_free_formula(green); ctl_free_formula(prop1);

    /* Property 2: AG (Yellow -> AF Red) */
    printf("─── Property 2: Liveness ───\n");
    ctl_formula *yellow2 = ctl_mk_atom("yellow");
    ctl_formula *red_f = ctl_mk_atom("red");
    ctl_formula *af_red = ctl_mk_af(red_f);
    ctl_formula *implies_live = ctl_mk_implies(ctl_clone_formula(yellow2), af_red);
    ctl_formula *prop2 = ctl_mk_ag(implies_live);

    printf("Formula: AG (yellow -> AF red)\n");
    printf("Meaning: \"Whenever yellow, red inevitably follows.\"\n");

    ctl_mc_result r2 = ctl_model_check(k, prop2, NULL);
    printf("Result: %s\n\n",
           r2 == CTL_MC_SATISFIED ? "SATISFIED ✓" : "VIOLATED ✗");

    ctl_free_formula(yellow2); ctl_free_formula(prop2);

    /* Property 3: AG EF Green — always eventually possible */
    printf("─── Property 3: Possibility ───\n");
    ctl_formula *green_ef = ctl_mk_atom("green");
    ctl_formula *ef_green = ctl_mk_ef(green_ef);
    ctl_formula *prop3 = ctl_mk_ag(ef_green);

    printf("Formula: AG EF green\n");
    printf("Meaning: \"From any state, it is always possible "
           "to eventually see green.\"\n");

    ctl_mc_result r3 = ctl_model_check(k, prop3, NULL);
    printf("Result: %s\n\n",
           r3 == CTL_MC_SATISFIED ? "SATISFIED ✓" : "VIOLATED ✗");

    ctl_free_formula(prop3);

    /* Print Kripke structure in DOT format for visualization */
    printf("─── DOT (Graphviz) Output ───\n");
    ctl_kripke_print_dot(k, NULL);

    printf("\n─── Summary ───\n");
    printf("Correct sequencing:  %s\n", r1 == CTL_MC_SATISFIED ? "PASS" : "FAIL");
    printf("Liveness:            %s\n", r2 == CTL_MC_SATISFIED ? "PASS" : "FAIL");
    printf("Possibility:         %s\n", r3 == CTL_MC_SATISFIED ? "PASS" : "FAIL");

    ctl_kripke_destroy(k);
    return 0;
}
