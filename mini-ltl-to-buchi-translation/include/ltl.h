/**
 * @file ltl.h
 * @brief Linear Temporal Logic (LTL) — syntax, semantics, and formula manipulation
 *
 * LTL is a modal temporal logic with modalities referring to time.
 * It was introduced by Amir Pnueli (1977) for specifying properties of
 * reactive systems. LTL formulas are interpreted over infinite sequences
 * of states (ω-words).
 *
 * Syntax (in BNF):
 *   φ ::= ⊤ | ⊥ | p | ¬φ | φ₁∧φ₂ | φ₁∨φ₂ | φ₁→φ₂
 *        | X φ  (neXt)
 *        | F φ  (Finally / Eventually)
 *        | G φ  (Globally / Always)
 *        | φ₁ U φ₂  (Until)
 *        | φ₁ R φ₂  (Release, dual of Until)
 *        | φ₁ W φ₂  (Weak Until)
 *        | φ₁ M φ₂  (Strong Release)
 *
 * Semantics (over ω-words w = w₀w₁w₂...):
 *   w,i ⊨ p         iff p ∈ L(wᵢ)
 *   w,i ⊨ ¬φ        iff w,i ⊭ φ
 *   w,i ⊨ φ₁∧φ₂    iff w,i ⊨ φ₁ and w,i ⊨ φ₂
 *   w,i ⊨ X φ       iff w,i+1 ⊨ φ
 *   w,i ⊨ φ₁ U φ₂  iff ∃k≥i: w,k ⊨ φ₂ and ∀j, i≤j<k: w,j ⊨ φ₁
 *   w,i ⊨ F φ       iff ∃k≥i: w,k ⊨ φ     (= true U φ)
 *   w,i ⊨ G φ       iff ∀k≥i: w,k ⊨ φ     (= ¬F¬φ)
 *   w,i ⊨ φ₁ R φ₂  iff ∀k≥i: w,k ⊨ φ₂ or ∃j, i≤j<k: w,j ⊨ φ₁
 *
 * Textbook Reference:
 *   - Pnueli, "The Temporal Logic of Programs" (FOCS 1977)
 *   - Manna & Pnueli, "The Temporal Logic of Reactive and Concurrent Systems" (1992)
 *   - Baier & Katoen, "Principles of Model Checking" (2008), Ch.5
 *   - Clarke, Grumberg & Peled, "Model Checking" (1999), Ch.9
 *
 * Course Alignment:
 *   - MIT 6.045/18.400: Automata, Computability, Complexity → temporal logics
 *   - CMU 15-414: Bug Catching — Automated Program Verification (LTL model checking)
 *   - Oxford: Computer-Aided Formal Verification (LTL-to-Büchi)
 *   - ETH 263-2800: Model Checking (LTL, CTL, automata)
 */

#ifndef LTL_H
#define LTL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * L1: Core Definitions — LTL formula types and operators
 * ========================================================================== */

/**
 * LTL operator / node type enumeration.
 *
 * Covers all standard LTL temporal and Boolean connectives.
 * The set is functionally complete: X, U (and Boolean connectives)
 * suffice to express all LTL-definable properties.
 */
typedef enum {
    LTL_CONST_TRUE,          /**< ⊤  — Verum / True                    */
    LTL_CONST_FALSE,         /**< ⊥  — Falsum / False                  */
    LTL_ATOM,                /**< p  — Atomic proposition               */
    LTL_NOT,                 /**< ¬φ — Negation                        */
    LTL_AND,                 /**< φ∧ψ — Conjunction                    */
    LTL_OR,                  /**< φ∨ψ — Disjunction                    */
    LTL_IMPLIES,             /**< φ→ψ — Implication                    */
    LTL_EQUIV,               /**< φ↔ψ — Equivalence                    */
    LTL_XOR,                 /**< φ⊕ψ — Exclusive OR                   */
    LTL_NEXT,                /**< X φ  — Next state                    */
    LTL_FINALLY,             /**< F φ  — Eventually (Future)           */
    LTL_GLOBALLY,            /**< G φ  — Always (Globally)             */
    LTL_UNTIL,               /**< φ U ψ — Strong Until                 */
    LTL_RELEASE,             /**< φ R ψ — Release (dual of Until)      */
    LTL_WEAK_UNTIL,          /**< φ W ψ — Weak Until                   */
    LTL_STRONG_RELEASE,      /**< φ M ψ — Strong Release               */
    LTL_LITERAL_POS,         /**< positive literal (internal NNF use)   */
    LTL_LITERAL_NEG          /**< negative literal (internal NNF use)   */
} ltl_op_t;

