/**
 * ctl_equiv.h - CTL Formula Equivalences, Normal Forms, Transformations
 *
 * CTL equivalences are algebraic laws governing temporal operators.
 * Used for formula simplification, normal form conversion, expressiveness
 * analysis, and reduction to core operators.
 *
 * The minimal adequate set for CTL is {EX, EG, EU} with {false, NOT, AND}
 * (Emerson 1990, Handbook of Theoretical Computer Science).
 *
 * Knowledge: L4 (Fundamental Laws), L2 (Core Concepts)
 * Reference: Emerson "Temporal and Modal Logic" (1990)
 *            Baier & Katoen (2008), Chapter 6.2
 */

#ifndef CTL_EQUIV_H
#define CTL_EQUIV_H

#include "ctl_ast.h"

/* --- Equivalence Testing --- */

int ctl_equivalent(const ctl_formula *phi, const ctl_formula *psi);
int ctl_entails(const ctl_formula *phi, const ctl_formula *psi);

/* --- Normal Forms --- */

ctl_formula *ctl_to_nnf(const ctl_formula *f);
ctl_formula *ctl_to_enf(const ctl_formula *f);
ctl_formula *ctl_to_unf(const ctl_formula *f);

/* --- Duality --- */

ctl_formula *ctl_dual(const ctl_formula *f);

/* --- Expansion Laws --- */

ctl_formula *ctl_expand(const ctl_formula *f, int depth);

/* --- Reduction to Core --- */

ctl_formula *ctl_reduce_to_core(const ctl_formula *f);

/* --- Simplification --- */

ctl_formula *ctl_simplify(const ctl_formula *f);

/* --- Subformula Ordering --- */

int ctl_subformula_order(const ctl_formula *phi, ctl_formula **out, int capacity);

/* --- CTL / LTL Relationship --- */

int ctl_is_ltl_expressible(const ctl_formula *phi);
char *ctl_to_ltl_string(const ctl_formula *phi);

#endif /* CTL_EQUIV_H */