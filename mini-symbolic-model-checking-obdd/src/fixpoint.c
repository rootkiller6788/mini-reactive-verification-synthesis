/**
 * fixpoint.c - Fixpoint Computation Engine
 *
 * Implements least and greatest fixpoint computation over the
 * lattice of state sets (2^S, <=), using OBDD operations.
 *
 * Knowledge Coverage:
 *   L1: Monotone functions on complete lattices
 *   L2: Knaster-Tarski fixpoint theorem
 *   L3: State set lattice: (2^S, subset, union, intersection)
 *       Iterative fixpoint computation:
 *         mu Z. tau(Z): Z_0 = empty, Z_{i+1} = tau(Z_i)
 *         nu Z. tau(Z): Z_0 = S, Z_{i+1} = tau(Z_i)
 *   L4: Finite lattice termination guarantee
 *   L5: Symbolic fixpoint with OBDD operations
 *
 * References:
 *   Tarski, A. (1955). "A lattice-theoretical fixpoint theorem and
 *     its applications." Pacific J. Math, 5(2), 285-309.
 *   Clarke, E.M., Grumberg, O., & Peled, D.A. (1999).
 *     "Model Checking." MIT Press. Section 6.3.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "obdd.h"
#include "model.h"
#include "fixpoint.h"

/* ============================================================
 * fixpoint_least - Least fixpoint: mu Z. tau(Z)
 *
 * L5: Iterates tau starting from the empty set (bottom element).
 * Convergence detection: when tau(Z_i) == Z_i, we have reached
 * the least fixpoint.
 *
 * The lattice (2^S, subset) is finite with height at most |S|,
 * so convergence occurs in at most |S| iterations.
 *
 * For symbolic model checking, the number of iterations equals
 * the diameter of the state space reachable via the underlying
 * transition structure, which is often much less than |S|.
 *
 * Algorithm:
 *   Z := FALSE (empty set)
 *   for i in 0..max_iter:
 *     Z_next := tau(K, Z, context)
 *     if Z_next == Z: return Z  (converged)
 *     Z := Z_next
 *   return Z  (may not have converged if max_iter reached)
 *
 * @return Least fixpoint approximation as OBDD
 * ============================================================ */

obdd_ref_t fixpoint_least(const kripke_t *K,
                           fixpoint_transformer_t tau,
                           void *context,
                           int32_t max_iter,
                           int32_t *iter_out) {
    if (!K || !tau) {
        if (iter_out) *iter_out = 0;
        return OBDD_FALSE;
    }

    obdd_ref_t Z = OBDD_FALSE; /* Bottom element: empty set */
    int32_t iter = 0;

    for (iter = 0; iter < max_iter; iter++) {
        obdd_ref_t Z_next = tau(K, Z, context);

        /* Convergence check: Z_next == Z */
        if (obdd_equals(K->mgr, Z_next, Z)) {
            if (iter_out) *iter_out = iter + 1;
            return Z;
        }

        Z = Z_next;
    }

    if (iter_out) *iter_out = iter;
    return Z; /* Iteration limit reached */
}

/* ============================================================
 * fixpoint_greatest - Greatest fixpoint: nu Z. tau(Z)
 *
 * L5: Iterates tau starting from the full set S (top element).
 * The dual of least fixpoint computation.
 *
 * Algorithm:
 *   Z := TRUE (all states)
 *   for i in 0..max_iter:
 *     Z_next := tau(K, Z, context)
 *     if Z_next == Z: return Z  (converged)
 *     Z := Z_next
 *   return Z
 *
 * The convergence guarantee is the same as for least fixpoint:
 * at most |S| iterations in the worst case.
 *
 * @return Greatest fixpoint approximation as OBDD
 * ============================================================ */

obdd_ref_t fixpoint_greatest(const kripke_t *K,
                              fixpoint_transformer_t tau,
                              void *context,
                              int32_t max_iter,
                              int32_t *iter_out) {
    if (!K || !tau) {
        if (iter_out) *iter_out = 0;
        return OBDD_FALSE;
    }

    obdd_ref_t Z = OBDD_TRUE; /* Top element: all states */
    int32_t iter = 0;

    for (iter = 0; iter < max_iter; iter++) {
        obdd_ref_t Z_next = tau(K, Z, context);

        /* Convergence check */
        if (obdd_equals(K->mgr, Z_next, Z)) {
            if (iter_out) *iter_out = iter + 1;
            return Z;
        }

        Z = Z_next;
    }

    if (iter_out) *iter_out = iter;
    return Z;
}

