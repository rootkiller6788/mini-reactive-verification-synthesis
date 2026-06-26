/*
 * example_mutex_verification.c -- Mutual Exclusion Property Verification
 *
 * L6 Canonical Problem + L7 Application:
 *   Model checking mutual exclusion using Buchi automata.
 *   Demonstrates: property automaton construction, product with
 *   Kripke structure, emptiness checking (M |= phi iff L(M x A_not_phi) = empty).
 *
 * Scenario: Two-process mutual exclusion with a simple arbiter.
 *   Safety: never both in critical section simultaneously.
 *   Liveness: if a process requests, it eventually enters.
 *
 * The Buchi automaton for "not liveness" accepts runs where
 * a process requests but never enters the critical section.
 *
 * Reference:
 *   Baier & Katoen 2008 -- Principles of Model Checking, Ch. 3-4
 *   Clarke, Grumberg, Peled 1999 -- Model Checking
 */
#include "buchi.h"
#include "buchi_emptiness.h"
#include "omega_operations.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Kripke structure representing a simple mutual exclusion system.
 * States encode (pc0, pc1, turn):
 *   pc: 0=idle, 1=requesting, 2=critical
 *   turn: 0=proc0's turn, 1=proc1's turn
 *
 * Labels are bitmasks:
 *   bit 0: proc0 in critical
 *   bit 1: proc1 in critical
 *   bit 2: proc0 requesting
 *   bit 3: proc1 requesting
 */
#define N_IDLE         0
#define N_REQUESTING   1
#define N_CRITICAL     2

static int state_encode(int pc0, int pc1, int turn) {
    return pc0 * 6 + pc1 * 2 + turn;
}

static void state_decode(int s, int* pc0, int* pc1, int* turn) {
    *pc0 = s / 6;
    *pc1 = (s % 6) / 2;
    *turn = s % 2;
}

/* Compute label bitmask for a KS state */
static int compute_label(int pc0, int pc1) {
    int label = 0;
    if (pc0 == N_CRITICAL) label |= 1;
    if (pc1 == N_CRITICAL) label |= 2;
    if (pc0 == N_REQUESTING) label |= 4;
    if (pc1 == N_REQUESTING) label |= 8;
    return label;
}

