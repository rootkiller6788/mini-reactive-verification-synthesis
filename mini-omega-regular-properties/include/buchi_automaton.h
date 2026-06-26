/**
 * buchi_automaton.h — Nondeterministic Büchi Automata (NBA)
 *
 * Defines the fundamental data structures and operations for Büchi automata
 * on infinite words. The Büchi acceptance condition requires that a run
 * visits an accepting state infinitely often.
 *
 * Key References:
 *   - Büchi, J.R. "On a decision method in restricted second order arithmetic"
 *     (1962) — Original definition of Büchi automata
 *   - Vardi, M.Y. & Wolper, P. "Automata-theoretic techniques for modal logics
 *     of programs" (JCSS 1986) — NBA-based LTL model checking
 *   - Couvreur, J.-M. "On-the-fly verification of temporal logic" (FM 1999)
 *
 * Knowledge Coverage:
 *   L1: NBA definition (Q, Σ, δ, q₀, F)
 *   L2: Büchi acceptance, emptiness, complementation
 *   L3: Formal NBA tuple, transition relation
 *   L4: Büchi's theorem, ω-regular = NBA-recognizable
 *   L5: Emptiness check (SCC-based), union, intersection
 */

#ifndef BUCHI_AUTOMATON_H
#define BUCHI_AUTOMATON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "omega_regular.h"

/* ---------------------------------------------------------------------------
 * L1: NBA Definition
 * ------------------------------------------------------------------------- */

/** Maximum states in an NBA (tunable) */
#define NBA_MAX_STATES   64
/** Maximum alphabet size consistent with omega_regular.h */
#define NBA_MAX_ALPHABET 32
/** Maximum transitions per state-symbol pair */
#define NBA_MAX_TRANS    8

/**
 * nba_state — A single state in a Büchi automaton.
 */
typedef struct {
    int  id;                    /** State identifier (0..n-1) */
    bool is_initial;            /** Is this the initial state? */
    bool is_accepting;          /** Is this a Büchi accepting (final) state? */
    char name[32];              /** Human-readable label */
} nba_state_t;

/**
 * nba_transition — A single transition in the NBA.
 *
 * From state src, on reading symbol sym, the automaton may move
 * to state dst (nondeterminism is permitted).
 */
typedef struct {
    int     src;                /** Source state id */
    uint8_t sym;                /** Input symbol (0..MAX_ALPHABET-1) */
    int     dst;                /** Destination state id */
} nba_trans_t;

/**
 * nba_t — Nondeterministic Büchi Automaton.
 *
 * Formal definition: NBA A = (Q, Σ, δ, q₀, F) where
 *   - Q: finite set of states (nstates)
 *   - Σ: finite alphabet (alphabet_size)
 *   - δ: Q × Σ → 2^Q (transition relation, stored as array of lists)
 *   - q₀ ∈ Q: initial state (states[initial])
 *   - F ⊆ Q: set of accepting states (is_accepting flag)
 */
typedef struct {
    nba_state_t  states[NBA_MAX_STATES];
    int          nstates;                       /** |Q| */
    size_t       alphabet_size;                 /** |Σ| */
    int          initial;                       /** q₀ state index */
    /* transition adjacency list:
     * trans_head[s] = index into trans_next of first outgoing edge from s
     * trans_next[e] = next transition index from same source (-1 = end)
     * We store transitions indexed by symbol: trans_by_sym[s][sym] = edge index */
    nba_trans_t  transitions[NBA_MAX_STATES * NBA_MAX_ALPHABET * NBA_MAX_TRANS];
    int          ntrans;                        /** |δ| total transition count */
    int          trans_head[NBA_MAX_STATES];    /** First transition from each state */
    int          trans_next[NBA_MAX_STATES * NBA_MAX_ALPHABET * NBA_MAX_TRANS];
    /** For quick lookup: trans_by_sym[state * alphabet_size + sym] = first edge
     *  whose src=state, sym=sym (-1 if none) */
    int          trans_by_sym[NBA_MAX_STATES * NBA_MAX_ALPHABET];
} nba_t;

