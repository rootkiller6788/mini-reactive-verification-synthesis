/**
 * example_mutex_arbiter.c — Mutual Exclusion Arbiter using GR(1) Synthesis
 *
 * Demonstrates a complete GR(1) synthesis workflow for a simple
 * mutual exclusion arbiter with two clients.
 *
 * The specification:
 *   - 2 clients that can request a shared resource
 *   - System must grant the resource to at most one client at a time
 *   - System must eventually grant each requesting client
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "grg1_types.h"
#include "grg1_spec.h"
#include "grg1_game.h"
#include "grg1_synthesis.h"

/* =========================================================================
 * Domain-specific predicates for the mutex example
 * ========================================================================*/

static bool client1_requesting(const grg1_valuation_t* v,
                                const grg1_variable_t* vars, int nv) {
    (void)vars; (void)nv;
    return v->values[0] == 1; /* req1 = 1 (requesting) */
}

static bool client2_requesting(const grg1_valuation_t* v,
                                const grg1_variable_t* vars, int nv) {
    (void)vars; (void)nv;
    return v->values[1] == 1; /* req2 = 1 (requesting) */
}

static bool grant_is_idle(const grg1_valuation_t* v,
                           const grg1_variable_t* vars, int nv) {
    (void)vars; (void)nv;
    return v->values[2] == 0; /* grant = 0 (idle) */
}

static bool grant1_active(const grg1_valuation_t* v,
                           const grg1_variable_t* vars, int nv) {
    (void)vars; (void)nv;
    return v->values[2] == 1; /* grant = 1 (grant to client1) */
}

static bool grant2_active(const grg1_valuation_t* v,
                           const grg1_variable_t* vars, int nv) {
    (void)vars; (void)nv;
    return v->values[2] == 2; /* grant = 2 (grant to client2) */
}

static bool sys_init_pred(const grg1_valuation_t* v,
                           const grg1_variable_t* vars, int nv) {
    (void)vars; (void)nv;
    return v->values[2] == 0; /* grant starts idle */
}

static bool env_init_pred(const grg1_valuation_t* v,
                           const grg1_variable_t* vars, int nv) {
    (void)vars; (void)nv;
    return v->values[0] == 0 && v->values[1] == 0; /* both clients idle */
}

static bool sys_trans_safe(const grg1_valuation_t* pre,
                            const grg1_valuation_t* post,
                            const grg1_variable_t* vars, int nv) {
    (void)vars; (void)nv;
    /* System safety: grant values 1 and 2 are never both active */
    /* Since there's only one grant variable, this is automatically satisfied */
    /* Additional safety: grant only changes to valid values */
    int g_pre = pre->values[2];
    int g_post = post->values[2];
    /* Grant can go from 0→0, 0→1, 0→2, 1→1, 1→0, 2→2, 2→0 */
    /* Forbid going from 1→2 or 2→1 directly (must go through idle) */
    if (g_pre == 1 && g_post == 2) return false;
    if (g_pre == 2 && g_post == 1) return false;
    return true;
}

static bool env_trans_safe(const grg1_valuation_t* pre,
                            const grg1_valuation_t* post,
                            const grg1_variable_t* vars, int nv) {
    (void)pre; (void)vars; (void)nv;
    /* Environment safety: requests can toggle freely */
    int r1 = post->values[0];
    int r2 = post->values[1];
    /* Both requests active is allowed */
    if (r1 != 0 && r1 != 1) return false;
    if (r2 != 0 && r2 != 1) return false;
    return true;
}

int main(void) {
    printf("========================================\n");
    printf("GR(1) Synthesis: Mutual Exclusion Arbiter\n");
    printf("========================================\n\n");

    /* Step 1: Define variables */
    grg1_variable_t vars[3];

    vars[0].var_id = 0;
    strcpy(vars[0].name, "req1");
    vars[0].domain_size = 2; /* 0=idle, 1=requesting */
    vars[0].is_system_controlled = false; /* environment */
    vars[0].initial_value = 0;

    vars[1].var_id = 1;
    strcpy(vars[1].name, "req2");
    vars[1].domain_size = 2;
    vars[1].is_system_controlled = false;
    vars[1].initial_value = 0;

    vars[2].var_id = 2;
    strcpy(vars[2].name, "grant");
    vars[2].domain_size = 3; /* 0=idle, 1=grant1, 2=grant2 */
    vars[2].is_system_controlled = true; /* system */
    vars[2].initial_value = 0;

    /* Step 2: Create specification */
    grg1_spec_t* spec = grg1_spec_create("Mutex Arbiter", 3, vars);
    assert(spec != NULL);

    grg1_spec_set_env_init(spec, "clients_start_idle", env_init_pred);
    grg1_spec_set_sys_init(spec, "grant_starts_idle", sys_init_pred);

    grg1_spec_set_env_safety(spec, "requests_are_valid", env_trans_safe);
    grg1_spec_set_sys_safety(spec, "grant_transitions_safe", sys_trans_safe);

    /* System justice: infinitely often, grant is not stuck for either client */
    grg1_spec_add_sys_justice(spec, "eventually_grant1", grant1_active);
    grg1_spec_add_sys_justice(spec, "eventually_grant2", grant2_active);

    /* Step 3: Validate specification */
    grg1_spec_validation_t val = grg1_spec_validate(spec);
    printf("Specification validation: %s\n",
           val == GRG1_SPEC_VALID ? "VALID" : "INVALID");

    if (val != GRG1_SPEC_VALID) {
        printf("Validation error code: %d\n", val);
        grg1_spec_free(spec);
        return 1;
    }

    /* Step 4: Print specification */
    grg1_spec_print(spec);

    /* Step 5: Run synthesis */
    printf("\n--- Running synthesis ---\n");
    grg1_synthesis_config_t config = GRG1_SYNTHESIS_CONFIG_DEFAULT;
    config.verbose = true;
    config.compute_strategy = true;

    grg1_synthesis_result_t* result = grg1_synthesize(spec, &config);
    assert(result != NULL);

    /* Step 6: Print result */
    grg1_synthesis_print_report(result);

    /* Step 7: Analyze winning region */
    if (result->winning_region) {
        printf("\nWinning region analysis:\n");
        int wc = grg1_region_count(result->winning_region);
        printf("  %d winning states out of %d total\n",
               wc, result->stats.num_states);
        printf("  Coverage: %.1f%%\n",
               100.0 * wc / result->stats.num_states);
    }

    /* Step 8: Check if strategy was extracted */
    if (result->strategy && result->strategy->is_complete) {
        printf("\nStrategy: COMPLETE (%d edges)\n",
               result->stats.strategy_edges);
    } else if (result->strategy) {
        printf("\nStrategy: extracted but incomplete\n");
    }

    grg1_synthesis_result_free(result);
    grg1_spec_free(spec);

    printf("\nDone.\n");
    return 0;
}
