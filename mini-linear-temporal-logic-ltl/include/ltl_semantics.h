/*
 * ltl_semantics.h — LTL Semantics over Infinite Traces & Kripke Structures
 *
 * L1 Definitions:
 *   An infinite trace (ω-word) over atomic propositions AP is a function
 *     σ : ℕ → 2^AP
 *   Each σ(i) is the set of propositions true at step i.
 *
 * L2 Core Concepts — Satisfaction Relation σ, i ⊨ φ:
 *   σ, i ⊨ p       iff  p ∈ σ(i)
 *   σ, i ⊨ ¬φ      iff  σ, i ⊭ φ
 *   σ, i ⊨ φ ∧ ψ   iff  σ, i ⊨ φ and σ, i ⊨ ψ
 *   σ, i ⊨ X φ     iff  σ, i+1 ⊨ φ
 *   σ, i ⊨ F φ     iff  ∃j ≥ i : σ, j ⊨ φ
 *   σ, i ⊨ G φ     iff  ∀j ≥ i : σ, j ⊨ φ
 *   σ, i ⊨ φ U ψ   iff  ∃j ≥ i : σ, j ⊨ ψ and ∀k ∈ [i, j) : σ, k ⊨ φ
 *   σ, i ⊨ φ R ψ   iff  ∀j ≥ i : σ, j ⊨ ψ or ∃k ∈ [i, j) : σ, k ⊨ φ
 *                       (equivalently: ψ U (φ ∧ ψ) ∨ G ψ)
 *   σ, i ⊨ φ W ψ   iff  σ, i ⊨ φ U ψ or σ, i ⊨ G φ
 *
 *   σ ⊨ φ (σ satisfies φ) iff σ, 0 ⊨ φ.
 *
 * L3 Mathematical Structures:
 *   - Trace: finite representation of infinite path (lasso: prefix · cycle^ω)
 *   - Kripke Structure: M = (S, →, L) where S is states, → ⊆ S×S total,
 *     L : S → 2^AP labels states.
 *   - Path: infinite sequence of states from a Kripke structure.
 *
 * L4 Fundamental Laws:
 *   - Duality:  F φ ≡ ¬G ¬φ,   G φ ≡ ¬F ¬φ
 *   - Until-Release Duality: ¬(φ U ψ) ≡ (¬φ) R (¬ψ)
 *   - Weak Until: φ W ψ ≡ (φ U ψ) ∨ G φ ≡ (G φ) ∨ (φ U ψ)
 *   - Expansion: φ U ψ ≡ ψ ∨ (φ ∧ X(φ U ψ))
 *                G φ ≡ φ ∧ X(G φ)
 *                F φ ≡ φ ∨ X(F φ)
 *
 * L6 Canonical Problems:
 *   - LTL Model Checking: Given Kripke structure M and LTL formula φ,
 *     does every path from every initial state satisfy ¬φ?
 *     (i.e., M ⊨ φ iff ∀π starting from init: π ⊨ φ)
 *
 * Reference:
 *   Manna & Pnueli 1992 — The Temporal Logic of Reactive and Concurrent Systems
 *   Baier & Katoen 2008 — Principles of Model Checking (Ch. 5.1)
 *   Clarke, Grumberg, Peled 1999 — Model Checking (Ch. 2)
 *   Vardi & Wolper 1986 — An automata-theoretic approach to program verification
 */

#ifndef LTL_SEMANTICS_H
#define LTL_SEMANTICS_H

#include "ltl_ast.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ── Proposition Set Representation ───────────────────────────── */
/*
 * A proposition set is a bitmask over {0, ..., max_props-1}.
 * We use uint64_t for up to 64 atomic propositions.
 */
#define LTL_MAX_PROPS 64

typedef uint64_t LtlPropSet;

#define LTL_PROP_EMPTY     ((LtlPropSet)0)
#define LTL_PROP_CONTAINS(set, idx)  (((set) >> (idx)) & 1ULL)
#define LTL_PROP_ADD(set, idx)       ((set) | (1ULL << (idx)))
#define LTL_PROP_REMOVE(set, idx)    ((set) & ~(1ULL << (idx)))
#define LTL_PROP_UNION(a, b)         ((a) | (b))
#define LTL_PROP_INTER(a, b)         ((a) & (b))

void ltl_propset_print(LtlPropSet set, int max_idx);

