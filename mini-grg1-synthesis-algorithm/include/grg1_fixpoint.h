/**
 * grg1_fixpoint.h — Fixpoint computation for GR(1) synthesis
 *
 * The GR(1) synthesis algorithm relies heavily on fixpoint iteration
 * over the lattice of state sets (P(S), ⊆). The core computation is:
 *
 *   For each system justice guarantee gⱼ:
 *     Zⱼ = νX. μY. (gⱼ ∨ CPreₛ(Z_{j-1})) ∧ CPreₑ(Y)
 *
 * This is a nested fixpoint: the inner μ (least fixpoint) computes
 * states from which gⱼ is reachable; the outer ν (greatest fixpoint)
 * ensures this can be repeated infinitely often.
 *
 * The algorithm implements the fixpoint characterization of
 * Streett game solutions via the ranking construction.
 *
 * References:
 *   Emerson, Clarke (1980). "Characterizing Correctness Properties of
 *     Parallel Programs Using Fixpoints." ICALP.
 *   Emerson, Lei (1986). "Efficient Model Checking in Fragments of the
 *     Propositional Mu-Calculus." LICS.
 *   Piterman, Pnueli, Sa'ar (2006). §5 — Fixpoint Algorithm.
 *   Cousot, Cousot (1977). "Abstract Interpretation: A Unified Lattice
 *     Model." POPL. — lattice-theoretic foundations.
 *
 * Knowledge coverage:
 *   L1: Fixpoint definition (least and greatest)
 *   L2: Lattice of state sets, monotonicity
 *   L3: Complete lattice (P(S), ⊆), Knaster-Tarski theorem
 *   L4: Soundness and completeness of fixpoint algorithm
 *   L5: Nested fixpoint computation, acceleration techniques
 */

#ifndef GRG1_FIXPOINT_H
#define GRG1_FIXPOINT_H

#include "grg1_types.h"
#include "grg1_game.h"

/* ---------------------------------------------------------------------------
 * L1/L3: Fixpoint fundamentals
 * ---------------------------------------------------------------------------
 * In the lattice (P(S), ⊆) of subsets of S ordered by inclusion:
 *   - Least fixpoint μF = ∩{X | F(X) ⊆ X} = ∪_{k≥0} Fᵏ(∅)
 *   - Greatest fixpoint νF = ∪{X | X ⊆ F(X)} = ∩_{k≥0} Fᵏ(S)
 *
 * For monotone F: P(S) → P(S), both fixpoints exist by the
 * Knaster-Tarski theorem.
 */

/**
 * Compute the least fixpoint (LFP) of a monotone transformer F.
 *
 * Algorithm: Kleene iteration starting from empty set.
 *   X₀ = ∅
 *   X_{i+1} = F(X_i)
 *   Stop when X_{i+1} = X_i
 *
 * @param initial  Starting region (typically empty set for LFP)
 * @param transformer  Function pointer: F: region → region
 * @param context  Opaque context passed to transformer
 * @param result  Output: μX. F(X), allocated by caller
 * @param max_iter  Maximum iterations (safety bound)
 * @param iterations  Output: number of iterations performed
 * @return  true if converged, false if max_iter reached
 *
 * Complexity: O(max_iter × complexity(F))
 * Theorem: Knaster-Tarski — μF exists in any complete lattice for monotone F
 * Reference: Kleene (1952) — first recursion theorem
 */
bool grg1_fixpoint_lfp(const grg1_region_t* initial,
                        void (*transformer)(const grg1_region_t* in,
                                             grg1_region_t* out,
                                             void* context),
                        void* context,
                        grg1_region_t* result,
                        int max_iter,
                        int* iterations);

/**
 * Compute the greatest fixpoint (GFP) of a monotone transformer F.
 *
 * Algorithm: Kleene iteration starting from the full set.
 *   X₀ = S
 *   X_{i+1} = F(X_i)
 *   Stop when X_{i+1} = X_i
 *
 * @param initial  Starting region (typically full set for GFP)
 * @param transformer  Function pointer
 * @param context  Opaque context
 * @param result  Output: νX. F(X), allocated by caller
 * @param max_iter  Maximum iterations
 * @param iterations  Output: number of iterations performed
 * @return  true if converged
 *
 * Complexity: O(max_iter × complexity(F))
 * Theorem: The GFP can be computed as the complement of the LFP of the
 *          dual function: νF = S \ μ(λX. S \ F(S \ X))
 */
