/**
 * ex_mutex.c — Mutual Exclusion Verification with CTL (L6 Canonical Problem)
 *
 * Verifies a 2-process mutual exclusion protocol using CTL model checking.
 * This is the classic example from Clarke, Emerson, Sistla (1986).
 *
 * States encode: (p1_location, p2_location)
 *   Locations: N (non-critical), T (trying), C (critical)
 *
 * Properties checked:
 *   1. AG !(c1 AND c2) — Mutual exclusion (safety)
 *   2. AG (t1 -> AF c1) — No starvation for process 1 (liveness)
 *   3. AG (t1 -> EF c1) — Possibility of entering (weaker liveness)
 *
 * Knowledge: L6 (Canonical Problems), L7 (Applications)
 */

#include "ctl_ast.h"
#include "ctl_kripke.h"
#include "ctl_modelcheck.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  CTL Model Checking — Mutual Exclusion           ║\n");
    printf("║  Reference: Clarke, Emerson, Sistla (1986)       ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    /* Atomic propositions:
     *   n1, n2 — process i in non-critical section
     *   t1, t2 — process i trying to enter
     *   c1, c2 — process i in critical section
     */
    const char *ap_names[] = {"n1", "t1", "c1", "n2", "t2", "c2"};
    int nap = 6;

    /* Build Kripke structure for simple mutex protocol:
     * State encoding: (pc1, pc2) with pc in {N, T, C}
     *
     * 9 states: all combinations of (N,T,C) x (N,T,C)
     * Only legal transitions enforced by protocol.
     */
    ctl_kripke *k = ctl_kripke_create(9, nap, ap_names);

    /* State map: 0=NN, 1=NT, 2=NC, 3=TN, 4=TT, 5=TC, 6=CN, 7=CT, 8=CC
     * Index: pc1 * 3 + pc2, where N=0, T=1, C=2
     */
    const char *state_names[] = {
        "NN", "NT", "NC", "TN", "TT", "TC", "CN", "CT", "CC"
    };

    /* Label each state */
    int loc1[9] = {0,0,0, 1,1,1, 2,2,2}; /* pc1: N=0,T=1,C=2 */
    int loc2[9] = {0,1,2, 0,1,2, 0,1,2}; /* pc2 */

    for (int s = 0; s < 9; s++) {
        ctl_kripke_set_label(k, (ctl_state_id)s, 0, loc1[s] == 0);
        ctl_kripke_set_label(k, (ctl_state_id)s, 1, loc1[s] == 1);
        ctl_kripke_set_label(k, (ctl_state_id)s, 2, loc1[s] == 2);
        ctl_kripke_set_label(k, (ctl_state_id)s, 3, loc2[s] == 0);
        ctl_kripke_set_label(k, (ctl_state_id)s, 4, loc2[s] == 1);
        ctl_kripke_set_label(k, (ctl_state_id)s, 5, loc2[s] == 2);
    }

    /* Transitions: simplified mutex protocol
     * Process 1 moves: N<->T, T->C->N
     * Process 2 moves: N<->T, T->C->N
     * Both can't be in C simultaneously (no transitions into CC)
     */
    int edges[9][3] = {
        {1, 3, -1},    /* NN -> NT, TN */
        {0, 2, 4},     /* NT -> NN, NC, TT */
        {1, -1, -1},   /* NC -> NT */
        {0, 4, 6},     /* TN -> NN, TT, CN */
        {1, 3, -1},    /* TT -> NT, TN */
        {4, -1, -1},   /* TC (should be avoided in correct protocol) */
        {3, 7, -1},    /* CN -> TN, CT */
        {6, 8, -1},    /* CT -> CN, CC (bug: should prevent CC) */
        {7, -1, -1}    /* CC -> CT (not legal but add transition for totality) */
    };

    for (int s = 0; s < 9; s++) {
        for (int e = 0; e < 3 && edges[s][e] >= 0; e++) {
            ctl_kripke_add_edge(k, (ctl_state_id)s, (ctl_state_id)edges[s][e]);
        }
    }
    ctl_kripke_make_total(k);

    /* Initial state: both in non-critical (NN = state 0) */
    ctl_state_id init = 0;
    ctl_kripke_set_initial(k, &init, 1);

    /* Print structure */
    printf("Kripke structure: %u states, %d atomic propositions\n",
           k->nstates, k->nap);
    printf("States: ");
    for (int s = 0; s < 9; s++) printf("%s ", state_names[s]);
    printf("\n\n");

    /* ─── Property 1: Mutual Exclusion ─── */
    printf("─── Property 1: Mutual Exclusion ───\n");
    ctl_formula *c1 = ctl_mk_atom("c1");
    ctl_formula *c2 = ctl_mk_atom("c2");
    ctl_formula *both_critical = ctl_mk_and(ctl_clone_formula(c1),
                                             ctl_clone_formula(c2));
    ctl_formula *not_both = ctl_mk_not(both_critical);
    ctl_formula *mutex = ctl_mk_ag(not_both);

    printf("Formula: AG !(c1 AND c2)\n");
    printf("Meaning: \"In all reachable states, both processes "
           "are never simultaneously in the critical section.\"\n");

    ctl_mc_result r1 = ctl_model_check(k, mutex, NULL);
    printf("Result: %s\n\n",
           r1 == CTL_MC_SATISFIED ? "SATISFIED ✓" :
           r1 == CTL_MC_VIOLATED ? "VIOLATED ✗" : "ERROR");

    ctl_free_formula(c1); ctl_free_formula(c2); ctl_free_formula(mutex);

    /* ─── Property 2: No Starvation (Liveness) ─── */
    printf("─── Property 2: No Starvation for Process 1 ───\n");
    ctl_formula *t1 = ctl_mk_atom("t1");
    ctl_formula *c1_af = ctl_mk_atom("c1");
    ctl_formula *af_c1 = ctl_mk_af(c1_af);
    ctl_formula *no_starvation = ctl_mk_ag(
        ctl_mk_implies(ctl_clone_formula(t1), af_c1));

    printf("Formula: AG (t1 -> AF c1)\n");
    printf("Meaning: \"Whenever process 1 is trying, it will "
           "inevitably enter the critical section.\"\n");

    ctl_mc_result r2 = ctl_model_check(k, no_starvation, NULL);
    printf("Result: %s\n\n",
           r2 == CTL_MC_SATISFIED ? "SATISFIED ✓" :
           r2 == CTL_MC_VIOLATED ? "VIOLATED ✗" : "ERROR");

    ctl_free_formula(t1); ctl_free_formula(no_starvation);

    /* ─── Property 3: Reachability of Critical Section ─── */
    printf("─── Property 3: Reachability of Critical Section ───\n");
    ctl_formula *t2 = ctl_mk_atom("t2");
    ctl_formula *c2_ef = ctl_mk_atom("c2");
    ctl_formula *ef_c2 = ctl_mk_ef(c2_ef);
    ctl_formula *can_enter = ctl_mk_ag(
        ctl_mk_implies(ctl_clone_formula(t2), ef_c2));

    printf("Formula: AG (t2 -> EF c2)\n");
    printf("Meaning: \"Whenever process 2 is trying, it is possible "
           "to eventually enter the critical section.\"\n");

    ctl_mc_result r3 = ctl_model_check(k, can_enter, NULL);
    printf("Result: %s\n\n",
           r3 == CTL_MC_SATISFIED ? "SATISFIED ✓" :
           r3 == CTL_MC_VIOLATED ? "VIOLATED ✗" : "ERROR");

    ctl_free_formula(t2); ctl_free_formula(can_enter);

    /* ─── Summary ─── */
    printf("─── Verification Summary ───\n");
    printf("Mutual exclusion:     %s\n",
           r1 == CTL_MC_SATISFIED ? "PASS" : "FAIL");
    printf("No starvation (P1):   %s\n",
           r2 == CTL_MC_SATISFIED ? "PASS" : "FAIL");
    printf("Reachability (P2):    %s\n",
           r3 == CTL_MC_SATISFIED ? "PASS" : "FAIL");

    ctl_kripke_destroy(k);
    return 0;
}
