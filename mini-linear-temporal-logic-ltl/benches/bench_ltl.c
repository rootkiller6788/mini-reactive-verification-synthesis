/*
 * bench_ltl.c — Performance Benchmarks for LTL operations
 *
 * Measures throughput of key LTL operations:
 *   - Formula construction and cloning
 *   - NNF conversion
 *   - Formula simplification
 *   - Trace evaluation
 *   - Pattern instantiation
 */
#include "ltl_ast.h"
#include "ltl_semantics.h"
#include "ltl_equiv.h"
#include "ltl_patterns.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now_ms(void) {
    return (double)clock() / CLOCKS_PER_SEC * 1000.0;
}

int main(void) {
    printf("=== LTL Benchmarks ===\n\n");

    double start, elapsed;
    int n_iter = 50000;

    /* Benchmark: Formula construction */
    start = now_ms();
    for (int i = 0; i < n_iter; i++) {
        LtlNode* f = ltl_mk_globally(
            ltl_mk_implies(ltl_mk_atom(0), ltl_mk_finally(ltl_mk_atom(1))));
        ltl_free(f);
    }
    elapsed = now_ms() - start;
    printf("Formula construction: %d iterations in %.2f ms (%.1f ops/s)\n",
           n_iter, elapsed, n_iter * 1000.0 / (elapsed + 0.001));

    /* Benchmark: NNF conversion */
    LtlNode* base = ltl_mk_globally(
        ltl_mk_implies(ltl_mk_atom(0),
            ltl_mk_until(ltl_mk_atom(1), ltl_mk_atom(2))));
    start = now_ms();
    for (int i = 0; i < n_iter / 10; i++) {
        LtlNode* nnf = ltl_to_nnf(base);
        ltl_free(nnf);
    }
    elapsed = now_ms() - start;
    printf("NNF conversion:       %d iterations in %.2f ms (%.1f ops/s)\n",
           n_iter/10, elapsed, (n_iter/10) * 1000.0 / (elapsed + 0.001));
    ltl_free(base);

    /* Benchmark: Trace evaluation */
    LtlPropSet cycle[1] = { LTL_PROP_ADD(LTL_PROP_EMPTY, 0) };
    LtlTrace* trace = ltl_trace_create_lasso(NULL, 0, cycle, 1);
    LtlNode* prop = ltl_mk_globally(ltl_mk_atom(0));
    start = now_ms();
    for (int i = 0; i < n_iter; i++) {
        ltl_satisfies(trace, prop);
    }
    elapsed = now_ms() - start;
    printf("Trace evaluation:     %d iterations in %.2f ms (%.1f ops/s)\n",
           n_iter, elapsed, n_iter * 1000.0 / (elapsed + 0.001));
    ltl_trace_free(trace);
    ltl_free(prop);

    /* Benchmark: Pattern instantiation */
    start = now_ms();
    for (int i = 0; i < n_iter; i++) {
        LtlNode* p = ltl_pattern_response_global(0, 1);
        ltl_free(p);
    }
    elapsed = now_ms() - start;
    printf("Pattern generation:   %d iterations in %.2f ms (%.1f ops/s)\n",
           n_iter, elapsed, n_iter * 1000.0 / (elapsed + 0.001));

    printf("\n=== Done ===\n");
    return 0;
}