/**
 * LTL formula node — a tree representing the parse tree of an LTL formula.
 *
 * Formulas are stored in DAG form with structural sharing for efficiency.
 * Each node carries:
 *   - op: the top-level operator
 *   - atom_id: for atomic propositions, the proposition index
 *   - left / right: subformula pointers (NULL for unary/const)
 *   - ref_count: reference count for memory management
 *   - closure_flag: used during Fischer-Ladner closure computation
 */
typedef struct ltl_formula {
    ltl_op_t          op;         /**< Top-level operator               */
    uint32_t          atom_id;    /**< Atomic proposition id (ATOM ops) */
    struct ltl_formula *left;     /**< Left subformula (or only child)  */
    struct ltl_formula *right;    /**< Right subformula                 */
    int               ref_count;  /**< Reference count                  */
    uint32_t          id;         /**< Unique formula id                */
    bool              flag;       /**< General-purpose flag             */
    uint32_t          closure_id; /**< Index in Fischer-Ladner closure  */
} ltl_formula_t;

/* ==========================================================================
 * L2: Core Concepts — Formula construction, manipulation, normalization
 * ========================================================================== */

/**
 * Formula construction API.
 *
 * All constructors return a new formula with ref_count = 1.
 * The caller owns the reference and must call ltl_free() when done.
 */
ltl_formula_t *ltl_true(void);
ltl_formula_t *ltl_false(void);
ltl_formula_t *ltl_atom(uint32_t atom_id);
ltl_formula_t *ltl_not(ltl_formula_t *phi);
ltl_formula_t *ltl_and(ltl_formula_t *phi, ltl_formula_t *psi);
ltl_formula_t *ltl_or(ltl_formula_t *phi, ltl_formula_t *psi);
ltl_formula_t *ltl_implies(ltl_formula_t *phi, ltl_formula_t *psi);
ltl_formula_t *ltl_equiv(ltl_formula_t *phi, ltl_formula_t *psi);
ltl_formula_t *ltl_xor(ltl_formula_t *phi, ltl_formula_t *psi);
ltl_formula_t *ltl_next(ltl_formula_t *phi);
ltl_formula_t *ltl_finally(ltl_formula_t *phi);
ltl_formula_t *ltl_globally(ltl_formula_t *phi);
ltl_formula_t *ltl_until(ltl_formula_t *phi, ltl_formula_t *psi);
ltl_formula_t *ltl_release(ltl_formula_t *phi, ltl_formula_t *psi);
ltl_formula_t *ltl_weak_until(ltl_formula_t *phi, ltl_formula_t *psi);
ltl_formula_t *ltl_strong_release(ltl_formula_t *phi, ltl_formula_t *psi);

/**
 * LTL derived operators — implemented via equivalences:
 *   F φ ≡ ⊤ U φ
 *   G φ ≡ ¬F¬φ ≡ ⊥ R φ
 *   φ W ψ ≡ (G φ) ∨ (φ U ψ)  ≡  φ U ψ ∨ G φ
 *   φ R ψ ≡ ¬(¬φ U ¬ψ)
 *   φ M ψ ≡ ¬(¬φ W ¬ψ)
 */

/** Create a deep copy of a formula (structure sharing not preserved). */
ltl_formula_t *ltl_clone(const ltl_formula_t *phi);

