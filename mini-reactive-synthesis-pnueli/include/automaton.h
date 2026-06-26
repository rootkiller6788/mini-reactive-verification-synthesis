/**
 * automaton.h �� Automata on Infinite Words and Trees
 *
 * Defines automata-theoretic structures central to reactive synthesis:
 * nondeterministic B��chi automata (NBA), deterministic parity automata (DPA),
 * deterministic Rabin automata (DRA), and operations between them.
 *
 * The automata-theoretic approach to synthesis:
 *   LTL formula �� �� NBA A_�� �� DPA/DRA A'_�� �� parity game �� strategy
 *
 * Each step is implemented as a module function here.
 *
 * Reference: Vardi & Wolper (1986) "An Automata-Theoretic Approach to
 *            Automatic Program Verification"
 *            Safra (1988) "On the Complexity of ��-Automata", FOCS 1988
 *            Piterman (2006) "From Nondeterministic B��chi and Streett
 *            Automata to Deterministic Parity Automata"
 *
 * Knowledge Level: L3 Math Structures, L5 Algorithms, L4 Fundamental Laws
 */

#ifndef AUTOMATON_H
#define AUTOMATON_H

#include "reactive_types.h"
#include "game_structure.h"

/* ====================================================================
 * Nondeterministic B��chi Automaton (NBA)
 * ==================================================================== */

/**
 * A nondeterministic B��chi automaton (NBA) is a tuple:
 *   A = (Q, ��, ��, q0, F)
 *
 * where:
 *   Q: finite set of states
 *   ��: finite alphabet (boolean valuations over k propositions)
 *   ��: Q �� �� �� 2^Q (nondeterministic transition function)
 *   q0 �� Q: initial state
 *   F ? Q: set of accepting states
 *
 * An infinite word w �� ��^�� is accepted iff there exists a run r
 * such that r starts in q0, respects ��, and Inf(r) �� F �� ?
 * (visits accepting states infinitely often).
 */
typedef struct {
    int32_t     num_states;     /* |Q| */
    int32_t     alphabet_size;  /* k = number of atomic propositions */
    int32_t     init_state;     /* q0 (single initial state for simplicity) */
    bool       *is_accepting;   /* F[q] = true iff q is accepting */
    /* Transition: for each state q and each letter ��, the set of
     * successor states. Stored as a bit-matrix:
     *   trans[q][letter] = bitmask of successor states.
     * letter is encoded as an integer 0..(1<<k)-1 */
    uint32_t  **trans;          /* trans[q][letter] = successor bitmask */
    char       *state_names[256]; /* optional human-readable names */
} nba_t;

/** Deterministic parity automaton (DPA):
 *  Same as NBA but with deterministic transitions (��: Q������Q)
 *  and a parity acceptance condition c: Q �� {0,...,d}.
 *
 *  A word is accepted if the minimal priority seen infinitely often is even.
 *  (This is the min-even parity condition; dual to max-even on games.) */
typedef struct {
    int32_t     num_states;     /* |Q| */
    int32_t     alphabet_size;  /* k */
    int32_t     init_state;     /* q0 */
    int32_t    *transition;     /* transition[q][letter] = next state, flat [q * 2^k + letter] */
    int32_t    *priority;       /* c(q) �� {0, ..., d} */
    int32_t     max_priority;   /* d */
} dpa_t;

/** Deterministic Rabin automaton (DRA):
 *  Deterministic automaton with Rabin acceptance: set of pairs (E_i, F_i).
 *  A run is accepting iff ? i: Inf(r) �� E_i = ? and Inf(r) �� F_i �� ?. */
typedef struct {
    int32_t     num_states;
    int32_t     alphabet_size;
    int32_t     init_state;
    int32_t    *transition;
    int32_t     num_pairs;      /* number of Rabin pairs */
    bool      **E;              /* E[i][q] = true if q �� E_i */
    bool      **F;              /* F[i][q] = true if q �� F_i */
} dra_t;

/* ====================================================================
 * NBA Construction and Manipulation
 * ==================================================================== */

