/*
 * example_emptiness_demo.c -- Emptiness Checking Comparison Demo
 *
 * L5 Algorithms + L6 Canonical Problems:
 *   Compares SCC-based vs Nested DFS emptiness checking on
 *   a family of Buchi automata.
 *
 * Demonstrates: SCC-based emptiness, nested DFS, counterexample
 * extraction, GBA emptiness, and degeneralization.
 */
#include "buchi.h"
#include "buchi_emptiness.h"
#include "omega_operations.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Build a ladder automaton with n rungs:
 * Each rung has states {off_i, on_i}. From off_i you can go to on_i
 * (accepting) or off_{i+1}. on_i loops back to itself (accepting cycle).
 * Language: eventually stays in some accepting state. */
static BuchiAutomaton* build_ladder(int n) {
    int total = n * 2;
    BuchiAutomaton* ba = buchi_create(total, 0, 2);
    BuchiSymbol alpha[] = {0, 1};
    buchi_set_alphabet(ba, alpha, 2);
    buchi_set_name(ba, "ladder");

    for (int i = 0; i < n; i++) {
        int off_i = i * 2;
        int on_i = i * 2 + 1;
        buchi_add_accepting(ba, on_i);

        /* off_i --0--> on_i (enter accepting) */
        buchi_add_transition(ba, off_i, 0, on_i);
        /* off_i --1--> off_{i+1} (continue, or loop if last) */
        int next_off = (i < n - 1) ? (i + 1) * 2 : off_i;
        buchi_add_transition(ba, off_i, 1, next_off);

        /* on_i --0/1--> on_i (stay in accepting loop) */
        buchi_add_transition(ba, on_i, 0, on_i);
        buchi_add_transition(ba, on_i, 1, on_i);
    }
    return ba;
}

int main(void) {
    printf("=== Emptiness Checking Demo ===\n\n");

    for (int n = 1; n <= 5; n++) {
        printf("-- Ladder automaton (n=%d) --\n", n);
        BuchiAutomaton* ba = build_ladder(n);

        /* SCC-based emptiness */
        EmptinessReport r = buchi_emptiness_detailed(ba);
        printf("  SCC method: %s (%d SCCs, %d accepting)\n",
               r.found_empty ? "EMPTY" : "NON-EMPTY",
               r.n_sccs, r.n_accepting_sccs);

        if (!r.found_empty) {
            /* SCC-based counterexample */
            OmegaWord* ce_scc = buchi_find_accepting_lasso(ba);
            if (ce_scc) {
                printf("  SCC counterexample: ");
                omega_print(ce_scc, 20);
                omega_free(ce_scc);
            }
        }

        /* Nested DFS emptiness */
        NestedDFS* nd = nested_dfs_create(buchi_num_states(ba));
        int ndfs_empty = nested_dfs_check(ba, nd);
        printf("  Nested DFS: %s\n", ndfs_empty ? "EMPTY" : "NON-EMPTY");

        if (!ndfs_empty) {
            OmegaWord* ce_ndfs = nested_dfs_extract_lasso(nd, ba);
            if (ce_ndfs) {
                printf("  Nested DFS counterexample: ");
                omega_print(ce_ndfs, 20);
                omega_free(ce_ndfs);
            }
        }

        nested_dfs_free(nd);
        buchi_free(ba);
        printf("\n");
    }

    /* GBA emptiness demo */
    printf("-- Generalized Buchi Emptiness --\n");

    /* Build automaton with 3 acceptance sets, all intersecting one SCC */
    BuchiAutomaton* ba = buchi_create(4, 0, 2);
    BuchiSymbol alpha[] = {0, 1};
    buchi_set_alphabet(ba, alpha, 2);
    /* SCC: 0->1->2->3->1 (cycle: 1,2,3) */
    buchi_add_transition(ba, 0, 0, 1);
    buchi_add_transition(ba, 1, 0, 2);
    buchi_add_transition(ba, 2, 0, 3);
    buchi_add_transition(ba, 3, 0, 1);
    /* Self-loops for stability */
    buchi_add_transition(ba, 1, 1, 1);
    buchi_add_transition(ba, 2, 1, 2);
    buchi_add_transition(ba, 3, 1, 3);

    BuchiState F0[] = {1};
    BuchiState F1[] = {2};
    BuchiState F2[] = {3};
    const BuchiState* F_sets[3] = {F0, F1, F2};
    int F_sizes[3] = {1, 1, 1};

    int gba_empty = gba_is_empty(ba, F_sets, F_sizes, 3);
    printf("  GBA with 3 acceptance sets: %s\n",
           gba_empty ? "EMPTY" : "NON-EMPTY");

    /* Degeneralize and verify */
    GBA* gba = gba_create(ba, 3);
    gba_add_accepting_set(gba, 0, F0, 1);
    gba_add_accepting_set(gba, 1, F1, 1);
    gba_add_accepting_set(gba, 2, F2, 1);
    BuchiAutomaton* nba = gba_degeneralize(gba);
    printf("  Degeneralized NBA: %d states\n", buchi_num_states(nba));
    int nba_empty = buchi_is_empty(nba);
    printf("  Degeneralized NBA emptiness: %s\n",
           nba_empty ? "EMPTY" : "NON-EMPTY");

    buchi_free(nba);
    gba_free(gba);
    buchi_free(ba);

    printf("\nDemo complete.\n");
    return 0;
}
