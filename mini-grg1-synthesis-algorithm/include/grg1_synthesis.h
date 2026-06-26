/**
 * grg1_synthesis.h — Main GR(1) synthesis algorithm
 *
 * The GR(1) synthesis algorithm determines whether there exists a
 * system strategy that satisfies the guarantees Gₛ under the assumption
 * that the environment satisfies Aₑ. If realizable, it extracts a
 * winning strategy.
 *
 * Core algorithm (Piterman et al., 2006):
 *
 *   Input: GR(1) specification φ = (θₑ, ρₑ, {Jₑᵢ}, θₛ, ρₛ, {Jₛⱼ})
 *   Output: (realizable?, strategy)
 *
 *   1. Build game graph from specification
 *   2. Compute winning region via nested fixpoint:
 *      W₀ = S (all states)
 *      For j = 1..k (k = number of system justice guarantees):
 *        Wⱼ = νX. μY. [ (Jₛⱼ ∧ W_{j-1}) ∨ CPreₛ(Y) ] ∧ CPreₑ(Y)
 *             ~= νX. Jₛⱼ-reach and back to W_{j-1}
 *   3. Winning region W = W_k
 *   4. If all initial states ⊆ W, specification is realizable
 *   5. Extract strategy from W
 *
 * The algorithm runs in time O(|S|² × k × |T|) for explicit-state,
 * and can be implemented symbolically with BDDs for O(2ⁿ) states.
 *
 * References:
 *   Piterman, Pnueli, Sa'ar. "Synthesis of Reactive(1) Designs." VMCAI 2006.
 *   Bloem, Jobstmann, Piterman, Pnueli, Sa'ar. JCSS 2012.
 *   Pnueli, Rosner. "On the Synthesis of a Reactive Module." POPL 1989.
 *   Church. "Logic, Arithmetic, and Automata." 1962. (Church's problem)
 *
 * Knowledge coverage:
 *   L1: Synthesis problem definition, realizability, strategy
 *   L2: GR(1) as a synthesis fragment, comparison to full LTL synthesis
 *   L3: Game-theoretic formulation, fixpoint characterization
 *   L4: Soundness and completeness of the synthesis algorithm
 *   L5: Full synthesis algorithm implementation
 *   L6: Canonical synthesis benchmarks (arbiter, traffic light, etc.)
 */

#ifndef GRG1_SYNTHESIS_H
#define GRG1_SYNTHESIS_H

#include "grg1_types.h"
#include "grg1_spec.h"
#include "grg1_game.h"
#include "grg1_fixpoint.h"

/* ---------------------------------------------------------------------------
 * L5: Synthesis configuration
 * ---------------------------------------------------------------------------
 */

/** Synthesis algorithm variants */
typedef enum {
    GRG1_SYNTH_EXPLICIT = 0,          /**< Explicit-state synthesis */
    GRG1_SYNTH_SYMBOLIC = 1,          /**< BDD-based symbolic synthesis */
    GRG1_SYNTH_ENUMERATIVE = 2        /**< Enumerative (SAT-based) synthesis */
} grg1_synthesis_mode_t;

/** Synthesis algorithm configuration */
typedef struct {
    grg1_synthesis_mode_t mode;        /**< Which algorithm variant to use */
    int max_iterations;                /**< Maximum fixpoint iterations */
    int max_states;                    /**< Maximum explicit states (0 = unlimited) */
    bool verbose;                      /**< Print progress information */
    bool compute_strategy;             /**< Whether to extract strategy */
    bool minimize_strategy;            /**< Whether to minimize strategy */
    bool check_correctness;            /**< Post-synthesis verification */
    double timeout_seconds;            /**< Timeout (0 = no timeout) */
} grg1_synthesis_config_t;

/** Default synthesis configuration */
#define GRG1_SYNTHESIS_CONFIG_DEFAULT { \
    .mode = GRG1_SYNTH_EXPLICIT, \
    .max_iterations = 100000, \
    .max_states = 0, \
    .verbose = false, \
    .compute_strategy = true, \
    .minimize_strategy = false, \
    .check_correctness = false, \
    .timeout_seconds = 0.0 \
}

/* ---------------------------------------------------------------------------
 * L1/L5: Synthesis result
 * ---------------------------------------------------------------------------
 */