/** Free a formula and decrement reference counts of children. */
void ltl_free(ltl_formula_t *phi);

/** Increment reference count (for shared subformulas). */
ltl_formula_t *ltl_ref(ltl_formula_t *phi);

/** Number of nodes in the formula tree. */
size_t ltl_size(const ltl_formula_t *phi);

/** Depth of the formula tree (max nesting of operators). */
size_t ltl_depth(const ltl_formula_t *phi);

/** Number of distinct subformulas. */
size_t ltl_subformula_count(const ltl_formula_t *phi);

/** Set of atomic propositions appearing in phi (bitmask, max 32 atoms). */
uint32_t ltl_atoms_used(const ltl_formula_t *phi);

/* ==========================================================================
 * L3: Mathematical Structures — Negation Normal Form (NNF)
 *
 * An LTL formula is in NNF if negations only appear in front of
 * atomic propositions. Every LTL formula has an equivalent NNF.
 *
 * Transformation rules (push negations inward):
 *   ¬¬φ     → φ
 *   ¬(φ∧ψ)  → ¬φ ∨ ¬ψ
 *   ¬(φ∨ψ)  → ¬φ ∧ ¬ψ
 *   ¬X φ    → X ¬φ
 *   ¬F φ    → G ¬φ
 *   ¬G φ    → F ¬φ
 *   ¬(φUψ)  → (¬φ)R(¬ψ)
 *   ¬(φRψ)  → (¬φ)U(¬ψ)
 *   ¬(φWψ)  → (¬φ)M(¬ψ)
 *   ¬(φMψ)  → (¬φ)W(¬ψ)
 * ========================================================================== */

/** Convert formula to Negation Normal Form (destructive: returns new formula). */
ltl_formula_t *ltl_to_nnf(ltl_formula_t *phi);

/** Check if formula is in NNF. */
bool ltl_is_nnf(const ltl_formula_t *phi);

/* ==========================================================================
 * L4: Fundamental Laws — LTL identity and duality principles
 *
 * Duality pairs:
 *   F / G  :  F φ ≡ ¬G ¬φ
 *   U / R  :  φ U ψ ≡ ¬(¬φ R ¬ψ)
 *   W / M  :  φ W ψ ≡ ¬(¬φ M ¬ψ)
 *
 * Idempotence:
 *   F F φ ≡ F φ,   G G φ ≡ G φ
 *
 * Absorption:
 *   F G F φ ≡ G F φ
 *
 * Expansion laws (fixed-point characterizations):
 *   φ U ψ ≡ ψ ∨ (φ ∧ X(φ U ψ))
 *   φ R ψ ≡ ψ ∧ (φ ∨ X(φ R ψ))
 *   F φ   ≡ φ ∨ X (F φ)
 *   G φ   ≡ φ ∧ X (G φ)
 *
 * These expansion laws are the foundation of tableau-based
 * LTL-to-Büchi translation.
 * ========================================================================== */

/** Check semantic implication: does phi entail psi? (approximate via simplifications) */
bool ltl_implies_syntactic(const ltl_formula_t *phi, const ltl_formula_t *psi);

/** Compute expansion: return ψ∨(φ∧X(φUψ)) for phi U psi. */
ltl_formula_t *ltl_unfold_until(const ltl_formula_t *phi, const ltl_formula_t *psi);

/** Compute expansion: return ψ∧(φ∨X(φRψ)) for phi R psi. */
ltl_formula_t *ltl_unfold_release(const ltl_formula_t *phi, const ltl_formula_t *psi);

/** Apply simple simplification rules: ¬⊤→⊥, ¬⊥→⊤, ⊤∧φ→φ, etc. */
ltl_formula_t *ltl_simplify(ltl_formula_t *phi);

/* ==========================================================================
 * L5: Algorithms — Formula rewriting and manipulation
 * ========================================================================== */

/** Convert derived temporal operators to core set {X, U, ¬, ∧}. */
ltl_formula_t *ltl_to_core(ltl_formula_t *phi);

