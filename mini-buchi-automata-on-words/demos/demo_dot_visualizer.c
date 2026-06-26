/*
 * demo_dot_visualizer.c -- Graphviz DOT Visualization Generator
 *
 * L3 Math Structures + L7 Application:
 *   Generates DOT files for all acceptance condition types,
 *   enabling visual inspection of automaton structure.
 *
 * Produces visual representations useful for teaching and debugging.
 */
#include "buchi.h"
#include "buchi_emptiness.h"
#include "omega_operations.h"
#include "omega_automata.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Build a rich demo automaton with interesting structure */
static BuchiAutomaton* build_demo_automaton(void) {
    BuchiAutomaton* ba = buchi_create(5, 0, 2);
    BuchiSymbol alpha[] = {0, 1};
    buchi_set_alphabet(ba, alpha, 2);
    buchi_set_name(ba, "demo-NBA");
    buchi_add_accepting(ba, 2);
    buchi_add_accepting(ba, 4);

    /* Interesting topology: 0->1, 0->2, 1->3, 2->3, 3->4, 4->2, 3->1 */
    buchi_add_transition(ba, 0, 0, 1);
    buchi_add_transition(ba, 0, 1, 2);
    buchi_add_transition(ba, 1, 0, 3);
    buchi_add_transition(ba, 2, 0, 3);
    buchi_add_transition(ba, 3, 1, 4);
    buchi_add_transition(ba, 4, 1, 2);
    buchi_add_transition(ba, 3, 0, 1);
    return ba;
}

int main(void) {
    printf("=== DOT Visualizer Demo ===\n\n");

    /* 1. Basic automaton DOT */
    printf("Generating DOT files...\n");
    BuchiAutomaton* ba = build_demo_automaton();
    buchi_print_dot(ba, "build/demo_automaton.dot");
    printf("  demo_automaton.dot\n");

    /* 2. Trimmed version */
    BuchiAutomaton* trimmed = buchi_trim(ba);
    buchi_print_dot(trimmed, "build/demo_trimmed.dot");
    printf("  demo_trimmed.dot\n");

    /* 3. Union with self */
    BuchiAutomaton* self_union = buchi_union(ba, ba);
    buchi_print_dot(self_union, "build/demo_self_union.dot");
    printf("  demo_self_union.dot\n");

    /* 4. Print automaton structure */
    printf("\n-- Automaton Structure --\n");
    buchi_print(ba);

    /* 5. Acceptance condition display */
    printf("\n-- Acceptance Conditions --\n");
    printf("  Type: %s\n", acc_condition_name(ACC_BUCHI));
    printf("  Dual of Buchi: %s\n", acc_condition_name(ACC_COBUCHI));

    /* 6. SCC analysis */
    printf("\n-- SCC Analysis --\n");
    BuchiSCCDecomp* scc = buchi_scc_decompose(ba);
    if (scc) {
        printf("  Total SCCs: %d\n", scc->n_sccs);
        for (int i = 0; i < scc->n_sccs; i++) {
            printf("  SCC %d: %s %s\n", i,
                   scc->scc_nontrivial[i] ? "non-trivial" : "trivial",
                   scc->scc_accepting[i] ? "(accepting)" : "");
        }
        buchi_scc_free(scc);
    }

    /* 7. Emptiness check */
    printf("\n-- Emptiness --\n");
    EmptinessReport r = buchi_emptiness_detailed(ba);
    buchi_emptiness_report_print(&r);

    /* 8. Find accepting word */
    if (!r.found_empty) {
        OmegaWord* w = buchi_find_accepting_lasso(ba);
        if (w) {
            printf("\n-- Accepting Word --\n  ");
            omega_print(w, 30);
            omega_free(w);
        }
    }

    /* Note about Graphviz rendering */
    printf("\n-- Usage --\n");
    printf("  Render DOT files with: dot -Tpng demo_automaton.dot -o demo.png\n");
    printf("  Or use: fdp -Tsvg demo_automaton.dot -o demo.svg\n\n");

    /* Cleanup */
    buchi_free(ba);
    buchi_free(trimmed);
    buchi_free(self_union);

    printf("Demo complete.\n");
    return 0;
}
