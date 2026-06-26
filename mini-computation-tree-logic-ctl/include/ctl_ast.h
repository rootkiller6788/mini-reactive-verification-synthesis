/**
 * ctl_ast.h — Computation Tree Logic (CTL) Abstract Syntax Tree
 *
 * CTL Syntax (Clarke, Emerson, Sistla 1986):
 *   State formulas:
 *     Φ ::= ⊤ | ⊥ | p | ¬Φ | Φ₁ ∧ Φ₂ | Φ₁ ∨ Φ₂ | Φ₁ → Φ₂ | Φ₁ ↔ Φ₂
 *         | AX Φ | EX Φ | AF Φ | EF Φ | AG Φ | EG Φ
 *         | A[Φ₁ U Φ₂] | E[Φ₁ U Φ₂] | A[Φ₁ R Φ₂] | E[Φ₁ R Φ₂]
 *
 * Path quantifiers: A (for all paths), E (there exists a path)
 * Temporal operators: X (next), F (eventually/future),
 *                      G (globally/always), U (until), R (release)
 *
 * Knowledge: L1 (Definitions), L3 (Mathematical Structures)
 * Reference: Baier & Katoen "Principles of Model Checking" (2008), Chapter 6
 */

#ifndef CTL_AST_H
#define CTL_AST_H

#include <stddef.h>

/* ─── Formula Type Enumeration ─────────────────────────────────────── */

/**
 * ctl_formula_type — classifies each CTL formula node in the AST.
 *
 * Base cases: TOP, BOT, ATOM
 * Boolean: NOT, AND, OR, IMPLIES, IFF
 * Existential path: EX, EF, EG, EU
 * Universal path:   AX, AF, AG, AU, AR, ER
 */
typedef enum {
    CTL_TOP,        /* ⊤  — verum / truth constant                          */
    CTL_BOT,        /* ⊥  — falsum / false constant                         */
    CTL_ATOM,       /* p  — atomic proposition (indexed by name)            */
    CTL_NOT,        /* ¬φ — negation                                        */
    CTL_AND,        /* φ ∧ ψ — conjunction                                  */
    CTL_OR,         /* φ ∨ ψ — disjunction                                  */
    CTL_IMPLIES,    /* φ → ψ — implication                                  */
    CTL_IFF,        /* φ ↔ ψ — biconditional / equivalence                  */
    CTL_EX,         /* EX φ — there exists a next state where φ holds      */
    CTL_AX,         /* AX φ — for all next states, φ holds                 */
    CTL_EF,         /* EF φ — there exists a path: φ eventually holds       */
    CTL_AF,         /* AF φ — for all paths: φ eventually holds             */
    CTL_EG,         /* EG φ — there exists a path: φ globally holds        */
    CTL_AG,         /* AG φ — for all paths: φ globally holds              */
    CTL_EU,         /* E[φ U ψ] — there exists a path: φ until ψ            */
    CTL_AU,         /* A[φ U ψ] — for all paths: φ until ψ                  */
    CTL_ER,         /* E[φ R ψ] — there exists a path: φ releases ψ         */
    CTL_AR          /* A[φ R ψ] — for all paths: φ releases ψ               */
} ctl_formula_type;

/* ─── Formula Node — Tagged Union ──────────────────────────────────── */

/**
 * ctl_formula — a single node in the CTL formula AST.
 *
 * Memory discipline:
 *   - Construct with ctl_mk_*() factory functions.
 *   - Destroy with ctl_free_formula() (recursive).
 *   - Formulas are immutable after construction.
 *
 * Unary nodes (NOT, EX, AX, EF, AF, EG, AG): use data.unary.child
 * Binary nodes (AND, OR, IMPLIES, IFF, EU, AU, ER, AR): use data.binary.{left,right}
 * Leaf nodes (TOP, BOT): no children
 * Atom node (ATOM): use data.atom_name
 */
