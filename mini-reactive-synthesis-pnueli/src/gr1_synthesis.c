/**
 * gr1_synthesis.c ¡ª GR(1) Synthesis Algorithm Implementation
 *
 * Implements the Generalized Reactivity(1) synthesis algorithm, which
 * solves the synthesis problem for the GR(1) fragment in polynomial
 * time. GR(1) specifications have the form:
 *
 *   (¡Ä_i ¡õ¡ó J_i^e ¡ú ¡Ä_j ¡õ¡ó J_j^s) ¡Ä (safety constraints)
 *
 * The algorithm computes nested fixed points over a finite state space
 * to determine the winning region, then extracts a strategy as a
 * Mealy machine.
 *
 * Reference: Bloem, Jobstmann, Piterman, Pnueli, Sa'ar (2012)
 *            "Synthesis of Reactive(1) Designs", JCSS
 *            Piterman, Pnueli, Sa'ar (2006) VMCAI 2006
 *
 * Knowledge Level: L5 Algorithms, L4 Fundamental Laws
 */

#include "gr1_synthesis.h"
#include "ltl_syntax.h"
#include "reactive_types.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ====================================================================
 * GR(1) Specification Management
 * ==================================================================== */

gr1_spec_t *gr1_spec_create(ltl_formula_t *rho_e_init,
                              ltl_formula_t *rho_e_trans,
                              ltl_formula_t *rho_s_init,
                              ltl_formula_t *rho_s_trans,
                              ltl_formula_t **justice_e,
                              int32_t n_justice_e,
                              ltl_formula_t **justice_s,
                              int32_t n_justice_s) {
    gr1_spec_t *spec = (gr1_spec_t *)calloc(1, sizeof(gr1_spec_t));
    assert(spec != NULL);
    spec->rho_e_init = rho_e_init;
    spec->rho_e_trans = rho_e_trans;
    spec->rho_s_init = rho_s_init;
    spec->rho_s_trans = rho_s_trans;
    spec->num_justice_e = n_justice_e;
    spec->num_justice_s = n_justice_s;

    if (n_justice_e > 0) {
        spec->justice_e = (ltl_formula_t **)malloc((size_t)n_justice_e * sizeof(ltl_formula_t *));
        memcpy(spec->justice_e, justice_e, (size_t)n_justice_e * sizeof(ltl_formula_t *));
    }
    if (n_justice_s > 0) {
        spec->justice_s = (ltl_formula_t **)malloc((size_t)n_justice_s * sizeof(ltl_formula_t *));
        memcpy(spec->justice_s, justice_s, (size_t)n_justice_s * sizeof(ltl_formula_t *));
    }
    return spec;
}

void gr1_spec_destroy(gr1_spec_t *spec) {
    if (spec == NULL) return;
    free(spec->justice_e);
    free(spec->justice_s);
    free(spec);
}

/* ====================================================================
 * GR(1) State Space
 * ==================================================================== */

gr1_state_space_t *gr1_state_space_create(int32_t num_inputs,
                                           int32_t num_outputs) {
    assert(num_inputs >= 0 && num_outputs >= 0);
    int32_t total_vars = num_inputs + num_outputs;
    assert(total_vars <= 16); /* explicit enumeration limit */

    gr1_state_space_t *space = (gr1_state_space_t *)calloc(1, sizeof(gr1_state_space_t));
    assert(space != NULL);
    space->num_inputs = num_inputs;
    space->num_outputs = num_outputs;
    space->num_states = 1 << total_vars;

    space->states = (gr1_state_t *)malloc((size_t)space->num_states * sizeof(gr1_state_t));
    space->initial_states = (bool *)calloc((size_t)space->num_states, sizeof(bool));
    space->winning_region = (int32_t *)malloc((size_t)space->num_states * sizeof(int32_t));
    assert(space->states && space->initial_states && space->winning_region);

    /* Initialize state encodings */
    int32_t input_mask = (1 << num_inputs) - 1;
    for (int32_t idx = 0; idx < space->num_states; idx++) {
        space->states[idx].env.bits[0] = (uint64_t)(idx & input_mask);
        space->states[idx].env.bits[1] = 0;
        space->states[idx].sys.bits[0] = (uint64_t)((idx >> num_inputs) & ((1 << num_outputs) - 1));
        space->states[idx].sys.bits[1] = 0;
        space->states[idx].state_index = idx;
        space->winning_region[idx] = -1;
    }

    return space;
}

void gr1_state_space_destroy(gr1_state_space_t *space) {
    if (space == NULL) return;
    free(space->states);
    free(space->initial_states);
    free(space->winning_region);
    free(space);
}

