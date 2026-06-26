/**
 * params.c - Configuration Parameters Implementation
 *
 * Implements default configuration and validation for the
 * symbolic model checker.
 *
 * Knowledge Coverage:
 *   L2: Trade-off parameters for symbolic model checking
 *   L7: Practical configuration for real-world verification
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "params.h"

/* ============================================================
 * smc_config_default - Return sensible default configuration
 * ============================================================ */

smc_config_t smc_config_default(void) {
    smc_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.max_vars            = PARAM_OBDD_MAX_VARS;
    cfg.max_nodes           = PARAM_OBDD_MAX_NODES;
    cfg.unique_table_size   = PARAM_OBDD_UNIQUE_TABLE_SIZE;
    cfg.cache_size          = PARAM_OBDD_CACHE_SIZE;
    cfg.fixpoint_max_iter   = PARAM_FIXPOINT_MAX_ITER;
    cfg.fixpoint_early_term = PARAM_FIXPOINT_EARLY_TERM;
    cfg.reorder_max_passes  = PARAM_REORDER_MAX_PASSES;
    cfg.sift_window_size    = PARAM_SIFT_WINDOW_SIZE;
    cfg.reorder_growth_factor = PARAM_REORDER_GROWTH_FACTOR;
    cfg.auto_reorder        = 0;
    cfg.max_atomic_props    = PARAM_MAX_ATOMIC_PROPS;
    cfg.max_state_bits      = PARAM_MAX_STATE_BITS;
    cfg.counterexample_depth = PARAM_COUNTEREXAMPLE_DEPTH;
    cfg.verbose             = 0;
    cfg.stats_enabled       = 0;

    return cfg;
}

/* ============================================================
 * smc_config_print - Print configuration for diagnostics
 * ============================================================ */

void smc_config_print(const smc_config_t *cfg) {
    if (!cfg) {
        printf("Configuration: NULL\n");
        return;
    }
    printf("=== Symbolic Model Checker Configuration ===\n");
    printf("  OBDD:\n");
    printf("    max_vars:          %d\n", cfg->max_vars);
    printf("    max_nodes:         %d\n", cfg->max_nodes);
    printf("    unique_table_size: %d\n", cfg->unique_table_size);
    printf("    cache_size:        %d\n", cfg->cache_size);
    printf("  Fixpoint:\n");
    printf("    max_iter:          %d\n", cfg->fixpoint_max_iter);
    printf("    early_term:        %d\n", cfg->fixpoint_early_term);
    printf("  Reordering:\n");
    printf("    max_passes:        %d\n", cfg->reorder_max_passes);
    printf("    sift_window:       %d\n", cfg->sift_window_size);
    printf("    growth_factor:     %.2f\n", cfg->reorder_growth_factor);
    printf("    auto_reorder:      %d\n", cfg->auto_reorder);
    printf("  Application:\n");
    printf("    max_atomic_props:  %d\n", cfg->max_atomic_props);
    printf("    max_state_bits:    %d\n", cfg->max_state_bits);
    printf("    counterexample_depth: %d\n", cfg->counterexample_depth);
    printf("    verbose:           %d\n", cfg->verbose);
    printf("    stats_enabled:     %d\n", cfg->stats_enabled);
    printf("=============================================\n");
}

/* ============================================================
 * smc_config_validate - Validate configuration consistency
 * ============================================================ */

int smc_config_validate(const smc_config_t *cfg) {
    if (!cfg) return 0;

    if (cfg->max_vars <= 0 || cfg->max_vars > 64) {
        fprintf(stderr, "Invalid max_vars: %d (must be 1..64)\n", cfg->max_vars);
        return 0;
    }
    if (cfg->max_nodes <= 0) {
        fprintf(stderr, "Invalid max_nodes: %d\n", cfg->max_nodes);
        return 0;
    }
    if (cfg->fixpoint_max_iter <= 0) {
        fprintf(stderr, "Invalid fixpoint_max_iter: %d\n", cfg->fixpoint_max_iter);
        return 0;
    }
    if (cfg->counterexample_depth <= 0) {
        fprintf(stderr, "Invalid counterexample_depth: %d\n", cfg->counterexample_depth);
        return 0;
    }
    if (cfg->max_state_bits <= 0 || cfg->max_state_bits > 32) {
        fprintf(stderr, "Invalid max_state_bits: %d (must be 1..32)\n", cfg->max_state_bits);
        return 0;
    }
    return 1;
}