typedef struct ctl_formula {
    ctl_formula_type type;

    union {
        /* Unary operators — one child subformula */
        struct {
            struct ctl_formula *child;
        } unary;

        /* Binary operators — two child subformulae */
        struct {
            struct ctl_formula *left;
            struct ctl_formula *right;
        } binary;

        /* Atomic proposition — string identifier (owned) */
        char *atom_name;
    } data;
} ctl_formula;

/* ─── Factory Functions (Constructors) ─────────────────────────────── */

/** Create constant ⊤ (true).  O(1). */
ctl_formula *ctl_mk_top(void);

/** Create constant ⊥ (false).  O(1). */
ctl_formula *ctl_mk_bot(void);

/** Create atomic proposition with a copy of `name`. O(n). */
ctl_formula *ctl_mk_atom(const char *name);

/** Create negation ¬child. Result owns child (do not free child separately). */
ctl_formula *ctl_mk_not(ctl_formula *child);

/** Create conjunction child1 ∧ child2. Result owns both children. */
ctl_formula *ctl_mk_and(ctl_formula *child1, ctl_formula *child2);

/** Create disjunction child1 ∨ child2. Result owns both children. */
ctl_formula *ctl_mk_or(ctl_formula *child1, ctl_formula *child2);

/** Create implication child1 → child2. Result owns both children. */
ctl_formula *ctl_mk_implies(ctl_formula *child1, ctl_formula *child2);

/** Create biconditional child1 ↔ child2. Result owns both children. */
ctl_formula *ctl_mk_iff(ctl_formula *child1, ctl_formula *child2);

/* ─── Temporal Operator Constructors ───────────────────────────────── */

/** Create EX φ — existential next. */
ctl_formula *ctl_mk_ex(ctl_formula *child);

/** Create AX φ — universal next. */
ctl_formula *ctl_mk_ax(ctl_formula *child);

/** Create EF φ — existential eventually. */
ctl_formula *ctl_mk_ef(ctl_formula *child);

/** Create AF φ — universal eventually (inevitability). */
ctl_formula *ctl_mk_af(ctl_formula *child);

/** Create EG φ — existential globally. */
ctl_formula *ctl_mk_eg(ctl_formula *child);

/** Create AG φ — universal globally (invariance). */
ctl_formula *ctl_mk_ag(ctl_formula *child);

/** Create E[φ U ψ] — existential until. */
ctl_formula *ctl_mk_eu(ctl_formula *left, ctl_formula *right);

/** Create A[φ U ψ] — universal until. */
ctl_formula *ctl_mk_au(ctl_formula *left, ctl_formula *right);

/** Create E[φ R ψ] — existential release. */
ctl_formula *ctl_mk_er(ctl_formula *left, ctl_formula *right);

/** Create A[φ R ψ] — universal release. */
ctl_formula *ctl_mk_ar(ctl_formula *left, ctl_formula *right);

/* ─── Destructor ───────────────────────────────────────────────────── */

/** Recursively free a formula and all its children. Safe to call on NULL. */
void ctl_free_formula(ctl_formula *f);

/* ─── Utility Functions ────────────────────────────────────────────── */

/** Return a deep copy of a formula. O(|f|). */
ctl_formula *ctl_clone_formula(const ctl_formula *f);

/** Return the height (maximum nesting depth) of a formula tree. */
size_t ctl_formula_height(const ctl_formula *f);

/**
 * Return the size (total number of nodes) of a formula tree.
 * Includes all subformula nodes. Leaf node = size 1.
 */
size_t ctl_formula_size(const ctl_formula *f);

/**
 * Return the set of atomic proposition names used in f.
 * `names` is an array of `capacity` char* pointers.
 * Returns number of atoms found. If `names` is NULL, just counts.
 * The names point into the formula's internal storage — do not free.
 */
size_t ctl_collect_atoms(const ctl_formula *f, const char **names, size_t capacity);

/**
 * Return 1 iff two formulas are structurally identical (same AST shape).
 * Does not perform semantic equivalence checking.
 * Atoms compared by string equality.
 */