/* ── Infinite Trace (ω-word over 2^AP) ────────────────────────── */
/*
 * An infinite trace is represented finitely as a lasso:
 *   prefix · cycle^ω
 *
 * prefix:  s₀ s₁ ... s_{p-1}   (finite prefix)
 * cycle:   s_p s_{p+1} ... s_{p+c-1}  (repeating cycle)
 *
 * Total trace: prefix then cycle repeated infinitely.
 * If prefix_len = 0, it's a purely periodic word.
 *
 * This is the standard representation since every ω-regular word
 * accepted by a finite automaton can be represented as a lasso
 * (Büchi 1962, ultimately periodic word).
 */
typedef struct {
    LtlPropSet* prefix;       /* first p steps */
    int         prefix_len;
    LtlPropSet* cycle;        /* repeating c steps */
    int         cycle_len;
} LtlTrace;

/*
 * Create/destroy traces.
 * ltl_trace_create_lasso: creates trace from prefix + cycle arrays.
 * ltl_trace_create_single: single infinite state (cycle of length 1, no prefix).
 * ltl_trace_empty: empty trace (for initial state allocation).
 */
LtlTrace* ltl_trace_create_lasso(const LtlPropSet* prefix, int plen,
                                  const LtlPropSet* cycle, int clen);
LtlTrace* ltl_trace_create_single(LtlPropSet state);
LtlTrace* ltl_trace_create_empty(void);
void      ltl_trace_free(LtlTrace* trace);

/*
 * Get the proposition set at step i (zero-indexed).
 * For i < prefix_len: returns prefix[i].
 * For i ≥ prefix_len: returns cycle[(i - prefix_len) % cycle_len].
 */
LtlPropSet ltl_trace_get(const LtlTrace* trace, int step);

/*
 * Print the trace up to max_steps (default: 20).
 */
void       ltl_trace_print(const LtlTrace* trace, int max_steps);

/*
 * Check if two traces agree up to N steps.
 */
int        ltl_trace_equals_up_to(const LtlTrace* a, const LtlTrace* b, int n);

/*
 * Clone a trace.
 */
LtlTrace*  ltl_trace_clone(const LtlTrace* trace);

/* ── LTL Satisfaction over a Trace ────────────────────────────── */
/*
 * Core semantic function: evaluate σ, i ⊨ φ.
 *
 * Parameters:
 *   trace  — the infinite trace σ
 *   step   — the current position i (0-based)
 *   phi    — the LTL formula φ
 *   max_depth — recursion bound (prevents infinite loops);
 *               set to -1 for unbounded (may diverge on G/F with
 *               non-lasso traces)
 *
 * Returns: 1 if σ, i ⊨ φ, 0 otherwise.
 *
 * Time complexity:
 *   For lasso traces and bounded temporal depth: O(|trace| · |φ|)
 *   For arbitrary unbounded evaluation on lassos: O(|prefix| + |cycle| · |φ|)
 *
 * Note: For truly unbounded evaluation (G φ on a non-lasso trace),
 * this may not terminate. Use the model checking algorithm instead.
 */
int ltl_evaluate(const LtlTrace* trace, int step, const LtlNode* phi,
                 int max_depth);

/*
 * Top-level satisfaction: σ ⊨ φ  iff  σ, 0 ⊨ φ.
 */
int ltl_satisfies(const LtlTrace* trace, const LtlNode* phi);

/*
 * Bounded semantics: σ, i ⊨ₖ φ — check satisfaction within first k steps.
 * This is used in runtime verification (monitoring).
 * For safety properties, bounded violation ⊆ actual violation.
 */
int ltl_evaluate_bounded(const LtlTrace* trace, int step,
                          const LtlNode* phi, int bound);

/*
 * Step-by-step evaluation: given the current set of "pending"
 * subformulas (a state in the tableau), and the next observed
 * proposition set, compute the next set of pending subformulas.
 *
 * This is the core of the LTL monitoring algorithm
 * (Manna & Pnueli 1992, Geilen 2001, Bauer et al. 2011).
 */

/* A set of LTL formulas (for tableau state) */
typedef struct {
    LtlNode** formulas;
    int       count;
    int       capacity;
} LtlFormulaSet;

void ltl_formula_set_init(LtlFormulaSet* fs, int cap);
void ltl_formula_set_add(LtlFormulaSet* fs, LtlNode* f);
void ltl_formula_set_free(LtlFormulaSet* fs);
int  ltl_formula_set_equal(const LtlFormulaSet* a, const LtlFormulaSet* b);
void ltl_formula_set_print(const LtlFormulaSet* fs);

/*
 * Monitor step: given current monitoring state (formula set) and
 * next observed proposition set, compute the next monitoring state.
 * Returns 1 on success, 0 if formula reduces to false (violation).
 */
