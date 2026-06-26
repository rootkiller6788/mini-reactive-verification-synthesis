/**
 * reach.c - Reachability Analysis for Symbolic Model Checking
 *
 * Implements forward and backward reachability algorithms using
 * OBDD-based symbolic state exploration.
 *
 * Knowledge Coverage:
 *   L1: Reachable state set: Reach(S0) = fixpoint of Image
 *   L2: Forward reachability: BFS from initial states
 *       Backward reachability: reverse BFS from target states
 *   L5: Symbolic breadth-first search using OBDDs
 *       On-the-fly invariant checking
 *   L6: Safety verification: check if bad states are reachable
 *       Deadlock detection: states with no outgoing transitions
 *   L7: Protocol verification, hardware reachability analysis
 *
 * References:
 *   Burch, J.R., Clarke, E.M., McMillan, K.L., Dill, D.L., & Hwang, L.J. (1992).
 *     "Symbolic model checking: 10^20 states and beyond."
 *     Information and Computation, 98(2), 142-170.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "obdd.h"
#include "model.h"

/* ============================================================
 * forward_reachability - Compute all forward-reachable states
 *
 * L5: Symbolic BFS from initial states. Iterates the forward
 * image operator until convergence:
 *   R_0 = S0
 *   R_{i+1} = R_i OR Image(R_i, R)
 *   Converge when R_{i+1} == R_i
 *
 * This is a least fixpoint computation:
 *   mu Z. S0 OR Image(Z, R)
 *
 * The resulting OBDD represents the characteristic function
 * of all states reachable from any initial state via any
 * number of transitions.
 *
 * Complexity: O(diameter * BDD_ops) where diameter is the
 * longest shortest path in the state graph.
 *
 * @param max_steps  Maximum BFS steps (safety bound)
 * @param steps_out  Output: actual number of steps taken
 * @return           Reachable state set as OBDD
 * ============================================================ */

obdd_ref_t forward_reachability(const kripke_t *K,
                                 int32_t max_steps,
                                 int32_t *steps_out) {
    if (!K) {
        if (steps_out) *steps_out = 0;
        return OBDD_FALSE;
    }

    obdd_ref_t reached = K->init_states;
    obdd_ref_t frontier = K->init_states;
    int32_t step = 0;

    for (step = 0; step < max_steps; step++) {
        /* Compute image of current frontier */
        obdd_ref_t image = kripke_image(K, frontier);

        /* New states = image minus already-reached states */
        obdd_ref_t not_reached = obdd_not(K->mgr, reached);
        obdd_ref_t new_states = obdd_and(K->mgr, image, not_reached);

        /* Check if new states exist */
        if (!obdd_is_satisfiable(K->mgr, new_states)) {
            /* Converged: no new states reachable */
            if (steps_out) *steps_out = step + 1;
            return reached;
        }

        /* Add new states to reached set */
        reached = obdd_or(K->mgr, reached, new_states);
        frontier = new_states;
    }

    if (steps_out) *steps_out = step;
    return reached;
}

/* ============================================================
 * backward_reachability - Compute states that can reach target
 *
 * L5: Reverse BFS from target states using preimage:
 *   B_0 = target
 *   B_{i+1} = B_i OR PreImage(B_i, R)
 *
 * This computes all states from which the target is reachable.
 * Least fixpoint: mu Z. target OR PreImage(Z, R)
 *
 * @param target     Characteristic function of target states
 * @param max_steps  Maximum BFS steps
 * @param steps_out  Output: actual steps taken
 * @return           States that can reach target
 * ============================================================ */

obdd_ref_t backward_reachability(const kripke_t *K,
                                  obdd_ref_t target,
                                  int32_t max_steps,
                                  int32_t *steps_out) {
    if (!K) {
        if (steps_out) *steps_out = 0;
        return OBDD_FALSE;
    }

    obdd_ref_t reached = target;
    obdd_ref_t frontier = target;
    int32_t step = 0;

    for (step = 0; step < max_steps; step++) {
        obdd_ref_t pre = kripke_preimage(K, frontier);
        obdd_ref_t not_reached = obdd_not(K->mgr, reached);
        obdd_ref_t new_states = obdd_and(K->mgr, pre, not_reached);

        if (!obdd_is_satisfiable(K->mgr, new_states)) {
            if (steps_out) *steps_out = step + 1;
            return reached;
        }

        reached = obdd_or(K->mgr, reached, new_states);
        frontier = new_states;
    }

    if (steps_out) *steps_out = step;
    return reached;
}

