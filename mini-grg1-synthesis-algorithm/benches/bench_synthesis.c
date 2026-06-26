/**
 * bench_synthesis.c — Performance benchmark for GR(1) synthesis
 *
 * Measures synthesis time and memory for increasing state space sizes.
 * Used to characterize the O(|S|² × k) complexity of the algorithm.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "grg1_types.h"
#include "grg1_spec.h"
#include "grg1_game.h"
#include "grg1_fixpoint.h"
#include "grg1_synthesis.h"

static double get_time_sec(void) {
    return (double)clock() / CLOCKS_PER_SEC;
}

static bool bench_pred_state(const grg1_valuation_t* v,
                              const grg1_variable_t* vars, int nv) {
    (void)v; (void)vars; (void)nv;
    return true;
}

static bool bench_pred_trans(const grg1_valuation_t* pre,
                              const grg1_valuation_t* post,
                              const grg1_variable_t* vars, int nv) {
    (void)pre; (void)post; (void)vars; (void)nv;
    return true;
}

int main(void) {
    printf("=== GR(1) Synthesis Benchmarks ===\n\n");
    printf("%8s %8s %8s %10s %10s\n",
           "Vars", "States", "Trans", "Time(s)", "Status");

    /* Benchmark with increasing variable counts */
    for (int nv = 1; nv <= 5; nv++) {
        grg1_variable_t vars[8];
        for (int i = 0; i < nv && i < 8; i++) {
            vars[i].var_id = i;
            snprintf(vars[i].name, sizeof(vars[i].name), "v%d", i);
            vars[i].domain_size = 2;
            vars[i].is_system_controlled = (i % 2 == 1);
            vars[i].initial_value = 0;
        }

        grg1_spec_t* spec = grg1_spec_create("bench", nv, vars);
        if (!spec) { printf("  (spec creation failed)\n"); continue; }

        grg1_spec_set_env_init(spec, "ei", bench_pred_state);
        grg1_spec_set_sys_init(spec, "si", bench_pred_state);
        grg1_spec_set_env_safety(spec, "es", bench_pred_trans);
        grg1_spec_set_sys_safety(spec, "ss", bench_pred_trans);
        grg1_spec_add_sys_justice(spec, "j1", bench_pred_state);

        grg1_synthesis_config_t config = GRG1_SYNTHESIS_CONFIG_DEFAULT;
        config.compute_strategy = false;
        config.verbose = false;
        config.max_iterations = 5000;
        config.max_states = 1000;

        double t0 = get_time_sec();
        grg1_synthesis_result_t* result = grg1_synthesize(spec, &config);
        double t1 = get_time_sec();

        if (result) {
            const char* status;
            switch (result->status) {
                case GRG1_REALIZABLE: status = "REAL"; break;
                case GRG1_UNREALIZABLE: status = "UNREAL"; break;
                case GRG1_UNKNOWN: status = "UNK"; break;
                default: status = "OTHER"; break;
            }
            printf("%8d %8d %8d %10.3f %10s\n",
                   nv,
                   result->stats.num_states,
                   result->stats.num_transitions,
                   t1 - t0,
                   status);
            grg1_synthesis_result_free(result);
        }

        grg1_spec_free(spec);
    }

    printf("\nDone.\n");
    return 0;
}
