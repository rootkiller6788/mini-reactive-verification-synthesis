/**
 * @file ltl_to_buchi.h
 * @brief LTL-to-Buchi Translation - algorithms for converting LTL formulas
 *        to Buchi automata
 *
 * This module implements the classic tableau-based LTL-to-Buchi translation.
 * The core insight (Vardi & Wolper 1986): model checking can be reduced to
 * automata-theoretic problems: a system S satisfies an LTL property phi iff
 * L(B_S) intersect L(B_neg(phi)) = empty, where B_S is the system automaton
 * and B_neg(phi) is the Buchi automaton for the negated property.
 *
 * Algorithms implemented:
 * 1. Tableau construction (Wolper, Vardi, Sistla 1983)
 * 2. Gerth-Peled-Vardi-Wolper (GPVW) on-the-fly construction (1995)
 *
 * The translation pipeline:
 *   1. Negate the formula: phi -> NNF(neg(phi))
 *   2. Compute the Fischer-Ladner closure
 *   3. Build the tableau graph (states = maximal consistent subsets)
 *   4. Add transitions via X-subformula tracking
 *   5. Define acceptance condition from Until-subformulas
 *   6. (Optional) Simplify / degeneralize
 *
 * Textbook Reference:
 *   - Vardi & Wolper, "An Automata-Theoretic Approach to Automatic Program
 *     Verification" (LICS 1986)
 *   - Gerth, Peled, Vardi, Wolper, "Simple On-the-Fly Automatic Verification
 *     of Linear Temporal Logic" (PSTV 1995)
 *   - Baier & Katoen, "Principles of Model Checking" (2008), Ch.5
 *   - Clarke, Grumberg & Peled, "Model Checking" (1999), Ch.9
 *
 * Course Alignment:
 *   - CMU 15-414: Bug Catching - LTL model checking
 *   - Oxford: Computer-Aided Formal Verification
 *   - ETH 263-2800: Model Checking (LTL-to-Buchi)
 *   - MIT 6.045/18.400: Automata -> omega-automata
 */

#ifndef LTL_TO_BUCHI_H
#define LTL_TO_BUCHI_H

#include "ltl.h"
#include "buchi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * L1: Core Definitions - Translation configuration and results
 * ========================================================================== */

/**
 * Translation method enumeration.
 * Different algorithms offer different trade-offs between automaton size
 * and construction time.
 */
typedef enum {
    TRANSLATE_TABLEAU,       /* Classic Wolper-Vardi-Sistla tableau */
    TRANSLATE_GPVW,          /* Gerth-Peled-Vardi-Wolper on-the-fly */
    TRANSLATE_SIMPLIFIED     /* Simplified tableau with optimization */
} ltl_to_buchi_method_t;

/**
 * Translation statistics and metadata.
 */
typedef struct {
    uint32_t    formula_nodes;       /* Nodes in original formula */
    uint32_t    formula_depth;       /* Depth of formula tree */
    uint32_t    closure_size;        /* Size of Fischer-Ladner closure */
    uint32_t    automaton_states;    /* States in resulting automaton */
    uint32_t    automaton_trans;     /* Transitions in resulting automaton */
    uint32_t    acceptance_sets;     /* Number of acceptance sets */
    double      construction_time_ms;/* Construction time in milliseconds */
    const char *method_name;         /* Name of translation method used */
} ltl_to_buchi_stats_t;

/* ==========================================================================
 * L2: Core Concepts - Translation drivers
 * ========================================================================== */

/**
 * Translate an LTL formula to a Buchi automaton.
 *
 * @param phi       The LTL formula (in NNF expected, but any form accepted)
 * @param method    Which translation algorithm to use
 * @param stats_out Optional output for statistics (may be NULL)
 * @return A Buchi automaton recognizing exactly the models of phi (omega-words)
 *
 * Note: The returned automaton accepts exactly the infinite words satisfying
 * phi. For model checking, you typically want the automaton for NEG(phi),
 * then check L(System) intersect L(B_neg_phi) = empty.
 */
buchi_t *ltl_to_buchi(const ltl_formula_t *phi,
                       ltl_to_buchi_method_t method,
                       ltl_to_buchi_stats_t *stats_out);

/**
 * Translate an LTL formula to a Buchi automaton for its NEGATION.
 * This is the standard setup for automata-theoretic model checking:
 *   Sys |= phi  iff  L(B_Sys) cap L(B_neg(phi)) = empty
 *
 * Internally negates phi, converts to NNF, then calls ltl_to_buchi.
 */
buchi_t *ltl_negation_to_buchi(const ltl_formula_t *phi,
                                ltl_to_buchi_method_t method,
                                ltl_to_buchi_stats_t *stats_out);

/* ==========================================================================
 * L3: Mathematical Structures - Tableau graph
 * ========================================================================== */

/**
 * A tableau node represents a set of formulas that must hold at a
 * given position in the omega-word. It must be propositionally consistent
 * and maximal with respect to the Fischer-Ladner closure.
 */
typedef struct {
    ltl_formula_t **formulas;    /* Formulas in this node */
    size_t          count;       /* Number of formulas */
    uint32_t        id;          /* Unique node identifier */

    /* For GPVW: the node also stores "incoming" formula sets */
    ltl_formula_t **old;         /* Already processed formulas */
    size_t          old_count;
    ltl_formula_t **new;         /* Yet-to-be-processed formulas */
    size_t          new_count;
    ltl_formula_t **next;        /* X-formulas for next state */
    size_t          next_count;

    /* Acceptance information */
    bool            is_initial;  /* Is this an initial node? */
    uint32_t        accept_mask; /* Which acceptance sets this node satisfies */
} tableau_node_t;

