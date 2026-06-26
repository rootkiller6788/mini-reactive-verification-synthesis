/*
 * ltl_patterns.c — LTL Specification Patterns (Dwyer, Avrunin, Corbett 1999)
 *
 * Implements the canonical specification pattern library: occurrence patterns
 * (absence, universality, existence), order patterns (response, precedence),
 * recurrence patterns (infinitely often, eventually always), fairness constraints,
 * and real-time bounded patterns.
 *
 * L7 Applications: Each pattern represents a real-world specification template
 * validated across 555 industrial specifications (Dwyer et al.).
 *
 * L6 Canonical Problems: Each pattern instantiation is a concrete LTL formula
 * that can be model-checked against a system model.
 *
 * Reference:
 *   Dwyer, Avrunin, Corbett 1999 — Patterns in Property Specifications
 *     for Finite-State Verification (ICSE 1999)
 *   Manna & Pnueli 1992 — The Temporal Logic of Reactive and Concurrent Systems
 *   Baier & Katoen 2008 — Principles of Model Checking (Ch. 5.3)
 */

#include "ltl_patterns.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ── Helper Macros ────────────────────────────────────────────── */
#define MK_ATOM(p)    ltl_mk_atom(p)
#define MK_NOT(p)     ltl_mk_not(p)
#define MK_AND(a,b)   ltl_mk_and(a,b)
#define MK_OR(a,b)    ltl_mk_or(a,b)
#define MK_IMPLIES(a,b) ltl_mk_implies(a,b)
#define MK_NEXT(p)    ltl_mk_next(p)
#define MK_FINALLY(p) ltl_mk_finally(p)
#define MK_GLOBALLY(p) ltl_mk_globally(p)
#define MK_UNTIL(a,b) ltl_mk_until(a,b)
#define MK_RELEASE(a,b) ltl_mk_release(a,b)
#define MK_WEAK(a,b)  ltl_mk_weak_until(a,b)

/* ── Occurrence Patterns: Absence ─────────────────────────────── */

LtlNode* ltl_pattern_absence_global(int p) {
    /* G ¬p — p never happens */
    return MK_GLOBALLY(MK_NOT(MK_ATOM(p)));
}

LtlNode* ltl_pattern_absence_before(int p, int r) {
    /* F r → (¬p U r) — p absent until r first occurs */
    return MK_IMPLIES(
        MK_FINALLY(MK_ATOM(r)),
        MK_UNTIL(MK_NOT(MK_ATOM(p)), MK_ATOM(r))
    );
}

LtlNode* ltl_pattern_absence_after(int p, int q) {
    /* G(q → G ¬p) — after q, p never holds */
    return MK_GLOBALLY(
        MK_IMPLIES(MK_ATOM(q), MK_GLOBALLY(MK_NOT(MK_ATOM(p))))
    );
}

LtlNode* ltl_pattern_absence_between(int p, int q, int r) {
    /* G((q ∧ ¬r ∧ F r) → (¬p U r)) — between q and next r, p never holds */
    return MK_GLOBALLY(
        MK_IMPLIES(
            MK_AND(
                MK_ATOM(q),
                MK_AND(
                    MK_NOT(MK_ATOM(r)),
                    MK_FINALLY(MK_ATOM(r))
                )
            ),
            MK_UNTIL(MK_NOT(MK_ATOM(p)), MK_ATOM(r))
        )
    );
}

LtlNode* ltl_pattern_absence_after_until(int p, int q, int r) {
    /* G(q → (¬p W r)) — after q, p absent until r (or forever if r never) */
    return MK_GLOBALLY(
        MK_IMPLIES(MK_ATOM(q), MK_WEAK(MK_NOT(MK_ATOM(p)), MK_ATOM(r)))
    );
}

/* ── Occurrence Patterns: Universality ────────────────────────── */

LtlNode* ltl_pattern_universality_global(int p) {
    /* G p — p always holds */
    return MK_GLOBALLY(MK_ATOM(p));
}