int ctl_formula_equal(const ctl_formula *a, const ctl_formula *b);

/**
 * Return 1 iff `sub` is a subformula of `f`.
 * A formula is considered a subformula of itself.  O(|f|·|sub|) worst case.
 */
int ctl_is_subformula(const ctl_formula *f, const ctl_formula *sub);

/**
 * Return the set of immediate subformulas. For unary ops, the child;
 * for binary ops, left and right; for atoms/bool, returns empty set.
 * `out` buffer of size `capacity`. Returns count written.
 */
size_t ctl_immediate_subformulas(const ctl_formula *f, ctl_formula **out, size_t capacity);

/**
 * Return a human-readable name for a formula type.
 * Example: CTL_AX → "AX", CTL_EU → "EU", CTL_ATOM → "ATOM".
 */
const char *ctl_formula_type_name(ctl_formula_type t);

/**
 * Pretty-print a formula to a string buffer in infix notation.
 * Writes at most `size` bytes (including null terminator).
 * Returns number of bytes that would have been written (excluding null).
 * Uses minimal parentheses based on operator precedence.
 *
 * Precedence (highest to lowest):
 *   ¬, AX, EX, AF, EF, AG, EG
 *   ∧
 *   ∨
 *   →, ↔
 *   AU, EU, AR, ER
 */
int ctl_snprint_formula(char *buf, size_t size, const ctl_formula *f);

/**
 * Write formula to stdout in a readable format.
 */
void ctl_print_formula(const ctl_formula *f);

/**
 * Hash a formula for use in hash tables. Structural hash.
 * Two structurally equal formulas produce the same hash.
 */
unsigned long ctl_formula_hash(const ctl_formula *f);

/**
 * Normalize atom ordering in a formula for canonical representation.
 * Atoms are sorted alphabetically. Used before caching/set operations.
 * Returns a new formula (caller owns).
 */
ctl_formula *ctl_canonicalize_formula(const ctl_formula *f);

/**
 * Convert formula to Positive Normal Form (PNF).
 * In PNF, negation appears only before atomic propositions.
 * Uses De Morgan laws for temporal operators:
 *   ¬AX φ ≡ EX ¬φ,  ¬EX φ ≡ AX ¬φ
 *   ¬AF φ ≡ EG ¬φ,  ¬EF φ ≡ AG ¬φ
 *   ¬AG φ ≡ EF ¬φ,  ¬EG φ ≡ AF ¬φ
 *   ¬A[φ U ψ] ≡ E[¬φ R ¬ψ],  ¬E[φ U ψ] ≡ A[¬φ R ¬ψ]
 *   ¬A[φ R ψ] ≡ E[¬φ U ¬ψ],  ¬E[φ R ψ] ≡ A[¬φ U ¬ψ]
 * Returns a new formula (caller owns). Does not modify input.
 */
ctl_formula *ctl_to_pnf(const ctl_formula *f);

/**
 * Compute the set of temporal operators used in a formula.
 * Returns a bitmask: bit i set if operator type i is used.
 * Temporal operators: EX=bit0, AX=bit1, EF=bit2, AF=bit3,
 *                      EG=bit4, AG=bit5, EU=bit6, AU=bit7,
 *                      ER=bit8, AR=bit9
 */
unsigned int ctl_temporal_operators_used(const ctl_formula *f);

/**
 * Return 1 iff the formula is a CTL state formula (syntactically valid).
 * A state formula is one where every temporal operator is immediately
 * preceded by a path quantifier (A or E).
 */
int ctl_is_state_formula(const ctl_formula *f);

/**
 * Compute the alternation depth of a CTL formula.
 * Alternation depth measures nesting of A/E quantifiers.
 * Base case: 0 for atomic/boolean.
 * Max of children for same quantifier; max+1 for switching quantifier.
 * Key parameter in complexity of model checking (Emerson-Lei 1986).
 */
int ctl_alternation_depth(const ctl_formula *f);

#endif /* CTL_AST_H */