/** Detailed synthesis statistics */
typedef struct {
    int num_states;                    /**< Number of states in game */
    int num_transitions;               /**< Number of transitions */
    int num_initial_states;            /**< Number of initial states */
    int num_winning_states;            /**< Size of winning region */
    int winning_initial_states;        /**< Winning initial states count */
    int fixpoint_iterations;           /**< Total fixpoint iterations */
    int outer_iterations;              /**< Outer (greatest fixpoint) iterations */
    int inner_iterations;              /**< Inner (least fixpoint) iterations */
    int strategy_edges;                /**< Number of edges in strategy */
    double time_seconds;               /**< Computation time */
    bool timed_out;                    /**< Whether computation timed out */
} grg1_synthesis_stats_t;

/** Complete synthesis result */
typedef struct {
    grg1_realizability_t status;       /**< Realizability verdict */
    grg1_region_t* winning_region;     /**< Winning states (NULL if not computed) */
    grg1_strategy_t* strategy;         /**< Extracted strategy (NULL if not computed) */
    grg1_synthesis_stats_t stats;      /**< Detailed statistics */
    char message[512];                 /**< Human-readable result message */
} grg1_synthesis_result_t;

/* ---------------------------------------------------------------------------
 * L5: Main synthesis API
 * ---------------------------------------------------------------------------
 */

/**
 * Execute the full GR(1) synthesis algorithm.
 *
 * This is the top-level entry point. Given a validated GR(1)
 * specification, it:
 *   1. Builds the game graph (or symbolic representation)
 *   2. Computes the winning region via nested fixpoint iteration
 *   3. Checks realizability
 *   4. Optionally extracts a strategy
 *   5. Returns a detailed result
 *
 * @param spec  A validated GR(1) specification
 * @param config  Synthesis configuration
 * @return  Synthesis result (caller must grg1_synthesis_result_free())
 *
 * Complexity: O(|S|² × k) for explicit, where |S| is state count,
 *             k is number of system justice guarantees.
 *             For symbolic: O(k × n² × 2ⁿ) in worst case.
 *
 * Theorem: The algorithm is sound (any extracted strategy is winning)
 *          and complete (if realizable, algorithm finds a strategy).
 *
 * Reference: Piterman et al. (2006) §5, Algorithm 1 — GR(1) Synthesis
 *            Bloem et al. (2012) §3 — Detailed algorithm description
 */
grg1_synthesis_result_t* grg1_synthesize(const grg1_spec_t* spec,
                                          const grg1_synthesis_config_t* config);

/**
 * Free a synthesis result and all associated memory.
 *
 * Complexity: O(num_states)
 */
void grg1_synthesis_result_free(grg1_synthesis_result_t* result);

/**
 * Quick realizability check (no strategy extraction).
 *
 * A lighter-weight version that only determines realizability
 * without computing or extracting a strategy.
 *
 * @param spec  The GR(1) specification
 * @return  Realizability status
 *
 * Complexity: Same as full synthesis but skips strategy extraction
 */
grg1_realizability_t grg1_check_realizability(const grg1_spec_t* spec);

/* ---------------------------------------------------------------------------
 * L5: Winning region computation
 * ---------------------------------------------------------------------------
 * The core fixpoint algorithm exposed for analysis and testing.
 */

/**
 * Compute the winning region for the system player.
 *
 * This implements the nested fixpoint:
 *   W₀ = S
 *   For j = 1..k:
 *     Wⱼ = νX. μY. (Jₛⱼ(X) ∨ CPreₛ(W_{j-1} ∧ Y)) ∧ CPreₑ(Y)
 *
 * The algorithm is parameterized by the context containing the
 * game graph and specification for use with the fixpoint library.
 *
 * @param game  The game arena
 * @param spec  The GR(1) specification
 * @param config  Synthesis configuration
 * @param winning_region  Output: set of winning states
 * @param stats  Output: accumulated statistics
 * @return  true if winning region is non-empty at initial states
 *
 * Complexity: O(|S|² × k)
 * Reference: Piterman et al. (2006) Theorem 5.2 — correctness of fixpoint
 */
bool grg1_compute_winning_region(const grg1_game_t* game,
                                  const grg1_spec_t* spec,
                                  const grg1_synthesis_config_t* config,
                                  grg1_region_t* winning_region,
                                  grg1_synthesis_stats_t* stats);