LtlNode* ltl_pattern_universality_before(int p, int r) {
    /* F r → (p U r) — p always holds until r */
    return MK_IMPLIES(
        MK_FINALLY(MK_ATOM(r)),
        MK_UNTIL(MK_ATOM(p), MK_ATOM(r))
    );
}

LtlNode* ltl_pattern_universality_after(int p, int q) {
    /* G(q → G p) — after q, p always holds */
    return MK_GLOBALLY(
        MK_IMPLIES(MK_ATOM(q), MK_GLOBALLY(MK_ATOM(p)))
    );
}

LtlNode* ltl_pattern_universality_between(int p, int q, int r) {
    /* G((q ∧ ¬r ∧ F r) → (p U r)) — between q and r, p always holds */
    return MK_GLOBALLY(
        MK_IMPLIES(
            MK_AND(MK_ATOM(q),
                   MK_AND(MK_NOT(MK_ATOM(r)), MK_FINALLY(MK_ATOM(r)))),
            MK_UNTIL(MK_ATOM(p), MK_ATOM(r))
        )
    );
}

LtlNode* ltl_pattern_universality_after_until(int p, int q, int r) {
    /* G(q → (p W r)) — after q, p holds until r (or forever) */
    return MK_GLOBALLY(
        MK_IMPLIES(MK_ATOM(q), MK_WEAK(MK_ATOM(p), MK_ATOM(r)))
    );
}

/* ── Occurrence Patterns: Existence ───────────────────────────── */

LtlNode* ltl_pattern_existence_global(int p) {
    /* F p — p eventually holds */
    return MK_FINALLY(MK_ATOM(p));
}

LtlNode* ltl_pattern_existence_before(int p, int r) {
    /* ¬r W (p ∧ ¬r) — p holds before r (if at all) */
    return MK_WEAK(MK_NOT(MK_ATOM(r)),
                   MK_AND(MK_ATOM(p), MK_NOT(MK_ATOM(r))));
}

LtlNode* ltl_pattern_existence_after(int p, int q) {
    /* G(¬q) ∨ F(q ∧ F p) — either q never happens, or after q, p eventually */
    return MK_OR(
        MK_GLOBALLY(MK_NOT(MK_ATOM(q))),
        MK_FINALLY(MK_AND(MK_ATOM(q), MK_FINALLY(MK_ATOM(p))))
    );
}

LtlNode* ltl_pattern_existence_between(int p, int q, int r) {
    /* G((q ∧ ¬r) → (¬r W (p ∧ ¬r))) — between q and r, p eventually */
    return MK_GLOBALLY(
        MK_IMPLIES(
            MK_AND(MK_ATOM(q), MK_NOT(MK_ATOM(r))),
            MK_WEAK(MK_NOT(MK_ATOM(r)),
                    MK_AND(MK_ATOM(p), MK_NOT(MK_ATOM(r))))
        )
    );
}

LtlNode* ltl_pattern_existence_after_until(int p, int q, int r) {
    /* G(q → (¬r W (p ∧ ¬r))) — after q, p holds before r (or r never) */
    return MK_GLOBALLY(
        MK_IMPLIES(MK_ATOM(q),
                   MK_WEAK(MK_NOT(MK_ATOM(r)),
                           MK_AND(MK_ATOM(p), MK_NOT(MK_ATOM(r)))))
    );
}

/* ── Order Patterns: Response ─────────────────────────────────── */

LtlNode* ltl_pattern_response_global(int p, int q) {
    /* G(p → F q) — every p is eventually followed by q
     * This is the most common LTL specification pattern. */
    return MK_GLOBALLY(
        MK_IMPLIES(MK_ATOM(p), MK_FINALLY(MK_ATOM(q)))
    );
}

LtlNode* ltl_pattern_response_before(int p, int q, int r) {
    /* F r → (p → (¬r U (q ∧ ¬r))) U r — before r, p is followed by q */
    return MK_IMPLIES(
        MK_FINALLY(MK_ATOM(r)),
        MK_UNTIL(
            MK_IMPLIES(MK_ATOM(p),
                       MK_UNTIL(MK_NOT(MK_ATOM(r)), MK_AND(MK_ATOM(q), MK_NOT(MK_ATOM(r))))),
            MK_ATOM(r)
        )
    );
}

