/**
 * model.c - Kripke Structures Implementation for Symbolic Model Checking
 *
 * Implements finite-state transition system models (Kripke structures)
 * using OBDDs for symbolic state set representation.
 *
 * Knowledge Coverage:
 *   L1: Kripke structure M = (S, S0, R, L, AP)
 *       State space S encoded as boolean vectors in {0,1}^n
 *       Transition relation R as boolean function over (x, x')
 *   L2: Symbolic vs. explicit representation
 *       Characteristic functions for state sets
 *   L3: State encoding: n bits encode up to 2^n states
 *       Prime variable convention for transition relations
 *       Variable renaming (substitution) for fixpoint iteration
 *   L6: Reachable state computation, invariant checking
 *
 * References:
 *   Clarke, E.M., Grumberg, O., & Peled, D.A. (1999).
 *     "Model Checking." MIT Press. Chapter 2-3.
 *   McMillan, K.L. (1993). "Symbolic Model Checking." Kluwer.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "obdd.h"
#include "model.h"

/* ============================================================
 * kripke_create - Allocate a new Kripke structure
 *
 * L1: Initializes a Kripke structure with n state bits and m atomic
 * propositions. State encoding uses variables 0..n-1 for current
 * state and n..2n-1 for next state (primed).
 *
 * The OBDD manager is created with 2n variables.
 * ============================================================ */

kripke_t *kripke_create(int32_t num_state_bits, int32_t num_props) {
    if (num_state_bits <= 0 || num_state_bits > 32) return NULL;
    if (num_props < 0 || num_props > 64) return NULL;

    kripke_t *K = (kripke_t *)calloc(1, sizeof(kripke_t));
    if (!K) return NULL;

    K->num_state_bits = num_state_bits;
    K->num_primed_bits = num_state_bits;
    K->num_props = num_props;
    K->var_offset = 0;

    /* Create OBDD manager with 2n variables: n current + n next */
    K->mgr = obdd_mgr_create(2 * num_state_bits);
    if (!K->mgr) { free(K); return NULL; }

    /* Initial states: empty (FALSE) */
    K->init_states = OBDD_FALSE;

    /* Transition relation: empty (FALSE) */
    K->trans_rel = OBDD_FALSE;

    /* Labeling: allocate arrays */
    K->labels_capacity = num_props > 0 ? num_props : 1;
    K->prop_names = (char **)calloc((size_t)K->labels_capacity, sizeof(char *));
    K->labels     = (obdd_ref_t *)calloc((size_t)K->labels_capacity, sizeof(obdd_ref_t));
    if (!K->prop_names || !K->labels) {
        free(K->prop_names); free(K->labels);
        obdd_mgr_destroy(K->mgr); free(K);
        return NULL;
    }

    for (int32_t i = 0; i < K->labels_capacity; i++) {
        K->labels[i] = OBDD_FALSE;
        K->prop_names[i] = NULL;
    }

    return K;
}

/* ============================================================
 * kripke_destroy - Free a Kripke structure
 * ============================================================ */

void kripke_destroy(kripke_t *K) {
    if (!K) return;
    for (int32_t i = 0; i < K->labels_capacity; i++) {
        free(K->prop_names[i]);
    }
    free(K->prop_names);
    free(K->labels);
    obdd_mgr_destroy(K->mgr);
    free(K);
}

/* ============================================================
 * kripke_encode_state - Build OBDD for a single state
 *
 * L3: Creates the characteristic function chi_{state}(x) representing
 * the singleton set {state}. This is the conjunction of literals:
 *   AND_{i=0}^{n-1} (x_i == bit_i(state))
 *
 * Each bit: if bit i is 1, use x_i; else use NOT x_i.
 *
 * Time: O(n) to build the conjunction of n literals
 * ============================================================ */

obdd_ref_t kripke_encode_state(const kripke_t *K, uint64_t state_bits) {
    if (!K) return OBDD_FALSE;

    obdd_ref_t result = OBDD_TRUE;
    for (int32_t i = 0; i < K->num_state_bits; i++) {
        uint64_t bit = (state_bits >> (uint64_t)i) & 1ULL;
        obdd_ref_t lit = obdd_var(K->mgr, (obdd_var_t)i);
        if (!bit) lit = obdd_not(K->mgr, lit);
        result = obdd_and(K->mgr, result, lit);
    }
    return result;
}

/* ============================================================
 * kripke_encode_prime_state - Build OBDD for a single primed state
 *
 * Uses variables n..2n-1 (the primed copies) to encode the state.
 * Used in transition relation construction and image computation.
 *
 * Time: O(n)
 * ============================================================ */