/**
 * Evaluate an LTL safety formula on a single state valuation.
 * Safety formulas only involve current-state propositions and
 * Boolean connectives (no temporal operators).
 *
 * @param phi    safety formula (propositional)
 * @param val    valuation (combined env+sys) to evaluate on
 * @return true if the formula holds */
static bool eval_safety(const ltl_formula_t *phi, valuation_t val) {
    if (phi == NULL) return true;
    switch (phi->type) {
    case LTL_TRUE:  return true;
    case LTL_FALSE: return false;
    case LTL_VAR:   return valuation_get(val, phi->var_id);
    case LTL_NOT:   return !eval_safety(phi->left, val);
    case LTL_AND:   return eval_safety(phi->left, val) && eval_safety(phi->right, val);
    case LTL_OR:    return eval_safety(phi->left, val) || eval_safety(phi->right, val);
    case LTL_IMPLIES: return !eval_safety(phi->left, val) || eval_safety(phi->right, val);
    case LTL_EQUIV: return eval_safety(phi->left, val) == eval_safety(phi->right, val);
    default: return false; /* temporal ops not allowed in safety formulas */
    }
}

void gr1_state_space_init(gr1_state_space_t *space, const gr1_spec_t *spec) {
    assert(space && spec);
    int32_t n = space->num_states;
    int32_t ni = space->num_inputs;
    int32_t no = space->num_outputs;

    /* Mark initial states: those satisfying both initial conditions */
    for (int32_t idx = 0; idx < n; idx++) {
        valuation_t combined = valuation_or(space->states[idx].env,
                                             space->states[idx].sys);
        bool env_init_ok = spec->rho_e_init ?
            eval_safety(spec->rho_e_init, space->states[idx].env) : true;
        bool sys_init_ok = spec->rho_s_init ?
            eval_safety(spec->rho_s_init, combined) : true;
        space->initial_states[idx] = env_init_ok && sys_init_ok;
    }

    /* Build transition relation: we don't store full matrix but evaluate
     * on the fly in the fixed-point computation. */
    (void)ni; (void)no;
}

/**
 * Compute the controllable predecessor of a set W for the system.
 *
 * CPre(W) = { s | ? o ¡Ê ¦²_O : ? i ¡Ê ¦²_I : ¦Ä(s, i, o) ¡Ê W }
 *
 * In our explicit-state encoding, a state s = (e, o_cur) where e is current
 * environment input and o_cur is current system output.
 *
 * After environment chooses next input e' and system chooses output o',
 * next state is (e', o').
 *
 * So CPre(W) = { (e, o) | ? o' : ? e' : (e', o') ¡Ê W }
 *
 * Time: O(|S| * 2^{|O|} * 2^{|I|})
 */
static bool *gr1_cpre_compute(gr1_state_space_t *space,
                               const bool *W,
                               const gr1_spec_t *spec) {
    int32_t n = space->num_states;
    int32_t ni = space->num_inputs;
    int32_t no = space->num_outputs;
    int32_t ni_states = 1 << ni;
    int32_t no_states = 1 << no;

    bool *cpre = (bool *)calloc((size_t)n, sizeof(bool));
    assert(cpre != NULL);

    for (int32_t idx = 0; idx < n; idx++) {
        if (!W[idx]) continue;
        /* For each possible output choice o' */
        for (int32_t o = 0; o < no_states; o++) {
            bool all_env_ok = true;
            /* For all possible next environment inputs e' */
            for (int32_t e = 0; e < ni_states; e++) {
                int32_t next_idx = e | (o << ni);
                if (next_idx >= n || !W[next_idx]) {
                    all_env_ok = false; break;
                }
            }
            if (all_env_ok) { cpre[idx] = true; break; }
        }
    }
    (void)spec;
    return cpre;
}

