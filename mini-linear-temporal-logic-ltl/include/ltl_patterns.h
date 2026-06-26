/*
 * ltl_patterns.h — LTL Specification Patterns for Reactive Systems
 *
 * L7 Applications:
 *   Dwyer, Avrunin, and Corbett (1999) catalogued 555 temporal logic
 *   specifications from real-world systems and identified recurring
 *   patterns. These patterns form a library of commonly-used LTL
 *   formulas for specifying reactive system properties.
 *
 * Pattern Classification (Dwyer et al. 1999):
 *
 *   Occurrence Patterns:
 *     - Absence:       a given state/event never occurs
 *     - Universality:  a given state/event always occurs
 *     - Existence:     a given state/event eventually occurs
 *     - Bounded Existence: event occurs at least k times
 *
 *   Order Patterns:
 *     - Precedence:    P precedes Q (Q occurs only if P occurred before)
 *     - Response:      P is followed by Q (request → response)
 *     - Chain Precedence: sequence of events in order
 *     - Chain Response:   sequence of events triggers response sequence
 *
 * Pattern Scopes (where the pattern must hold):
 *   - Globally:        entire execution
 *   - Before R:        up to the first occurrence of R
 *   - After Q:         after the first occurrence of Q
 *   - Between Q and R: between Q and the next R
 *   - After Q until R: after Q until R
 *
 * L2 Core Concept:
 *   These patterns bridge the gap between system requirements
 *   (written in natural language) and formal LTL specifications.
 *   They embody the observation that most real-world LTL formulas
 *   fall into a small number of semantic patterns.
 *
 * L6 Canonical Problems:
 *   Each pattern is a canonical formula scheme instantiated with
 *   specific atomic propositions to express a concrete property.
 *
 * Reference:
 *   Dwyer, Avrunin, Corbett 1999 — Patterns in Property Specifications
 *     for Finite-State Verification (ICSE)
 *   Dwyer, Avrunin, Corbett — Property Specification Patterns website
 *     (http://patterns.projects.cis.ksu.edu/)
 *   Manna & Pnueli 1992 — The Temporal Logic of Reactive and Concurrent Systems
 *   Baier & Katoen 2008 — Principles of Model Checking (Ch. 5.3)
 *
 * Courses: MIT 6.841, Stanford CS254, Berkeley CS278, Princeton COS 522
 */

#ifndef LTL_PATTERNS_H
#define LTL_PATTERNS_H

#include "ltl_ast.h"

/* ── Occurrence Patterns ──────────────────────────────────────── */
/*
 * Absence: "p never holds" (in a given scope).
 *
 * Globally:       G ¬p
 * Before r:       F r → (¬p U r)
 * After q:        G(q → G ¬p)
 * Between q and r: G((q ∧ ¬r ∧ F r) → (¬p U r))
 * After q until r: G(q → (¬p W r))
 */
LtlNode* ltl_pattern_absence_global(int p);
LtlNode* ltl_pattern_absence_before(int p, int r);
LtlNode* ltl_pattern_absence_after(int p, int q);
LtlNode* ltl_pattern_absence_between(int p, int q, int r);
LtlNode* ltl_pattern_absence_after_until(int p, int q, int r);

/*
 * Universality: "p always holds" (in a given scope).
 *
 * Globally:       G p
 * Before r:       F r → (p U r)
 * After q:        G(q → G p)
 * Between q and r: G((q ∧ ¬r ∧ F r) → (p U r))
 * After q until r: G(q → (p W r))
 */
LtlNode* ltl_pattern_universality_global(int p);
LtlNode* ltl_pattern_universality_before(int p, int r);
LtlNode* ltl_pattern_universality_after(int p, int q);
LtlNode* ltl_pattern_universality_between(int p, int q, int r);
LtlNode* ltl_pattern_universality_after_until(int p, int q, int r);