bool grg1_fixpoint_gfp(const grg1_region_t* initial,
                        void (*transformer)(const grg1_region_t* in,
                                             grg1_region_t* out,
                                             void* context),
                        void* context,
                        grg1_region_t* result,
                        int max_iter,
                        int* iterations);

/**
 * Compute a nested fixpoint: νX. μY. F(X, Y)
 *
 * This is the key operation for GR(1) synthesis.
 * The outer GFP (νX) iterates over the system's ability to repeatedly
 * satisfy justice guarantees. The inner LFP (μY) computes reachability
 * under environment constraints.
 *
 * Algorithm:
 *   X = S (full set)
 *   repeat:
 *     X_prev = X
 *     Y = ∅
 *     repeat: Y = F(X, Y) until Y stabilizes (inner LFP)
 *     X = Y
 *   until X = X_prev (outer GFP)
 *
 * @param inner_transformer  F(X, Y) → output; X from outer, Y current inner
 * @param context  Opaque context for inner_transformer
 * @param result  Output: νX. μY. F(X, Y)
 * @param outer_max_iter  Max outer iterations
 * @param inner_max_iter  Max inner iterations per outer step
 * @param outer_iters  Output: outer iterations performed
 * @param total_inner_iters  Output: total inner iterations across all outer steps
 * @return  true if both fixpoints converged
 *
 * Complexity: O(outer_iters × inner_max_iter × complexity(F))
 * Reference: Emerson & Lei (1986) — nested fixpoint in μ-calculus
 *            Piterman et al. (2006) §5.1 — nested fixpoint for GR(1)
 */
bool grg1_fixpoint_nested(
    const grg1_region_t* full_set,
    void (*inner_transformer)(const grg1_region_t* outer_approx,
                               const grg1_region_t* inner_approx,
                               grg1_region_t* out,
                               void* context),
    void* context,
    grg1_region_t* result,
    int outer_max_iter,
    int inner_max_iter,
    int* outer_iters,
    int* total_inner_iters);

/* ---------------------------------------------------------------------------
 * L5: Fixpoint acceleration — widening and narrowing
 * ---------------------------------------------------------------------------
 * For large state spaces, naive fixpoint iteration can be slow.
 * Widening overapproximates to accelerate convergence (sacrifices
 * precision temporarily); narrowing refines back to the exact fixpoint.
 *
 * Based on abstract interpretation theory (Cousot & Cousot, 1977).
 * These techniques are applicable when the lattice is infinite or
 * very large, but we include them here for large finite lattices.
 */

/**
 * Widen two regions using a widening operator ∇.
 *
 * Widening ensures termination by extrapolating from a finite number
 * of iterations. We implement a simple union-based widening:
 *   X ∇ Y = X ∪ Y
 *
 * More sophisticated widening (e.g., threshold-based) can be used
 * for specific domains. This is provided as a template for extension.
 *
 * @param prev  Previous iteration result
 * @param current  Current iteration result
 * @param result  Output: prev ∇ current
 *
 * Complexity: O(num_states / 64)
 * Reference: Cousot & Cousot (1977) — widening operator
 */
void grg1_fixpoint_widen(const grg1_region_t* prev,
                          const grg1_region_t* current,
                          grg1_region_t* result);

/**
 * Apply narrowing to refine an overapproximated fixpoint.
 *
 * Narrowing: starting from a post-fixpoint (F(X) ⊆ X), iterate F
 * to converge to the least fixpoint. For finite lattices, this is
 * just standard iteration.
 *
 * @param overapprox  Starting overapproximation (post-fixpoint)
 * @param transformer  The monotone function F
 * @param context  Context for transformer
 * @param result  Output: refined fixpoint
 * @param max_iter  Maximum iterations
 * @param iterations  Output: iterations performed
 * @return  true if converged
 *
 * Complexity: O(max_iter × complexity(F))
 * Reference: Cousot & Cousot (1979) — narrowing operator
 */
bool grg1_fixpoint_narrow(const grg1_region_t* overapprox,
                           void (*transformer)(const grg1_region_t* in,
                                                grg1_region_t* out,
                                                void* context),
                           void* context,
                           grg1_region_t* result,
                           int max_iter,
                           int* iterations);

/* ---------------------------------------------------------------------------
 * L5: Worklist-based fixpoint computation
 * ---------------------------------------------------------------------------
 * Instead of recomputing F on the entire state space each iteration,
 * a worklist algorithm tracks which states may have changed and only
 * recomputes for those states. This can dramatically reduce runtime
 * for large but sparse game graphs.
 */