obdd_ref_t kripke_encode_prime_state(const kripke_t *K, uint64_t state_bits) {
    if (!K) return OBDD_FALSE;

    obdd_ref_t result = OBDD_TRUE;
    for (int32_t i = 0; i < K->num_state_bits; i++) {
        uint64_t bit = (state_bits >> (uint64_t)i) & 1ULL;
        obdd_var_t pvar = (obdd_var_t)(K->num_state_bits + i);
        obdd_ref_t lit = obdd_var(K->mgr, pvar);
        if (!bit) lit = obdd_not(K->mgr, lit);
        result = obdd_and(K->mgr, result, lit);
    }
    return result;
}

/* ============================================================
 * kripke_add_initial_state - Add a state to S0
 *
 * Updates: S0(x) := S0(x) OR chi_{state}(x)
 *
 * Time: O(n) for state encoding + OBDD OR
 * ============================================================ */

void kripke_add_initial_state(kripke_t *K, uint64_t state_bits) {
    if (!K) return;
    obdd_ref_t state_enc = kripke_encode_state(K, state_bits);
    K->init_states = obdd_or(K->mgr, K->init_states, state_enc);
}

/* ============================================================
 * kripke_add_transition - Add a transition (src -> dst)
 *
 * Updates: R(x, xprime) := R(x, xprime) OR
 *            (chi_{src}(x) AND chi_{dst}(xprime))
 *
 * This builds the characteristic function of each transition as
 * the conjunction of source state encoding (current vars) and
 * destination state encoding (primed vars).
 *
 * Time: O(n) for state encoding + OBDD AND/OR
 * ============================================================ */

void kripke_add_transition(kripke_t *K, uint64_t src, uint64_t dst) {
    if (!K) return;

    obdd_ref_t src_enc = kripke_encode_state(K, src);
    obdd_ref_t dst_enc = kripke_encode_prime_state(K, dst);
    obdd_ref_t trans   = obdd_and(K->mgr, src_enc, dst_enc);

    K->trans_rel = obdd_or(K->mgr, K->trans_rel, trans);
}

/* ============================================================
 * kripke_set_label - Set labeling for an atomic proposition
 *
 * The label is the characteristic function of states where
 * proposition p holds: label[p](x) = chi_{states satisfying p}(x)
 *
 * Time: OBDD assignment (O(1) pointer copy in this implementation)
 * ============================================================ */

void kripke_set_label(kripke_t *K, int32_t prop, const char *prop_name,
                      obdd_ref_t states) {
    if (!K || prop < 0 || prop >= K->labels_capacity) return;

    free(K->prop_names[prop]);
    K->prop_names[prop] = prop_name ? strdup(prop_name) : NULL;
    K->labels[prop] = states;
}

/* ============================================================
 * kripke_set_label_mask - Set labeling from a bit mask
 *
 * For each state i in [0, num_states), if bit i of mask is set,
 * the proposition holds in that state. Builds the characteristic
 * function by OR-ing individual state encodings.
 *
 * Time: O(num_states * n) for encoding + OBDD ORs
 * ============================================================ */

void kripke_set_label_mask(kripke_t *K, int32_t prop, const char *prop_name,
                           uint64_t mask, int32_t num_states) {
    if (!K || prop < 0 || prop >= K->labels_capacity) return;

    free(K->prop_names[prop]);
    K->prop_names[prop] = prop_name ? strdup(prop_name) : NULL;

    obdd_ref_t result = OBDD_FALSE;
    for (int32_t i = 0; i < num_states && i < 64; i++) {
        uint64_t bit = (mask >> (uint64_t)i) & 1ULL;
        if (bit) {
            obdd_ref_t state_enc = kripke_encode_state(K, (uint64_t)i);
            result = obdd_or(K->mgr, result, state_enc);
        }
    }
    K->labels[prop] = result;
}

/* ============================================================
 * kripke_state_union / intersect / complement
 *
 * Basic set operations on symbolically represented state sets.
 * Each is a direct OBDD boolean operation on the characteristic
 * functions.
 *
 * L3: Set operations via boolean connectives on characteristic
 * functions. For sets A, B subset S encoded as chi_A, chi_B:
 *   chi_{A union B}        = chi_A OR chi_B
 *   chi_{A intersect B}    = chi_A AND chi_B
 *   chi_{complement(A)}    = NOT chi_A
 * ============================================================ */

obdd_ref_t kripke_state_union(const kripke_t *K, obdd_ref_t A, obdd_ref_t B) {
    if (!K) return OBDD_FALSE;
    return obdd_or(K->mgr, A, B);
}

obdd_ref_t kripke_state_intersect(const kripke_t *K, obdd_ref_t A, obdd_ref_t B) {
    if (!K) return OBDD_FALSE;
    return obdd_and(K->mgr, A, B);
}

obdd_ref_t kripke_state_complement(const kripke_t *K, obdd_ref_t A) {
    if (!K) return OBDD_FALSE;
    return obdd_not(K->mgr, A);
}

