/**
 * ltl_syntax.h ¡ª LTL Formula Syntax, Parsing, and Transformations
 *
 * Implements Linear Temporal Logic formula construction, parsing from
 * string representation, semantic evaluation on traces, and essential
 * transformations (negation normal form, closure, subformula enumeration).
 *
 * Reference: Pnueli (1977) "The Temporal Logic of Programs"
 *            Vardi & Wolper (1986) "An Automata-Theoretic Approach
 *            to Automatic Program Verification"
 *
 * Knowledge Level: L1 Definitions, L5 Algorithms
 */

#ifndef LTL_SYNTAX_H
#define LTL_SYNTAX_H

#include "reactive_types.h"

/* ====================================================================
 * LTL Formula Construction (Smart Constructors with Sharing)
 * ==================================================================== */

/**
 * Construct a constant TRUE formula node.
 * Returns a shared singleton node.
 * Time: O(1) */
ltl_formula_t *ltl_true(void);

/**
 * Construct a constant FALSE formula node.
 * Returns a shared singleton node.
 * Time: O(1) */
ltl_formula_t *ltl_false(void);

/**
 * Construct a propositional variable formula node.
 * @param var_id  index of the atomic proposition
 * Time: O(1) */
ltl_formula_t *ltl_var(int32_t var_id);

/**
 * Construct negation: NOT(phi).
 * Applies simplifications: NOT(NOT(x)) ¡ú x, NOT(TRUE) ¡ú FALSE, etc.
 * Time: O(1) after simplifications */
ltl_formula_t *ltl_not(ltl_formula_t *phi);

/**
 * Construct conjunction: phi AND psi.
 * Applies simplifications: AND(x, TRUE)=x, AND(x, FALSE)=FALSE, etc.
 * Time: O(1) */
ltl_formula_t *ltl_and(ltl_formula_t *phi, ltl_formula_t *psi);

/**
 * Construct disjunction: phi OR psi.
 * Implemented via De Morgan: OR(x,y) = NOT(AND(NOT(x), NOT(y)))
 * Time: O(1) */
ltl_formula_t *ltl_or(ltl_formula_t *phi, ltl_formula_t *psi);

/**
 * Construct implication: phi ¡ú psi.
 * Implemented as: OR(NOT(phi), psi)
 * Time: O(1) */
ltl_formula_t *ltl_implies(ltl_formula_t *phi, ltl_formula_t *psi);

/**
 * Construct equivalence: phi ? psi.
 * Implemented as: AND(IMPLIES(phi,psi), IMPLIES(psi,phi))
 * Time: O(1) */
ltl_formula_t *ltl_equiv(ltl_formula_t *phi, ltl_formula_t *psi);

/**
 * Construct NEXT: X phi.
 * Time: O(1) */
ltl_formula_t *ltl_next(ltl_formula_t *phi);

/**
 * Construct EVENTUALLY: F phi.
 * Implemented as: TRUE U phi
 * Time: O(1) */
ltl_formula_t *ltl_eventually(ltl_formula_t *phi);

/**
 * Construct ALWAYS: G phi.
 * Implemented as: NOT(F(NOT(phi)))
 * Time: O(1) */
ltl_formula_t *ltl_always(ltl_formula_t *phi);

/**
 * Construct UNTIL: phi U psi.
 * Time: O(1) */
ltl_formula_t *ltl_until(ltl_formula_t *phi, ltl_formula_t *psi);

/**
 * Construct RELEASE: phi R psi.
 * Dual of UNTIL: NOT((NOT phi) U (NOT psi))
 * Time: O(1) */
ltl_formula_t *ltl_release(ltl_formula_t *phi, ltl_formula_t *psi);

/**
 * Construct WEAK UNTIL: phi W psi.
 * Defined as: (phi U psi) OR (G phi)
 * Time: O(1) */
ltl_formula_t *ltl_weak_until(ltl_formula_t *phi, ltl_formula_t *psi);

/**
 * Construct STRONG RELEASE: phi M psi.
 * Dual of W: (phi R psi) | (psi U phi)... defined as psi U (phi & psi) | G psi
 * Time: O(1) */
ltl_formula_t *ltl_strong_release(ltl_formula_t *phi, ltl_formula_t *psi);

/* ====================================================================
 * Memory Management
 * ==================================================================== */

/**
 * Increment reference count for formula sharing.
 * Time: O(1) */
ltl_formula_t *ltl_clone(ltl_formula_t *phi);

/**
 * Decrement reference count; free if zero (recursive).
 * Time: O(|phi|) */
void ltl_free(ltl_formula_t *phi);

/* ====================================================================
 * Formula Analysis
 * ==================================================================== */

/**
 * Compute the size (number of nodes) of the formula DAG.
 * Time: O(|phi|) */
int32_t ltl_size(const ltl_formula_t *phi);