/* ---------------------------------------------------------------------------
 * L1/L3: NBA Construction Operations
 * ------------------------------------------------------------------------- */

/**
 * nba_init — Initialize an empty NBA.
 *
 * Post: nstates=0, ntrans=0, alphabet_size=0, initial=-1.
 */
void nba_init(nba_t *nba, size_t alphabet_size);

/**
 * nba_add_state — Add a state to the automaton.
 * Returns the state id, or -1 if full.
 *
 * @param accepting: true if this is a Büchi accepting state (∈ F)
 */
int nba_add_state(nba_t *nba, bool initial, bool accepting, const char *name);

/**
 * nba_set_initial — Set (or change) the initial state.
 */
void nba_set_initial(nba_t *nba, int state_id);

/**
 * nba_add_transition — Add a transition (src, sym, dst) to δ.
 * Returns true on success, false if the automaton is full.
 *
 * Nondeterminism: multiple transitions for the same (src, sym) are permitted.
 */
bool nba_add_transition(nba_t *nba, int src, uint8_t sym, int dst);

/* ---------------------------------------------------------------------------
 * L2: Run, Acceptance, and Emptiness
 * ------------------------------------------------------------------------- */

/**
 * nba_run — A run of the NBA on an ω-word.
 *
 * A run r: N → Q is an infinite sequence of states such that
 * r(0) = q₀ and ∀i: (r(i), w(i), r(i+1)) ∈ δ.
 *
 * For ultimately periodic words, a run is ultimately periodic and can be
 * represented finitely.
 */
typedef struct {
    int     prefix[NBA_MAX_STATES * 2]; /** States visited in prefix */
    size_t  prefix_len;
    int     period[NBA_MAX_STATES * 2]; /** Repeated states in period */
    size_t  period_len;
} nba_run_t;

/**
 * nba_accepts — Decide whether the NBA accepts an ultimately periodic
 * ω-word (i.e., whether there exists an accepting run).
 *
 * Algorithm: A nondeterministic choice of run is simulated via a search
 * for a reachable accepting state that lies on a cycle reachable from
 * itself (the "lasso" shape for ultimately periodic words).
 *
 * Returns: true iff w ∈ L(A).
 *
 * Complexity: O(|Q|·(|v|+|u|)) for word u·v^ω.
 */
bool nba_accepts(const nba_t *nba, const omega_word *w);

/**
 * nba_run_find — Find an accepting run (if any) for a given ω-word.
 * Returns true and fills in 'run' if accepting; false otherwise.
 */
bool nba_run_find(const nba_t *nba, const omega_word *w, nba_run_t *run);

/**
 * nba_is_empty — Decide whether the language L(A) is empty.
 *
 * Theorem (Büchi 1962): L(A) ≠ ∅ iff there exists a reachable accepting
 * state that is reachable from itself (i.e., lies on a cycle).
 *
 * Algorithm: Compute SCCs via Tarjan's algorithm. Check if any nontrivial
 * SCC or trivial SCC with a self-loop contains an accepting state.
 *
 * Complexity: O(|Q| + |δ|).
 *
 * This is the fundamental decision procedure for ω-regular model checking:
 *   System ⊨ ¬φ  ⇔  L(A_sys × A_¬φ) = ∅
 */
bool nba_is_empty(const nba_t *nba);

/**
 * nba_is_universal — Decide whether L(A) = Σ^ω.
 *
 * Theorem: L(A) = Σ^ω iff L(¬A) = ∅.
 * Since complementation may be expensive, this is reduced to emptiness.
 * For small alphabets, we can also use a direct construction.
 */
bool nba_is_universal(const nba_t *nba);

