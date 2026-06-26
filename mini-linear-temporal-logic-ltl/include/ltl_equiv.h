/*
 * ltl_equiv.h — LTL Equivalence Laws, Normal Forms & Simplification
 *
 * L4 Fundamental Laws:
 *   LTL has a rich algebraic structure of equivalences that enable
 *   formula normalization, simplification, and efficient model checking.
 *
 * Duality Laws (De Morgan for temporal operators):
 *   ¬X φ  ≡ X ¬φ
 *   ¬F φ  ≡ G ¬φ
 *   ¬G φ  ≡ F ¬φ
 *   ¬(φ U ψ) ≡ (¬φ) R (¬ψ)
 *   ¬(φ R ψ) ≡ (¬φ) U (¬ψ)
 *   ¬(φ W ψ) ≡ (¬ψ) U (¬φ ∧ ¬ψ)
 *
 * Expansion (Fixed-Point) Laws:
 *   F φ  ≡ φ ∨ X(F φ)            — φ eventually means φ now or later
 *   G φ  ≡ φ ∧ X(G φ)            — φ always means φ now and always in the future
 *   φ U ψ ≡ ψ ∨ (φ ∧ X(φ U ψ))  — Until expansion
 *   φ R ψ ≡ ψ ∧ (φ ∨ X(φ R ψ))  — Release expansion
 *   φ W ψ ≡ ψ ∨ (φ ∧ X(φ W ψ))  — Weak until expansion
 *
 * Idempotence:
 *   F F φ ≡ F φ                  — eventually-eventually = eventually
 *   G G φ ≡ G φ                  — always-always = always
 *
 * Absorption:
 *   F G F φ ≡ G F φ              — "infinitely often" pattern
 *   G F G φ ≡ F G φ              — "eventually always" pattern
 *
 * Distributive Laws:
 *   F(φ ∨ ψ) ≡ F φ ∨ F ψ         — F distributes over ∨
 *   G(φ ∧ ψ) ≡ G φ ∧ G ψ         — G distributes over ∧
 *   X(φ ∧ ψ) ≡ X φ ∧ X ψ         — X distributes over ∧
 *   X(φ ∨ ψ) ≡ X φ ∨ X ψ         — X distributes over ∨
 *   (φ₁ ∧ φ₂) U ψ ≡ (φ₁ U ψ) ∧ (φ₂ U ψ)  — U-left distributes ∧
 *   φ U (ψ₁ ∨ ψ₂) ≡ (φ U ψ₁) ∨ (φ U ψ₂)  — U-right distributes ∨
 *
 * L5 Algorithms:
 *   - Negation Normal Form (NNF): push negations to atomic level
 *   - Formula simplification via rewrite rules
 *   - Basic equivalence testing (syntactic normalization)
 *
 * Negation Normal Form:
 *   An LTL formula is in NNF if negation (¬) only appears directly
 *   before atomic propositions. Every LTL formula can be converted
 *   to an equivalent NNF formula using the duality laws.
 *
 *   NNF grammar:
 *     φ ::= p | ¬p | true | false | φ ∧ ψ | φ ∨ ψ
 *         | X φ | F φ | G φ | φ U ψ | φ R ψ | φ W ψ
 *
 * Reference:
 *   Manna & Pnueli 1992 — The Temporal Logic of Reactive and Concurrent Systems
 *   Somenzi & Bloem 2000 — Efficient Büchi automata from LTL formulae
 *   Etessami & Holzmann 2000 — Optimizing Büchi automata
 *   Baier & Katoen 2008 — Principles of Model Checking (Ch. 5)
 */

#ifndef LTL_EQUIV_H
#define LTL_EQUIV_H

#include "ltl_ast.h"

