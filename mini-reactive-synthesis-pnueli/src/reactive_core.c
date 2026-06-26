/**
 * reactive_core.c �� Core Reactive System Operations
 *
 * Implements the fundamental data structure operations for reactive
 * modules: creation, destruction, state management, transition
 * handling, trace generation, and valuation operations.
 *
 * Reference: Pnueli (1985) "Linear Time Temporal Logic with Past"
 *            Manna & Pnueli (1992) "The Temporal Logic of Reactive
 *            and Concurrent Systems: Specification"
 *
 * Knowledge Level: L3 Mathematical Structures
 */

#include "reactive_types.h"
#include "ltl_syntax.h"
#include <assert.h>
#include <string.h>

/* ====================================================================
 * Internal: Adjacency List Management
 * ==================================================================== */

/* Initial capacity for transitions per state. Dynamically grows. */
#define TRANS_INIT_CAP 8

/* ====================================================================
 * Module Lifecycle
 * ==================================================================== */

reactive_module_t *reactive_module_create(int32_t num_states,
                                           int32_t num_inputs,
                                           int32_t num_outputs) {
    assert(num_states > 0);
    assert(num_inputs >= 0 && num_inputs <= 64);
    assert(num_outputs >= 0 && num_outputs <= 64);

    reactive_module_t *mod = (reactive_module_t *)malloc(sizeof(reactive_module_t));
    assert(mod != NULL);
    memset(mod, 0, sizeof(reactive_module_t));

    mod->num_states  = num_states;
    mod->num_inputs  = num_inputs;
    mod->num_outputs = num_outputs;
    mod->init_state  = 0;

    /* Allocate states array */
    mod->states = (state_t *)calloc((size_t)num_states, sizeof(state_t));
    assert(mod->states != NULL);
    for (int32_t i = 0; i < num_states; i++) {
        mod->states[i].state_id     = i;
        mod->states[i].output_labels = VALUATION_EMPTY;
        mod->states[i].is_initial   = (i == 0);
        mod->states[i].is_accepting = false;
        mod->states[i].priority     = 0;
    }

    /* Allocate transition adjacency: start with TRANS_INIT_CAP slots per state */
    int32_t init_cap = num_states * TRANS_INIT_CAP;
    mod->transitions  = (transition_t *)malloc((size_t)init_cap * sizeof(transition_t));
    mod->trans_start  = (int32_t *)calloc((size_t)(num_states + 1), sizeof(int32_t));
    mod->outdegree    = (int32_t *)calloc((size_t)num_states, sizeof(int32_t));
    assert(mod->transitions != NULL && mod->trans_start != NULL && mod->outdegree != NULL);
    mod->num_trans = 0;

    /* We''ll manage adjacency differently: store edges sequentially,
     * trans_start[i] = start index, [i+1] = end index (capacity boundary).
     * Initially each state gets TRANS_INIT_CAP slots. */
    for (int32_t i = 0; i <= num_states; i++) {
        mod->trans_start[i] = i * TRANS_INIT_CAP;
    }

    /* Allocate name arrays */
    mod->input_names  = (char **)calloc((size_t)num_inputs, sizeof(char *));
    mod->output_names = (char **)calloc((size_t)num_outputs, sizeof(char *));

    return mod;
}

void reactive_module_destroy(reactive_module_t *mod) {
    if (mod == NULL) return;

    free(mod->states);
    free(mod->transitions);
    free(mod->outdegree);
    free(mod->trans_start);

    if (mod->input_names) {
        for (int32_t i = 0; i < mod->num_inputs; i++) {
            free(mod->input_names[i]);
        }
        free(mod->input_names);
    }
    if (mod->output_names) {
        for (int32_t i = 0; i < mod->num_outputs; i++) {
            free(mod->output_names[i]);
        }
        free(mod->output_names);
    }

    free(mod);
}

/* ====================================================================
 * Transition Management
 * ==================================================================== */