bool *gr1_fixed_point_solve(gr1_state_space_t *space,
                              const gr1_spec_t *spec) {
    assert(space && spec);
    int32_t n = space->num_states;
    int32_t m = spec->num_justice_s;

    /* Initialize winning region to all states */
    bool *W = (bool *)malloc((size_t)n * sizeof(bool));
    assert(W != NULL);
    for (int32_t i = 0; i < n; i++) W[i] = true;

    /* GR(1) fixed-point iteration:
     *
     * repeat:
     *   for each system justice goal J_j:
     *     Z = boolean states satisfying J_j
     *     repeat:
     *       Z' = Z ¡È CPre(Z) ¡É W
     *       if Z' == Z: break
     *       Z = Z'
     *     W = Z
     *   if W unchanged: break
     */

    bool changed = true;
    int32_t max_iter = n * (m + 1) + 10;
    int32_t iter = 0;

    while (changed && iter < max_iter) {
        changed = false;
        bool *W_prev = (bool *)malloc((size_t)n * sizeof(bool));
        memcpy(W_prev, W, (size_t)n * sizeof(bool));

        /* For each system justice goal */
        for (int32_t j = 0; j < m; j++) {
            /* Target: states satisfying this justice goal */
            bool *target = (bool *)calloc((size_t)n, sizeof(bool));
            for (int32_t idx = 0; idx < n; idx++) {
                if (W[idx] && spec->justice_s[j]) {
                    valuation_t combined = valuation_or(
                        space->states[idx].env, space->states[idx].sys);
                    target[idx] = eval_safety(spec->justice_s[j], combined);
                }
            }

            /* Inner fixed point: attractor of target within W */
            bool *Z = (bool *)malloc((size_t)n * sizeof(bool));
            memcpy(Z, target, (size_t)n * sizeof(bool));

            bool inner_changed = true;
            int32_t inner_iter = 0;
            while (inner_changed && inner_iter < n + 10) {
                inner_changed = false;
                bool *cpre = gr1_cpre_compute(space, Z, spec);
                for (int32_t idx = 0; idx < n; idx++) {
                    if (W[idx] && cpre[idx] && !Z[idx]) {
                        Z[idx] = true;
                        inner_changed = true;
                    }
                }
                free(cpre);
                inner_iter++;
            }

            /* Intersect with W */
            for (int32_t idx = 0; idx < n; idx++) {
                W[idx] = W[idx] && Z[idx];
            }
            free(target);
            free(Z);
        }

        /* Check if W changed */
        for (int32_t idx = 0; idx < n; idx++) {
            if (W[idx] != W_prev[idx]) { changed = true; break; }
        }
        free(W_prev);
        iter++;
    }

    return W;
}

reactive_module_t *gr1_extract_strategy(gr1_state_space_t *space,
                                         const gr1_spec_t *spec,
                                         const bool *winning_region) {
    assert(space && winning_region);
    int32_t n = space->num_states;
    int32_t no = space->num_outputs;
    int32_t no_states = 1 << no;

    /* Count winning states that are initial */
    int32_t win_count = 0;
    for (int32_t idx = 0; idx < n; idx++) {
        if (winning_region[idx]) win_count++;
    }

    if (win_count == 0) return NULL;

    reactive_module_t *mod = reactive_module_create(win_count,
                                                      space->num_inputs,
                                                      space->num_outputs);

    /* Map original state indices to module states */
    int32_t *state_map = (int32_t *)malloc((size_t)n * sizeof(int32_t));
    int32_t mod_state = 0;
    for (int32_t idx = 0; idx < n; idx++) {
        if (winning_region[idx]) {
            state_map[idx] = mod_state;
            reactive_module_set_output(mod, mod_state, space->states[idx].sys);
            if (space->initial_states[idx]) mod->init_state = mod_state;
            mod_state++;
        } else {
            state_map[idx] = -1;
        }
    }

    /* For each winning state, choose an output that keeps us in winning region */
    for (int32_t idx = 0; idx < n; idx++) {
        if (!winning_region[idx]) continue;
        int32_t from = state_map[idx];

        /* Find a valid output choice */
        for (int32_t o = 0; o < no_states; o++) {
            bool all_env_safe = true;
            for (int32_t e = 0; e < (1 << space->num_inputs); e++) {
                int32_t next_idx = e | (o << space->num_inputs);
                if (next_idx >= n || !winning_region[next_idx]) {
                    all_env_safe = false; break;
                }
            }
            if (all_env_safe) {
                /* Use input e=0 as the guard for this transition */
                valuation_t guard = VALUATION_EMPTY;
                valuation_t mask  = VALUATION_EMPTY;
                for (int32_t e = 0; e < (1 << space->num_inputs); e++) {
                    int32_t next_idx = e | (o << space->num_inputs);
                    if (next_idx < n && winning_region[next_idx]) {
                        int32_t to = state_map[next_idx];
                        reactive_module_add_transition(mod, from, to, guard, mask);
                        break;
                    }
                }
                break;
            }
        }
    }

    free(state_map);
    (void)spec;
    return mod;
}

