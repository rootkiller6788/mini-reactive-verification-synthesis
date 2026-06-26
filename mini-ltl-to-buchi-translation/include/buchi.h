/**
 * @file buchi.h
 * @brief Buchi Automata - data structures and core operations
 *
 * A Buchi automaton is a tuple B = (Q, Sigma, delta, Q0, F) where:
 *   - Q  : finite set of states
 *   - Sigma  : finite alphabet (valuations of atomic propositions)
 *   - delta subset of Q x Sigma x Q : transition relation
 *   - Q0 subset of Q : set of initial states
 *   - F subset of Q : set of accepting states (Buchi condition)
 *
 * A run of B on an omega-word w in Sigma^omega is an infinite sequence
 * rho = q0 q1 q2 ... such that q0 in Q0 and (qi, wi, q_{i+1}) in delta.
 *
 * A run rho is accepting if Inf(rho) intersect F != empty, i.e., some
 * accepting state is visited infinitely often.
 *
 * Acceptance condition variants:
 *   - Buchi: Inf(rho) cap F != empty (standard, one acceptance set)
 *   - Generalized Buchi: AND_{f in F} (Inf(rho) cap f != empty)
 *
 * Textbook Reference:
 *   - J.R. Buchi, "On a Decision Method in Restricted Second Order Arithmetic" (1962)
 *   - Thomas, "Automata on Infinite Objects" (Handbook of TCS, 1990)
 *   - Baier & Katoen, "Principles of Model Checking" (2008), Ch.4
 *   - Vardi & Wolper, "An Automata-Theoretic Approach to Automatic Program
 *     Verification" (LICS 1986)
 *
 * Course Alignment:
 *   - MIT 6.045/18.400: Automata Theory -> omega-automata
 *   - CMU 15-414: Bug Catching - Verifying with automata
 *   - Oxford: Computer-Aided Formal Verification (Buchi automata)
 *   - ETH 263-2800: Model Checking
 */

#ifndef BUCHI_H
#define BUCHI_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * L1: Core Definitions - Buchi automaton structure
 * ========================================================================== */

/**
 * Label on a transition: a Boolean formula over atomic propositions.
 * We represent labels as a conjunction of literals (positive or negative).
 * For the full alphabet Sigma = 2^AP, a label restricts which letters
 * enable the transition.
 */
typedef struct {
    uint32_t pos_mask;   /* atoms that must be TRUE  */
    uint32_t neg_mask;   /* atoms that must be FALSE */
    uint32_t care_mask;  /* atoms this label constrains */
} buchi_label_t;

/**
 * A single transition q --[label]--> qprime in the automaton.
 */
typedef struct buchi_trans {
    uint32_t            target;     /* Index of target state        */
    buchi_label_t       label;      /* Guard label on transition    */
    struct buchi_trans *next;       /* Next transition in list      */
} buchi_trans_t;

/**
 * A state in the Buchi automaton.
 */
typedef struct buchi_state {
    uint32_t        id;             /* State index                    */
    bool            initial;        /* Is this an initial state?      */
    bool            accepting;      /* Is in the Buchi acceptance set? */
    buchi_trans_t  *trans;          /* Linked list of outgoing trans  */
    char           *name;           /* Optional state name            */
    uint32_t        prop_mask;      /* Tableau: propositional constraints */
    uint32_t        next_mask;      /* Tableau: X-subformula obligations */
    uint32_t        flags;          /* Bit flags for state properties */
} buchi_state_t;

/* State property flags */
#define BUCHI_FLAG_REACHABLE   (1u << 0)
#define BUCHI_FLAG_DEAD        (1u << 1)
#define BUCHI_FLAG_SINK        (1u << 2)

/**
 * Generalized Buchi automaton - supports multiple acceptance sets.
 * A run is accepting iff it visits each acceptance set infinitely often.
 */
typedef struct {
    uint32_t        num_states;     /* Number of states              */
    uint32_t        capacity;       /* Allocated capacity            */
    buchi_state_t  *states;         /* Array of states               */

    uint32_t        num_accept_sets; /* Number of acceptance sets    */
    uint32_t       *accept_sets;    /* Per-state bitmask of accept sets */

    uint32_t        num_initial;    /* Number of initial states      */
    uint32_t       *initial;        /* Indices of initial states     */

    uint32_t        num_atoms;      /* Number of atomic propositions */
    char          **atom_names;     /* Optional names for atoms      */
} buchi_t;