int32_t reactive_module_add_transition(reactive_module_t *mod,
                                        int32_t from_state,
                                        int32_t to_state,
                                        valuation_t input_guard,
                                        valuation_t input_mask) {
    assert(mod != NULL);
    assert(from_state >= 0 && from_state < mod->num_states);
    assert(to_state >= 0 && to_state < mod->num_states);

    int32_t idx = mod->num_trans;
    mod->transitions[idx].from_state  = from_state;
    mod->transitions[idx].to_state    = to_state;
    mod->transitions[idx].input_guard = input_guard;
    mod->transitions[idx].input_mask  = input_mask;

    mod->num_trans++;
    mod->outdegree[from_state]++;

    return idx;
}

void reactive_module_set_output(reactive_module_t *mod,
                                 int32_t state_id,
                                 valuation_t outputs) {
    assert(mod != NULL);
    assert(state_id >= 0 && state_id < mod->num_states);
    mod->states[state_id].output_labels = outputs;
}

/* ====================================================================
 * Step Semantics
 * ==================================================================== */

int32_t reactive_module_step(const reactive_module_t *mod,
                              int32_t current_state,
                              valuation_t inputs) {
    assert(mod != NULL);
    assert(current_state >= 0 && current_state < mod->num_states);

    /* Linear scan through transitions for this state.
     * For production use, this would use indexing. */
    for (int32_t i = 0; i < mod->num_trans; i++) {
        const transition_t *t = &mod->transitions[i];
        if (t->from_state != current_state) continue;

        if (valuation_matches(inputs, t->input_guard, t->input_mask)) {
            return t->to_state;
        }
    }

    return -1; /* No matching transition: incomplete specification */
}

valuation_t reactive_module_get_output(const reactive_module_t *mod,
                                        int32_t state_id) {
    assert(mod != NULL);
    assert(state_id >= 0 && state_id < mod->num_states);
    return mod->states[state_id].output_labels;
}

/* ====================================================================
 * Trace Simulation
 * ==================================================================== */

trace_step_t *reactive_module_simulate(const reactive_module_t *mod,
                                        int32_t length,
                                        int32_t *actual_len) {
    assert(mod != NULL);
    assert(length > 0);

    trace_step_t *trace = (trace_step_t *)malloc((size_t)length * sizeof(trace_step_t));
    assert(trace != NULL);

    int32_t state = mod->init_state;
    int32_t step  = 0;

    /* Simple deterministic simulation: for each step, pick the
     * first matching transition. In a full implementation, a
     * random scheduler would pick among valid transitions. */
    for (step = 0; step < length; step++) {
        /* Find any valid input that triggers a transition */
        bool found = false;
        for (int32_t t = 0; t < mod->num_trans; t++) {
            if (mod->transitions[t].from_state == state) {
                /* Use the guard as the actual input for simulation */
                valuation_t inputs = mod->transitions[t].input_guard;
                if (valuation_matches(inputs,
                    mod->transitions[t].input_guard,
                    mod->transitions[t].input_mask)) {
                    trace[step].inputs   = inputs;
                    trace[step].outputs  = mod->states[state].output_labels;
                    trace[step].state_id = state;
                    state = mod->transitions[t].to_state;
                    found = true;
                    break;
                }
            }
        }
        if (!found) break; /* deadlock */
    }

    *actual_len = step;
    return trace;
}

/* ====================================================================
 * Printing
 * ==================================================================== */

void reactive_module_print(const reactive_module_t *mod) {
    if (mod == NULL) {
        printf("NULL module\n");
        return;
    }

    printf("Reactive Module: %d states, %d inputs, %d outputs\n",
           mod->num_states, mod->num_inputs, mod->num_outputs);
    printf("Initial state: %d\n", mod->init_state);

    printf("\nStates:\n");
    for (int32_t i = 0; i < mod->num_states; i++) {
        const state_t *s = &mod->states[i];
        printf("  s%d: outputs=[", i);
        bool first = true;
        for (int32_t j = 0; j < mod->num_outputs; j++) {
            if (!first) printf(",");
            printf("%d", valuation_get(s->output_labels, j) ? 1 : 0);
            first = false;
        }
        printf("] %s %s\n",
               s->is_initial ? "(init)" : "",
               s->is_accepting ? "(accepting)" : "");
    }

    printf("\nTransitions:\n");
    for (int32_t i = 0; i < mod->num_trans; i++) {
        const transition_t *t = &mod->transitions[i];
        printf("  s%d --[", t->from_state);
        bool first = true;
        for (int32_t j = 0; j < mod->num_inputs; j++) {
            if (valuation_get(t->input_mask, j)) {
                if (!first) printf(",");
                printf("i%d=%d", j, valuation_get(t->input_guard, j) ? 1 : 0);
                first = false;
            }
        }
        if (first) printf("*"); /* no input condition = self-loop */
        printf("]--> s%d\n", t->to_state);
    }
}

