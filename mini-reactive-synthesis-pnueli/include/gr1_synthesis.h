/**
 * gr1_synthesis.h 〞 GR(1) Synthesis Algorithm
 *
 * Implements the Generalized Reactivity(1) synthesis algorithm,
 * the most practical fragment of LTL synthesis with polynomial
 * time complexity. GR(1) specifications are expressive enough
 * for many real-world reactive system designs.
 *
 * The key insight (Pnueli et al.): for GR(1) specifications,
 * the synthesis problem reduces to computing a greatest fixed
 * point of a monotone operator over a finite lattice of
 * environment assumptions and system guarantees.
 *
 * Reference: Piterman, Pnueli, Sa'ar (2006) "Synthesis of Reactive(1)
 *            Designs", VMCAI 2006
 *            Bloem, Jobstmann, Piterman, Pnueli, Sa'ar (2012)
 *            "Synthesis of Reactive(1) Designs", JCSS
 *
 * Knowledge Level: L4 Fundamental Laws, L5 Algorithms, L6 Canonical Problems
 */

#ifndef GR1_SYNTHESIS_H
#define GR1_SYNTHESIS_H

#include "reactive_types.h"
#include "game_structure.h"

/* ====================================================================
 * GR(1) Formula Components
 * ==================================================================== */

/**
 * A GR(1) specification is structured as:
 *
 *   耳_e ↙ 耳_s  where
 *
 *   耳_e = ↓老_e ＿ (＿_i ↓⊙ J_i^e)   (environment: safety + justice)
 *   耳_s = ↓老_s ＿ (＿_j ↓⊙ J_j^s)   (system: safety + justice)
 *
 * 老_e, 老_s are safety formulas (past and present only)
 * J_i^e, J_j^s are justice formulas (B邦chi conditions, "infinitely often")
 *
 * The synthesis problem for GR(1) is:
 *   Find a strategy for the system such that for all environment behaviors
 *   satisfying 耳_e, the system behavior satisfies 耳_s.
 */

/** Precondition/postcondition pair used in GR(1) safety decomposition.
 *  Safety formulas are decomposed into initial condition + transition
 *  relation. A state s = (e, o) is a valuation of all variables. */
typedef struct {
    ltl_formula_t *rho_e_init;     /* 老_e^init: initial env constraint */
    ltl_formula_t *rho_e_trans;    /* 老_e^trans(e, e'): env transition relation */
    ltl_formula_t *rho_s_init;     /* 老_s^init: initial sys constraint */
    ltl_formula_t *rho_s_trans;    /* 老_s^trans(e, e', o, o'): sys transition relation */
    int32_t        num_justice_e;  /* number of environment justice goals */
    int32_t        num_justice_s;  /* number of system justice goals */
    ltl_formula_t **justice_e;     /* array of env justice formulas J_i^e */
    ltl_formula_t **justice_s;     /* array of sys justice formulas J_j^s */
} gr1_spec_t;

/**
 * A state in the GR(1) synthesis game.
 * Each state is a valuation of all input and output variables,
 * encoded as (env_valuation, sys_valuation). */
typedef struct {
    valuation_t env;            /* input valuation */
    valuation_t sys;            /* output valuation */
    int32_t     state_index;    /* index into the state array */
} gr1_state_t;

/* ====================================================================
 * GR(1) Synthesis State Space
 * ==================================================================== */

/**
 * The GR(1) synthesis algorithm operates over a finite state space
 * S ? 2^I ℅ 2^O (all valuations of input and output variables).
 *
 * The algorithm maintains:
 *   - Winning region W ? S (system can guarantee 耳_s from here)
 *   - For each justice goal J_j^s, a "progress measure" tracking
 *     distance to satisfying the goal.
 */