LtlNode* ltl_pattern_response_after(int p, int q, int q0) {
    /* G(q0 → G(p → F q)) — after q0, every p is followed by q */
    return MK_GLOBALLY(
        MK_IMPLIES(MK_ATOM(q0),
                   MK_GLOBALLY(MK_IMPLIES(MK_ATOM(p), MK_FINALLY(MK_ATOM(q)))))
    );
}

LtlNode* ltl_pattern_response_between(int p, int q, int q0, int r) {
    /* G((q0 ∧ ¬r ∧ F r) → (p → (¬r U (q ∧ ¬r))) U r) */
    return MK_GLOBALLY(
        MK_IMPLIES(
            MK_AND(MK_ATOM(q0),
                   MK_AND(MK_NOT(MK_ATOM(r)), MK_FINALLY(MK_ATOM(r)))),
            MK_UNTIL(
                MK_IMPLIES(MK_ATOM(p),
                    MK_UNTIL(MK_NOT(MK_ATOM(r)),
                             MK_AND(MK_ATOM(q), MK_NOT(MK_ATOM(r))))),
                MK_ATOM(r)
            )
        )
    );
}

LtlNode* ltl_pattern_response_after_until(int p, int q, int q0, int r) {
    /* G(q0 → ((p → (¬r U (q ∧ ¬r))) W r)) — after q0, p leads to q until r */
    return MK_GLOBALLY(
        MK_IMPLIES(MK_ATOM(q0),
            MK_WEAK(
                MK_IMPLIES(MK_ATOM(p),
                    MK_UNTIL(MK_NOT(MK_ATOM(r)),
                             MK_AND(MK_ATOM(q), MK_NOT(MK_ATOM(r))))),
                MK_ATOM(r)
            )
        )
    );
}

/* ── Order Patterns: Precedence ───────────────────────────────── */

LtlNode* ltl_pattern_precedence_global(int p, int q) {
    /* (¬q) W p — q only occurs if p occurred earlier (or q never)
     * Equivalently: F q → (¬q U p) */
    return MK_WEAK(MK_NOT(MK_ATOM(q)), MK_ATOM(p));
}

LtlNode* ltl_pattern_precedence_before(int p, int q, int r) {
    /* F r → (¬q U (p ∨ r)) — before r, q needs preceding p */
    return MK_IMPLIES(
        MK_FINALLY(MK_ATOM(r)),
        MK_UNTIL(MK_NOT(MK_ATOM(q)), MK_OR(MK_ATOM(p), MK_ATOM(r)))
    );
}

LtlNode* ltl_pattern_precedence_after(int p, int q, int q0) {
    /* G(q0 → (¬q W p)) — after q0, p must precede q */
    return MK_GLOBALLY(
        MK_IMPLIES(MK_ATOM(q0), MK_WEAK(MK_NOT(MK_ATOM(q)), MK_ATOM(p)))
    );
}

LtlNode* ltl_pattern_precedence_between(int p, int q, int q0, int r) {
    /* G((q0 ∧ ¬r ∧ F r) → (¬q U (p ∨ r))) */
    return MK_GLOBALLY(
        MK_IMPLIES(
            MK_AND(MK_ATOM(q0),
                   MK_AND(MK_NOT(MK_ATOM(r)), MK_FINALLY(MK_ATOM(r)))),
            MK_UNTIL(MK_NOT(MK_ATOM(q)), MK_OR(MK_ATOM(p), MK_ATOM(r)))
        )
    );
}

LtlNode* ltl_pattern_precedence_after_until(int p, int q, int q0, int r) {
    /* G(q0 → (¬q W (p ∨ r))) — after q0, p precedes q until r */
    return MK_GLOBALLY(
        MK_IMPLIES(MK_ATOM(q0),
                   MK_WEAK(MK_NOT(MK_ATOM(q)), MK_OR(MK_ATOM(p), MK_ATOM(r))))
    );
}

