/*
 * buchi_emptiness.h — Emptiness Checking for Büchi Automata
 *
 * L4 Fundamental Theorem:
 *   The emptiness problem for NBA is in NLOGSPACE (hence decidable).
 *   L(A) ≠ ∅  iff  there exists a reachable accepting state
 *   that lies on a cycle (i.e., lies in a non-trivial accepting SCC).
 *
 * L5 Algorithms:
 *   Algorithm 1 (SCC-based): Compute SCC decomposition of the
 *     transition graph. Check if any non-trivial SCC contains an
 *     accepting state. O(|Q|+|δ|) time (Tarjan's SCC algorithm).
 *
 *   Algorithm 2 (Nested DFS / Courcoubetis et al. 1992):
 *     First DFS: find all reachable accepting states.
 *     Second DFS (nested from each accepting state found):
 *       search for a cycle back to the accepting state.
 *     Returns: counterexample lasso if non-empty.
 *     O(|Q|+|δ|) time, used in SPIN model checker.
 *
 *   Algorithm 3 (On-the-fly emptiness / Holzmann 1997):
 *     Perform nested DFS while generating transitions on demand.
 *     Used for large automata (e.g., from LTL translation).
 *
 * Courcoubetis, Vardi, Wolper, Yannakakis (CAV 1990, FMSD 1992):
 *   Memory-efficient algorithms for verification of temporal properties
 *
 * Reference:
 *   Baier & Katoen 2008 — Principles of Model Checking (Ch. 4.4)
 *   Holzmann 2004 — The SPIN Model Checker
 *   Clarke, Grumberg, Peled 1999 — Model Checking (Ch. 2)
 *
 * Courses: MIT 6.841, Stanford CS254, CMU 15-855
 */

#ifndef BUCHI_EMPTINESS_H
#define BUCHI_EMPTINESS_H

#include "buchi.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

/* ── Core emptiness checking ───────────────────────────────── */
/*
 * Returns 1 if L(A) = ∅, 0 otherwise.
 * Uses SCC-based method (Tarjan's strongly connected components).
 * O(|Q| + |δ|) time complexity.
 */
int buchi_is_empty(const BuchiAutomaton* A);

/* ── Counterexample extraction ─────────────────────────────── */
/*
 * If L(A) ≠ ∅, extract an accepting lasso:
 *   prefix (to accepting SCC) · cycle (within accepting SCC)
 *
 * The lasso is a finite representation of an ω-word w such that w ∈ L(A).
 * Returns NULL if A is empty. Caller must free with omega_free().
 */
OmegaWord* buchi_find_accepting_lasso(const BuchiAutomaton* A);

/* ── Nested DFS method ─────────────────────────────────────── */
/*
 * Implementation of the Courcoubetis-Vardi-Wolper-Yannakakis
 * nested depth-first search for emptiness + counterexample.
 *
 * Two-phase DFS:
 *   Phase 1 (blue DFS): postorder traversal, mark states visited
 *   Phase 2 (red DFS): from accepting states, search for cycle
 *
 * This is the algorithm used in SPIN.
 */
typedef struct {
    /* Blue DFS (outer) */
    uint8_t* blue_visited;     /* states visited in outer DFS */
    uint8_t* red_visited;      /* states visited in inner DFS */

    /* Counterexample tracking */
    int*     stack_blue;       /* outer DFS stack */
    int      stack_blue_top;
    int*     stack_red;        /* inner DFS stack */
    int      stack_red_top;

    /* Result */
    int      found_cycle;      /* 1 if accepting cycle found */
    int*     prefix;           /* path from q0 to accepting state */
    int      prefix_len;
    int*     cycle;            /* cycle from accepting state to itself */
    int      cycle_len;
    int      accepting_state;  /* the accepting state in the cycle */
} NestedDFS;

NestedDFS* nested_dfs_create(int n_states);
void       nested_dfs_free(NestedDFS* nd);
int        nested_dfs_check(const BuchiAutomaton* A, NestedDFS* nd);
OmegaWord* nested_dfs_extract_lasso(const NestedDFS* nd, const BuchiAutomaton* A);

/* ── Generalized Büchi Emptiness ────────────────────────────── */
/*
 * Generalized Büchi Automaton (GBA) has k acceptance sets F₁,...,Fₖ.
 * A run is accepting iff it visits EACH Fᵢ infinitely often.
 *
 * Emptiness for GBA can be reduced to NBA emptiness via:
 *   (a) Degeneralization: GBA → NBA (|Q|·k states)
 *   (b) SCC-based: check SCC with all acceptance sets
 *   (c) Nested DFS for GBA (Tauriainen 2004)
 *
 * Here we implement SCC-based approach for GBA.
 */
typedef void* GBAHandle; /* forward: generalized Büchi automaton */

/* Check whether there exists a run visiting all acceptance sets ∞-often */
int gba_is_empty(const BuchiAutomaton* A, const BuchiState** F_sets,
                 const int* F_sizes, int n_F_sets);

/* SCC-based GBA counterexample: find a reachable SCC that
 * intersects all acceptance sets */
OmegaWord* gba_find_accepting_lasso(const BuchiAutomaton* A,
                                     const BuchiState** F_sets,
                                     const int* F_sizes, int n_F_sets);

/* ── Degeneralization ──────────────────────────────────────── */
/*
 * Convert Generalized Büchi Automaton (k acceptance sets)
 * to standard NBA (1 acceptance set).
 *
 * Standard construction:
 *   States: Q × {0,1,...,k-1}
 *   The counter i tracks "next acceptance set to visit."
 *   On visiting F_i, increment counter (mod k).
 *   Accepting: reach counter = k-1 after visiting F_{k-1}.
 *
 * Optimized (Tauriainen 2003):
 *   Remove impossible states, use SCC information.
 *
 * Complexity: O(k·|Q|) states, O(k·|δ|) transitions
 */

typedef struct {
    BuchiAutomaton* base;      /* original automaton graph */
    int             n_F_sets;
    BuchiState**    F_sets;
    int*            F_sizes;
} GBA;

GBA* gba_create(const BuchiAutomaton* base,
                int n_F_sets);
void gba_add_accepting_set(GBA* gba, int set_idx,
                            const BuchiState* states, int n);
void gba_free(GBA* gba);

BuchiAutomaton* gba_degeneralize(const GBA* gba);

/* ── Optimal degeneralization (Tauriainen 2006) ────────────── */
/*
 * Improved degeneralization that uses the fewest states possible.
 * Instead of k copies on every state, only add copies when
 * necessary (based on which acceptance sets are reachable from
 * each state).
 */
BuchiAutomaton* gba_degeneralize_optimized(const GBA* gba);

/* ── Emptiness verification statistics ─────────────────────── */
typedef struct {
    int    n_states;
    int    n_trans;
    int    n_sccs;
    int    n_nontrivial_sccs;
    int    n_accepting_sccs;
    int    nested_dfs_steps_blue;
    int    nested_dfs_steps_red;
    int    found_empty;
    double time_ms;
} EmptinessReport;

EmptinessReport buchi_emptiness_detailed(const BuchiAutomaton* A);
void            buchi_emptiness_report_print(const EmptinessReport* r);

#endif /* BUCHI_EMPTINESS_H */
