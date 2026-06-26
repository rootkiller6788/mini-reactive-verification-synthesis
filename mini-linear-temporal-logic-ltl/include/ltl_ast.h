/*
 * ltl_ast.h — Linear Temporal Logic (LTL) Abstract Syntax Tree
 *
 * L1 Definitions:
 *   LTL (Pnueli 1977) extends propositional logic with temporal operators
 *   for specifying properties of infinite sequences (ω-words).
 *
 *   Syntax (φ, ψ are LTL formulas; p is an atomic proposition):
 *     φ ::= true | false | p
 *         | ¬φ | φ ∧ ψ | φ ∨ ψ | φ → ψ | φ ↔ ψ
 *         | X φ (Next)
 *         | F φ (Future / Eventually)
 *         | G φ (Globally / Always)
 *         | φ U ψ (Until)
 *         | φ R ψ (Release)
 *         | φ W ψ (Weak Until)
 *
 * L2 Core Concepts:
 *   LTL formulas are interpreted over infinite traces σ = s₀ s₁ s₂ ...
 *   where each sᵢ is a set of atomic propositions that hold at step i.
 *   - X φ holds at step i iff φ holds at step i+1
 *   - F φ holds iff φ holds at some step j ≥ i
 *   - G φ holds iff φ holds at every step j ≥ i
 *   - φ U ψ holds iff ψ holds at some step j ≥ i, and φ holds at all k ∈ [i, j)
 *   - φ R ψ holds iff ψ holds up to and including the step where φ first holds
 *   - φ W ψ ≡ (φ U ψ) ∨ G φ  (weak until: φ may hold forever)
 *
 * L3 Mathematical Structure:
 *   LTL formulas form the set of star-free ω-regular expressions
 *   (McNaughton & Papert 1971, Thomas 1979, Kamp 1968).
 *
 * Key Theorems:
 *   - LTL ≡ First-Order Logic over (ℕ, <)  (Kamp 1968)
 *   - LTL ≡ Star-Free ω-Regular Languages  (Thomas 1979)
 *   - LTL satisfiability is PSPACE-complete (Sistla & Clarke 1985)
 *   - LTL model checking is PSPACE-complete  (Sistla & Clarke 1985)
 *
 * Reference:
 *   Pnueli 1977 — The temporal logic of programs (FOCS)
 *   Manna & Pnueli 1992 — The Temporal Logic of Reactive and Concurrent Systems
 *   Baier & Katoen 2008 — Principles of Model Checking (Ch. 5)
 *   Clarke, Grumberg, Peled 1999 — Model Checking (Ch. 2)
 *
 * Courses:
 *   MIT 6.841 (Advanced Complexity), Stanford CS254, Berkeley CS278,
 *   CMU 15-855, Princeton COS 522, ETH 252-0400
 */

#ifndef LTL_AST_H
#define LTL_AST_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ── LTL Formula Node Types ───────────────────────────────────── */
/*
 * Each constructor corresponds to a distinct semantic operator.
 * The types encode the entire BNF of LTL syntax.
 */
typedef enum {
    LTL_TRUE        = 0,   /* ⊤ : verum (always true) */
    LTL_FALSE       = 1,   /* ⊥ : falsum (always false) */
    LTL_ATOM        = 2,   /* p  : atomic proposition (indexed by int) */
    LTL_NOT         = 3,   /* ¬φ : logical negation */
    LTL_AND         = 4,   /* φ ∧ ψ : logical conjunction */
    LTL_OR          = 5,   /* φ ∨ ψ : logical disjunction */
    LTL_IMPLIES     = 6,   /* φ → ψ : logical implication */
    LTL_EQUIV       = 7,   /* φ ↔ ψ : logical equivalence */
    LTL_NEXT        = 8,   /* X φ (○ φ) : next state */
    LTL_FINALLY     = 9,   /* F φ (◇ φ) : eventually / in the future */
    LTL_GLOBALLY    = 10,  /* G φ (□ φ) : globally / always */
    LTL_UNTIL       = 11,  /* φ U ψ : strong until */
    LTL_RELEASE     = 12,  /* φ R ψ : release (dual of until) */
    LTL_WEAK_UNTIL  = 13   /* φ W ψ : weak until / unless */
} LtlNodeType;

/* Human-readable names for each node type */
const char* ltl_node_type_name(LtlNodeType t);
int         ltl_node_is_temporal(LtlNodeType t);   /* X, F, G, U, R, W */
int         ltl_node_is_binary(LtlNodeType t);     /* ∧, ∨, →, ↔, U, R, W */
int         ltl_node_is_unary(LtlNodeType t);      /* ¬, X, F, G */
int         ltl_node_is_leaf(LtlNodeType t);       /* true, false, atom */

/* ── LTL Formula AST Node ─────────────────────────────────────── */
typedef struct LtlNode LtlNode;

struct LtlNode {
    LtlNodeType type;
    int         atom_idx;       /* for LTL_ATOM: index into proposition set */
    LtlNode*    left;           /* left child (unary: operand) */
    LtlNode*    right;          /* right child (binary: second operand) */
    int         id;             /* unique node id (for hashing/comparison) */
    LtlNode*    next;           /* linked list for subformula management */
};

/*
 * Global atom counter for unique IDs.
 * In a real system this would be per-formula; here we use a simple counter.
 */
void   ltl_reset_id_counter(void);
int    ltl_next_id(void);

/* ── Formula Constructors ─────────────────────────────────────── */
/*
 * Each constructor creates a node of the corresponding type.
 * Memory is allocated via malloc; caller must free with ltl_free().
 * All constructors return NULL on allocation failure.
 */