/* ── Negation Normal Form (NNF) ───────────────────────────────── */
/*
 * Convert formula to NNF: all negations pushed to atoms.
 * Uses duality laws:
 *   nnf(¬(φ ∧ ψ))    = nnf(¬φ) ∨ nnf(¬ψ)
 *   nnf(¬(φ ∨ ψ))    = nnf(¬φ) ∧ nnf(¬ψ)
 *   nnf(¬X φ)        = X nnf(¬φ)
 *   nnf(¬F φ)        = G nnf(¬φ)
 *   nnf(¬G φ)        = F nnf(¬φ)
 *   nnf(¬(φ U ψ))    = nnf(¬φ) R nnf(¬ψ)
 *   nnf(¬(φ R ψ))    = nnf(¬φ) U nnf(¬ψ)
 *   nnf(¬¬φ)         = nnf(φ)
 *
 * Returns a new formula; caller must free with ltl_free().
 */
LtlNode* ltl_to_nnf(const LtlNode* phi);

/* ── Formula Simplification ───────────────────────────────────── */
/*
 * Apply a set of rewrite rules to produce a simplified (shorter)
 * equivalent formula. Rewrite rules include:
 *
 * Boolean simplification:
 *   true ∧ φ  →  φ              false ∧ φ  →  false
 *   true ∨ φ  →  true           false ∨ φ  →  φ
 *   true → φ  →  φ              false → φ  →  true
 *   φ → true  →  true           φ → false  →  ¬φ
 *   ¬true     →  false          ¬false     →  true
 *
 * Temporal simplification:
 *   F true    →  true           G false    →  false
 *   X true    →  true           X false    →  false
 *   φ U true  →  true           φ U false  →  false
 *   true U φ  →  F φ            false U φ  →  φ
 *   φ R false →  false          φ R true   →  true
 *   true R φ  →  G φ            false R φ  →  φ
 *   F F φ     →  F φ            G G φ      →  G φ
 *   φ W true  →  true           φ W false  →  G φ
 *
 * Duplicate removal:
 *   φ ∧ φ  →  φ                 φ ∨ φ  →  φ
 *
 * Returns simplified formula; may be same as input if no simplifications apply.
 * The returned formula must be freed by caller with ltl_free().
 */
LtlNode* ltl_simplify(const LtlNode* phi);

/*
 * Aggressive simplification: repeatedly apply simplify until
 * fixpoint is reached. More expensive but produces minimal form.
 */
LtlNode* ltl_simplify_fixpoint(const LtlNode* phi);

/* ── Formula Rewriting ────────────────────────────────────────── */
/*
 * Eliminate derived operators, converting to core set:
 *   φ → ψ      →  ¬φ ∨ ψ
 *   φ ↔ ψ      →  (φ → ψ) ∧ (ψ → φ)
 *   φ W ψ      →  (φ U ψ) ∨ G φ
 *   φ R ψ      →  ψ W (φ ∧ ψ)     (Release as dual of Until via Weak)
 *   equivalently:  φ R ψ  →  ¬(¬φ U ¬ψ)
 *
 * Core operators after elimination: ¬, ∧, ∨, X, F, G, U
 *
 * This is useful before translation to Büchi automata where
 * fewer operators simplify the translation.
 *
 * Returns new formula; caller must free.
 */
LtlNode* ltl_eliminate_derived(const LtlNode* phi);

/*
 * Eliminate eventually and globally using Until:
 *   F φ  →  true U φ
 *   G φ  →  false R φ  →  ¬(true U ¬φ)
 *
 * Core operators after full elimination: ¬, ∧, X, U
 * (∨ can also be eliminated: φ ∨ ψ → ¬(¬φ ∧ ¬ψ))
 *
 * Returns new formula; caller must free.
 */
LtlNode* ltl_to_core_until(const LtlNode* phi);

/*
 * Eliminate release using until:
 *   φ R ψ  →  ¬(¬φ U ¬ψ)
 */
LtlNode* ltl_release_to_until(const LtlNode* phi);

/* ── Strictly Future Fragment ─────────────────────────────────── */
/*
 * Check if formula uses only future temporal operators
 * (X, F, G, U, R, W) — i.e., is a pure future LTL formula.
 * The standard LTL has only future operators; past operators
 * (Y, O, H, S, T) are in LTL+Past (L8 advanced topic).
 */
