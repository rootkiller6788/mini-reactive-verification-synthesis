/*
 * example_nba_operations.c -- Buchi Automaton Operations Demonstration
 *
 * L6 Canonical Problems + L7 Application:
 *   Demonstrates core NBA operations: union, intersection, complement,
 *   emptiness checking, and counterexample extraction.
 *
 * Builds several small NBA, applies operations, and verifies results.
 */
#include "buchi.h"
#include "buchi_emptiness.h"
#include "omega_operations.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Build NBA that accepts words where 'a' appears infinitely often.
 * Alphabet: 0='a', 1='b' */
static BuchiAutomaton* build_inf_a(void) {
    BuchiAutomaton* ba = buchi_create(2, 0, 2);
    BuchiSymbol alpha[] = {0, 1};
    buchi_set_alphabet(ba, alpha, 2);
    buchi_set_name(ba, "inf-a");
    buchi_add_accepting(ba, 0);
    buchi_add_transition(ba, 0, 0, 0);
    buchi_add_transition(ba, 0, 1, 1);
    buchi_add_transition(ba, 1, 0, 0);
    buchi_add_transition(ba, 1, 1, 1);
    return ba;
}

/* Build NBA that accepts words where 'b' appears infinitely often */
static BuchiAutomaton* build_inf_b(void) {
    BuchiAutomaton* ba = buchi_create(2, 0, 2);
    BuchiSymbol alpha[] = {0, 1};
    buchi_set_alphabet(ba, alpha, 2);
    buchi_set_name(ba, "inf-b");
    buchi_add_accepting(ba, 1);
    buchi_add_transition(ba, 0, 0, 0);
    buchi_add_transition(ba, 0, 1, 1);
    buchi_add_transition(ba, 1, 0, 1);
    buchi_add_transition(ba, 1, 1, 1);
    return ba;
}

/* Build NBA that accepts words with exactly one 'a' then only 'b's.
 * This tests safety + liveness combination. */
static BuchiAutomaton* build_eventually_always_b(void) {
    BuchiAutomaton* ba = buchi_create(3, 0, 2);
    BuchiSymbol alpha[] = {0/*a*/, 1/*b*/};
    buchi_set_alphabet(ba, alpha, 2);
    buchi_set_name(ba, "FGb");
    buchi_add_accepting(ba, 2);
    buchi_add_transition(ba, 0, 0, 1);
    buchi_add_transition(ba, 0, 1, 2);
    buchi_add_transition(ba, 1, 1, 2);
    buchi_add_transition(ba, 2, 1, 2);
    return ba;
}

static void print_automaton_info(const BuchiAutomaton* ba) {
    printf("  %s: %d states, %d trans, %d accepting",
           ba->name ? ba->name : "(unnamed)",
           buchi_num_states(ba), buchi_num_trans(ba),
           ba->n_accepting);
    EmptinessReport r = buchi_emptiness_detailed(ba);
    printf(", %s", r.found_empty ? "EMPTY" : "NON-EMPTY");
    if (!r.found_empty)
        printf(", %d accepting SCCs", r.n_accepting_sccs);
    printf("\n");
}

int main(void) {
    printf("=== Buchi Automaton Operations ===\n\n");

    /* 1. Build basic automata */
    printf("-- Building basic automata --\n");
    BuchiAutomaton *A1 = build_inf_a();
    BuchiAutomaton *A2 = build_inf_b();
    BuchiAutomaton *A3 = build_eventually_always_b();

    print_automaton_info(A1);
    print_automaton_info(A2);
    print_automaton_info(A3);

    /* 2. Union */
    printf("\n-- Union: L(A1) U L(A2) --\n");
    BuchiAutomaton* U = buchi_union(A1, A2);
    print_automaton_info(U);

    /* Test words against union */
    BuchiSymbol ab[] = {0, 1};
    OmegaWord* w_ab = omega_make_periodic(ab, 2);
    printf("  (ab)^omega in L(union)? %s\n",
           buchi_accepts_lasso(U, w_ab) ? "YES" : "NO");
    omega_free(w_ab);

    /* 3. Intersection */
    printf("\n-- Intersection: L(A1) cap L(A2) --\n");
    BuchiAutomaton* I = buchi_intersect(A1, A2);
    print_automaton_info(I);

    OmegaWord* w_ab2 = omega_make_periodic(ab, 2);
    printf("  (ab)^omega in L(intersect)? %s\n",
           buchi_accepts_lasso(I, w_ab2) ? "YES" : "NO");

    BuchiSymbol a_only[] = {0};
    OmegaWord* w_a = omega_make_periodic(a_only, 1);
    printf("  a^omega in L(intersect)? %s\n",
           buchi_accepts_lasso(I, w_a) ? "YES" : "NO");
    omega_free(w_ab2); omega_free(w_a);

    /* 4. Trimming */
    printf("\n-- Trimming --\n");
    BuchiAutomaton* T = buchi_trim(U);
    print_automaton_info(T);

    /* 5. Emptiness + counterexample */
    printf("\n-- Emptiness + Counterexample --\n");
    OmegaWord* ce = buchi_find_accepting_lasso(A3);
    if (ce) {
        printf("  L(A3) non-empty, example word:\n  ");
        omega_print(ce, 20);
        omega_free(ce);
    }

    /* 6. Statistics */
    printf("\n-- Statistics --\n");
    BuchiStats bs = buchi_stats(A3);
    buchi_stats_print(&bs);

    /* 7. DOT output */
    printf("\n-- Generating DOT files --\n");
    buchi_print_dot(A1, "build/example_inf_a.dot");
    buchi_print_dot(A2, "build/example_inf_b.dot");
    buchi_print_dot(A3, "build/example_FGb.dot");
    printf("  Generated inf_a.dot, inf_b.dot, FGb.dot\n");

    /* Cleanup */
    buchi_free(A1); buchi_free(A2); buchi_free(A3);
    buchi_free(U); buchi_free(I); buchi_free(T);

    printf("\nDone.\n");
    return 0;
}