/** Remove all release operators using duality: φ R ψ ≡ ¬(¬φ U ¬ψ). */
ltl_formula_t *ltl_remove_release(ltl_formula_t *phi);

/** Remove weak-until: φ W ψ ≡ (G φ) ∨ (φ U ψ). */
ltl_formula_t *ltl_remove_weak_until(ltl_formula_t *phi);

/** Replace implication and equivalence with Boolean combinations. */
ltl_formula_t *ltl_remove_implies_equiv(ltl_formula_t *phi);

/** Compute the set of elementary subformulas (for tableau construction). */
size_t ltl_elementary_set(const ltl_formula_t *phi, ltl_formula_t ***out);

/** Substitute atom `from` with formula `to` throughout phi. */
ltl_formula_t *ltl_substitute(ltl_formula_t *phi, uint32_t from, ltl_formula_t *to);

/** Compute Fischer-Ladner closure: all subformulas needed for tableau. */
typedef struct {
    ltl_formula_t **formulas;
    size_t           count;
    size_t           capacity;
} ltl_closure_t;

ltl_closure_t *ltl_fischer_ladner_closure(const ltl_formula_t *phi);
void ltl_closure_free(ltl_closure_t *cl);

/** Check propositional consistency of a set of formulas (for tableau nodes). */
bool ltl_set_consistent(ltl_formula_t **formulas, size_t count);

/* ==========================================================================
 * L6: Canonical Problems — LTL formula analysis
 * ========================================================================== */

/** Check if formula is a safety property (⊨ ¬φ means finite prefix violation). */
bool ltl_is_safety(const ltl_formula_t *phi);

/** Check if formula is a liveness property (⊨ infinitely often). */
bool ltl_is_liveness(const ltl_formula_t *phi);

/** Check if formula is an obligation property (Boolean combination of safety+guarantee). */
bool ltl_is_obligation(const ltl_formula_t *phi);

/** Check if formula is a recurrence property (GF-type). */
bool ltl_is_recurrence(const ltl_formula_t *phi);

/** Check if formula is a persistence property (FG-type). */
bool ltl_is_persistence(const ltl_formula_t *phi);

/** Characterize the formula in the safety-progress hierarchy (Manna & Pnueli). */
typedef enum {
    LTL_CLASS_SAFETY,
    LTL_CLASS_GUARANTEE,
    LTL_CLASS_OBLIGATION,
    LTL_CLASS_RECURRENCE,
    LTL_CLASS_PERSISTENCE,
    LTL_CLASS_REACTIVITY
} ltl_property_class_t;

ltl_property_class_t ltl_classify(const ltl_formula_t *phi);

/* ==========================================================================
 * L7: Applications — Formula statistics and analysis utilities
 * ========================================================================== */

/** Compute temporal operator depth (ignoring Boolean operators). */
size_t ltl_temporal_depth(const ltl_formula_t *phi);

/** Count occurrences of each temporal operator. */
void ltl_operator_count(const ltl_formula_t *phi,
                        size_t *x_count, size_t *f_count, size_t *g_count,
                        size_t *u_count, size_t *r_count,
                        size_t *w_count, size_t *m_count);

/** Check if formula has no temporal operators (purely propositional). */
bool ltl_is_propositional(const ltl_formula_t *phi);

/** Check if two formulas are syntactically identical. */
bool ltl_equal(const ltl_formula_t *phi, const ltl_formula_t *psi);

/** Compute a hash of the formula structure (for hash consing). */
uint32_t ltl_hash(const ltl_formula_t *phi);

/** Pretty-print formula to string (caller owns buffer, must be ≥ 4096 bytes). */
int ltl_snprint(char *buf, size_t buf_size, const ltl_formula_t *phi);

/** Pretty-print formula to stdout. */
void ltl_print(const ltl_formula_t *phi);

/** Print formula in Spin/Promela LTL syntax. */
void ltl_print_spin(const ltl_formula_t *phi);

#ifdef __cplusplus
}
#endif

#endif /* LTL_H */
