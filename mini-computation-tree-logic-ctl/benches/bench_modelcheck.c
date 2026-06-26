/**
 * bench_modelcheck.c — CTL Model Checking Performance Benchmarks
 *
 * Benchmarks the CTL model checking algorithm on increasingly
 * large Kripke structures to measure scalability.
 */

#include "ctl_ast.h"
#include "ctl_kripke.h"
#include "ctl_modelcheck.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double time_diff_ms(clock_t start, clock_t end) {
    return 1000.0 * (double)(end - start) / (double)CLOCKS_PER_SEC;
}

static void bench_chain(int nstates) {
    /* Create a linear chain and check EF p */
    const char *aps[] = {"p"};
    ctl_kripke *k = ctl_kripke_create((ctl_state_id)nstates, 1, aps);
    for (int i = 0; i < nstates - 1; i++)
        ctl_kripke_add_edge(k, (ctl_state_id)i, (ctl_state_id)(i + 1));
    ctl_kripke_add_edge(k, (ctl_state_id)(nstates - 1), (ctl_state_id)(nstates - 1));
    ctl_kripke_set_label(k, (ctl_state_id)(nstates - 1), 0, 1);
    ctl_state_id init = 0;
    ctl_kripke_set_initial(k, &init, 1);

    ctl_formula *p = ctl_mk_atom("p");
    ctl_formula *ef_p = ctl_mk_ef(p);

    clock_t start = clock();
    ctl_mc_result res = ctl_model_check(k, ef_p, NULL);
    clock_t end = clock();

    printf("  Chain(%d): %s in %.2f ms\n", nstates,
           res == CTL_MC_SATISFIED ? "SAT" : "FAIL", time_diff_ms(start, end));

    ctl_free_formula(ef_p);
    ctl_kripke_destroy(k);
}

static void bench_clique(int nstates) {
    /* Create a complete graph */
    const char *aps[] = {"p"};
    ctl_kripke *k = ctl_kripke_create((ctl_state_id)nstates, 1, aps);
    for (int i = 0; i < nstates; i++)
        for (int j = 0; j < nstates; j++)
            ctl_kripke_add_edge(k, (ctl_state_id)i, (ctl_state_id)j);
    ctl_state_id init = 0;
    ctl_kripke_set_initial(k, &init, 1);

    ctl_formula *p = ctl_mk_atom("p");
    ctl_formula *ag_p = ctl_mk_ag(p);

    clock_t start = clock();
    ctl_mc_result res = ctl_model_check(k, ag_p, NULL);
    clock_t end = clock();

    printf("  Clique(%d, %d edges): %s in %.2f ms\n",
           nstates, nstates * nstates,
           res == CTL_MC_SATISFIED ? "SAT" : (res == CTL_MC_VIOLATED ? "VIOL" : "ERR"),
           time_diff_ms(start, end));

    ctl_free_formula(ag_p);
    ctl_kripke_destroy(k);
}

int main(void) {
    printf("═══ CTL Model Checking Benchmarks ═══\n\n");

    printf("--- Chain Topology (linear reachability) ---\n");
    bench_chain(10);
    bench_chain(100);
    bench_chain(500);
    bench_chain(1000);

    printf("\n--- Clique Topology (complete graph) ---\n");
    bench_clique(10);
    bench_clique(50);
    bench_clique(100);

    printf("\n═══ Benchmarks complete ═══\n");
    return 0;
}