/**
 * A tableau graph: the intermediate structure before extracting the
 * Buchi automaton. Nodes are consistent formula sets, edges represent
 * temporal evolution.
 */
typedef struct {
    tableau_node_t *nodes;       /* Array of tableau nodes */
    size_t          num_nodes;   /* Number of nodes */
    size_t          capacity;    /* Allocated capacity */

    /* Edge information: for each node, list of successor node indices */
    uint32_t      **edges;       /* edges[i] = array of successors of node i */
    size_t         *edge_counts; /* edge_counts[i] = number of successors */
    size_t         *edge_caps;   /* Capacity of each edge array */

    /* Acceptance tracking */
    uint32_t        num_accept_sets; /* Number of acceptance conditions */

    /* Statistics */
    size_t          num_atoms;   /* Number of atomic propositions */
} tableau_t;

/* ==========================================================================
 * L4: Fundamental Laws - Tableau construction theorems
 *
 * The correctness of the tableau construction rests on the expansion laws:
 *
 *   phi U psi  equiv  psi or (phi and X(phi U psi))
 *   phi R psi  equiv  psi and (phi or X(phi R psi))
 *
 * These fixed-point characterizations allow us to reduce temporal
 * reasoning about the infinite future to reasoning about the next
 * state and local (propositional) constraints.
 *
 * The Fischer-Ladner closure of phi is the smallest set FL(phi) such that:
 *   1. phi in FL(phi)
 *   2. If psi in FL(phi) then subformulas of psi are in FL(phi)
 *   3. If psi in FL(phi) and psi is not a literal, then certain
 *      "witness" formulas are also in FL(phi):
 *      - If psi = alpha U beta: beta, alpha and X(alpha U beta) in FL(phi)
 *      - If psi = alpha R beta: beta, alpha and X(alpha R beta) in FL(phi)
 *   4. FL(phi) is closed under single negation
 *
 * |FL(phi)| is O(|phi|), which bounds the tableau size.
 * ========================================================================== */

/** Build the full tableau graph for a formula (in NNF). */
tableau_t *tableau_build(const ltl_formula_t *phi);

/** Free a tableau graph. */
void tableau_free(tableau_t *tab);

/** Build the tableau using the GPVW on-the-fly algorithm.
 *  This avoids pre-computing the full closure, instead expanding
 *  nodes as needed during the construction. */
tableau_t *tableau_build_gpvw(const ltl_formula_t *phi);

/** Extract a generalized Buchi automaton from a tableau. */
buchi_t *tableau_to_buchi(const tableau_t *tab, uint32_t num_atoms);

/* ==========================================================================
 * L5: Algorithms - Specific translation methods
 * ========================================================================== */

/** Classic Wolper-Vardi-Sistla tableau construction. */
buchi_t *ltl_to_buchi_tableau(const ltl_formula_t *phi,
                               ltl_to_buchi_stats_t *stats);

/** Gerth-Peled-Vardi-Wolper on-the-fly construction. */
buchi_t *ltl_to_buchi_gpvw(const ltl_formula_t *phi,
                            ltl_to_buchi_stats_t *stats);

/** Simplified translation with post-processing optimization. */
buchi_t *ltl_to_buchi_simplified(const ltl_formula_t *phi,
                                  ltl_to_buchi_stats_t *stats);

/* ==========================================================================
 * L6: Canonical Problems - Standard LTL patterns to Buchi
 * ========================================================================== */

/**
 * Generate a Buchi automaton for common LTL specification patterns.
 *
 * Pattern strings use a compact syntax:
 *   "G(p -> F q)"     - always, if p then eventually q (response)
 *   "G F p"            - infinitely often p (recurrence)
 *   "F G p"            - eventually always p (persistence)
 *   "G(p -> X q)"     - always, if p then next q
 *   "p U q"            - p until q
 *   "G(!p || !q)"     - mutual exclusion (safety)
 *
 * Returns NULL if pattern string is unrecognized.
 */
buchi_t *ltl_pattern_to_buchi(const char *pattern,
                               ltl_to_buchi_stats_t *stats);

/* ==========================================================================
 * L7: Applications - Model checking interface
 * ========================================================================== */

/**
 * Check if a system automaton satisfies an LTL property.
 *
 * Sys |= phi  iff  L(B_sys) cap L(B_neg_phi) = empty
 *
 * @param sys       Buchi automaton representing the system
 * @param phi       LTL property formula
 * @param counterexample_out  If non-NULL and property fails, stores a
 *                             counterexample trace (lasso violating phi)
 * @return true if Sys |= phi, false otherwise
 */
bool ltl_model_check(const buchi_t *sys, const ltl_formula_t *phi,
                      buchi_trace_t **counterexample_out);

/**
 * Check satisfiability of an LTL formula.
 * phi is satisfiable iff L(B_phi) != empty.
 */
bool ltl_satisfiable(const ltl_formula_t *phi);

/**
 * Check validity of an LTL formula.
 * phi is valid iff neg(phi) is unsatisfiable iff L(B_neg_phi) = empty.
 */
bool ltl_valid(const ltl_formula_t *phi);

/** Print translation statistics. */
void ltl_to_buchi_stats_print(const ltl_to_buchi_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* LTL_TO_BUCHI_H */