/**
 * Context structure for worklist-based fixpoint computation.
 * The worklist tracks state IDs whose status in the fixpoint may change.
 */
typedef struct {
    int* worklist;                     /**< Queue of state IDs to process */
    int head;                          /**< Dequeue position */
    int tail;                          /**< Enqueue position */
    int capacity;                      /**< Maximum queue size */
    bool* in_queue;                    /**< Quick check: is state in worklist */
    const grg1_game_t* game;           /**< Reference to game (read-only) */
} grg1_worklist_t;

/**
 * Allocate and initialize a worklist for fixpoint computation.
 *
 * Complexity: O(num_states)
 */
grg1_worklist_t* grg1_worklist_alloc(int num_states);

/** Free a worklist */
void grg1_worklist_free(grg1_worklist_t* wl);

/** Add a state to the worklist (no-op if already present) */
void grg1_worklist_push(grg1_worklist_t* wl, int state_id);

/** Remove and return the next state from the worklist, or -1 if empty */
int grg1_worklist_pop(grg1_worklist_t* wl);

/** Check if the worklist is empty */
bool grg1_worklist_is_empty(const grg1_worklist_t* wl);

/**
 * Compute the least fixpoint using a worklist algorithm.
 *
 * Instead of iterating globally, only states whose predecessors'
 * status changed are reprocessed. This is asymptotically faster
 * when the transformer has local dependencies.
 *
 * @param game  The game arena
 * @param initial  Seed states for the worklist
 * @param decide_fn  Decision function for individual states
 * @param context  Context for decide_fn
 * @param result  Output: fixpoint region
 * @param iterations  Output: total state evaluations
 *
 * Complexity: O(|S| × log|S|) typical, O(|S|²) worst case
 * Reference: Kildall (1973) — worklist algorithm for dataflow analysis
 */
void grg1_fixpoint_worklist_lfp(
    const grg1_game_t* game,
    const grg1_region_t* initial,
    bool (*decide_fn)(int state_id, const grg1_region_t* current,
                       const grg1_game_t* game, void* context),
    void* context,
    grg1_region_t* result,
    int* iterations);

/* ---------------------------------------------------------------------------
 * L2: Fixpoint utilities
 * ---------------------------------------------------------------------------
 */

/**
 * Check if a region is a fixpoint of a transformer F.
 *
 * @param region  Candidate fixpoint
 * @param transformer  The function F
 * @param context  Context for transformer
 * @return  true if F(region) = region
 *
 * Complexity: O(complexity(F) + |S|)
 * Theorem: X is a fixpoint iff F(X) = X
 */
bool grg1_fixpoint_is_fixpoint(const grg1_region_t* region,
                                void (*transformer)(const grg1_region_t* in,
                                                     grg1_region_t* out,
                                                     void* context),
                                void* context);

/**
 * Check if a region is a post-fixpoint: F(X) ⊆ X.
 * Post-fixpoints overapproximate the GFP.
 *
 * Complexity: O(complexity(F) + |S|)
 */
bool grg1_fixpoint_is_postfixpoint(const grg1_region_t* region,
                                    void (*transformer)(const grg1_region_t* in,
                                                         grg1_region_t* out,
                                                         void* context),
                                    void* context);

/**
 * Check if a region is a pre-fixpoint: X ⊆ F(X).
 * Pre-fixpoints underapproximate the LFP.
 *
 * Complexity: O(complexity(F) + |S|)
 */
bool grg1_fixpoint_is_prefixpoint(const grg1_region_t* region,
                                   void (*transformer)(const grg1_region_t* in,
                                                        grg1_region_t* out,
                                                        void* context),
                                   void* context);

/**
 * Compute the dual transformer of F: G(X) = S \ F(S \ X).
 * If F is monotone, G is also monotone and νF = μG.
 *
 * @param transformer  Original F
 * @param context  Context for F
 * @param full_set  The complete state set S
 * @param result  Output: G(S \ X) applied to complement of X
 *
 * Complexity: O(complexity(F) + |S|)
 * Theorem: De Morgan duality for fixpoints in complemented lattices
 */
void grg1_fixpoint_dual_transform(
    const grg1_region_t* X,
    void (*transformer)(const grg1_region_t* in, grg1_region_t* out,
                         void* context),
    void* context,
    const grg1_region_t* full_set,
    grg1_region_t* result);

#endif /* GRG1_FIXPOINT_H */