/* ── Recurrence Patterns ──────────────────────────────────────── */

LtlNode* ltl_pattern_infinitely_often(int p) {
    /* G F p — p holds infinitely often */
    return MK_GLOBALLY(MK_FINALLY(MK_ATOM(p)));
}

LtlNode* ltl_pattern_eventually_always(int p) {
    /* F G p — eventually p holds permanently */
    return MK_FINALLY(MK_GLOBALLY(MK_ATOM(p)));
}

/* ── Bounded Response ─────────────────────────────────────────── */

LtlNode* ltl_pattern_bounded_response(int p, int q, int k) {
    /* G(p → (q ∨ X(q ∨ X(q ∨ ... X q)...))) with k nestings
     * p is followed by q within at most k steps. */
    if (k < 0) return NULL;

    /* Build: q ∨ X(q ∨ X(...)) from inside out */
    LtlNode* inner = MK_ATOM(q);
    for (int i = 0; i < k; i++) {
        inner = MK_OR(MK_ATOM(q), MK_NEXT(inner));
    }

    return MK_GLOBALLY(MK_IMPLIES(MK_ATOM(p), inner));
}

/* ── At Least K ───────────────────────────────────────────────── */

LtlNode* ltl_pattern_at_least_k(int p, int k) {
    /* F(p ∧ X F(p ∧ X F(... p)...)) — p occurs at least k times */
    if (k <= 0) return MK_ATOM(p);
    if (k == 1) return MK_FINALLY(MK_ATOM(p));

    LtlNode* inner = MK_ATOM(p);
    for (int i = 1; i < k; i++) {
        inner = MK_AND(MK_ATOM(p), MK_NEXT(MK_FINALLY(inner)));
    }
    return MK_FINALLY(inner);
}

/* ── Mutual Exclusion ─────────────────────────────────────────── */

LtlNode* ltl_pattern_mutex(int p, int q) {
    /* G ¬(p ∧ q) — p and q are never simultaneously true */
    return MK_GLOBALLY(MK_NOT(MK_AND(MK_ATOM(p), MK_ATOM(q))));
}

/* ── Alternating ──────────────────────────────────────────────── */

LtlNode* ltl_pattern_alternating(int p, int q) {
    /* G(p → X(¬p W q)) ∧ G(q → X(¬q W p))
     * p and q strictly alternate. */
    LtlNode* c1 = MK_GLOBALLY(
        MK_IMPLIES(MK_ATOM(p),
                   MK_NEXT(MK_WEAK(MK_NOT(MK_ATOM(p)), MK_ATOM(q))))
    );
    LtlNode* c2 = MK_GLOBALLY(
        MK_IMPLIES(MK_ATOM(q),
                   MK_NEXT(MK_WEAK(MK_NOT(MK_ATOM(q)), MK_ATOM(p))))
    );
    return MK_AND(c1, c2);
}

/* ── Sequence ─────────────────────────────────────────────────── */

LtlNode* ltl_pattern_sequence(int* props, int n) {
    /* F(p0 ∧ X F(p1 ∧ X F(... p_{n-1}))) */
    if (!props || n <= 0) return NULL;
    if (n == 1) return MK_FINALLY(MK_ATOM(props[0]));

    LtlNode* inner = MK_ATOM(props[n - 1]);
    for (int i = n - 2; i >= 0; i--) {
        inner = MK_AND(MK_ATOM(props[i]), MK_NEXT(MK_FINALLY(inner)));
    }
    return MK_FINALLY(inner);
}

/* ── Fairness ─────────────────────────────────────────────────── */

LtlNode* ltl_pattern_weak_fairness(int enabled, int executed) {
    /* F G(enabled) → G F(executed)
     * If eventually permanently enabled, then infinitely often executed.
     * Equivalently: G F ¬enabled ∨ G F executed
     * Better known as: (F G enabled) → (G F executed) */
    return MK_IMPLIES(
        MK_FINALLY(MK_GLOBALLY(MK_ATOM(enabled))),
        MK_GLOBALLY(MK_FINALLY(MK_ATOM(executed)))
    );
}