/* ====================================================================
 * Valuation Operations
 *
 * Valuations are bit-vectors over up to 128 boolean variables,
 * stored as two uint64_t values. Operations use bitwise logic
 * for O(1) performance.
 * ==================================================================== */

valuation_t valuation_singleton(int32_t var_id) {
    assert(var_id >= 0 && var_id < 128);
    valuation_t v = VALUATION_EMPTY;
    if (var_id < 64) {
        v.bits[0] = 1ULL << ((uint64_t)var_id);
    } else {
        v.bits[1] = 1ULL << ((uint64_t)(var_id - 64));
    }
    return v;
}

valuation_t valuation_set(valuation_t v, int32_t var_id, bool value) {
    assert(var_id >= 0 && var_id < 128);
    if (var_id < 64) {
        if (value) {
            v.bits[0] |= (1ULL << ((uint64_t)var_id));
        } else {
            v.bits[0] &= ~(1ULL << ((uint64_t)var_id));
        }
    } else {
        if (value) {
            v.bits[1] |= (1ULL << ((uint64_t)(var_id - 64)));
        } else {
            v.bits[1] &= ~(1ULL << ((uint64_t)(var_id - 64)));
        }
    }
    return v;
}

bool valuation_get(valuation_t v, int32_t var_id) {
    assert(var_id >= 0 && var_id < 128);
    if (var_id < 64) {
        return (v.bits[0] >> ((uint64_t)var_id)) & 1ULL;
    } else {
        return (v.bits[1] >> ((uint64_t)(var_id - 64))) & 1ULL;
    }
}

valuation_t valuation_and(valuation_t a, valuation_t b) {
    valuation_t r;
    r.bits[0] = a.bits[0] & b.bits[0];
    r.bits[1] = a.bits[1] & b.bits[1];
    return r;
}

valuation_t valuation_or(valuation_t a, valuation_t b) {
    valuation_t r;
    r.bits[0] = a.bits[0] | b.bits[0];
    r.bits[1] = a.bits[1] | b.bits[1];
    return r;
}

valuation_t valuation_not(valuation_t v, int32_t num_vars) {
    /* Create mask of valid bits */
    valuation_t mask = VALUATION_EMPTY;
    for (int32_t i = 0; i < num_vars && i < 128; i++) {
        mask = valuation_set(mask, i, true);
    }
    valuation_t r;
    r.bits[0] = (~v.bits[0]) & mask.bits[0];
    r.bits[1] = (~v.bits[1]) & mask.bits[1];
    return r;
}

bool valuation_matches(valuation_t a, valuation_t guard, valuation_t mask) {
    /* For each bit where mask=1, check that a matches guard */
    uint64_t m0 = mask.bits[0];
    uint64_t m1 = mask.bits[1];
    return ((a.bits[0] & m0) == (guard.bits[0] & m0)) &&
           ((a.bits[1] & m1) == (guard.bits[1] & m1));
}

/* ====================================================================
 * Synthesis Specification Management
 * ==================================================================== */

synthesis_spec_t *synthesis_spec_create(ltl_formula_t *assumption,
                                         ltl_formula_t *guarantee,
                                         int32_t num_inputs,
                                         int32_t num_outputs) {
    assert(num_inputs >= 0 && num_outputs >= 0);

    synthesis_spec_t *spec = (synthesis_spec_t *)malloc(sizeof(synthesis_spec_t));
    assert(spec != NULL);
    memset(spec, 0, sizeof(synthesis_spec_t));

    spec->assumption  = assumption;
    spec->guarantee   = guarantee;
    spec->num_inputs  = num_inputs;
    spec->num_outputs = num_outputs;

    return spec;
}