int ltl_is_future_only(const LtlNode* phi);

/* ── Safety Fragment Detection ────────────────────────────────── */
/*
 * L7/L8: Safety properties are those where every violation has a
 * finite witness (i.e., can be detected on a finite prefix).
 *
 * Syntactically, a formula is in the safety fragment if its NNF
 * contains only X, G, R, W (no U, no unguarded F).
 *
 * Safety properties: G φ, φ W ψ, G(¬a ∨ X G ¬b), etc.
 * Liveness properties: F φ, φ U ψ, G F φ, etc.
 *
 * The safety-liveness classification (Alpern & Schneider 1985):
 *   Every LTL property is the intersection of a safety and
 *   a liveness property.
 */
int ltl_is_safety_fragment(const LtlNode* phi);

/*
 * Check if formula is a liveness property syntactically.
 * Liveness: something good eventually happens.
 * Characterized by the presence of F or U in the NNF (not under G).
 */
int ltl_is_liveness_fragment(const LtlNode* phi);

/* ── Obligation Fragment ──────────────────────────────────────── */
/*
 * Obligation properties (Chang, Manna, Pnueli 1992):
 *   Boolean combination of safety and guarantee (co-safety) properties.
 *   Syntactically: positive Boolean combination of safety formulas.
 *
 * Obligation fragment is the lowest class in the reactivity hierarchy
 * (Manna & Pnueli 1990). Classes: Safety ⊂ Guarantee ⊂ Obligation
 * ⊂ Response ⊂ Persistence ⊂ Reactivity.
 */
int ltl_is_obligation_fragment(const LtlNode* phi);

/* ── Until/Release Duality Check ──────────────────────────────── */
/*
 * Verify that ¬(φ U ψ) is logically equivalent to (¬φ) R (¬ψ)
 * by constructing both formulas and comparing syntax after NNF.
 * Returns 1 if the duality holds structurally.
 */
int ltl_check_until_release_duality(const LtlNode* phi, const LtlNode* psi);

/* ── Expansion Law Verification ───────────────────────────────── */
/*
 * Construct and return the expanded form of a temporal formula
 * using the fixed-point expansion law:
 *
 *   expand(X φ)     = cannot expand (primitive)
 *   expand(F φ)     = φ ∨ X(F φ)
 *   expand(G φ)     = φ ∧ X(G φ)
 *   expand(φ U ψ)   = ψ ∨ (φ ∧ X(φ U ψ))
 *   expand(φ R ψ)   = ψ ∧ (φ ∨ X(φ R ψ))
 *   expand(φ W ψ)   = ψ ∨ (φ ∧ X(φ W ψ))
 *
 * For other node types, returns a clone.
 * This is used in tableau construction.
 */
LtlNode* ltl_expand(const LtlNode* phi);

/*
 * Full unwinding: expand a temporal formula k times.
 * ltl_unwind(F φ, 3) = φ ∨ X(φ ∨ X(φ ∨ X(F φ)))
 * For bounded model checking applications.
 */
LtlNode* ltl_unwind(const LtlNode* phi, int k);

/* ── Formula Comparison ───────────────────────────────────────── */
/*
 * Strong equivalence check: two formulas are equivalent iff for ALL
 * traces σ, σ ⊨ φ ↔ σ ⊨ ψ.
 *
 * This is PSPACE-complete in general (Sistla & Clarke 1985).
 * Our implementation uses a bounded trace enumeration:
 * for all lassos of bounded size, check agreement.
 * Returns 0 if counterexample found within bound, 1 if no
 * counterexample within bound (may be incomplete).
 */
int ltl_check_equivalence_bounded(const LtlNode* phi, const LtlNode* psi,
                                   int max_prefix, int max_cycle,
                                   int n_props);

/*
 * Syntactic equivalence after simplification: apply simplify_fixpoint
 * to both formulas and compare structurally. This is sound
 * (if equal → equivalent) but not complete.
 */
int ltl_syntactic_equiv(const LtlNode* phi, const LtlNode* psi);

#endif /* LTL_EQUIV_H */