/* ---------------------------------------------------------------------------
 * L2: NBA Set Operations
 * ------------------------------------------------------------------------- */

/**
 * nba_union — Construct NBA for L(A1) ∪ L(A2).
 *
 * Standard construction: disjoint union of state sets + new initial state
 * with ε-transitions to both original initial states. We eliminate
 * ε-transitions by replicating initial transitions.
 *
 * Complexity: O(|A1| + |A2|). Result size: |Q1|+|Q2|+1 states.
 */
bool nba_union(const nba_t *a1, const nba_t *a2, nba_t *result);

/**
 * nba_intersection — Construct NBA for L(A1) ∩ L(A2).
 *
 * Product construction: states are pairs (q1, q2). A run is accepting if
 * both component runs are accepting, which requires visiting accepting
 * states of both automata infinitely often.
 *
 * Standard technique: 3-copy construction. Track whether we've seen
 * accepting states of each component. We use a simpler approach:
 * two copies — the run is accepting if we visit F1 in copy 0 and then
 * F2 in copy 1, infinitely often (or vice versa).
 *
 * Complexity: O(|A1| · |A2|). Result uses at most 3·|Q1|·|Q2| states.
 */
bool nba_intersection(const nba_t *a1, const nba_t *a2, nba_t *result);

/**
 * nba_complement — Complement an NBA.
 *
 * This is the hard case. Deterministic Büchi automata (DBA) are not
 * closed under complement. NBA complementation requires exponential
 * blowup in general.
 *
 * Classical construction: Büchi's original (1962) uses Ramsey's theorem
 * and is doubly exponential. Safra (1988) gives 2^O(n log n) states.
 * Here we implement Schewe's (2009) optimal 2^n construction.
 *
 * Complexity: O(2^n) states in worst case.
 */
bool nba_complement(const nba_t *nba, nba_t *result);

/**
 * nba_include — Decide whether L(A1) ⊆ L(A2).
 *
 * Theorem: L(A1) ⊆ L(A2) iff L(A1) ∩ L(¬A2) = ∅.
 *
 * For tractable cases, use this reduction.
 */
bool nba_includes(const nba_t *a1, const nba_t *a2);

/* ---------------------------------------------------------------------------
 * L5: Emptiness per Emptiness — Couvreur's algorithm detail
 * ------------------------------------------------------------------------- */

/**
 * nba_emptiness_couvreur — Emptiness check using Couvreur's on-the-fly
 * algorithm (FM'99). Same correctness as nba_is_empty but can be used
 * during on-the-fly model checking without constructing the full product.
 *
 * States that are "detected" as part of an accepting SCC are accumulated.
 * Returns: count of accepting SCCs found (> 0 means non-empty).
 */
int nba_emptiness_couvreur(const nba_t *nba, int *accepting_scc_count);

/**
 * nba_accepting_cycle_find — Given a known-nonempty NBA, extract a witness
 * in the form of a reachable accepting state lying on a cycle.
 *
 * Returns: true and fills q_accept, prefix run, cycle run.
 */
bool nba_accepting_cycle_find(const nba_t *nba,
                              int *q_accept, nba_run_t *lasso);

/**
 * nba_trim — Remove states that are not reachable from the initial state
 * or that cannot reach an accepting state (and are thus useless).
 *
 * Returns: number of states removed.
 */
int nba_trim(nba_t *nba);

/* ---------------------------------------------------------------------------
 * Omega-Regular Expression to NBA (Buchi Theorem)
 * ------------------------------------------------------------------------- */

/**
 * ore_to_nba -- Convert an omega-regular expression to an equivalent NBA.
 *
 * Constructive proof of Buchi's Theorem (1962): every omega-regular
 * language is recognizable by an NBA.
 */
struct ore_node;
bool ore_to_nba(const struct ore_node *expr, size_t alphabet_size, nba_t *out);

#endif /* BUCHI_AUTOMATON_H */