LtlNode* ltl_pattern_strong_fairness(int enabled, int executed) {
    /* G F(enabled) → G F(executed)
     * If infinitely often enabled, then infinitely often executed.
     * This is the standard strong fairness (compassion) requirement. */
    return MK_IMPLIES(
        MK_GLOBALLY(MK_FINALLY(MK_ATOM(enabled))),
        MK_GLOBALLY(MK_FINALLY(MK_ATOM(executed)))
    );
}

LtlNode* ltl_pattern_compassion(int enabled, int executed) {
    /* Same as strong fairness: (G F enabled) → (G F executed)
     * The term "compassion" is used in the reactive synthesis
     * literature (Pnueli & Rosner 1989, GR(1) synthesis). */
    return ltl_pattern_strong_fairness(enabled, executed);
}

LtlNode* ltl_pattern_justice(int enabled, int executed) {
    /* (F G enabled) → (G F executed)
     * Justice = weak fairness. If continuously enabled from some point,
     * executed infinitely often. */
    return MK_IMPLIES(
        MK_FINALLY(MK_GLOBALLY(MK_ATOM(enabled))),
        MK_GLOBALLY(MK_FINALLY(MK_ATOM(executed)))
    );
}

/* ── Real-Time (Bounded LTL) Patterns ─────────────────────────── */

LtlNode* ltl_pattern_timed_absence(int p, int k) {
    /* ¬p ∧ X ¬p ∧ X² ¬p ∧ ... ∧ X^k ¬p
     * p never holds in the first k+1 steps. */
    if (k < 0) return NULL;
    LtlNode* result = MK_NOT(MK_ATOM(p));
    for (int i = 1; i <= k; i++) {
        result = MK_AND(result, MK_NEXT(MK_NOT(MK_ATOM(p))));
    }
    return result;
}

LtlNode* ltl_pattern_timed_response(int p, int q, int k) {
    /* G(p → (q ∨ X q ∨ X² q ∨ ... ∨ X^k q))
     * Same as bounded_response — whenever p happens, q happens within k steps. */
    return ltl_pattern_bounded_response(p, q, k);
}

LtlNode* ltl_pattern_deadline(int p, int k) {
    /* p ∨ X p ∨ X² p ∨ ... ∨ X^k p
     * p must hold at or before step k. */
    if (k < 0) return NULL;
    LtlNode* result = MK_ATOM(p);
    for (int i = 1; i <= k; i++) {
        result = MK_OR(result, MK_NEXT(MK_ATOM(p)));
    }
    return result;
}

/* ── Pattern Library ──────────────────────────────────────────── */

const char* ltl_pattern_name(LtlPatternID id) {
    switch (id) {
        case LTL_PAT_ABSENCE_GLOBAL:     return "Absence (Global)";
        case LTL_PAT_UNIVERSALITY_GLOBAL: return "Universality (Global)";
        case LTL_PAT_EXISTENCE_GLOBAL:   return "Existence (Global)";
        case LTL_PAT_RESPONSE_GLOBAL:    return "Response (Global)";
        case LTL_PAT_PRECEDENCE_GLOBAL:  return "Precedence (Global)";
        case LTL_PAT_INFINITELY_OFTEN:   return "Infinitely Often";
        case LTL_PAT_EVENTUALLY_ALWAYS:  return "Eventually Always";
        case LTL_PAT_MUTEX:              return "Mutual Exclusion";
        case LTL_PAT_ALTERNATING:        return "Alternating";
        case LTL_PAT_WEAK_FAIRNESS:      return "Weak Fairness";
        case LTL_PAT_STRONG_FAIRNESS:    return "Strong Fairness";
        case LTL_PAT_COMPASSION:         return "Compassion";
        case LTL_PAT_JUSTICE:            return "Justice";
        default:                         return "Unknown";
    }
}