/*
 * Existence: "p eventually holds" (in a given scope).
 *
 * Globally:       F p
 * Before r:       ¬r W (p ∧ ¬r)      (p occurs before r, if at all)
 * After q:        G(¬q) ∨ F(q ∧ F p)
 * Between q and r: G((q ∧ ¬r) → (¬r W (p ∧ ¬r)))
 * After q until r: G(q → (¬r W (p ∧ ¬r)))
 */
LtlNode* ltl_pattern_existence_global(int p);
LtlNode* ltl_pattern_existence_before(int p, int r);
LtlNode* ltl_pattern_existence_after(int p, int q);
LtlNode* ltl_pattern_existence_between(int p, int q, int r);
LtlNode* ltl_pattern_existence_after_until(int p, int q, int r);

/* ── Order Patterns ───────────────────────────────────────────── */
/*
 * Response: "p is followed by q" (request → response).
 * Also known as: "leads-to", "p guarantees q eventually".
 *
 * Globally:       G(p → F q)
 * Before r:       F r → (p → (¬r U (q ∧ ¬r))) U r
 * After q0:       G(q0 → G(p → F q))
 * Between q0 and r: G((q0 ∧ ¬r ∧ F r) → (p → (¬r U (q ∧ ¬r))) U r)
 * After q0 until r: G(q0 → ((p → (¬r U (q ∧ ¬r))) W r))
 */
LtlNode* ltl_pattern_response_global(int p, int q);
LtlNode* ltl_pattern_response_before(int p, int q, int r);
LtlNode* ltl_pattern_response_after(int p, int q, int q0);
LtlNode* ltl_pattern_response_between(int p, int q, int q0, int r);
LtlNode* ltl_pattern_response_after_until(int p, int q, int q0, int r);

/*
 * Precedence: "p precedes q" (q only occurs if p occurred earlier).
 * Also known as: "p is necessary precondition for q".
 *
 * Globally:       (¬q) W p    or equivalently:  F q → (¬q U p)
 * Before r:       F r → (¬q U (p ∨ r))
 * After q0:       G(q0 → (¬q W p))
 * Between q0 and r: G((q0 ∧ ¬r ∧ F r) → (¬q U (p ∨ r)))
 * After q0 until r: G(q0 → (¬q W (p ∨ r)))
 */
LtlNode* ltl_pattern_precedence_global(int p, int q);
LtlNode* ltl_pattern_precedence_before(int p, int q, int r);
LtlNode* ltl_pattern_precedence_after(int p, int q, int q0);
LtlNode* ltl_pattern_precedence_between(int p, int q, int q0, int r);
LtlNode* ltl_pattern_precedence_after_until(int p, int q, int q0, int r);

/* ── Recurrence Patterns ──────────────────────────────────────── */
/*
 * Infinitely Often: "p holds infinitely often".
 *   G F p   (Globally, eventually p)
 *
 * Eventually Always: "p holds eventually and then permanently".
 *   F G p   (Eventually, globally p)
 */
LtlNode* ltl_pattern_infinitely_often(int p);
LtlNode* ltl_pattern_eventually_always(int p);

/*
 * Bounded Response: "p is followed by q within at most k steps".
 *   G(p → (q ∨ X(q ∨ X(q ∨ ... X q)...)))   with k X operators
 *
 * This is a safety property (can be checked on finite prefixes).
 */
LtlNode* ltl_pattern_bounded_response(int p, int q, int k);

/*
 * k-Response: "p holds at least k times"
 *   F(p ∧ X F(p ∧ X F(... p)...)) with k F-X-F... patterns
 */
LtlNode* ltl_pattern_at_least_k(int p, int k);

/* ── Mutual Exclusion / Synchronization Patterns ──────────────── */
/*
 * Mutual Exclusion: "p and q are never true simultaneously"
 *   G ¬(p ∧ q)
 */
LtlNode* ltl_pattern_mutex(int p, int q);

/*
 * Alternating: "p and q strictly alternate starting with p"
 *   p ∧ G(p → X(¬p ∧ X q)) ∧ G(q → X(¬q ∧ X p))
 * Simplified: G(p → X(¬p W q)) ∧ G(q → X(¬q W p))
 */
LtlNode* ltl_pattern_alternating(int p, int q);