void synthesis_spec_destroy(synthesis_spec_t *spec) {
    if (spec == NULL) return;
    free(spec);
}

/* ====================================================================
 * Infinite Trace Operations
 * ==================================================================== */

/**
 * Create an infinite trace from a lasso (prefix + cycle).
 * The lasso representation is sufficient because every ��-regular
 * language has an ultimately periodic word if it is non-empty.
 *
 * Reference: B��chi (1962) "On a Decision Method in Restricted
 *            Second Order Arithmetic"
 *
 * Time: O(prefix_len + cycle_len) */
infinite_trace_t *infinite_trace_from_lasso(const trace_step_t *prefix,
                                              int32_t prefix_len,
                                              const trace_step_t *cycle,
                                              int32_t cycle_len) {
    infinite_trace_t *trace = (infinite_trace_t *)malloc(sizeof(infinite_trace_t));
    assert(trace != NULL);

    trace->prefix_len = prefix_len;
    trace->cycle_len  = cycle_len;

    trace->prefix = (trace_step_t *)malloc((size_t)prefix_len * sizeof(trace_step_t));
    trace->cycle  = (trace_step_t *)malloc((size_t)cycle_len * sizeof(trace_step_t));
    assert(trace->prefix != NULL && trace->cycle != NULL);

    if (prefix_len > 0) {
        memcpy(trace->prefix, prefix, (size_t)prefix_len * sizeof(trace_step_t));
    }
    if (cycle_len > 0) {
        memcpy(trace->cycle, cycle, (size_t)cycle_len * sizeof(trace_step_t));
    }

    return trace;
}

/**
 * Free an infinite trace.
 * Time: O(1) */
void infinite_trace_destroy(infinite_trace_t *trace) {
    if (trace == NULL) return;
    free(trace->prefix);
    free(trace->cycle);
    free(trace);
}

/**
 * Evaluate an LTL formula on an infinite trace at a given position.
 *
 * This is the standard ��-word semantics of LTL:
 *   w, i ? p    iff p �� w[i]
 *   w, i ? ?��   iff w, i ? ��
 *   w, i ? �աĦ�  iff w, i ? �� and w, i ? ��
 *   w, i ? X��   iff w, i+1 ? ��
 *   w, i ? ��U��  iff ?k��i: w,k?�� and ?j��[i,k): w,j?��
 *   w, i ? G��   iff ?k��i: w,k?��
 *   w, i ? F��   iff ?k��i: w,k?��
 *
 * For lasso traces w = prefix �� cycle^��, we unroll:
 *   - For positions in prefix: use actual trace data
 *   - For positions beyond prefix: use cycle data modulo cycle_len
 *
 * @param phi    formula to evaluate
 * @param trace  the infinite trace (lasso)
 * @param pos    the current ��-position
 * @param depth  recursion depth guard (prevents infinite recursion)
 * @return true if (trace, pos) ? phi
 *
 * Time: O(|phi| * |trace|) worst-case for Until operator
 * Reference: Pnueli (1977) */