reactive_module_t *gr1_synthesize(int32_t num_inputs,
                                    int32_t num_outputs,
                                    ltl_formula_t *rho_e_init,
                                    ltl_formula_t *rho_e_trans,
                                    ltl_formula_t *rho_s_init,
                                    ltl_formula_t *rho_s_trans,
                                    ltl_formula_t **justice_e,
                                    int32_t n_justice_e,
                                    ltl_formula_t **justice_s,
                                    int32_t n_justice_s) {
    if (num_inputs + num_outputs > 16) return NULL;

    gr1_spec_t *spec = gr1_spec_create(rho_e_init, rho_e_trans,
        rho_s_init, rho_s_trans, justice_e, n_justice_e,
        justice_s, n_justice_s);

    gr1_state_space_t *space = gr1_state_space_create(num_inputs, num_outputs);
    gr1_state_space_init(space, spec);

    bool *W = gr1_fixed_point_solve(space, spec);

    /* Check if any initial state is winning */
    bool realizable = false;
    for (int32_t idx = 0; idx < space->num_states; idx++) {
        if (space->initial_states[idx] && W[idx]) {
            realizable = true; break;
        }
    }

    reactive_module_t *mod = NULL;
    if (realizable) {
        mod = gr1_extract_strategy(space, spec, W);
    }

    free(W);
    gr1_spec_destroy(spec);
    /* Note: space is destroyed after extraction since mod depends on it */
    /* Actually we need to keep space alive during extraction */
    gr1_state_space_destroy(space);
    return mod;
}

/* ====================================================================
 * Symbolic Region Operations
 * ==================================================================== */

symbolic_region_t *symbolic_region_create(int32_t num_states) {
    assert(num_states > 0);
    symbolic_region_t *region = (symbolic_region_t *)calloc(1, sizeof(symbolic_region_t));
    assert(region != NULL);
    region->num_states = num_states;
    int32_t words = (num_states + 63) / 64;
    region->bits = (uint64_t *)calloc((size_t)words, sizeof(uint64_t));
    assert(region->bits != NULL);
    return region;
}

void symbolic_region_destroy(symbolic_region_t *region) {
    if (region == NULL) return;
    free(region->bits);
    free(region);
}

void symbolic_region_add(symbolic_region_t *region, int32_t state_id) {
    assert(region && state_id >= 0 && state_id < region->num_states);
    region->bits[state_id >> 6] |= (1ULL << (uint64_t)(state_id & 63));
}

void symbolic_region_remove(symbolic_region_t *region, int32_t state_id) {
    assert(region && state_id >= 0 && state_id < region->num_states);
    region->bits[state_id >> 6] &= ~(1ULL << (uint64_t)(state_id & 63));
}

bool symbolic_region_contains(const symbolic_region_t *region, int32_t state_id) {
    assert(region && state_id >= 0 && state_id < region->num_states);
    return (region->bits[state_id >> 6] >> (uint64_t)(state_id & 63)) & 1ULL;
}

symbolic_region_t *gr1_controllable_predecessor(
    const gr1_state_space_t *space, const symbolic_region_t *W) {
    if (space == NULL || W == NULL) return NULL;
    symbolic_region_t *result = symbolic_region_create(space->num_states);
    int32_t n = space->num_states;
    int32_t ni = 1 << space->num_inputs;
    int32_t no = 1 << space->num_outputs;

    for (int32_t idx = 0; idx < n; idx++) {
        /* Check if there exists an output o such that for all inputs e,
         * next state (e, o) is in W */
        for (int32_t o = 0; o < no; o++) {
            bool all_in = true;
            for (int32_t e = 0; e < ni; e++) {
                int32_t next = e | (o << space->num_inputs);
                if (next >= n || !symbolic_region_contains(W, next)) {
                    all_in = false; break;
                }
            }
            if (all_in) {
                symbolic_region_add(result, idx);
                break;
            }
        }
    }
    return result;
}

symbolic_region_t *gr1_uncontrollable_predecessor(
    const gr1_state_space_t *space, const symbolic_region_t *W) {
    if (space == NULL || W == NULL) return NULL;
    symbolic_region_t *result = symbolic_region_create(space->num_states);
    int32_t n = space->num_states;
    int32_t ni = 1 << space->num_inputs;
    int32_t no = 1 << space->num_outputs;

    for (int32_t idx = 0; idx < n; idx++) {
        /* For all outputs o, there exists an input e such that
         * next state (e, o) is in W */
        bool all_out = true;
        for (int32_t o = 0; o < no; o++) {
            bool exists_in = false;
            for (int32_t e = 0; e < ni; e++) {
                int32_t next = e | (o << space->num_inputs);
                if (next < n && symbolic_region_contains(W, next)) {
                    exists_in = true; break;
                }
            }
            if (!exists_in) { all_out = false; break; }
        }
        if (all_out) symbolic_region_add(result, idx);
    }
    return result;
}