/*
 * Strict Sequencing: "sequence p, q, r occurs in order"
 *   F(p ∧ X F(q ∧ X F r))
 */
LtlNode* ltl_pattern_sequence(int* props, int n);

/* ── Liveness / Fairness Patterns ─────────────────────────────── */
/*
 * Weak Fairness: "if p is continuously enabled, eventually q"
 *   G F p → G F q   or equivalently:  F G ¬p ∨ G F q
 * Sometimes written: G(G F enabled → F executed)
 */
LtlNode* ltl_pattern_weak_fairness(int enabled, int executed);

/*
 * Strong Fairness: "if p is enabled infinitely often, eventually q"
 *   G F p → G F q
 * More commonly: G F enabled → G F executed
 */
LtlNode* ltl_pattern_strong_fairness(int enabled, int executed);

/*
 * Compassion (strong fairness constraint for reactive synthesis):
 *   (G F enabled) → (G F executed)
 * This is identical to strong fairness; included for completeness
 * with the reactive synthesis literature.
 */
LtlNode* ltl_pattern_compassion(int enabled, int executed);

/*
 * Justice (weak fairness constraint):
 *   (F G enabled) → (G F executed)
 * If continuously enabled from some point, executed infinitely often.
 */
LtlNode* ltl_pattern_justice(int enabled, int executed);

/* ── Real-Time Patterns (Bounded LTL) ─────────────────────────── */
/*
 * Timed Absence: "p never holds within the next k steps"
 *   G(¬p) for the next k steps:  ¬p ∧ X ¬p ∧ ... X^k ¬p
 */
LtlNode* ltl_pattern_timed_absence(int p, int k);

/*
 * Timed Response: "if p holds, q holds within next k steps"
 *   G(p → (q ∨ X(q ∨ X(... ∨ X q)...)))
 * Same as bounded response, kept as alias.
 */
LtlNode* ltl_pattern_timed_response(int p, int q, int k);

/*
 * Deadline: "p must hold before or at step k"
 *   F≤k p = p ∨ X p ∨ X² p ∨ ... ∨ X^k p
 */
LtlNode* ltl_pattern_deadline(int p, int k);

/* ── Template Instantiation ───────────────────────────────────── */
/*
 * Given a pattern template (function pointer) and an array of
 * proposition indices, instantiate and return the formula.
 * Used for programmatic construction of specifications.
 */
typedef LtlNode* (*LtlPatternFunc)(int p, int q);

/* ── Pattern Library Enumeration ──────────────────────────────── */
typedef enum {
    LTL_PAT_ABSENCE_GLOBAL = 0,
    LTL_PAT_UNIVERSALITY_GLOBAL,
    LTL_PAT_EXISTENCE_GLOBAL,
    LTL_PAT_RESPONSE_GLOBAL,
    LTL_PAT_PRECEDENCE_GLOBAL,
    LTL_PAT_INFINITELY_OFTEN,
    LTL_PAT_EVENTUALLY_ALWAYS,
    LTL_PAT_MUTEX,
    LTL_PAT_ALTERNATING,
    LTL_PAT_WEAK_FAIRNESS,
    LTL_PAT_STRONG_FAIRNESS,
    LTL_PAT_COMPASSION,
    LTL_PAT_JUSTICE,
    LTL_PAT_COUNT
} LtlPatternID;

const char* ltl_pattern_name(LtlPatternID id);
const char* ltl_pattern_description(LtlPatternID id);

/*
 * Create a formula by pattern ID with up to 3 parameters.
 * Parameters not used by the pattern are ignored.
 */
LtlNode* ltl_pattern_create(LtlPatternID id, int p, int q, int r);

/*
 * Create all standard patterns for given proposition indices
 * and return them as an array of named formulas.
 */
typedef struct {
    char*    name;
    LtlNode* formula;
} LtlNamedFormula;

int           ltl_pattern_library_create(int base_prop,
                                          LtlNamedFormula** out);
void          ltl_pattern_library_free(LtlNamedFormula* lib, int n);

#endif /* LTL_PATTERNS_H */
