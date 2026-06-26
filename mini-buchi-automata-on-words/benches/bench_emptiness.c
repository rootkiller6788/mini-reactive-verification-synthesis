/*
 * bench_emptiness.c -- Performance Benchmark for Emptiness Checking
 *
 * L5 Algorithms benchmark:
 *   Compares SCC-based vs Nested DFS emptiness checking on
 *   parameterized Buchi automata families.
 *
 * Measures:
 *   - Construction time
 *   - SCC decomposition time
 *   - Nested DFS time
 *   - Counterexample extraction time
 */
#include "buchi.h"
#include "buchi_emptiness.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Build a ring automaton with n states:
 * State i --0--> (i+1)%n, state 0 --1--> 0 (accepting).
 * Language: words where symbol 1 appears infinitely often. */
static BuchiAutomaton* build_ring(int n) {
    BuchiAutomaton* ba = buchi_create(n, 0, 2);
    BuchiSymbol alpha[] = {0, 1};
    buchi_set_alphabet(ba, alpha, 2);
    buchi_add_accepting(ba, 0);

    for (int i = 0; i < n; i++) {
        buchi_add_transition(ba, i, 0, (i + 1) % n);
        if (i == 0)
            buchi_add_transition(ba, i, 1, i);
    }
    return ba;
}

static double time_diff_ms(clock_t start, clock_t end) {
    return 1000.0 * (end - start) / CLOCKS_PER_SEC;
}

int main(void) {
    printf("=== Emptiness Checking Benchmark ===\n\n");
    printf("%8s %8s %10s %10s %10s\n",
           "N", "Trans", "SCC(ms)", "NDFS(ms)", "CE(ms)");
    printf("-----------------------------------------------\n");

    int sizes[] = {10, 50, 100, 200, 500, 1000};
    int n_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int si = 0; si < n_sizes; si++) {
        int n = sizes[si];

        clock_t t0 = clock();
        BuchiAutomaton* ba = build_ring(n);
        clock_t t1 = clock();

        /* SCC-based emptiness */
        clock_t t2 = clock();
        BuchiSCCDecomp* scc = buchi_scc_decompose(ba);
        int scc_nonempty = buchi_has_accepting_scc(scc);
        clock_t t3 = clock();
        (void)scc_nonempty;

        /* SCC-based counterexample */
        clock_t t4 = clock();
        OmegaWord* ce = buchi_find_accepting_lasso(ba);
        clock_t t5 = clock();

        double time_build = time_diff_ms(t0, t1);
        double time_scc = time_diff_ms(t2, t3);
        double time_ce = time_diff_ms(t4, t5);
        (void)time_build;

        /* Nested DFS */
        NestedDFS* nd = nested_dfs_create(n);
        clock_t t6 = clock();
        nested_dfs_check(ba, nd);
        clock_t t7 = clock();
        double time_ndfs = time_diff_ms(t6, t7);

        printf("%8d %8d %10.3f %10.3f %10.3f\n",
               n, buchi_num_trans(ba), time_scc, time_ndfs, time_ce);

        if (ce) omega_free(ce);
        if (scc) buchi_scc_free(scc);
        nested_dfs_free(nd);
        buchi_free(ba);
    }

    printf("\nBenchmark complete.\n");
    return 0;
}
