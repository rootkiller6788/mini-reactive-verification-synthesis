/**
 * synthesis_engine.c �� Reactive Synthesis Engine
 *
 * Implements the complete LTL synthesis pipeline:
 *   LTL formula -> NBA -> DPA -> parity game -> strategy extraction
 *
 * Also implements specification validation and the main synthesis
 * entry points.
 *
 * Reference: Pnueli & Rosner (1989) "On the Synthesis of a Reactive Module"
 *
 * Knowledge Level: L4 Fundamental Laws, L5 Algorithms
 */

#include "synthesis.h"
#include "automaton.h"
#include "ltl_syntax.h"
#include "game_structure.h"
#include "gr1_synthesis.h"
#include "reactive_types.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

synthesis_config_t synthesis_config_default(void) {
    synthesis_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.algorithm = ALGO_AUTO;
    cfg.bound_k = 0;
    cfg.timeout_seconds = 60.0;
    cfg.verbose = false;
    cfg.minimize_result = true;
    cfg.max_states = 1000;
    return cfg;
}

void synthesis_result_destroy(synthesis_result_t *result) {
    if (result == NULL) return;
    if (result->module) reactive_module_destroy(result->module);
    if (result->counter_strategy) ltl_free(result->counter_strategy);
    free(result);
}

synthesis_result_t *synthesis_realizability_check(
    const synthesis_spec_t *spec,
    const synthesis_config_t *config) {
    assert(spec != NULL);
    (void)config; /* reserved for future use */

    synthesis_result_t *result = (synthesis_result_t *)calloc(1, sizeof(synthesis_result_t));
    assert(result != NULL);
    result->status = SYNTH_UNKNOWN;

    clock_t start = clock();

    if (spec->guarantee == NULL) {
        /* No specification given: any module vacuously satisfies */
        result->status = SYNTH_REALIZABLE;
        result->module = reactive_module_create(2, spec->num_inputs, spec->num_outputs);
        snprintf(result->message, sizeof(result->message),
                 "Trivial specification (no guarantee): any module works");
        goto done;
    }

    /* Step 1: Validate the specification */
    const char *err_msg = NULL;
    if (!synthesis_validate_spec(spec, &err_msg)) {
        result->status = SYNTH_ERROR;
        snprintf(result->message, sizeof(result->message),
                 "Invalid specification: %s", err_msg ? err_msg : "unknown error");
        goto done;
    }

    /* Step 2: Convert LTL assumption+guarantee to an NBA */
    /* The specification is: assumption -> guarantee = !assumption \/ guarantee */
    ltl_formula_t *neg_assumption = ltl_not(spec->assumption);
    ltl_formula_t *spec_formula = ltl_or(neg_assumption, spec->guarantee);
    ltl_free(neg_assumption);

    ltl_formula_t *spec_nnf = ltl_to_nnf(spec_formula);
    ltl_free(spec_formula);

    nba_t *nba = ltl_to_nba(spec_nnf);
    if (nba == NULL) {
        result->status = SYNTH_ERROR;
        snprintf(result->message, sizeof(result->message),
                 "Failed to build NBA from specification");
        ltl_free(spec_nnf);
        goto done;
    }

    /* Step 3: Check NBA emptiness (if empty, spec is vacuously true) */
    if (nba_is_empty(nba)) {
        result->status = SYNTH_REALIZABLE;
        result->module = reactive_module_create(1, spec->num_inputs, spec->num_outputs);
        snprintf(result->message, sizeof(result->message),
                 "Specification is universally valid (vacuous realizability)");
        nba_destroy(nba);
        ltl_free(spec_nnf);
        goto done;
    }

    /* Step 4: Determinize NBA to DPA */
    dpa_t *dpa = nba_to_dpa_piterman(nba);
    if (dpa == NULL) {
        result->status = SYNTH_ERROR;
        snprintf(result->message, sizeof(result->message),
                 "Failed to determinize NBA to DPA");
        nba_destroy(nba);
        ltl_free(spec_nnf);
        goto done;
    }

    /* Step 5: Build synthesis parity game from DPA */
    parity_game_t *game = dpa_to_parity_game(dpa, spec->num_inputs, spec->num_outputs);
    if (game == NULL) {
        result->status = SYNTH_ERROR;
        snprintf(result->message, sizeof(result->message),
                 "Failed to build parity game from DPA");
        dpa_destroy(dpa);
        nba_destroy(nba);
        ltl_free(spec_nnf);
        goto done;
    }

    /* Step 6: Solve the parity game */
    parity_game_solution_t *sol = parity_game_solve_zielonka(game);

    /* Step 7: Check if system wins from initial states */
    bool sys_wins = false;
    for (int32_t v = 0; v < game->arena.num_vertices; v++) {
        if (game->arena.is_initial[v] && sol->winning_region[PLAYER_SYS][v]) {
            sys_wins = true;
            break;
        }
    }

    if (sys_wins) {
        result->status = SYNTH_REALIZABLE;
        result->module = parity_game_extract_strategy(game, sol,
            spec->num_inputs, spec->num_outputs);
        if (result->module) {
            result->num_states = result->module->num_states;
        }
        snprintf(result->message, sizeof(result->message),
                 "Specification is realizable. Synthesized %d-state module.",
                 result->num_states);
    } else {
        result->status = SYNTH_UNREALIZABLE;
        snprintf(result->message, sizeof(result->message),
                 "Specification is unrealizable: environment can force violation.");
    }

    parity_game_solution_destroy(sol);
    parity_game_destroy(game);
    dpa_destroy(dpa);
    nba_destroy(nba);
    ltl_free(spec_nnf);

done:
    result->time_seconds = (double)(clock() - start) / CLOCKS_PER_SEC;
    return result;
}