/**
 * Create an NBA with given number of states and alphabet size.
 * @param num_states     |Q|
 * @param alphabet_size  k = number of atomic propositions
 * @return allocated nba_t
 *
 * Time: O(|Q| * 2^k)  Space: O(|Q| * 2^k) */
nba_t *nba_create(int32_t num_states, int32_t alphabet_size);

/**
 * Free an NBA and all associated memory.
 * Time: O(|Q|) */
void nba_destroy(nba_t *nba);

/**
 * Set the initial state of the NBA.
 * Time: O(1) */
void nba_set_initial(nba_t *nba, int32_t state);

/**
 * Add a transition: from state `src` on letter `letter` to state `dst`.
 * Nondeterminism is allowed (multiple successors per letter).
 * Time: O(1) */
void nba_add_transition(nba_t *nba, int32_t src,
                         int32_t letter, int32_t dst);

/**
 * Get successors of state `src` on letter `letter`.
 * Returns a bitmask where bit i = 1 iff state i is a successor.
 * Time: O(1) */
uint32_t nba_get_successors(const nba_t *nba,
                             int32_t src, int32_t letter);

/**
 * Build an NBA that accepts exactly the language L(��) for an LTL formula ��.
 *
 * This implements the standard LTL-to-NBA translation using the
 * tableau construction (Gerth, Peled, Vardi, Wolper 1995) or the
 * more recent LTL-2-approach.
 *
 * The resulting NBA has O(2^{|��|}) states in the worst case,
 * matching the tight bound.
 *
 * Reference: Vardi & Wolper (1986); Gerth, Peled, Vardi, Wolper (1995)
 *            "Simple On-the-fly Automatic Verification of Linear
 *            Temporal Logic", PSTV 1995
 *
 * @param phi the LTL formula (must be in NNF)
 * @return allocated nba_t accepting L(��)
 *
 * Time: O(2^{|��|})  Space: O(2^{|��|}) */
nba_t *ltl_to_nba(const ltl_formula_t *phi);

/**
 * Build an NBA that accepts exactly the complement language L(��)^c.
 * This uses the dual formula: NBA(L(?��)).
 *
 * Time: O(2^{|��|}) */
nba_t *ltl_to_nba_complement(const ltl_formula_t *phi);

/**
 * Compute the product of two NBAs (intersection).
 * L(A? �� A?) = L(A?) �� L(A?).
 *
 * This uses the standard product construction with two-track
 * acceptance: a run is accepting if it visits F? and F? infinitely often.
 *
 * @param a1  first NBA
 * @param a2  second NBA (must have same alphabet_size)
 * @return allocated nba_t for the product
 *
 * Time: O(|Q?| * |Q?| * 2^k)  Space: O(|Q?| * |Q?|) */
nba_t *nba_product(const nba_t *a1, const nba_t *a2);

/**
 * Compute the union of two NBAs.
 * L(A? �� A?) = L(A?) �� L(A?).
 *
 * Simple construction: add a new initial state with ��-transitions
 * (treated here as direct connections) to the original initial states.
 *
 * Time: O(|Q?| + |Q?|) */
nba_t *nba_union(const nba_t *a1, const nba_t *a2);

/* ====================================================================
 * NBA Decision Procedures
 * ==================================================================== */

/**
 * Check whether the language of an NBA is empty.
 *
 * The emptiness check reduces to finding a reachable accepting state
 * that lies on a cycle. Uses nested DFS (Courcoubetis et al. 1992)
 * or SCC decomposition.
 *
 * @return true if L(A) = ? (no accepting word exists)
 *
 * Time: O(|Q| + |��|) = O(|Q| * 2^k) */
bool nba_is_empty(const nba_t *nba);

/**
 * Check language containment: L(A?) ? L(A?)?
 *
 * L(A?) ? L(A?) ? L(A?) �� L(?A?) = ?.
 * We construct complement of A? and check emptiness of intersected product.
 *
 * @return true if L(A?) ? L(A?)
 *
 * Time: 2EXPTIME in worst case (due to complementation) */
bool nba_is_contained(const nba_t *a1, const nba_t *a2);

