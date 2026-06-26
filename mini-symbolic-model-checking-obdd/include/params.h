/**
 * params.h — Configuration Parameters for Symbolic Model Checker
 *
 * Centralizes all tunable parameters for the OBDD package,
 * model checker, and fixpoint engine.
 *
 * Knowledge Coverage:
 *   L2: Trade-offs in symbolic model checking: BDD size vs. time,
 *       variable ordering impact, fixpoint iteration bounds
 *   L3: Parameter spaces for algorithm configuration
 *   L7: Practical tuning for real-world verification
 *   L8: Heuristic parameters for dynamic variable reordering
 */

#ifndef PARAMS_H
#define PARAMS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * L2: OBDD Manager Parameters
 * ============================================================ */

#define PARAM_OBDD_MAX_VARS            64
#define PARAM_OBDD_MAX_NODES           65536
#define PARAM_OBDD_UNIQUE_TABLE_SIZE   16384
#define PARAM_OBDD_CACHE_SIZE          4096

/* ============================================================
 * L2: Fixpoint Computation Parameters
 * ============================================================ */

#define PARAM_FIXPOINT_MAX_ITER        10000
#define PARAM_FIXPOINT_EARLY_TERM      1

/* ============================================================
 * L8: Variable Ordering Heuristic Parameters
 * ============================================================ */

#define PARAM_REORDER_MAX_PASSES       5
#define PARAM_SIFT_WINDOW_SIZE         4
#define PARAM_REORDER_GROWTH_FACTOR    2.0f

/* ============================================================
 * L7: Application-Level Parameters
 * ============================================================ */

#define PARAM_MAX_ATOMIC_PROPS         32
#define PARAM_MAX_STATE_BITS           32
#define PARAM_COUNTEREXAMPLE_DEPTH     1000

/* ============================================================
 * L3: Configuration Structure
 * ============================================================ */

/**
 * smc_config_t — Runtime configuration for the symbolic model checker.
 *
 * All parameters can be tuned at runtime before building the Kripke
 * structure or OBDD manager.
 */
typedef struct {
    /* OBDD configuration */
    int32_t  max_vars;
    int32_t  max_nodes;
    int32_t  unique_table_size;
    int32_t  cache_size;

    /* Fixpoint configuration */
    int32_t  fixpoint_max_iter;
    int32_t  fixpoint_early_term;

    /* Reordering configuration */
    int32_t  reorder_max_passes;
    int32_t  sift_window_size;
    double   reorder_growth_factor;
    int32_t  auto_reorder;

    /* Application configuration */
    int32_t  max_atomic_props;
    int32_t  max_state_bits;
    int32_t  counterexample_depth;
    int32_t  verbose;
    int32_t  stats_enabled;
} smc_config_t;

/**
 * smc_config_default — Return the default configuration.
 *
 * All parameters initialized to sensible defaults for small-to-medium
 * verification problems (up to ~10^6 states symbolically).
 */
smc_config_t smc_config_default(void);

/**
 * smc_config_print — Print the current configuration to stdout.
 */
void smc_config_print(const smc_config_t *cfg);

/**
 * smc_config_validate — Validate configuration for consistency.
 *
 * Checks:
 *   - max_vars <= 64 (hardware limitation of uint64_t)
 *   - max_nodes > 0
 *   - fixpoint_max_iter > 0
 *   - counterexample_depth > 0
 *
 * @return  1 if configuration is valid, 0 otherwise
 */
int smc_config_validate(const smc_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* PARAMS_H */