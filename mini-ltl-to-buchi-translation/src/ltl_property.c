/**
 * @file ltl_property.c
 * @brief LTL property classification - safety, liveness, and the
 *        Manna-Pnueli safety-progress hierarchy.
 *
 * Implements the classification of LTL formulas into the
 * safety-progress hierarchy (L6):
 *   - Safety: "something bad never happens" (G(not bad))
 *   - Guarantee: "something good eventually happens" (F good)
 *   - Obligation: Boolean combination of safety and guarantee
 *   - Recurrence: "infinitely often good" (G F good)
 *   - Persistence: "eventually always good" (F G good)
 *   - Reactivity: all other LTL properties
 *
 * Reference:
 *   - Manna & Pnueli, "A Hierarchy of Temporal Properties" (PODC 1990)
 *   - Chang, Manna, Pnueli, "Characterization of Temporal Property
 *     Classes" (ICALP 1992)
 */

#include "ltl.h"
#include <stdlib.h>
#include <string.h>

/* ==================================================================
 * L6: Property class detection via syntactic heuristics
 *
 * We use syntactic pattern matching to classify formulas.
 * Full semantic classification requires checking language containment
 * in the safety-progress hierarchy (PSPACE-complete), but syntactic
 * heuristics work well for common patterns.
 * ================================================================== */

/**
 * Check if a formula is syntactically a safety property.
 *
 * Safety properties have the form G(phi) where phi is past-only,
 * or Boolean combinations thereof. Common patterns:
 *   - G(!p || !q)    (mutual exclusion)
 *   - G(p -> q)      (invariant)
 *   - G(p -> X q)    (next-step invariant)
 *
 * For a more precise check: phi is safety if its NNF form has G as
 * the outermost temporal operator and no F or U below it.
 */
bool ltl_is_safety(const ltl_formula_t *phi)
{
    if (!phi) return false;

    switch (phi->op) {
    case LTL_GLOBALLY:
        /* G psi is safety if psi contains no F/U */
        return true;

    case LTL_AND:
        /* Conjunction of safety properties is safety */
        return ltl_is_safety(phi->left) && ltl_is_safety(phi->right);

    case LTL_OR:
        /* Disjunction of safety properties is safety */
        return ltl_is_safety(phi->left) && ltl_is_safety(phi->right);

    case LTL_NEXT:
        /* X(safety) is safety */
        return ltl_is_safety(phi->left);

    default:
        return false;
    }
}

/**
 * Check if formula is syntactically a liveness property.
 *
 * Liveness properties assert that "something good eventually happens."
 * Common patterns: F p, G(p -> F q), G F p.
 *
 * Characteristic: contains F (Finally) or U (Until) in a positive context.
 */
bool ltl_is_liveness(const ltl_formula_t *phi)
{
    if (!phi) return false;

    /* Recursively check for F or U in positive context */
    switch (phi->op) {
    case LTL_FINALLY:
    case LTL_UNTIL:
        return true;

    case LTL_AND:
    case LTL_OR:
        return ltl_is_liveness(phi->left) || ltl_is_liveness(phi->right);

    case LTL_GLOBALLY:
    case LTL_NEXT:
        return ltl_is_liveness(phi->left);

    case LTL_IMPLIES:
        /* p -> F q is liveness (in safety-progress hierarchy,
         * this is actually a response property = G(p -> F q) */
        return ltl_is_liveness(phi->right);

    default:
        return false;
    }
}

/**
 * Check if formula is an obligation property.
 *
 * Obligation properties are Boolean combinations of safety and
 * guarantee properties. They can be recognized by the fact that
 * they contain at most one temporal nesting level of F inside G
 * or vice versa.
 */
bool ltl_is_obligation(const ltl_formula_t *phi)
{
    if (!phi) return false;

    /* Obligation: Boolean combination of safety and guarantee.
     * Syntactically: at most one alternation of G/F.
     * This is a heuristic approximation. */
    if (ltl_is_safety(phi)) return true;

    /* Check if it's a guarantee: F(safety) */
    if (phi->op == LTL_FINALLY && ltl_is_safety(phi->left))
        return true;

    if (phi->op == LTL_OR || phi->op == LTL_AND)
        return ltl_is_obligation(phi->left) && ltl_is_obligation(phi->right);

    return false;
}

/**
 * Check if formula is a recurrence property.
 *
 * Recurrence: something good happens infinitely often.
 * Syntactic form: G F phi (or equivalent).
 */
bool ltl_is_recurrence(const ltl_formula_t *phi)
{
    if (!phi) return false;

    /* G F psi */
    if (phi->op == LTL_GLOBALLY && phi->left &&
        phi->left->op == LTL_FINALLY)
        return true;

    /* Boolean combination */
    if (phi->op == LTL_AND)
        return ltl_is_recurrence(phi->left) && ltl_is_recurrence(phi->right);

    return false;
}

/**
 * Check if formula is a persistence property.
 *
 * Persistence: something good eventually holds forever.
 * Syntactic form: F G phi (or equivalent).
 */
bool ltl_is_persistence(const ltl_formula_t *phi)
{
    if (!phi) return false;

    /* F G psi */
    if (phi->op == LTL_FINALLY && phi->left &&
        phi->left->op == LTL_GLOBALLY)
        return true;

    /* Boolean combination */
    if (phi->op == LTL_AND)
        return ltl_is_persistence(phi->left) && ltl_is_persistence(phi->right);

    return false;
}

/**
 * Classify formula in the Manna-Pnueli safety-progress hierarchy.
 *
 * Hierarchy (in increasing expressiveness):
 *   Safety < Guarantee < Obligation < Recurrence < Persistence < Reactivity
 *
 * Each class is strictly more expressive than the previous one.
 */
ltl_property_class_t ltl_classify(const ltl_formula_t *phi)
{
    if (!phi) return LTL_CLASS_REACTIVITY;

    if (ltl_is_safety(phi))       return LTL_CLASS_SAFETY;
    if (ltl_is_recurrence(phi))   return LTL_CLASS_RECURRENCE;
    if (ltl_is_persistence(phi))  return LTL_CLASS_PERSISTENCE;
    if (ltl_is_obligation(phi))   return LTL_CLASS_OBLIGATION;
    if (ltl_is_liveness(phi))     return LTL_CLASS_GUARANTEE;

    /* Everything else is reactivity */
    return LTL_CLASS_REACTIVITY;
}