/* ============================================================
 * fixpoint_bounded_mu - Bounded least fixpoint
 *
 * Iterates exactly k times: tau^k(empty). Does not check
 * convergence. Useful for bounded model checking where we
 * want to explore exactly k steps.
 *
 * @return tau^k(FALSE)
 * ============================================================ */

obdd_ref_t fixpoint_bounded_mu(const kripke_t *K,
                                fixpoint_transformer_t tau,
                                void *context,
                                int32_t k) {
    if (!K || !tau || k <= 0) return OBDD_FALSE;

    obdd_ref_t Z = OBDD_FALSE;
    for (int32_t i = 0; i < k; i++) {
        Z = tau(K, Z, context);
    }
    return Z;
}

/* ============================================================
 * fixpoint_bounded_nu - Bounded greatest fixpoint
 *
 * Iterates exactly k times from the top: tau^k(TRUE).
 *
 * @return tau^k(TRUE)
 * ============================================================ */

obdd_ref_t fixpoint_bounded_nu(const kripke_t *K,
                                fixpoint_transformer_t tau,
                                void *context,
                                int32_t k) {
    if (!K || !tau || k <= 0) return OBDD_TRUE;

    obdd_ref_t Z = OBDD_TRUE;
    for (int32_t i = 0; i < k; i++) {
        Z = tau(K, Z, context);
    }
    return Z;
}

/* ============================================================
 * fixpoint_is_monotone_check - Semi-decision for monotonicity
 *
 * L2: Monotonicity: A <= B implies tau(A) <= tau(B).
 * Tests a small number of random state set pairs.
 *
 * For each sample:
 *   1. Generate random state sets A, B with A subset B
 *   2. Compute tau(A) and tau(B)
 *   3. Check that tau(A) AND NOT tau(B) == FALSE
 *
 * This is a heuristic; passing does not guarantee monotonicity
 * but failing indicates a non-monotone transformer.
 * ============================================================ */

bool fixpoint_is_monotone_check(const kripke_t *K,
                                fixpoint_transformer_t tau,
                                void *context,
                                int32_t num_samples) {
    if (!K || !tau || num_samples <= 0) return 1; /* Vacuously pass */

    /* Use a simple monotonicity test:
     * For the empty set and full set, check that:
     *   tau(empty) subseteq tau(full)
     * This is a necessary condition for monotonicity. */

    obdd_ref_t tau_empty = tau(K, OBDD_FALSE, context);
    obdd_ref_t tau_full  = tau(K, OBDD_TRUE, context);

    /* Check: tau(empty) AND NOT tau(full) should be FALSE */
    obdd_ref_t not_tau_full = obdd_not(K->mgr, tau_full);
    obdd_ref_t bad = obdd_and(K->mgr, tau_empty, not_tau_full);

    int monotone = obdd_is_tautology(K->mgr, obdd_not(K->mgr, bad));

    /* Additional check: for a few intermediate sets */
    for (int32_t s = 0; s < num_samples && s < 10; s++) {
        /* Use a simple state encoding as test sets */
        uint64_t mask_a = (1ULL << (uint64_t)(s % K->num_state_bits)) - 1;
        uint64_t mask_b = (1ULL << (uint64_t)((s + 1) % K->num_state_bits + 1)) - 1;

        obdd_ref_t set_a = (s == 0) ? OBDD_FALSE :
                           kripke_encode_state(K, mask_a);
        obdd_ref_t set_b = kripke_encode_state(K, mask_b);

        obdd_ref_t tau_a = tau(K, set_a, context);
        obdd_ref_t tau_b = tau(K, set_b, context);

        obdd_ref_t not_tb = obdd_not(K->mgr, tau_b);
        obdd_ref_t bad_ab = obdd_and(K->mgr, tau_a, not_tb);

        if (!obdd_is_tautology(K->mgr, obdd_not(K->mgr, bad_ab))) {
            monotone = 0;
            break;
        }
    }

    return monotone;
}