int ltl_monitor_step(LtlFormulaSet* current, LtlPropSet observed,
                     LtlFormulaSet* next);

/* ── Kripke Structure ─────────────────────────────────────────── */
/*
 * A Kripke structure M = (S, I, →, L) where:
 *   S  — finite set of states {0, 1, ..., n_states-1}
 *   I  — set of initial states
 *   →  — total transition relation (every state has ≥1 successor)
 *   L  — labeling function L : S → 2^AP
 */
typedef struct {
    int         n_states;       /* |S| */
    int*        initial;        /* I: initial state indices */
    int         n_initial;
    int*        successors;     /* flat adjacency list: for each state s,
                                   successors[offset[s]]...successors[offset[s]+degree[s]-1] */
    int*        offset;         /* start index in successors for each state */
    int*        degree;         /* out-degree of each state */
    LtlPropSet* labels;         /* L(s) for each state */
    char**      state_names;    /* optional: human-readable state names */
    int         n_props;        /* number of atomic propositions */
} LtlKripke;

/*
 * Create a Kripke structure with n_states.
 * Initially: no initial states, no transitions, empty labels.
 */
LtlKripke* ltl_kripke_create(int n_states);
void       ltl_kripke_free(LtlKripke* K);

/* Set the initial states */
void       ltl_kripke_set_initial(LtlKripke* K, const int* states, int n);

/* Add a directed edge s → t */
int        ltl_kripke_add_edge(LtlKripke* K, int s, int t);

/* Set the label of state s */
void       ltl_kripke_set_label(LtlKripke* K, int s, LtlPropSet label);

/* Ensure the transition relation is total (add self-loops to deadlocks) */
void       ltl_kripke_make_total(LtlKripke* K);

/* Convenience factory: build Kripke structure from edge list */
LtlKripke* ltl_kripke_from_edges(int n_states, const int* edges, int n_edges,
                                  const int* initial, int n_initial,
                                  const LtlPropSet* labels);

/* Begin/end edge building (for incremental construction) */
int        ltl_kripke_begin_build(LtlKripke* K);
int        ltl_kripke_end_build(LtlKripke* K);
int        ltl_kripke_add_edge_int(LtlKripke* K, int s, int t);

/* Print the Kripke structure */
void       ltl_kripke_print(const LtlKripke* K);
void       ltl_kripke_print_dot(const LtlKripke* K, const char* filename);

/* ── Paths in Kripke Structures ───────────────────────────────── */
/*
 * A path π in Kripke structure M is an infinite sequence s₀ s₁ s₂ ...
 * such that s₀ ∈ I and ∀i ≥ 0: sᵢ → sᵢ₊₁.
 *
 * We represent paths as lassos (prefix · cycle^ω) for finite representation.
 */
typedef struct {
    int* prefix;
    int  prefix_len;
    int* cycle;
    int  cycle_len;
} LtlPath;

LtlPath* ltl_path_create(const int* prefix, int plen,
                           const int* cycle, int clen);
void     ltl_path_free(LtlPath* path);
LtlTrace* ltl_path_to_trace(const LtlPath* path, const LtlKripke* K);
void     ltl_path_print(const LtlPath* path);

/* ── LTL Model Checking (Explicit-State) ──────────────────────── */
/*
 * Check M ⊨ φ (universal satisfaction):
 *   For every path π starting from every initial state,
 *   the trace L(π) satisfies φ.
 *
 * Equivalently: M ⊨ φ  iff  ¬∃π starting from init such that π ⊨ ¬φ.
 *
 * This function implements explicit-state LTL model checking by:
 *   1. Negating the formula: ψ = ¬φ
 *   2. Constructing a generalized Büchi automaton for ψ
 *   3. Computing the product M × A_ψ
 *   4. Checking emptiness of the product
 *
 * Returns:
 *   1 if M ⊨ φ (property holds)
 *   0 if M ⊭ φ (property violated)
 *
 * If violated, counterexample path is stored in *cex_path (if non-NULL).
 * Caller must free with ltl_path_free().
 */
int ltl_model_check(const LtlKripke* K, const LtlNode* phi,
                    LtlPath** cex_path);

/*
 * Simplified explicit model checking for small formulas:
 * Enumerate all lasso paths up to a given depth bound.
 * Only reliable for small state spaces; used for demonstration.
 */
int ltl_model_check_enumerate(const LtlKripke* K, const LtlNode* phi,
                               int max_prefix, int max_cycle,
                               LtlPath** cex_path);

#endif /* LTL_SEMANTICS_H */