/* ==========================================================================
 * L2: Core Concepts - Buchi automaton construction and manipulation
 * ========================================================================== */

/** Allocate a new empty Buchi automaton with given capacity. */
buchi_t *buchi_new(uint32_t capacity, uint32_t num_atoms);

/** Free a Buchi automaton and all its transitions. */
void buchi_free(buchi_t *ba);

/** Add a state, return its index. */
uint32_t buchi_add_state(buchi_t *ba, bool initial, bool accepting);

/** Add a named state. */
uint32_t buchi_add_named_state(buchi_t *ba, const char *name,
                                bool initial, bool accepting);

/** Add a transition between states with a given label. */
void buchi_add_transition(buchi_t *ba, uint32_t from, uint32_t to,
                          const buchi_label_t *label);

/** Add a transition that is always enabled (true label). */
void buchi_add_true_transition(buchi_t *ba, uint32_t from, uint32_t to);

/** Set the acceptance sets for a state (generalized Buchi). */
void buchi_set_accept_sets(buchi_t *ba, uint32_t state_idx,
                            uint32_t accept_mask);

/** Get the number of states. */
uint32_t buchi_state_count(const buchi_t *ba);

/** Get the number of transitions. */
uint32_t buchi_trans_count(const buchi_t *ba);

/** Grow the internal states array to accommodate needed capacity. */
void buchi_ensure_capacity(buchi_t *ba, uint32_t needed);

/* ==========================================================================
 * L3: Mathematical Structures - Label operations
 * ========================================================================== */

/** Create a label: pos atoms must be true, neg atoms must be false. */
buchi_label_t buchi_label_make(uint32_t pos, uint32_t neg);

/** Label that is always satisfied (top). */
buchi_label_t buchi_label_true(void);

/** Label that is never satisfied (bottom). */
buchi_label_t buchi_label_false(void);

/** Check if two labels are compatible (their conjunction is satisfiable). */
bool buchi_label_compatible(const buchi_label_t *a, const buchi_label_t *b);

/** Compute the conjunction of two labels. */
buchi_label_t buchi_label_conjoin(const buchi_label_t *a, const buchi_label_t *b);

/** Check if label_a implies label_b. */
bool buchi_label_implies(const buchi_label_t *a, const buchi_label_t *b);

/** Check if a concrete valuation satisfies a label. */
bool buchi_label_satisfies(const buchi_label_t *label, uint32_t valuation);

/** Check if two labels are equal. */
bool buchi_label_equal(const buchi_label_t *a, const buchi_label_t *b);

/** Pretty-print a label. */
void buchi_label_print(const buchi_label_t *label, uint32_t num_atoms,
                       char **atom_names);

/* ==========================================================================
 * L4: Fundamental Laws - Buchi acceptance properties
 *
 * Key theorems about omega-regular languages:
 * 1. Closure: omega-regular languages closed under union, intersection,
 *    and complementation.
 * 2. Determinization: NBA is strictly more expressive than DBA.
 *    There exist omega-regular languages requiring nondeterminism.
 * 3. Complementation: The complement of an NBA-recognizable language
 *    is also NBA-recognizable (Buchi 1962, Safra 1988).
 * 4. Emptiness: Checking L(B) = empty is in NLOGSPACE by finding a
 *    reachable accepting state on a cycle containing itself.
 * ========================================================================== */

/** Check if the language of the automaton is empty. */
bool buchi_is_empty(buchi_t *ba);

/** Find an accepting lasso (prefix + cycle) if language is non-empty. */
void buchi_find_accepting_lasso(buchi_t *ba,
                                 uint32_t **prefix, size_t *prefix_len,
                                 uint32_t **cycle, size_t *cycle_len);

/* ==========================================================================
 * L5: Algorithms - Buchi automaton operations
 * ========================================================================== */