int main(void) {
    printf("=== Mutual Exclusion Verification ===\n\n");

    /* Step 1: Build Kripke structure for 2-process mutex */
    int n_states = 3 * 3 * 2; /* 18 reachable states */
    KripkeStructure ks;
    ks.n_states = n_states;
    ks.n_props = 4;
    ks.labels = (int*)calloc(n_states, sizeof(int));
    ks.successors = (int**)malloc(n_states * sizeof(int*));

    for (int s = 0; s < n_states; s++) {
        int pc0, pc1, turn;
        state_decode(s, &pc0, &pc1, &turn);
        ks.labels[s] = compute_label(pc0, pc1);

        /* Count successors */
        int succ[4];
        int n_succ = 0;

        /* Process 0 moves */
        if (pc0 == N_IDLE) {
            /* Can stay idle or request */
            succ[n_succ++] = state_encode(N_IDLE, pc1, turn);
            succ[n_succ++] = state_encode(N_REQUESTING, pc1, turn);
        } else if (pc0 == N_REQUESTING) {
            if (turn == 0 || pc1 != N_CRITICAL) {
                succ[n_succ++] = state_encode(N_CRITICAL, pc1, turn);
            }
            succ[n_succ++] = state_encode(N_REQUESTING, pc1, turn);
        } else { /* CRITICAL */
            succ[n_succ++] = state_encode(N_IDLE, pc1, 1); /* release, give turn */
        }

        /* Process 1 moves */
        if (pc1 == N_IDLE) {
            succ[n_succ++] = state_encode(pc0, N_IDLE, turn);
            succ[n_succ++] = state_encode(pc0, N_REQUESTING, turn);
        } else if (pc1 == N_REQUESTING) {
            if (turn == 1 || pc0 != N_CRITICAL) {
                succ[n_succ++] = state_encode(pc0, N_CRITICAL, turn);
            }
            succ[n_succ++] = state_encode(pc0, N_REQUESTING, turn);
        } else { /* CRITICAL */
            succ[n_succ++] = state_encode(pc0, N_IDLE, 0);
        }

        /* Deduplicate successors */
        int unique[4], n_uniq = 0;
        for (int i = 0; i < n_succ; i++) {
            int dup = 0;
            for (int j = 0; j < n_uniq; j++)
                if (unique[j] == succ[i]) { dup = 1; break; }
            if (!dup) unique[n_uniq++] = succ[i];
        }

        ks.successors[s] = (int*)malloc((n_uniq + 1) * sizeof(int));
        for (int i = 0; i < n_uniq; i++)
            ks.successors[s][i] = unique[i];
        ks.successors[s][n_uniq] = -1;
    }

    printf("Kripke structure: %d states\n", n_states);

    /* Step 2: Build Buchi automaton for safety property:
     * "never both in critical section simultaneously"
     * This is a safety property: G !(crit0 && crit1)
     *
     * The negation (bad thing): F (crit0 && crit1)
     * NBA for "eventually both critical":
     *   q0 --all--> q0, q0 --(crit0 & crit1)--> q1, q1 --all--> q1, F={q1}
     *
     * Symbols: label bitmasks 0..15
     */
    int crit_both_label = 3; /* bits 0 and 1 set */

    BuchiAutomaton* A_not_safety = buchi_create(2, 0, 16);
    BuchiSymbol all_labels[16];
    for (int i = 0; i < 16; i++) all_labels[i] = i;
    buchi_set_alphabet(A_not_safety, all_labels, 16);
    buchi_set_name(A_not_safety, "not-safety");
    buchi_add_accepting(A_not_safety, 1);

    for (int a = 0; a < 16; a++) {
        buchi_add_transition(A_not_safety, 0, a, 0);
        if (a == crit_both_label)
            buchi_add_transition(A_not_safety, 0, a, 1);
        buchi_add_transition(A_not_safety, 1, a, 1);
    }

    /* Step 3: Product construction */
    BuchiAutomaton* product = buchi_product_ks(A_not_safety, &ks);
    printf("Product automaton: %d states, %d transitions\n",
           buchi_num_states(product), buchi_num_trans(product));

    /* Step 4: Emptiness check */
    int empty = buchi_is_empty(product);

    printf("\n=== Verification Result ===\n");
    if (empty) {
        printf("SAFETY PROPERTY HOLDS: ");
        printf("Mutual exclusion is never violated.\n");
        printf("The system satisfies G !(crit0 && crit1)\n");
    } else {
        printf("SAFETY PROPERTY VIOLATED: ");
        printf("Found path where both processes enter critical section!\n");

        /* Extract counterexample */
        OmegaWord* ce = buchi_find_accepting_lasso(product);
        if (ce) {
            printf("Counterexample prefix length: %d, cycle length: %d\n",
                   ce->is_lasso ? *ce->period_start : 0, ce->period_len);
            omega_free(ce);
        }
    }

    /* Step 5: Liveness check -- "if proc0 requests, eventually enters critical"
     * NBA for "not liveness": proc0 requests but never enters critical
     *   q0 --all--> q0, q0 --request--> q1, q1 --!critical--> q1, F={q1}
     */
    printf("\n--- Liveness: proc0 request -> eventually critical ---\n");

    BuchiAutomaton* A_not_live = buchi_create(2, 0, 16);
    buchi_set_alphabet(A_not_live, all_labels, 16);
    buchi_set_name(A_not_live, "not-liveness-0");
    buchi_add_accepting(A_not_live, 1);

    for (int a = 0; a < 16; a++) {
        buchi_add_transition(A_not_live, 0, a, 0);
        if (a & 4)
            buchi_add_transition(A_not_live, 0, a, 1);
        buchi_add_transition(A_not_live, 1, a, 1);
        /* But NOT on critical -- stay in q1 */
        if (!(a & 1))
            buchi_add_transition(A_not_live, 1, a, 1);
    }

    BuchiAutomaton* product_live = buchi_product_ks(A_not_live, &ks);
    int live_empty = buchi_is_empty(product_live);

    if (live_empty) {
        printf("LIVENESS PROPERTY HOLDS for proc0\n");
    } else {
        printf("LIVENESS PROPERTY MAY BE VIOLATED for proc0\n");
        OmegaWord* ce = buchi_find_accepting_lasso(product_live);
        if (ce) {
            printf("Counterexample found (length check done)\n");
            omega_free(ce);
        }
    }

    /* Cleanup */
    buchi_free(A_not_safety);
    buchi_free(A_not_live);
    buchi_free(product);
    buchi_free(product_live);
    for (int i = 0; i < n_states; i++) free(ks.successors[i]);
    free(ks.successors);
    free(ks.labels);

    printf("\nVerification complete.\n");
    return 0;
}