LtlNode* ltl_mk_true(void);
LtlNode* ltl_mk_false(void);
LtlNode* ltl_mk_atom(int atom_idx);
LtlNode* ltl_mk_not(LtlNode* phi);
LtlNode* ltl_mk_and(LtlNode* phi, LtlNode* psi);
LtlNode* ltl_mk_or(LtlNode* phi, LtlNode* psi);
LtlNode* ltl_mk_implies(LtlNode* phi, LtlNode* psi);
LtlNode* ltl_mk_equiv(LtlNode* phi, LtlNode* psi);
LtlNode* ltl_mk_next(LtlNode* phi);
LtlNode* ltl_mk_finally(LtlNode* phi);
LtlNode* ltl_mk_globally(LtlNode* phi);
LtlNode* ltl_mk_until(LtlNode* phi, LtlNode* psi);
LtlNode* ltl_mk_release(LtlNode* phi, LtlNode* psi);
LtlNode* ltl_mk_weak_until(LtlNode* phi, LtlNode* psi);

/*
 * Convenience: create nested conjunction/disjunction from array.
 * ltl_mk_and_n([φ₁, φ₂, ..., φₙ]) = φ₁ ∧ φ₂ ∧ ... ∧ φₙ
 * ltl_mk_or_n([φ₁, φ₂, ..., φₙ])  = φ₁ ∨ φ₂ ∨ ... ∨ φₙ
 */
LtlNode* ltl_mk_and_n(LtlNode** phis, int n);
LtlNode* ltl_mk_or_n(LtlNode** phis, int n);

/*
 * Convenience: create nested Next from depth k.
 * ltl_mk_next_k(φ, k) = X^k φ  (k nested X operators)
 */
LtlNode* ltl_mk_next_k(LtlNode* phi, int k);

/* ── Formula Destruction & Copying ────────────────────────────── */
void     ltl_free(LtlNode* node);
LtlNode* ltl_clone(const LtlNode* node);

/* ── Formula Queries ──────────────────────────────────────────── */
int      ltl_depth(const LtlNode* node);       /* max nesting depth */
int      ltl_size(const LtlNode* node);        /* number of nodes */
int      ltl_temporal_depth(const LtlNode* node); /* max nested temporal operators */
int      ltl_count_operators(const LtlNode* node, LtlNodeType t); /* count of specific op */
int      ltl_is_literal(const LtlNode* node);   /* atom or negated atom */
int      ltl_has_subformula(const LtlNode* haystack, const LtlNode* needle);

/* ── Formula Comparison ───────────────────────────────────────── */
/*
 * Structural equality: two formulas are equal iff they have identical
 * tree structure and identical atom indices.
 */
int      ltl_equals(const LtlNode* a, const LtlNode* b);

/*
 * Syntactic subsumption: φ ⊆s ψ if φ is a syntactic subformula of ψ
 * (including reflexive case φ ⊆s φ).
 */
int      ltl_is_syntactic_subformula(const LtlNode* sub, const LtlNode* super);

/* ── Formula I/O ──────────────────────────────────────────────── */
/*
 * Pretty-print formula in various notations:
 *   - infix:  "p0 ∧ G (p1 → X p2)"
 *   - prefix: "(AND (ATOM 0) (GLOBALLY (IMPLIES (ATOM 1) (NEXT (ATOM 2)))))"
 *   - dot:    Graphviz DOT format for visualization
 */
void     ltl_print(const LtlNode* node);
void     ltl_print_prefix(const LtlNode* node);
void     ltl_print_dot(const LtlNode* node, const char* filename);

/* Atomic proposition name lookup for printing */
void     ltl_set_atom_names(const char** names, int n);
const char* ltl_get_atom_name(int idx);

/* ── Subformula Collection ────────────────────────────────────── */
/*
 * Collect all subformulas into a set (no duplicates).
 * Used in tableau construction, closure computation.
 */
typedef struct {
    LtlNode** formulas;
    int       count;
    int       capacity;
} LtlSubformulaSet;

void     ltl_subformula_set_init(LtlSubformulaSet* s, int capacity);
void     ltl_subformula_set_add(LtlSubformulaSet* s, LtlNode* f);
int      ltl_subformula_set_contains(const LtlSubformulaSet* s, const LtlNode* f);
void     ltl_subformula_set_free(LtlSubformulaSet* s);
void     ltl_collect_subformulas(const LtlNode* root, LtlSubformulaSet* out);

/* ── Fischer-Ladner Closure ───────────────────────────────────── */
/*
 * The Fischer-Ladner closure (Fischer & Ladner 1979) of an LTL formula
 * contains all formulas relevant for satisfiability/model checking.
 * For PDL it's defined via a set of recursive rules; for LTL:
 *   FL(φ) is the smallest set containing φ closed under:
 *   1. subformula closure (if ψ₁ op ψ₂ ∈ FL, then ψ₁, ψ₂ ∈ FL)
 *   2. temporal unfolding (if Xψ ∈ FL, then ψ ∈ FL)
 *   3. Until unfolding: if ψ₁ U ψ₂ ∈ FL, then X(ψ₁ U ψ₂) ∈ FL
 *
 * Used in tableau-based decision procedures.
 */
void     ltl_fischer_ladner_closure(const LtlNode* phi, LtlSubformulaSet* out);

/* ── Formula Parser (simple s-expression input) ───────────────── */
/*
 * Parse an LTL formula from an s-expression string.
 * Example: "(U (ATOM 0) (G (ATOM 1)))" parses to p₀ U G p₁.
 *
 * This enables loading formulas from files for model checking.
 */
LtlNode* ltl_parse(const char* sexpr);
char*    ltl_to_sexpr(const LtlNode* node);

#endif /* LTL_AST_H */