/**
 * Degeneralization: Convert TGBA with k acceptance sets to standard Buchi
 * with a single acceptance set. Creates k copies of each state and cycles
 * through acceptance sets. Result has O(k * |Q|) states.
 * Reference: Choueka, "Theories of Automata on omega-Tapes" (1974)
 */
buchi_t *buchi_degeneralize(const buchi_t *tgba);

/** Remove unreachable states. */
void buchi_remove_unreachable(buchi_t *ba);

/** Remove states that cannot reach any accepting cycle. */
void buchi_remove_nonaccepting(buchi_t *ba);

/** Compute the product automaton (intersection of languages). */
buchi_t *buchi_product(const buchi_t *ba1, const buchi_t *ba2);

/** Compute the union automaton. */
buchi_t *buchi_union(const buchi_t *ba1, const buchi_t *ba2);

/** Rename states consecutively starting from 0. */
void buchi_compact(buchi_t *ba);

/** Compute strongly connected components (Tarjan's algorithm). */
uint32_t *buchi_scc(const buchi_t *ba, uint32_t *num_scc_out);

/** Check if an SCC is accepting. */
bool buchi_scc_is_accepting(const buchi_t *ba, const uint32_t *scc_ids,
                             uint32_t scc_id);

/* ==========================================================================
 * L6: Canonical Problems - Emptiness and counterexample generation
 * ========================================================================== */

/**
 * Nested depth-first search (NDFS) for Buchi emptiness.
 * Courcoubetis-Vardi-Wolper-Yannakakis algorithm.
 * Two DFS passes: first explores reachable states; second (nested)
 * starts from accepting states to find a cycle.
 * Complexity: O(|Q| + |delta|) time, O(|Q|) space.
 * Reference: CVWY, "Memory-Efficient Algorithms for the Verification
 *   of Temporal Properties" (CAV 1990)
 */
bool buchi_ndfs_empty(const buchi_t *ba,
                       uint32_t **cycle, size_t *cycle_len);

/** Trace: finite prefix + periodic suffix forming a lasso. */
typedef struct {
    uint32_t *states;
    size_t    length;
    uint32_t *valuations;
} buchi_trace_t;

buchi_trace_t *buchi_trace_from_lasso(const buchi_t *ba,
                                       uint32_t *prefix, size_t prefix_len,
                                       uint32_t *cycle, size_t cycle_len);
void buchi_trace_free(buchi_trace_t *trace);

/* ==========================================================================
 * L7: Applications - Automaton statistics and verification
 * ========================================================================== */

/** Verify that every initial state can reach an accepting cycle. */
bool buchi_verify_acceptance(buchi_t *ba);

/** Count number of transitions between two state subsets. */
uint32_t buchi_cross_transition_count(const buchi_t *ba,
                                       uint32_t *set_a, size_t len_a,
                                       uint32_t *set_b, size_t len_b);

/** Acceptance-SCC analysis info. */
typedef struct {
    uint32_t *scc_ids;
    uint32_t  num_accepting_sccs;
    uint32_t  total_sccs;
} buchi_scc_info_t;

buchi_scc_info_t *buchi_accepting_scc_analysis(buchi_t *ba);
void buchi_scc_info_free(buchi_scc_info_t *info);

/** Simulate a concrete word on the automaton (nondeterministic, BFS). */
bool buchi_simulate_word(const buchi_t *ba,
                          const uint32_t *word, size_t word_len,
                          bool infinite_mode);

/** Successors list for a given state and valuation. */
typedef struct {
    uint32_t *targets;
    size_t    count;
    size_t    capacity;
} buchi_successors_t;

buchi_successors_t *buchi_get_successors(const buchi_t *ba,
                                          uint32_t state,
                                          uint32_t valuation);
void buchi_successors_free(buchi_successors_t *succ);

/** Pretty-print the automaton in DOT format for Graphviz. */
void buchi_print_dot(const buchi_t *ba, const char **atom_names);

/** Print automaton statistics. */
void buchi_print_stats(const buchi_t *ba);

/** Clone a Buchi automaton (deep copy). */
buchi_t *buchi_clone(const buchi_t *ba);

#ifdef __cplusplus
}
#endif

#endif /* BUCHI_H */