/* ============================================================
 * invariant_check - Check if property holds in all reachable states
 *
 * L6: Safety verification. Given a property P (as characteristic
 * function of states where P holds), checks whether all reachable
 * states satisfy P.
 *
 * Algorithm:
 *   1. Compute reachable states R
 *   2. Check: R AND NOT P == FALSE
 *      (i.e., no reachable state violates P)
 *
 * This is equivalent to checking AG P on the Kripke structure.
 *
 * @param property  Characteristic function of states satisfying P
 * @return          1 if invariant holds, 0 if violated
 * ============================================================ */

int invariant_check(const kripke_t *K, obdd_ref_t property) {
    if (!K) return 0;

    /* Compute all reachable states */
    int32_t steps;
    obdd_ref_t reachable = forward_reachability(K, 10000, &steps);

    /* Check if any reachable state violates the property */
    obdd_ref_t not_prop = obdd_not(K->mgr, property);
    obdd_ref_t violation = obdd_and(K->mgr, reachable, not_prop);

    return obdd_is_tautology(K->mgr, obdd_not(K->mgr, violation));
}

/* ============================================================
 * deadlock_check - Detect states with no outgoing transitions
 *
 * L6: A state s is a deadlock if there is no t such that
 * R(s, t) holds. Symbolically:
 *   DeadlockStates(x) = FORALL xprime. NOT R(x, xprime)
 *                     = NOT EXISTS xprime. R(x, xprime)
 *                     = NOT Image({x}, R) is non-empty
 *
 * Computes the set of states that have no successors.
 *
 * @return  Characteristic function of deadlock states
 * ============================================================ */

obdd_ref_t deadlock_check(const kripke_t *K) {
    if (!K) return OBDD_FALSE;

    /* For each state, check if it has at least one successor.
     * A state has a successor iff EXISTS xprime. R(x, xprime).
     *
     * Compute: HasSuccessor(x) = EXISTS xprime_0..xprime_{n-1}. R(x, xprime)
     */
    obdd_ref_t has_succ = K->trans_rel;

    /* Existentially quantify all primed variables */
    for (int32_t i = 0; i < K->num_state_bits; i++) {
        obdd_var_t pvar = (obdd_var_t)(K->num_state_bits + i);
        has_succ = obdd_exists(K->mgr, has_succ, pvar);
    }

    /* Deadlock states = NOT HasSuccessor(x) */
    obdd_ref_t deadlocks = obdd_not(K->mgr, has_succ);

    /* Intersect with reachable states for meaningful results */
    int32_t steps;
    obdd_ref_t reachable = forward_reachability(K, 10000, &steps);
    deadlocks = obdd_and(K->mgr, deadlocks, reachable);

    return deadlocks;
}

/* ============================================================
 * reachable_distance - Compute distance of each state from S0
 *
 * L5: BFS layering. For each state, compute the minimum number
 * of transitions from any initial state.
 *
 * This is a more fine-grained reachability analysis used in
 * shortest counterexample generation.
 *
 * Note: This function returns an approximation only (the BFS
 * frontier at each step, not per-state distances). Full distance
 * computation requires storing per-state distances, which is
 * not directly supported by OBDDs without additional encoding.
 *
 * @return  OBDD for states reachable within exactly steps steps
 *          (the BFS frontier at the given depth)
 * ============================================================ */

obdd_ref_t reachable_distance(const kripke_t *K, int32_t steps) {
    if (!K || steps < 0) return OBDD_FALSE;

    if (steps == 0) return K->init_states;

    obdd_ref_t frontier = K->init_states;
    obdd_ref_t all_reached = K->init_states;

    for (int32_t s = 0; s < steps; s++) {
        obdd_ref_t image = kripke_image(K, frontier);
        obdd_ref_t not_reached = obdd_not(K->mgr, all_reached);
        frontier = obdd_and(K->mgr, image, not_reached);

        if (!obdd_is_satisfiable(K->mgr, frontier)) {
            return OBDD_FALSE; /* No new states at this distance */
        }

        all_reached = obdd_or(K->mgr, all_reached, frontier);
    }

    return frontier; /* States first reached at exactly 'steps' steps */
}