/**
 * Find an accepting word (lasso) for an NBA, if one exists.
 * Returns NULL if the language is empty.
 *
 * @return an infinite trace (lasso) accepted by the NBA, or NULL.
 *         Caller must free via infinite_trace_destroy().
 *
 * Time: O(|Q| + |��|) */
infinite_trace_t *nba_find_accepting_word(const nba_t *nba);

/* ====================================================================
 * Determinization
 * ==================================================================== */

/**
 * Determinize an NBA into a DPA using Safra's construction.
 *
 * Safra's determinization converts an NBA with n states into a DPA
 * with 2^{O(n log n)} states and O(n) priorities. This is asymptotically
 * optimal (lower bound by Michel 1988).
 *
 * Reference: Safra (1988) "On the Complexity of ��-Automata"
 *            Piterman (2006) "From Nondeterministic B��chi and Streett
 *            Automata to Deterministic Parity Automata" (simpler variant)
 *
 * @param nba  the nondeterministic B��chi automaton
 * @return allocated dpa_t
 *
 * Time: 2^{O(n log n)}  Space: 2^{O(n log n)} */
dpa_t *nba_to_dpa_safra(const nba_t *nba);

/**
 * Determinize an NBA into a DPA using Piterman's tight construction.
 * Simpler than Safra's original, uses a "compact Safra trees" approach.
 *
 * Reference: Piterman (2006) LMCS
 *
 * Time: 2^{O(n log n)}  Space: 2^{O(n log n)} */
dpa_t *nba_to_dpa_piterman(const nba_t *nba);

/* ====================================================================
 * DPA/DRA Operations
 * ==================================================================== */

/** Create a DPA with given dimensions.
 *  Time: O(|Q| * 2^k) */
dpa_t *dpa_create(int32_t num_states, int32_t alphabet_size,
                   int32_t max_priority);

/** Free a DPA.
 *  Time: O(|Q|) */
void dpa_destroy(dpa_t *dpa);

/** Set the transition: from state `src` on letter `letter` go to `dst`.
 *  Time: O(1) */
void dpa_set_transition(dpa_t *dpa, int32_t src,
                         int32_t letter, int32_t dst);

/** Get the successor state from `src` on `letter`.
 *  Time: O(1) */
int32_t dpa_get_successor(const dpa_t *dpa, int32_t src, int32_t letter);

/** Convert a DPA to a parity game.
 *  The DPA is combined with the input-output choice structure to produce
 *  a game where:
 *    - System states: states of the DPA (choose output)
 *    - Environment states: DPA states with pending input choice
 *
 *  @param dpa       the deterministic parity automaton for the specification
 *  @param num_inputs   number of input propositions
 *  @param num_outputs  number of output propositions
 *  @return parity_game_t representing the synthesis game
 *
 *  Time: O(|Q| * 2^{k}) */
parity_game_t *dpa_to_parity_game(const dpa_t *dpa,
                                    int32_t num_inputs,
                                    int32_t num_outputs);

/**
 * Convert a DRA to a DPA.
 * Rabin to parity is exponential in the number of pairs.
 *
 * Time: O(|Q| * 2^p * 2^k) where p = number of Rabin pairs */
dpa_t *dra_to_dpa(const dra_t *dra);

/** Create and free a DRA. */
dra_t *dra_create(int32_t num_states, int32_t alphabet_size,
                   int32_t num_pairs);
void dra_destroy(dra_t *dra);

/* ====================================================================
 * NBA Minimization (Simulation-Based)
 * ==================================================================== */

/**
 * Reduce the size of an NBA using direct simulation.
 *
 * If state q? is directly simulated by state q? (meaning
 * L(A^{q?}) ? L(A^{q?}) and transitions are compatible),
 * then q? can be removed.
 *
 * @return a new minimized NBA, or NULL if minimization fails.
 *         Caller owns the result.
 *
 * Reference: Etessami, Wilke, Schuller (2001) "Fair Simulation
 *            Relations, Parity Games, and State Space Reduction
 *            for B��chi Automata"
 *
 * Time: O(|Q|^2 * 2^k) */
nba_t *nba_minimize_simulation(const nba_t *nba);

#endif /* AUTOMATON_H */