typedef struct {
    int32_t         num_inputs;      /* |I| */
    int32_t         num_outputs;     /* |O| */
    int32_t         num_states;      /* |S| = 2^{|I|+|O|} */
    gr1_state_t    *states;          /* array of all states */
    bool           *initial_states;  /* which states satisfy initial condition */
    bool         ***trans_env;       /* trans_env[e][e'][idx] = transition possible? */
    bool           *sys_can_move;    /* from this state, can the system choose outputs? */
    int32_t        *winning_region;  /* W ? S: -1=uncomputed, 0=env wins, 1=sys wins */
} gr1_state_space_t;

/* ====================================================================
 * Core GR(1) API
 * ==================================================================== */

/**
 * Build a GR(1) specification from safety and justice components.
 *
 * @param rho_e_init    initial condition on environment
 * @param rho_e_trans   transition relation for environment
 * @param rho_s_init    initial condition on system
 * @param rho_s_trans   transition relation for system
 * @param justice_e     array of env justice formulas
 * @param n_justice_e   number of env justice formulas
 * @param justice_s     array of sys justice formulas
 * @param n_justice_s   number of sys justice formulas
 * @return allocated gr1_spec_t
 *
 * Time: O(n) */
gr1_spec_t *gr1_spec_create(ltl_formula_t *rho_e_init,
                              ltl_formula_t *rho_e_trans,
                              ltl_formula_t *rho_s_init,
                              ltl_formula_t *rho_s_trans,
                              ltl_formula_t **justice_e,
                              int32_t n_justice_e,
                              ltl_formula_t **justice_s,
                              int32_t n_justice_s);

/**
 * Free a GR(1) specification.
 * Time: O(n + m) */
void gr1_spec_destroy(gr1_spec_t *spec);

/**
 * Create the explicit state space for GR(1) synthesis.
 * Enumerates all 2^{|I|+|O|} valuations and precomputes
 * which satisfy the initial and transition constraints.
 *
 * @param num_inputs   number of input propositions
 * @param num_outputs  number of output propositions
 * @return allocated gr1_state_space_t
 *
 * Time: O(2^{|I|+|O|})  Space: same */
gr1_state_space_t *gr1_state_space_create(int32_t num_inputs,
                                           int32_t num_outputs);

/**
 * Free a GR(1) state space.
 * Time: O(|S|) */
void gr1_state_space_destroy(gr1_state_space_t *space);

/**
 * Initialize the state space with the GR(1) specification.
 * Marks which states are initial and computes the transition
 * relations.
 *
 * @param space  allocated state space
 * @param spec   the GR(1) specification
 *
 * Time: O(|S|^2 * (|J_e| + |J_s|)) */
void gr1_state_space_init(gr1_state_space_t *space, const gr1_spec_t *spec);

/**
 * The core GR(1) synthesis algorithm: fixed-point computation
 * of the winning region following the recurrence:
 *
 *   W^0 = S
 *   Z^{i+1} = 糸X. (﹎_{j=1..m} 米Y. (Pre(W^i ﹎ J_j^s) ﹍ X) ﹎ ...)
 *
 * This implements the standard GR(1) fixpoint with the following
 * nested fixed points (from innermost to outermost):
 *
 *   1. 米Y_j: attractor for justice goal j
 *   2. 糸X: greatest fixed point weaving all justice goals
 *   3. Outer iteration until convergence
 *
 * Reference: Bloem et al. (2012) JCSS, Algorithm 1
 *
 * @param space  initialized state space
 * @param spec   the GR(1) spec
 * @return winning_region array (caller frees): W[v]=1 if system wins from v
 *
 * Time: O(m * n * |S|^3) where m = |J_s|, n = |J_e|
 *       In practice, often O(m * |S|^2) due to monotonicity */
bool *gr1_fixed_point_solve(gr1_state_space_t *space,
                              const gr1_spec_t *spec);

/**
 * Extract a reactive module (strategy) from the winning region.
 *
 * For each winning state, we pick a system output that keeps the
 * play in the winning region and makes progress towards justice goals.
 *
 * @param space          state space with computed winning region
 * @param spec           the GR(1) spec (for progress measures)
 * @param winning_region boolean array of winning states
 * @return synthesized reactive module (caller owns)
 *
 * Time: O(|S| * 2^{|O|}) */