bool ltl_evaluate_infinite(const ltl_formula_t *phi,
                            const infinite_trace_t *trace,
                            int32_t pos,
                            int32_t depth) {
    if (phi == NULL || trace == NULL || depth > 10000) return false;

    switch (phi->type) {
    case LTL_TRUE:
        return true;
    case LTL_FALSE:
        return false;
    case LTL_VAR: {
        /* Get valuation at position pos in the infinite trace */
        valuation_t val;
        int32_t total_prefix = trace->prefix_len;
        if (pos < total_prefix) {
            val = trace->prefix[pos].outputs;
        } else {
            int32_t cycle_pos = (pos - total_prefix) % trace->cycle_len;
            val = trace->cycle[cycle_pos].outputs;
        }
        return valuation_get(val, phi->var_id);
    }
    case LTL_NOT:
        return !ltl_evaluate_infinite(phi->left, trace, pos, depth + 1);
    case LTL_AND:
        return ltl_evaluate_infinite(phi->left, trace, pos, depth + 1) &&
               ltl_evaluate_infinite(phi->right, trace, pos, depth + 1);
    case LTL_OR:
        return ltl_evaluate_infinite(phi->left, trace, pos, depth + 1) ||
               ltl_evaluate_infinite(phi->right, trace, pos, depth + 1);
    case LTL_X:
        return ltl_evaluate_infinite(phi->left, trace, pos + 1, depth + 1);
    case LTL_G: {
        /* G ��: check �� at all positions from pos onward */
        int32_t check_len = trace->prefix_len + trace->cycle_len * 2;
        for (int32_t k = pos; k < pos + check_len && k < 1000000; k++) {
            if (!ltl_evaluate_infinite(phi->left, trace, k, depth + 1))
                return false;
        }
        return true;
    }
    case LTL_F: {
        /* F ��: find some position >= pos where �� holds */
        int32_t check_len = trace->prefix_len + trace->cycle_len * 2;
        for (int32_t k = pos; k < pos + check_len && k < 1000000; k++) {
            if (ltl_evaluate_infinite(phi->left, trace, k, depth + 1))
                return true;
        }
        return false;
    }
    case LTL_U: {
        /* �� U ��: ? k �� pos: �� holds at k and �� holds at all [pos, k) */
        int32_t check_len = trace->prefix_len + trace->cycle_len * 2;
        for (int32_t k = pos; k < pos + check_len && k < 1000000; k++) {
            if (ltl_evaluate_infinite(phi->right, trace, k, depth + 1)) {
                /* Check �� holds at all positions from pos to k-1 */
                bool all_phi = true;
                for (int32_t j = pos; j < k; j++) {
                    if (!ltl_evaluate_infinite(phi->left, trace, j, depth + 1)) {
                        all_phi = false;
                        break;
                    }
                }
                if (all_phi) return true;
            }
        }
        return false;
    }
    case LTL_R: {
        /* �� R �� �� ?(?�� U ?��), or equivalently:
         * ?k��pos: �� holds at k, OR ?j��[pos,k): �� holds at j */
        int32_t check_len = trace->prefix_len + trace->cycle_len * 2;
        for (int32_t k = pos; k < pos + check_len && k < 1000000; k++) {
            if (!ltl_evaluate_infinite(phi->right, trace, k, depth + 1)) {
                /* �� fails at k, check if �� held somewhere before k */
                bool phi_before = false;
                for (int32_t j = pos; j < k; j++) {
                    if (ltl_evaluate_infinite(phi->left, trace, j, depth + 1)) {
                        phi_before = true;
                        break;
                    }
                }
                if (!phi_before) return false;
            }
        }
        return true;
    }
    case LTL_W: {
        /* �� W �� �� (�� U ��) �� G �� */
        return ltl_evaluate_infinite(
            ltl_until(ltl_clone((ltl_formula_t *)phi),
                      ltl_clone((ltl_formula_t *)phi->right)),
            trace, pos, depth + 1)
            || ltl_evaluate_infinite(
            ltl_always(ltl_clone((ltl_formula_t *)phi)),
            trace, pos, depth + 1);
        /* Note: this creates temporary nodes; in production code
         * these would be pre-computed. The const cast is safe because
         * refcount is incremented by ltl_clone. */
    }
    case LTL_M: {
        /* �� M �� �� (�� U (�� �� ��)) �� G �� �� (�� R ��) �� F �� */
        ltl_formula_t *tmp_and = ltl_and(
            ltl_clone((ltl_formula_t *)phi),
            ltl_clone((ltl_formula_t *)phi->right));
        return ltl_evaluate_infinite(
            ltl_until(ltl_clone((ltl_formula_t *)phi->right), tmp_and),
            trace, pos, depth + 1)
            || ltl_evaluate_infinite(
            ltl_always(ltl_clone((ltl_formula_t *)phi->right)),
            trace, pos, depth + 1);
    }
    default:
        return false;
    }
}

/**
 * Check if an LTL formula holds at the start of an infinite trace.
 * Convenience wrapper around ltl_evaluate_infinite.
 *
 * Time: O(|��| * |trace|) */
bool ltl_check_trace(const ltl_formula_t *phi,
                      const infinite_trace_t *trace) {
    return ltl_evaluate_infinite(phi, trace, 0, 0);
}