const char* ltl_pattern_description(LtlPatternID id) {
    switch (id) {
        case LTL_PAT_ABSENCE_GLOBAL:
            return "G ¬p : p never holds throughout execution";
        case LTL_PAT_UNIVERSALITY_GLOBAL:
            return "G p : p always holds throughout execution";
        case LTL_PAT_EXISTENCE_GLOBAL:
            return "F p : p eventually holds";
        case LTL_PAT_RESPONSE_GLOBAL:
            return "G(p → F q) : every request p is followed by response q";
        case LTL_PAT_PRECEDENCE_GLOBAL:
            return "(¬q) W p : q occurs only if p occurred earlier";
        case LTL_PAT_INFINITELY_OFTEN:
            return "G F p : p holds infinitely often";
        case LTL_PAT_EVENTUALLY_ALWAYS:
            return "F G p : p eventually holds permanently";
        case LTL_PAT_MUTEX:
            return "G ¬(p ∧ q) : p and q are never true simultaneously";
        case LTL_PAT_ALTERNATING:
            return "G(p→X(¬pWq))∧G(q→X(¬qWp)) : p and q strictly alternate";
        case LTL_PAT_WEAK_FAIRNESS:
            return "(F G enabled) → (G F executed) : weak fairness";
        case LTL_PAT_STRONG_FAIRNESS:
            return "(G F enabled) → (G F executed) : strong fairness";
        case LTL_PAT_COMPASSION:
            return "(G F enabled) → (G F executed) : compassion (same as strong)";
        case LTL_PAT_JUSTICE:
            return "(F G enabled) → (G F executed) : justice (weak fairness)";
        default:
            return "Unknown pattern";
    }
}

LtlNode* ltl_pattern_create(LtlPatternID id, int p, int q, int r) {
    switch (id) {
        case LTL_PAT_ABSENCE_GLOBAL:      return ltl_pattern_absence_global(p);
        case LTL_PAT_UNIVERSALITY_GLOBAL: return ltl_pattern_universality_global(p);
        case LTL_PAT_EXISTENCE_GLOBAL:    return ltl_pattern_existence_global(p);
        case LTL_PAT_RESPONSE_GLOBAL:     return ltl_pattern_response_global(p, q);
        case LTL_PAT_PRECEDENCE_GLOBAL:   return ltl_pattern_precedence_global(p, q);
        case LTL_PAT_INFINITELY_OFTEN:    return ltl_pattern_infinitely_often(p);
        case LTL_PAT_EVENTUALLY_ALWAYS:   return ltl_pattern_eventually_always(p);
        case LTL_PAT_MUTEX:               return ltl_pattern_mutex(p, q);
        case LTL_PAT_ALTERNATING:         return ltl_pattern_alternating(p, q);
        case LTL_PAT_WEAK_FAIRNESS:       return ltl_pattern_weak_fairness(p, q);
        case LTL_PAT_STRONG_FAIRNESS:     return ltl_pattern_strong_fairness(p, q);
        case LTL_PAT_COMPASSION:          return ltl_pattern_compassion(p, q);
        case LTL_PAT_JUSTICE:             return ltl_pattern_justice(p, q);
        default:                          return NULL;
    }
    (void)r; /* r is used by scoped patterns, not by simple ones */
}

int ltl_pattern_library_create(int base_prop, LtlNamedFormula** out) {
    int n = LTL_PAT_COUNT;
    LtlNamedFormula* lib = (LtlNamedFormula*)calloc(n, sizeof(LtlNamedFormula));
    if (!lib) return 0;

    for (int i = 0; i < n; i++) {
        lib[i].name = strdup(ltl_pattern_name((LtlPatternID)i));
        /* Use base_prop as p, base_prop+1 as q, base_prop+2 as r */
        lib[i].formula = ltl_pattern_create((LtlPatternID)i,
                                             base_prop, base_prop + 1, base_prop + 2);
    }

    *out = lib;
    return n;
}

void ltl_pattern_library_free(LtlNamedFormula* lib, int n) {
    if (!lib) return;
    for (int i = 0; i < n; i++) {
        free(lib[i].name);
        ltl_free(lib[i].formula);
    }
    free(lib);
}