reactive_module_t *gr1_extract_strategy(gr1_state_space_t *space,
                                         const gr1_spec_t *spec,
                                         const bool *winning_region);

/**
 * One-shot GR(1) synthesis: from LTL components to reactive module.
 * Convenience function that runs the complete pipeline.
 *
 * @param num_inputs     number of input propositions
 * @param num_outputs    number of output propositions
 * @param rho_e_init     initial env assumption
 * @param rho_e_trans    env transition relation
 * @param rho_s_init     initial sys guarantee
 * @param rho_s_trans    sys transition relation
 * @param justice_e      env justice formulas sequence
 * @param n_justice_e    count of env justice goals
 * @param justice_s      sys justice formulas sequence
 * @param n_justice_s    count of sys justice goals
 * @return synthesized reactive module, or NULL if unrealizable
 *
 * Time: O(m * n * 2^{3*(|I|+|O|)}) */
reactive_module_t *gr1_synthesize(int32_t num_inputs,
                                    int32_t num_outputs,
                                    ltl_formula_t *rho_e_init,
                                    ltl_formula_t *rho_e_trans,
                                    ltl_formula_t *rho_s_init,
                                    ltl_formula_t *rho_s_trans,
                                    ltl_formula_t **justice_e,
                                    int32_t n_justice_e,
                                    ltl_formula_t **justice_s,
                                    int32_t n_justice_s);

/* ====================================================================
 * GR(1) Helper: Symbolic (BDD-like) Representations
 * ==================================================================== */

/**
 * A symbolic region: represents a set of states using a boolean
 * function (here, a simple explicit bitmask or BDD placeholder).
 *
 * For small state spaces (up to ~20 variables), we use explicit
 * bitmasks for efficiency. For larger spaces, BDDs are needed. */
typedef struct {
    int32_t   num_states;   /* total number of states |S| */
    uint64_t *bits;         /* bit mask: bits[i>>6] has bit (i&63) set if state i is in set */
} symbolic_region_t;

/**
 * Create a symbolic region (empty set).
 * Time: O(|S| / 64) */
symbolic_region_t *symbolic_region_create(int32_t num_states);

/**
 * Free a symbolic region.
 * Time: O(1) */
void symbolic_region_destroy(symbolic_region_t *region);

/**
 * Add/remove a state from the region.
 * Time: O(1) */
void symbolic_region_add(symbolic_region_t *region, int32_t state_id);
void symbolic_region_remove(symbolic_region_t *region, int32_t state_id);
bool symbolic_region_contains(const symbolic_region_t *region, int32_t state_id);

/**
 * Compute the controllable predecessor of a region for the system player.
 *
 * CPre(W, G) = {s ﹋ W | ? o ﹋ 2^O: ? i ﹋ 2^I: G(s, i, o) ﹋ W}
 *
 * where G(s, i, o) is the next state after environment chooses input i
 * and system chooses output o. For GR(1), G is deterministic given
 * (s, o) since the environment input is the only nondeterminism.
 *
 * @param space  the GR(1) state space
 * @param W      current winning region
 * @return new region after one CPre step
 *
 * Time: O(|S| * 2^{|O|} * 2^{|I|}) */
symbolic_region_t *gr1_controllable_predecessor(
    const gr1_state_space_t *space,
    const symbolic_region_t *W);

/**
 * Compute the uncontrollable predecessor (environment's view).
 *
 * UPre(W) = {s ﹋ S | ? o ﹋ 2^O: ? i ﹋ 2^I: G(s, i, o) ﹋ W}
 *
 * Time: O(|S| * 2^{|O|} * 2^{|I|}) */
symbolic_region_t *gr1_uncontrollable_predecessor(
    const gr1_state_space_t *space,
    const symbolic_region_t *W);

#endif /* GR1_SYNTHESIS_H */