/**
 * Compute the temporal depth (maximum nesting of temporal operators).
 * Time: O(|phi|) */
int32_t ltl_temporal_depth(const ltl_formula_t *phi);

/**
 * Collect all atomic propositions (variable ids) used in the formula.
 * Returns a bitmap in a valuation_t where bit i=1 iff var i occurs.
 * Time: O(|phi|) */
valuation_t ltl_collect_propositions(const ltl_formula_t *phi);

/**
 * Check if two formulas are syntactically identical (by node pointer).
 * For semantic equality, use formula canonicalization first.
 * Time: O(1) */
bool ltl_equal(const ltl_formula_t *a, const ltl_formula_t *b);

/* ====================================================================
 * Formula Transformations
 * ==================================================================== */

/**
 * Push all negations inward to the propositional level, eliminating
 * IMPLIES and EQUIV. Result is in Negation Normal Form (NNF).
 *
 * NNF: negation only appears in front of atomic propositions.
 * This is the first step in LTL-to-automaton translation.
 *
 * Time: O(|phi|)   Reference: De Giacomo & Vardi (2013) */
ltl_formula_t *ltl_to_nnf(const ltl_formula_t *phi);

/**
 * Compute the Fischer-Ladner closure of a formula.
 * The closure contains all subformulas and their negations (in NNF),
 * needed for tableau and automaton constructions.
 *
 * Returns an array of formula pointers (caller frees array, not formulas).
 * The output parameter `closure_size` receives the count.
 *
 * Time: O(|phi|^2)   Reference: Fischer & Ladner (1979) */
ltl_formula_t **ltl_fischer_ladner_closure(const ltl_formula_t *phi,
                                            int32_t *closure_size);

/**
 * Expand the formula according to the LTL expansion laws:
 *   phi U psi = psi OR (phi AND X(phi U psi))
 *   F phi     = phi OR X(F phi)
 *   G phi     = phi AND X(G phi)
 *
 * Returns an equivalent formula with temporal operators under X.
 * This is the key step in tableau-based synthesis.
 *
 * Time: O(|phi|)   Reference: Lichtenstein & Pnueli (1985) */
ltl_formula_t *ltl_expand(const ltl_formula_t *phi);

/**
 * Compute the set of subformulas of phi (including phi itself).
 * Returns an array of formula pointers (caller frees array).
 *
 * Time: O(|phi|) */
ltl_formula_t **ltl_subformulas(const ltl_formula_t *phi, int32_t *count);

/* ====================================================================
 * Semantic Evaluation on Finite Traces
 * ==================================================================== */

/**
 * Evaluate an LTL formula on a finite trace (with position index).
 * This is the bounded semantics: evaluate phi at position `pos` of the trace.
 *
 * For temporal operators at the end of the trace:
 *   X phi ¡ú false (no next state exists)
 *   phi U psi ¡ú psi must hold at current position
 *   F phi ¡ú phi holds at some position >= pos
 *   G phi ¡ú phi holds at all positions from pos to trace_end
 *
 * @param phi       the LTL formula to evaluate
 * @param trace     array of valuations (one per step)
 * @param trace_len length of the trace
 * @param pos       current position in the trace (0-indexed)
 * @return true if the formula holds at position pos
 *
 * Time: O(|phi| * trace_len) in worst case */
bool ltl_evaluate_bounded(const ltl_formula_t *phi,
                           const valuation_t *trace,
                           int32_t trace_len,
                           int32_t pos);

/* ====================================================================
 * LTL Formula Printing
 * ==================================================================== */

/**
 * Print an LTL formula in infix notation to a string buffer.
 * Returns the buffer for chaining.
 *
 * Time: O(|phi|) */
char *ltl_to_string(const ltl_formula_t *phi, char *buf, int32_t buf_size);

/**
 * Pretty-print an LTL formula to stdout.
 * Time: O(|phi|) */
void ltl_print(const ltl_formula_t *phi);

/**
 * Parse an LTL formula from a string representation.
 *
 * Grammar:
 *   formula := atomic | "!" formula | formula "&" formula | formula "|" formula
 *            | formula "->" formula | formula "<->" formula
 *            | "X" formula | "F" formula | "G" formula
 *            | formula "U" formula | formula "R" formula
 *            | "(" formula ")" | "true" | "false"
 *   atomic  := "p"[0-9]+ | "q"[0-9]+ | "r"[0-9]+ | "a"[0-9]+ | "b"[0-9]+
 *
 * Variable names map to numeric ids internally.
 *
 * @param str        null-terminated string to parse
 * @param var_count  output: number of distinct variables found
 * @return parsed formula, or NULL on syntax error
 *
 * Time: O(n) where n = strlen(str) */
ltl_formula_t *ltl_parse(const char *str, int32_t *var_count);

#endif /* LTL_SYNTAX_H */