/**
 * Specialized: compute winning region for a single system justice.
 * Used as the inner step of the outer iteration.
 *
 * The computation for one justice J is:
 *   Yⱼ(Z) = νX. μY. (J(X) ∨ CPreₛ(Z)) ∧ CPreₑ(Y)
 * where Z is the target region (states that satisfy all previous guarantees).
 *
 * @param game  The game arena
 * @param justice_pred  The justice predicate J
 * @param target_region  The region Z (previous winning region)
 * @param full_set  The complete state set S
 * @param result  Output: Yⱼ(Z)
 * @param max_iter  Maximum iterations
 * @param iterations  Output: iterations performed
 * @return  true if converged
 *
 * Complexity: O(|S|²) worst case per justice
 */
bool grg1_single_justice_fixpoint(
    const grg1_game_t* game,
    bool (*justice_pred)(const grg1_state_t*),
    const grg1_region_t* target_region,
    const grg1_region_t* full_set,
    grg1_region_t* result,
    int max_iter,
    int* iterations);

/* ---------------------------------------------------------------------------
 * L5: Strategy extraction
 * ---------------------------------------------------------------------------
 */

/**
 * Extract a memoryless winning strategy from the winning region.
 *
 * For each winning system state s, the strategy selects a successor
 * s' such that s' is also in the winning region and the system can
 * enforce the specification from s'.
 *
 * Algorithm:
 *   For each winning system state s:
 *     Find a successor s' in the winning region such that taking
 *     this transition preserves the ranking (for justice guarantees).
 *
 * @param game  The game arena
 * @param winning_region  The computed winning region
 * @param spec  The specification (for justice predicates)
 * @return  Extracted strategy (caller must grg1_strategy_free())
 *
 * Complexity: O(|S| × outdegree)
 * Theorem: Memoryless strategies suffice for GR(1) games (Piterman et al., 2006 §6)
 */
grg1_strategy_t* grg1_extract_strategy(const grg1_game_t* game,
                                         const grg1_region_t* winning_region,
                                         const grg1_spec_t* spec);

/**
 * Verify that an extracted strategy is indeed winning.
 *
 * Uses simulation-based verification: for every environment behavior,
 * check that the strategy leads to satisfaction of all guarantees.
 *
 * @param game  The game arena
 * @param strategy  The strategy to verify
 * @param spec  The specification
 * @param winning_region  The winning region
 * @return  true if strategy is correct
 *
 * Complexity: O(|S| × k) per simulation path
 */
bool grg1_verify_strategy(const grg1_game_t* game,
                           const grg1_strategy_t* strategy,
                           const grg1_spec_t* spec,
                           const grg1_region_t* winning_region);

/* ---------------------------------------------------------------------------
 * L5: Counterstrategy (for unrealizable specifications)
 * ---------------------------------------------------------------------------
 */

/**
 * Compute an environment counterstrategy when the specification is
 * unrealizable. This provides a witness of unrealizability.
 *
 * The counterstrategy is an environment strategy that forces the
 * system to violate at least one guarantee, regardless of the
 * system's choices.
 *
 * @param game  The game arena
 * @param spec  The specification
 * @param losing_region  States from which the environment wins
 * @return  Environment strategy (NULL if spec is realizable)
 *
 * Complexity: O(|S| × outdegree)
 * Reference: Könighofer et al. (2009) — counterstrategy for debugging
 */
grg1_strategy_t* grg1_extract_counterstrategy(const grg1_game_t* game,
                                                const grg1_spec_t* spec,
                                                const grg1_region_t* losing_region);

/* ---------------------------------------------------------------------------
 * L7: Debug and diagnostic output
 * ---------------------------------------------------------------------------
 */

/**
 * Print a detailed synthesis report.
 *
 * Complexity: O(|S|)
 */
void grg1_synthesis_print_report(const grg1_synthesis_result_t* result);

/**
 * Print the winning region as a sorted list of state IDs.
 *
 * Complexity: O(|S|)
 */
void grg1_synthesis_print_winning_region(const grg1_region_t* winning_region);

/**
 * Compare two synthesis results for equivalence.
 * Used for testing and regression.
 *
 * @return  true if both results agree on realizability and winning region
 *
 * Complexity: O(|S|)
 */
bool grg1_synthesis_result_equivalent(const grg1_synthesis_result_t* a,
                                       const grg1_synthesis_result_t* b);

#endif /* GRG1_SYNTHESIS_H */