reactive_module_t *synthesis_synthesize(
    const synthesis_spec_t *spec,
    const synthesis_config_t *config) {
    synthesis_result_t *result = synthesis_realizability_check(spec, config);
    reactive_module_t *mod = NULL;
    if (result && result->status == SYNTH_REALIZABLE) {
        mod = result->module;
        result->module = NULL;
    }
    synthesis_result_destroy(result);
    return mod;
}

bool synthesis_validate_spec(const synthesis_spec_t *spec, const char **error_msg) {
    if (spec == NULL) {
        if (error_msg) *error_msg = "specification is NULL";
        return false;
    }
    if (spec->num_inputs < 0 || spec->num_inputs > 64) {
        if (error_msg) *error_msg = "number of inputs out of range [0, 64]";
        return false;
    }
    if (spec->num_outputs < 0 || spec->num_outputs > 64) {
        if (error_msg) *error_msg = "number of outputs out of range [0, 64]";
        return false;
    }
    /* Check that formulas are well-formed (non-NULL for guarantee) */
    if (spec->guarantee == NULL && spec->assumption == NULL) {
        if (error_msg) *error_msg = "both assumption and guarantee are NULL";
        return false;
    }
    /* Verify proposition ranges */
    if (spec->guarantee) {
        valuation_t props = ltl_collect_propositions(spec->guarantee);
        int32_t max_var = -1;
        for (int32_t i = 0; i < 128; i++) {
            if (valuation_get(props, i)) max_var = i;
        }
        if (max_var >= spec->num_inputs + spec->num_outputs) {
            if (error_msg) *error_msg = "guarantee formula references out-of-range variable";
            return false;
        }
    }
    return true;
}

int32_t synthesis_spec_subformula_count(const synthesis_spec_t *spec) {
    if (spec == NULL) return 0;
    int32_t c1 = 0, c2 = 0;
    int32_t n;
    if (spec->assumption) { ltl_subformulas(spec->assumption, &n); c1 = n; }
    if (spec->guarantee)   { ltl_subformulas(spec->guarantee, &n); c2 = n; }
    return c1 + c2;
}

synthesis_result_t *synthesis_gr1(
    int32_t num_inputs, int32_t num_outputs,
    ltl_formula_t **env_safety, int32_t num_env_safety,
    ltl_formula_t **env_justice, int32_t num_env_justice,
    ltl_formula_t **sys_safety, int32_t num_sys_safety,
    ltl_formula_t **sys_justice, int32_t num_sys_justice,
    const synthesis_config_t *config) {
    (void)config;
    synthesis_result_t *result = (synthesis_result_t *)calloc(1, sizeof(synthesis_result_t));
    assert(result != NULL);
    result->status = SYNTH_UNKNOWN;

    if (num_inputs + num_outputs > 16) {
        result->status = SYNTH_ERROR;
        snprintf(result->message, sizeof(result->message),
                 "GR(1) explicit-state synthesis limited to 16 total variables");
        return result;
    }

    /* Build GR(1) spec from individual formulas */
    ltl_formula_t *rho_e_init = ltl_true();
    ltl_formula_t *rho_e_trans = ltl_true();
    ltl_formula_t *rho_s_init = ltl_true();
    ltl_formula_t *rho_s_trans = ltl_true();

    for (int32_t i = 0; i < num_env_safety; i++) {
        ltl_formula_t *tmp = ltl_and(rho_e_init, env_safety[i]);
        ltl_free(rho_e_init);
        rho_e_init = tmp;
        tmp = ltl_and(rho_e_trans, env_safety[i]);
        ltl_free(rho_e_trans);
        rho_e_trans = tmp;
    }
    for (int32_t i = 0; i < num_sys_safety; i++) {
        ltl_formula_t *tmp = ltl_and(rho_s_init, sys_safety[i]);
        ltl_free(rho_s_init);
        rho_s_init = tmp;
        tmp = ltl_and(rho_s_trans, sys_safety[i]);
        ltl_free(rho_s_trans);
        rho_s_trans = tmp;
    }

    /* Run GR(1) synthesis */
    reactive_module_t *mod = gr1_synthesize(num_inputs, num_outputs,
        rho_e_init, rho_e_trans, rho_s_init, rho_s_trans,
        env_justice, num_env_justice, sys_justice, num_sys_justice);

    ltl_free(rho_e_init); ltl_free(rho_e_trans);
    ltl_free(rho_s_init); ltl_free(rho_s_trans);

    if (mod) {
        result->status = SYNTH_REALIZABLE;
        result->module = mod;
        result->num_states = mod->num_states;
        snprintf(result->message, sizeof(result->message),
                 "GR(1) synthesis successful: %d states", mod->num_states);
    } else {
        result->status = SYNTH_UNREALIZABLE;
        snprintf(result->message, sizeof(result->message),
                 "GR(1) specification is unrealizable");
    }
    return result;
}
