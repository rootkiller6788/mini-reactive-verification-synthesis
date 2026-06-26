/*
 * omega_operations.h — Operations on Büchi Automata
 *
 * L4 Fundamental Laws:
 *   Closure Properties of ω-regular languages (Büchi 1962):
 *     1. Union: L₁ ∪ L₂ is ω-regular (product construction + GBA)
 *     2. Intersection: L₁ ∩ L₂ is ω-regular (product with fairness)
 *     3. Projection: ∃a.L is ω-regular (alphabet projection)
 *     4. Complement: ω-ω-regular class is closed under complement
 *        (Büchi 1962 via monadic second-order logic; Landweber 1969 direct)
 *
 * L5 Algorithms:
 *   Union construction        O(|A₁|×|A₂|) states
 *   Intersection construction O(|A₁|×|A₂|) states (generalized Büchi)
 *   Projection                O(|A|) states (hide symbols)
 *   Complementation via:
 *     - Ramsey-based: O(2^{O(n log n)})
 *     - Safra determinization + Rabin complement: O(2^{O(n log n)})
 *     - Optimal: O((0.96n)^n) (Schewe 2009)
 *
 * Reference:
 *   Büchi 1962 — Decision problems of S1S
 *   Thomas 1990 — Automata on infinite objects
 *   Vardi & Wolper 1986 — Automata Theoretic Techniques
 *   Safra 1988 — Complexity of ω-automata
 *
 * Courses: MIT 6.841, Stanford CS254, CMU 15-855
 */

#ifndef OMEGA_OPERATIONS_H
#define OMEGA_OPERATIONS_H

#include "buchi.h"
#include "omega_automata.h"

/* ── Union ────────────────────────────────────────────────── */
/*
 * Construct NBA for L(A₁) ∪ L(A₂).
 * States: Q₁ ∪ Q₂ ∪ {new q₀} (disjoint union)
 * Method: add new initial state with ε-transitions to both
 *         (since NBA allow nondeterminism, we resolve alphabet intersection)
 * Complexity: O(|Q₁|+|Q₂|) states, O(|δ₁|+|δ₂|+|Σ|) transitions
 */
BuchiAutomaton* buchi_union(const BuchiAutomaton* A1,
                             const BuchiAutomaton* A2);

/* ── Intersection ─────────────────────────────────────────── */
/*
 * Construct NBA for L(A₁) ∩ L(A₂).
 * States: Q₁ × Q₂ × {1,2} (flag tracks which acceptance seen recently)
 * Method: Generalized Büchi with 2 acceptance sets, then degeneralize.
 * Complexity: O(|Q₁|×|Q₂|) states
 *
 * Theorem (Choueka 1974): L(A₁×A₂) = L(A₁) ∩ L(A₂)
 * The product tracks both runs and uses a flag that flips
 * whenever the current automaton visits an accepting state.
 */
BuchiAutomaton* buchi_intersect(const BuchiAutomaton* A1,
                                 const BuchiAutomaton* A2);

/* ── Projection ────────────────────────────────────────────── */
/*
 * Given NBA A over alphabet Σ and a projection function
 * projecting Σ to Σ', construct NBA over Σ' for the
 * projected language: {prj(w) : w ∈ L(A)}.
 *
 * Method: For each transition on symbol a, add transition
 * on symbol prj(a). This works because NBA are nondeterministic
 * and only existence of some run matters.
 */
typedef BuchiSymbol (*SymbolProjection)(BuchiSymbol sym);
BuchiAutomaton* buchi_project(const BuchiAutomaton* A,
                               SymbolProjection proj,
                               const BuchiSymbol* new_alphabet,
                               int new_alpha_size);

/* ── Complementation ──────────────────────────────────────── */
/*
 * Construct NBA for Σ^ω \ L(A).
 *
 * Approach 1 (Lazy complement for small automata):
 *   Determinize to deterministic Rabin (Safra trees),
 *   complement Rabin condition, convert back to NBA.
 *
 * Approach 2 (Ramsey-based):
 *   Use Ramsey's theorem on transition profiles
 *   of finite subwords (Büchi's original approach).
 *
 * Approach 3 (Rank-based, Kupferman-Vardi 2001):
 *   Track the "ranks" of states in breakpoint construction.
 *   O(3^n) states but practical for moderate n.
 *
 * Here we implement the rank-based complementation
 * (Kupferman & Vardi, 2001), which is conceptually clean
 * and practical for up to ~10 states.
 */
BuchiAutomaton* buchi_complement_kv(const BuchiAutomaton* A);

/* ── Product with Kripke Structure ────────────────────────── */
/*
 * Given NBA A (property automaton) and a labeled transition
 * system, construct the synchronous product for verification.
 *
 * For model checking: M ⊧ φ  iff  L(M ⊗ A_{¬φ}) = ∅
 */
typedef struct {
    int        n_states;
    int        n_props;
    int*       labels;    /* labels[s] = bitmask of atomic propositions */
    int**      successors; /* successors[s] = dynamic array, -1 terminated */
} KripkeStructure;

BuchiAutomaton* buchi_product_ks(const BuchiAutomaton* A,
                                  const KripkeStructure* ks);

/* ── Language containment ──────────────────────────────────── */
/*
 * Check L(A₁) ⊆ L(A₂).
 * Method: L(A₁) ⊆ L(A₂)  iff  L(A₁) ∩ L(¬A₂) = ∅
 * So we check emptiness of intersect(A₁, complement(A₂)).
 */
int buchi_language_contained(const BuchiAutomaton* A1,
                              const BuchiAutomaton* A2);

/* ── Equivalence ───────────────────────────────────────────── */
int buchi_language_equivalent(const BuchiAutomaton* A1,
                               const BuchiAutomaton* A2);

/* ── Simplification ────────────────────────────────────────── */
/*
 * Remove states that are:
 *   (a) not reachable from q₀
 *   (b) cannot reach any accepting SCC
 * Such states cannot appear in any accepting run.
 */
BuchiAutomaton* buchi_trim(const BuchiAutomaton* A);

/* ── Size statistics ───────────────────────────────────────── */
typedef struct {
    int n_states;
    int n_trans;
    int n_accepting;
    int n_reachable;
    int n_sccs;
    int n_accepting_sccs;
} BuchiStats;

BuchiStats buchi_stats(const BuchiAutomaton* A);
void       buchi_stats_print(const BuchiStats* bs);

#endif /* OMEGA_OPERATIONS_H */
