/**
 * fixpoint.h — Fixpoint Computation Engine for Symbolic Model Checking
 *
 * Provides least and greatest fixpoint computation over the lattice
 * of state sets (2^S, subseteq), using OBDD operations.
 *
 * Knowledge Coverage:
 *   L1: Monotone function tau: 2^S -> 2^S over complete lattice
 *   L2: Knaster-Tarski theorem: every monotone function on a complete
 *       lattice has a least and greatest fixpoint
 *   L3: State set lattice: (2^S, subset, union, intersection, complement)
 *       Lfp iteration: Z_0 = empty, Z_{i+1} = tau(Z_i)
 *       Gfp iteration: Z_0 = S, Z_{i+1} = tau(Z_i)
 *   L4: Termination guarantee: finite lattice ensures convergence in
 *       at most |S| iterations
 *   L5: Symbolic fixpoint with OBDD-based set operations
 *       Early termination: Z_{i+1} = Z_i implies fixpoint reached
 *
 * References:
 *   Tarski, A. (1955). "A lattice-theoretical fixpoint theorem and its
 *     applications." Pacific J. Math, 5(2), 285-309.
 *   Clarke, E.M., Grumberg, O., & Peled, D.A. (1999). "Model Checking."
 *     MIT Press. Section 6.3: Fixpoint Characterizations.
 */

#ifndef FIXPOINT_H
#define FIXPOINT_H

#include <stdint.h>
#include <stdbool.h>
#include "obdd.h"
#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * L1: Fixpoint Function Signature
 * ============================================================ */

/**
 * fixpoint_transformer_t — A monotone function tau: 2^S -> 2^S.
 *
 * The function takes a state set Z (as OBDD) and the Kripke structure,
 * plus optional context, and returns tau(Z).
 *
 * Context pointer allows passing parameters (e.g., a CTL subformula
 * result set) to the transformer.
 */
typedef obdd_ref_t (*fixpoint_transformer_t)(const kripke_t *K,
                                              obdd_ref_t Z,
                                              void *context);

/* ============================================================
 * L5: Fixpoint Computation
 * ============================================================ */

/**
 * fixpoint_least — Compute the least fixpoint of tau.
 *
 * mu Z. tau(Z)
 *
 * Algorithm:
 *   Z := empty_set
 *   repeat:
 *     Z' := tau(Z)
 *     if Z' == Z: return Z
 *     Z := Z'
 *
 * Tarski's theorem guarantees termination for monotone tau on
 * finite lattice. Complexity: at most |S| iterations.
 *
 * @param K          Kripke structure
 * @param tau        Monotone transformer
 * @param context    Arbitrary context passed to tau
 * @param max_iter   Maximum iterations (safety bound)
 * @param iter_out   Output: actual number of iterations performed
 * @return           Least fixpoint as OBDD
 */
obdd_ref_t fixpoint_least(const kripke_t *K,
                           fixpoint_transformer_t tau,
                           void *context,
                           int32_t max_iter,
                           int32_t *iter_out);

/**
 * fixpoint_greatest — Compute the greatest fixpoint of tau.
 *
 * nu Z. tau(Z)
 *
 * Algorithm:
 *   Z := S (all states)
 *   repeat:
 *     Z' := tau(Z)
 *     if Z' == Z: return Z
 *     Z := Z'
 *
 * @param K          Kripke structure
 * @param tau        Monotone transformer
 * @param context    Arbitrary context passed to tau
 * @Param max_iter   Maximum iterations (safety bound)
 * @param iter_out   Output: actual number of iterations performed
 * @return           Greatest fixpoint as OBDD
 */
obdd_ref_t fixpoint_greatest(const kripke_t *K,
                              fixpoint_transformer_t tau,
                              void *context,
                              int32_t max_iter,
                              int32_t *iter_out);

/**
 * fixpoint_bounded_mu — Bounded least fixpoint: mu^k Z. tau(Z).
 *
 * Iterates exactly k times regardless of convergence.
 *
 * @return  tau^k(empty_set)
 */
obdd_ref_t fixpoint_bounded_mu(const kripke_t *K,
                                fixpoint_transformer_t tau,
                                void *context,
                                int32_t k);

/**
 * fixpoint_bounded_nu — Bounded greatest fixpoint: nu^k Z. tau(Z).
 *
 * Iterates exactly k times from S.
 *
 * @return  tau^k(S)
 */
obdd_ref_t fixpoint_bounded_nu(const kripke_t *K,
                                fixpoint_transformer_t tau,
                                void *context,
                                int32_t k);

/**
 * fixpoint_is_monotone_check — Verify monotonicity by sampling.
 */
bool fixpoint_is_monotone_check(const kripke_t *K,
                                 fixpoint_transformer_t tau,
                                 void *context,
                                 int32_t num_samples);

#ifdef __cplusplus
}
#endif

#endif /* FIXPOINT_H */