/* ============================================================
 * kripke_image - Forward image computation
 *
 * Image(A, R) = { t | EXISTS s in A : (s, t) in R }
 *
 * Symbolically:
 *   chi_{Image(A)}(xprime) = EXISTS x_0,...,x_{n-1}.
 *       (chi_A(x_0,...,x_{n-1}) AND R(x_0,...,x_{n-1}, xprime_0,...,xprime_{n-1}))
 *
 * This EXISTS quantifies out all current-state variables,
 * leaving a function of primed variables only.
 *
 * This is THE fundamental operation for symbolic forward reachability
 * and the basis of all CTL operators involving EX.
 *
 * Algorithm:
 *   1. Compute chi_A(x) AND R(x, xprime)
 *   2. Existentially quantify each current-state var x_i
 *   3. Result is a function over xprime variables only
 *
 * Time: O(n * |chi_A| * |R|) since n successive EXISTS operations
 * ============================================================ */

obdd_ref_t kripke_image(const kripke_t *K, obdd_ref_t A) {
    if (!K) return OBDD_FALSE;

    /* Step 1: Intersect state set A with transition relation */
    obdd_ref_t conj = obdd_and(K->mgr, A, K->trans_rel);

    /* Step 2: Existentially quantify all current-state variables */
    for (int32_t i = 0; i < K->num_state_bits; i++) {
        conj = obdd_exists(K->mgr, conj, (obdd_var_t)i);
    }

    /* Result is over primed variables only */
    return conj;
}

/* ============================================================
 * kripke_preimage - Preimage computation
 *
 * PreImage(B, R) = { s | EXISTS t in B : (s, t) in R }
 *
 * Symbolically:
 *   chi_{PreImage(B)}(x) = EXISTS xprime_0,...,xprime_{n-1}.
 *       (chi_B(xprime_0,...,xprime_{n-1}) AND
 *        R(x_0,...,x_{n-1}, xprime_0,...,xprime_{n-1}))
 *
 * This quantifies out all primed variables.
 *
 * First, we must rename B from current-variable space to
 * primed-variable space, then intersect with R, then
 * quantify out the primed variables.
 *
 * Time: O(n * |chi_B| * |R|) as n successive EXISTS operations
 * ============================================================ */

obdd_ref_t kripke_preimage(const kripke_t *K, obdd_ref_t B) {
    if (!K) return OBDD_FALSE;

    /* Step 1: Rename B to primed variable space */
    obdd_ref_t B_prime = kripke_rename_to_prime(K, B);

    /* Step 2: Intersect with transition relation */
    obdd_ref_t conj = obdd_and(K->mgr, B_prime, K->trans_rel);

    /* Step 3: Existentially quantify all primed variables */
    for (int32_t i = 0; i < K->num_state_bits; i++) {
        obdd_var_t pvar = (obdd_var_t)(K->num_state_bits + i);
        conj = obdd_exists(K->mgr, conj, pvar);
    }

    return conj;
}

/* ============================================================
 * kripke_rename_to_prime - Variable renaming: x -> xprime
 *
 * L3: Rename each current-state variable x_i to the corresponding
 * primed variable xprime_i. This is implemented by composition:
 * for each i, substitute xprime_i for x_i in f.
 *
 * In fixpoint iterations, this allows us to convert the result
 * of an image computation (in primed variable space) back into
 * current-state variable space for the next iteration.
 *
 * Time: O(n * |f|) for n successive compositions
 * ============================================================ */

obdd_ref_t kripke_rename_to_prime(const kripke_t *K, obdd_ref_t f) {
    if (!K) return OBDD_FALSE;

    obdd_ref_t result = f;
    /* For each current-state variable, substitute with primed copy */
    for (int32_t i = 0; i < K->num_state_bits; i++) {
        obdd_var_t prime_var = (obdd_var_t)(K->num_state_bits + i);
        obdd_ref_t prime_lit = obdd_var(K->mgr, prime_var);
        result = obdd_compose(K->mgr, result, (obdd_var_t)i, prime_lit);
    }
    return result;
}

/* ============================================================
 * kripke_rename_to_unprime - Variable renaming: xprime -> x
 *
 * Reverse of above: substitute each primed variable with the
 * corresponding current-state variable.
 * ============================================================ */

obdd_ref_t kripke_rename_to_unprime(const kripke_t *K, obdd_ref_t f) {
    if (!K) return OBDD_FALSE;

    obdd_ref_t result = f;
    for (int32_t i = 0; i < K->num_state_bits; i++) {
        obdd_var_t prime_var = (obdd_var_t)(K->num_state_bits + i);
        obdd_ref_t curr_lit  = obdd_var(K->mgr, (obdd_var_t)i);
        result = obdd_compose(K->mgr, result, prime_var, curr_lit);
    }
    return result;
}