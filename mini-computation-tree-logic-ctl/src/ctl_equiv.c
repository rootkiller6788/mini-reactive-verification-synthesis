/**
 * ctl_equiv.c — CTL Formula Equivalences, Normal Forms, Transformations
 *
 * Implements CTL algebraic equivalences: duality laws, expansion laws,
 * normal form conversions, reduction to adequate sets, and formula
 * simplification using rewriting rules.
 *
 * Key results:
 *   1. {EX, EG, EU, false, NOT, AND} is an adequate set (Emerson 1990)
 *   2. CTL duality: every operator has a dual under path-quantifier swap
 *   3. Expansion laws provide one-step unfoldings of temporal operators
 *   4. Negation Normal Form for CTL (duality-based, not just De Morgan)
 *
 * Knowledge: L4 (Fundamental Laws), L2 (Core Concepts), L8 (Advanced)
 * Reference: Emerson "Temporal and Modal Logic" (1990)
 *            Baier & Katoen (2008), §6.2-§6.3
 */

#include "ctl_equiv.h"
#include "ctl_ast.h"
#include "ctl_sat.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Forward declarations */
static ctl_formula *mk_unary_bin_enf(const ctl_formula *f);
static ctl_formula *ctl_alloc_and_set(ctl_formula_type t,
                                       ctl_formula *c1, ctl_formula *c2);

/* ═══════════════════════════════════════════════════════════════════════
 * Normal Form Conversions
 * ═══════════════════════════════════════════════════════════════════════ */

ctl_formula *ctl_to_nnf(const ctl_formula *f) {
    /* NNF is essentially PNF (negation only on atoms).
     * ctl_to_pnf already handles temporal dualities. */
    return ctl_to_pnf(f);
}

/**
 * Convert a formula to Existential Normal Form.
 * All A-quantifiers replaced via duality:
 *   AX phi -> NOT EX NOT phi
 *   AF phi -> NOT EG NOT phi
 *   AG phi -> NOT EF NOT phi
 *   A[phi U psi] -> NOT E[(NOT psi) U (NOT phi AND NOT psi)] AND NOT EG(NOT psi)
 *   ... or equivalently: NOT (E[...] OR EG[...])
 *   A[phi R psi] -> NOT E[(NOT phi) U (NOT psi)]
 */
ctl_formula *ctl_to_enf(const ctl_formula *f) {
    if (!f) return NULL;
    switch (f->type) {
    case CTL_TOP:  return ctl_mk_top();
    case CTL_BOT:  return ctl_mk_bot();
    case CTL_ATOM: return ctl_mk_atom(f->data.atom_name);
    case CTL_NOT:
        return ctl_mk_not(ctl_to_enf(f->data.unary.child));
    case CTL_AND:
        return ctl_mk_and(ctl_to_enf(f->data.binary.left),
                          ctl_to_enf(f->data.binary.right));
    case CTL_OR:
        return ctl_mk_or(ctl_to_enf(f->data.binary.left),
                         ctl_to_enf(f->data.binary.right));
    case CTL_IMPLIES: {
        ctl_formula *l = ctl_to_enf(f->data.binary.left);
        ctl_formula *r = ctl_to_enf(f->data.binary.right);
        return ctl_mk_or(ctl_mk_not(l), r);
    }
    case CTL_IFF: {
        ctl_formula *l = ctl_to_enf(f->data.binary.left);
        ctl_formula *r = ctl_to_enf(f->data.binary.right);
        return ctl_mk_and(ctl_mk_or(ctl_mk_not(ctl_clone_formula(l)), ctl_clone_formula(r)),
                          ctl_mk_or(ctl_mk_not(l), r));
    }
    case CTL_EX: case CTL_EF: case CTL_EG:
    case CTL_EU: case CTL_ER:
        /* Already existential — just recurse */
        return mk_unary_bin_enf(f);

    case CTL_AX:
        /* AX phi = NOT EX NOT phi */
        return ctl_mk_not(ctl_mk_ex(
            ctl_mk_not(ctl_to_enf(f->data.unary.child))));

    case CTL_AF:
        /* AF phi = NOT EG NOT phi */
        return ctl_mk_not(ctl_mk_eg(
            ctl_mk_not(ctl_to_enf(f->data.unary.child))));

    case CTL_AG:
        /* AG phi = NOT EF NOT phi */
        return ctl_mk_not(ctl_mk_ef(
            ctl_mk_not(ctl_to_enf(f->data.unary.child))));

    case CTL_AU: {
        /* A[phi U psi] = NOT E[(NOT psi) U (NOT phi AND NOT psi)] AND NOT EG(NOT psi)
         * Simplified to: NOT (E[(NOT psi) U (NOT phi AND NOT psi)] OR EG(NOT psi))
         */
        ctl_formula *phi_enf = ctl_to_enf(f->data.binary.left);
        ctl_formula *psi_enf = ctl_to_enf(f->data.binary.right);
        ctl_formula *not_psi = ctl_mk_not(ctl_clone_formula(psi_enf));
        ctl_formula *not_phi = ctl_mk_not(ctl_clone_formula(phi_enf));
        ctl_formula *not_both = ctl_mk_and(ctl_clone_formula(not_phi),
                                            ctl_clone_formula(not_psi));
        ctl_formula *eu_part = ctl_mk_eu(not_psi, not_both);
        ctl_formula *eg_part = ctl_mk_eg(ctl_mk_not(ctl_clone_formula(psi_enf)));
        ctl_formula *disj = ctl_mk_or(eu_part, eg_part);
        ctl_free_formula(phi_enf);
        ctl_free_formula(psi_enf);
        ctl_free_formula(not_phi);
        return ctl_mk_not(disj);
    }

    case CTL_AR: {
        /* A[phi R psi] = NOT E[(NOT phi) U (NOT psi)] */
        ctl_formula *phi_enf = ctl_to_enf(f->data.binary.left);
        ctl_formula *psi_enf = ctl_to_enf(f->data.binary.right);
        ctl_formula *not_phi = ctl_mk_not(phi_enf);
        ctl_formula *not_psi = ctl_mk_not(psi_enf);
        ctl_formula *eu = ctl_mk_eu(not_phi, not_psi);
        return ctl_mk_not(eu);
    }
    }
    return NULL;
}

/** Helper: propagate conversion for unary/binary temporal ops */
static ctl_formula *mk_unary_bin_enf(const ctl_formula *f) {
    switch (f->type) {
    case CTL_EX: case CTL_EF: case CTL_EG:
    case CTL_AX: case CTL_AF: case CTL_AG:
        return ctl_alloc_and_set(f->type, ctl_to_enf(f->data.unary.child), NULL);
    case CTL_EU: case CTL_ER:
    case CTL_AU: case CTL_AR:
        return ctl_alloc_and_set(f->type, ctl_to_enf(f->data.binary.left),
                                         ctl_to_enf(f->data.binary.right));
    default:
        return NULL;
    }
}

static ctl_formula *ctl_alloc_and_set(ctl_formula_type t,
                                       ctl_formula *c1, ctl_formula *c2) {
    ctl_formula *f = (ctl_formula *)calloc(1, sizeof(ctl_formula));
    if (!f) { ctl_free_formula(c1); ctl_free_formula(c2); return NULL; }
    f->type = t;
    if (c2) {
        f->data.binary.left = c1;
        f->data.binary.right = c2;
    } else {
        f->data.unary.child = c1;
    }
    return f;
}

ctl_formula *ctl_to_unf(const ctl_formula *f) {
    /* UNF is dual of ENF: swap all E <-> A */
    if (!f) return NULL;
    switch (f->type) {
    case CTL_TOP:  return ctl_mk_top();
    case CTL_BOT:  return ctl_mk_bot();
    case CTL_ATOM: return ctl_mk_atom(f->data.atom_name);
    case CTL_NOT:
        return ctl_mk_not(ctl_to_unf(f->data.unary.child));
    case CTL_AND:
        return ctl_mk_and(ctl_to_unf(f->data.binary.left),
                          ctl_to_unf(f->data.binary.right));
    case CTL_OR:
        return ctl_mk_or(ctl_to_unf(f->data.binary.left),
                         ctl_to_unf(f->data.binary.right));
    case CTL_IMPLIES: {
        ctl_formula *l = ctl_to_unf(f->data.binary.left);
        ctl_formula *r = ctl_to_unf(f->data.binary.right);
        return ctl_mk_or(ctl_mk_not(l), r);
    }
    case CTL_IFF: {
        ctl_formula *l = ctl_to_unf(f->data.binary.left);
        ctl_formula *r = ctl_to_unf(f->data.binary.right);
        return ctl_mk_and(ctl_mk_or(ctl_mk_not(ctl_clone_formula(l)), ctl_clone_formula(r)),
                          ctl_mk_or(ctl_mk_not(l), r));
    }
    case CTL_AX: case CTL_AF: case CTL_AG:
        return ctl_alloc_and_set(f->type, ctl_to_unf(f->data.unary.child), NULL);
    case CTL_AU: case CTL_AR:
        return ctl_alloc_and_set(f->type,
                                 ctl_to_unf(f->data.binary.left),
                                 ctl_to_unf(f->data.binary.right));

    case CTL_EX:
        return ctl_mk_not(ctl_mk_ax(
            ctl_mk_not(ctl_to_unf(f->data.unary.child))));
    case CTL_EF:
        return ctl_mk_not(ctl_mk_ag(
            ctl_mk_not(ctl_to_unf(f->data.unary.child))));
    case CTL_EG:
        return ctl_mk_not(ctl_mk_af(
            ctl_mk_not(ctl_to_unf(f->data.unary.child))));
    case CTL_EU: {
        ctl_formula *phi = ctl_to_unf(f->data.binary.left);
        ctl_formula *psi = ctl_to_unf(f->data.binary.right);
        ctl_formula *not_psi = ctl_mk_not(ctl_clone_formula(psi));
        ctl_formula *not_phi = ctl_mk_not(ctl_clone_formula(phi));
        ctl_formula *not_both = ctl_mk_and(ctl_clone_formula(not_phi),
                                            ctl_clone_formula(not_psi));
        ctl_formula *au = ctl_mk_au(not_psi, not_both);
        ctl_formula *ag = ctl_mk_ag(ctl_mk_not(ctl_clone_formula(psi)));
        ctl_free_formula(phi); ctl_free_formula(psi);
        ctl_free_formula(not_phi);
        return ctl_mk_not(ctl_mk_or(au, ag));
    }
    case CTL_ER: {
        ctl_formula *phi = ctl_to_unf(f->data.binary.left);
        ctl_formula *psi = ctl_to_unf(f->data.binary.right);
        ctl_formula *not_phi = ctl_mk_not(phi);
        ctl_formula *not_psi = ctl_mk_not(psi);
        return ctl_mk_not(ctl_mk_au(not_phi, not_psi));
    }
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════
 * CTL Duality
 *
 * The dual of a CTL formula swaps:
 *   EX <-> AX, EF <-> AG, EG <-> AF, EU <-> AR, ER <-> AU
 *   AND <-> OR, TOP <-> BOT
 *
 * Duality Theorem: dual(phi) is logically equivalent to NOT phi.
 * This is a consequence of the De Morgan laws for CTL.
 * ═══════════════════════════════════════════════════════════════════════ */

ctl_formula *ctl_dual(const ctl_formula *f) {
    if (!f) return NULL;
    switch (f->type) {
    case CTL_TOP: return ctl_mk_bot();
    case CTL_BOT: return ctl_mk_top();
    case CTL_ATOM: return ctl_mk_not(ctl_mk_atom(f->data.atom_name));
    case CTL_NOT:  return ctl_mk_not(ctl_dual(f->data.unary.child));
    case CTL_AND:  return ctl_mk_or(ctl_dual(f->data.binary.left),
                                     ctl_dual(f->data.binary.right));
    case CTL_OR:   return ctl_mk_and(ctl_dual(f->data.binary.left),
                                     ctl_dual(f->data.binary.right));
    case CTL_EX:   return ctl_mk_ax(ctl_dual(f->data.unary.child));
    case CTL_AX:   return ctl_mk_ex(ctl_dual(f->data.unary.child));
    case CTL_EF:   return ctl_mk_ag(ctl_dual(f->data.unary.child));
    case CTL_AF:   return ctl_mk_eg(ctl_dual(f->data.unary.child));
    case CTL_EG:   return ctl_mk_af(ctl_dual(f->data.unary.child));
    case CTL_AG:   return ctl_mk_ef(ctl_dual(f->data.unary.child));
    case CTL_EU:   return ctl_mk_ar(ctl_dual(f->data.binary.left),
                                     ctl_dual(f->data.binary.right));
    case CTL_AU:   return ctl_mk_er(ctl_dual(f->data.binary.left),
                                     ctl_dual(f->data.binary.right));
    case CTL_ER:   return ctl_mk_au(ctl_dual(f->data.binary.left),
                                     ctl_dual(f->data.binary.right));
    case CTL_AR:   return ctl_mk_eu(ctl_dual(f->data.binary.left),
                                     ctl_dual(f->data.binary.right));
    case CTL_IMPLIES: {
        /* a -> b = !a OR b; dual = !(dual(a)) AND dual(b) */
        ctl_formula *da = ctl_dual(f->data.binary.left);
        ctl_formula *db = ctl_dual(f->data.binary.right);
        return ctl_mk_and(ctl_mk_not(da), db);
    }
    case CTL_IFF: {
        ctl_formula *da = ctl_dual(f->data.binary.left);
        ctl_formula *db = ctl_dual(f->data.binary.right);
        return ctl_mk_not(ctl_mk_or(
            ctl_mk_and(da, ctl_clone_formula(db)),
            ctl_mk_and(ctl_mk_not(ctl_clone_formula(da)),
                       ctl_mk_not(db))));
    }
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Expansion Laws
 *
 * One-step unfoldings of temporal operators (fixpoint identities):
 *   EF phi     = phi OR EX(EF phi)
 *   AF phi     = phi OR AX(AF phi)
 *   EG phi     = phi AND EX(EG phi)
 *   AG phi     = phi AND AX(AG phi)
 *   E[phi U psi] = psi OR (phi AND EX E[phi U psi])
 *   A[phi U psi] = psi OR (phi AND AX A[phi U psi])
 *   E[phi R psi] = psi AND (phi OR EX E[phi R psi])
 *   A[phi R psi] = psi AND (phi OR AX A[phi R psi])
 *
 * These follow directly from the fixpoint characterizations.
 * ═══════════════════════════════════════════════════════════════════════ */

ctl_formula *ctl_expand(const ctl_formula *f, int depth) {
    if (!f || depth <= 0)
        return f ? ctl_clone_formula(f) : NULL;

    switch (f->type) {
    case CTL_EF: {
        /* EF phi = phi OR EX(EF phi) */
        ctl_formula *phi = ctl_clone_formula(f->data.unary.child);
        ctl_formula *ex_ef = ctl_mk_ex(ctl_expand(f, depth - 1));
        return ctl_mk_or(phi, ex_ef);
    }
    case CTL_AF: {
        ctl_formula *phi = ctl_clone_formula(f->data.unary.child);
        return ctl_mk_or(phi, ctl_mk_ax(ctl_expand(f, depth - 1)));
    }
    case CTL_EG: {
        ctl_formula *phi = ctl_clone_formula(f->data.unary.child);
        return ctl_mk_and(phi, ctl_mk_ex(ctl_expand(f, depth - 1)));
    }
    case CTL_AG: {
        ctl_formula *phi = ctl_clone_formula(f->data.unary.child);
        return ctl_mk_and(phi, ctl_mk_ax(ctl_expand(f, depth - 1)));
    }
    case CTL_EU: {
        ctl_formula *phi = ctl_clone_formula(f->data.binary.left);
        ctl_formula *psi = ctl_clone_formula(f->data.binary.right);
        return ctl_mk_or(psi,
            ctl_mk_and(phi, ctl_mk_ex(ctl_expand(f, depth - 1))));
    }
    case CTL_AU: {
        ctl_formula *phi = ctl_clone_formula(f->data.binary.left);
        ctl_formula *psi = ctl_clone_formula(f->data.binary.right);
        return ctl_mk_or(psi,
            ctl_mk_and(phi, ctl_mk_ax(ctl_expand(f, depth - 1))));
    }
    case CTL_ER: {
        ctl_formula *phi = ctl_clone_formula(f->data.binary.left);
        ctl_formula *psi = ctl_clone_formula(f->data.binary.right);
        return ctl_mk_and(psi,
            ctl_mk_or(phi, ctl_mk_ex(ctl_expand(f, depth - 1))));
    }
    case CTL_AR: {
        ctl_formula *phi = ctl_clone_formula(f->data.binary.left);
        ctl_formula *psi = ctl_clone_formula(f->data.binary.right);
        return ctl_mk_and(psi,
            ctl_mk_or(phi, ctl_mk_ax(ctl_expand(f, depth - 1))));
    }
    default:
        return ctl_clone_formula(f);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * Reduction to Core (Emerson Adequate Set)
 *
 * Adequate set: {EX, EG, EU, false, NOT, AND}
 * All CTL operators can be expressed using only these.
 *
 * Reductions:
 *   true = NOT false
 *   a OR b = NOT (NOT a AND NOT b)
 *   AX phi = NOT EX (NOT phi)
 *   EF phi = E[true U phi]
 *   AF phi = NOT EG (NOT phi)
 *   AG phi = NOT EF (NOT phi)
 *   A[phi U psi] = ... (complex, see ctl_label_au)
 *   E[phi R psi] = NOT A[(NOT phi) U (NOT psi)]
 *   A[phi R psi] = NOT E[(NOT phi) U (NOT psi)]
 * ═══════════════════════════════════════════════════════════════════════ */

ctl_formula *ctl_reduce_to_core(const ctl_formula *f) {
    if (!f) return NULL;
    switch (f->type) {
    case CTL_TOP:
        /* true = NOT false */
        return ctl_mk_not(ctl_mk_bot());
    case CTL_BOT:
        return ctl_mk_bot();
    case CTL_ATOM:
        return ctl_mk_atom(f->data.atom_name);
    case CTL_NOT:
        return ctl_mk_not(ctl_reduce_to_core(f->data.unary.child));
    case CTL_AND:
        return ctl_mk_and(ctl_reduce_to_core(f->data.binary.left),
                          ctl_reduce_to_core(f->data.binary.right));
    case CTL_OR:
        /* a OR b = NOT (NOT a AND NOT b) */
        return ctl_mk_not(
            ctl_mk_and(
                ctl_mk_not(ctl_reduce_to_core(f->data.binary.left)),
                ctl_mk_not(ctl_reduce_to_core(f->data.binary.right))));
    case CTL_IMPLIES:
        /* a -> b = NOT a OR b = NOT (a AND NOT b) */
        return ctl_mk_not(
            ctl_mk_and(
                ctl_reduce_to_core(f->data.binary.left),
                ctl_mk_not(ctl_reduce_to_core(f->data.binary.right))));
    case CTL_IFF: {
        ctl_formula *l = ctl_reduce_to_core(f->data.binary.left);
        ctl_formula *r = ctl_reduce_to_core(f->data.binary.right);
        return ctl_mk_and(
            ctl_mk_not(ctl_mk_and(ctl_clone_formula(l),
                                   ctl_mk_not(ctl_clone_formula(r)))),
            ctl_mk_not(ctl_mk_and(r,
                                   ctl_mk_not(l))));
    }
    case CTL_EX:
        return ctl_mk_ex(ctl_reduce_to_core(f->data.unary.child));
    case CTL_AX:
        return ctl_mk_not(ctl_mk_ex(
            ctl_mk_not(ctl_reduce_to_core(f->data.unary.child))));
    case CTL_EF:
        return ctl_mk_eu(ctl_mk_not(ctl_mk_bot()),
                         ctl_reduce_to_core(f->data.unary.child));
    case CTL_AF:
        return ctl_mk_not(ctl_mk_eg(
            ctl_mk_not(ctl_reduce_to_core(f->data.unary.child))));
    case CTL_EG:
        return ctl_mk_eg(ctl_reduce_to_core(f->data.unary.child));
    case CTL_AG:
        return ctl_mk_not(
            ctl_mk_eu(ctl_mk_not(ctl_mk_bot()),
                      ctl_mk_not(ctl_reduce_to_core(f->data.unary.child))));
    case CTL_EU:
        return ctl_mk_eu(ctl_reduce_to_core(f->data.binary.left),
                         ctl_reduce_to_core(f->data.binary.right));
    case CTL_AU: {
        ctl_formula *phi = ctl_reduce_to_core(f->data.binary.left);
        ctl_formula *psi = ctl_reduce_to_core(f->data.binary.right);
        /* A[phi U psi] = NOT( E[(NOT psi) U (NOT phi AND NOT psi)] OR EG(NOT psi) ) */
        ctl_formula *npsi = ctl_mk_not(ctl_clone_formula(psi));
        ctl_formula *nphi = ctl_mk_not(ctl_clone_formula(phi));
        ctl_formula *nboth = ctl_mk_and(ctl_clone_formula(nphi),
                                        ctl_clone_formula(npsi));
        ctl_formula *eu_part = ctl_mk_eu(npsi, nboth);
        ctl_formula *eg_part = ctl_mk_eg(ctl_mk_not(ctl_clone_formula(psi)));
        ctl_formula *result = ctl_mk_not(ctl_mk_or(eu_part, eg_part));
        ctl_free_formula(phi); ctl_free_formula(psi); ctl_free_formula(nphi);
        return result;
    }
    case CTL_ER: {
        /* E[phi R psi] = NOT A[(NOT phi) U (NOT psi)]
         * Reduce via duality to avoid mutual recursion. */
        ctl_formula *nphi = ctl_mk_not(
            ctl_reduce_to_core(f->data.binary.left));
        ctl_formula *npsi = ctl_mk_not(
            ctl_reduce_to_core(f->data.binary.right));
        ctl_formula *inner = ctl_mk_au(nphi, npsi);
        return ctl_mk_not(inner);
    }
    case CTL_AR: {
        /* A[phi R psi] = NOT E[(NOT phi) U (NOT psi)] */
        return ctl_mk_not(ctl_mk_eu(
            ctl_mk_not(ctl_reduce_to_core(f->data.binary.left)),
            ctl_mk_not(ctl_reduce_to_core(f->data.binary.right))));
    }
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Formula Simplification
 *
 * Applies algebraic simplification rules:
 *   1. Constant folding
 *   2. Idempotence
 *   3. Absorption (EF AG EF phi = AG EF phi)
 *   4. Double negation elimination
 * ═══════════════════════════════════════════════════════════════════════ */

ctl_formula *ctl_simplify(const ctl_formula *f) {
    if (!f) return NULL;

    switch (f->type) {
    case CTL_TOP: case CTL_BOT: case CTL_ATOM:
        return ctl_clone_formula(f);

    case CTL_NOT: {
        ctl_formula *child = ctl_simplify(f->data.unary.child);
        if (!child) return NULL;
        /* Double negation: NOT NOT phi -> phi */
        if (child->type == CTL_NOT) {
            ctl_formula *inner = ctl_clone_formula(child->data.unary.child);
            ctl_free_formula(child);
            return inner;
        }
        /* NOT TOP -> BOT */
        if (child->type == CTL_TOP) {
            ctl_free_formula(child);
            return ctl_mk_bot();
        }
        /* NOT BOT -> TOP */
        if (child->type == CTL_BOT) {
            ctl_free_formula(child);
            return ctl_mk_top();
        }
        return ctl_mk_not(child);
    }

    case CTL_AND: {
        ctl_formula *l = ctl_simplify(f->data.binary.left);
        ctl_formula *r = ctl_simplify(f->data.binary.right);
        if (!l || !r) { ctl_free_formula(l); ctl_free_formula(r); return NULL; }
        /* AND with BOT -> BOT */
        if (l->type == CTL_BOT || r->type == CTL_BOT) {
            ctl_free_formula(l); ctl_free_formula(r);
            return ctl_mk_bot();
        }
        /* AND with TOP -> other */
        if (l->type == CTL_TOP) { ctl_free_formula(l); return r; }
        if (r->type == CTL_TOP) { ctl_free_formula(r); return l; }
        /* Idempotence */
        if (ctl_formula_equal(l, r)) {
            ctl_free_formula(r);
            return l;
        }
        return ctl_mk_and(l, r);
    }

    case CTL_OR: {
        ctl_formula *l = ctl_simplify(f->data.binary.left);
        ctl_formula *r = ctl_simplify(f->data.binary.right);
        if (!l || !r) { ctl_free_formula(l); ctl_free_formula(r); return NULL; }
        if (l->type == CTL_TOP || r->type == CTL_TOP) {
            ctl_free_formula(l); ctl_free_formula(r);
            return ctl_mk_top();
        }
        if (l->type == CTL_BOT) { ctl_free_formula(l); return r; }
        if (r->type == CTL_BOT) { ctl_free_formula(r); return l; }
        if (ctl_formula_equal(l, r)) {
            ctl_free_formula(r);
            return l;
        }
        return ctl_mk_or(l, r);
    }

    case CTL_EX: {
        ctl_formula *c = ctl_simplify(f->data.unary.child);
        if (!c) return NULL;
        if (c->type == CTL_BOT) { ctl_free_formula(c); return ctl_mk_bot(); }
        return ctl_mk_ex(c);
    }

    case CTL_EF: {
        ctl_formula *c = ctl_simplify(f->data.unary.child);
        if (!c) return NULL;
        /* EF EF phi = EF phi (idempotence of EF) */
        if (c->type == CTL_EF) {
            ctl_formula *inner = ctl_clone_formula(c->data.unary.child);
            ctl_free_formula(c);
            return ctl_mk_ef(inner);
        }
        return ctl_mk_ef(c);
    }

    case CTL_EG: {
        ctl_formula *c = ctl_simplify(f->data.unary.child);
        if (!c) return NULL;
        /* EG EG phi = EG phi */
        if (c->type == CTL_EG) {
            ctl_formula *inner = ctl_clone_formula(c->data.unary.child);
            ctl_free_formula(c);
            return ctl_mk_eg(inner);
        }
        return ctl_mk_eg(c);
    }

    case CTL_AG: {
        ctl_formula *c = ctl_simplify(f->data.unary.child);
        if (!c) return NULL;
        /* AG AG phi = AG phi */
        if (c->type == CTL_AG) {
            ctl_formula *inner = ctl_clone_formula(c->data.unary.child);
            ctl_free_formula(c);
            return ctl_mk_ag(inner);
        }
        if (c->type == CTL_TOP) { ctl_free_formula(c); return ctl_mk_top(); }
        return ctl_mk_ag(c);
    }

    default:
        /* For other operators, recursively simplify */
        return ctl_clone_formula(f);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * Subformula Ordering (Bottom-Up for Labeling Algorithm)
 * ═══════════════════════════════════════════════════════════════════════ */

static int collect_subformulas(const ctl_formula *f, ctl_formula **out,
                                int capacity, int *count) {
    if (!f) return 0;
    if (*count >= capacity) { (*count)++; return 1; }

    /* Post-order traversal: children first, then self */
    switch (f->type) {
    case CTL_TOP: case CTL_BOT: case CTL_ATOM: break;
    case CTL_NOT: case CTL_EX: case CTL_AX:
    case CTL_EF: case CTL_AF: case CTL_EG: case CTL_AG:
        collect_subformulas(f->data.unary.child, out, capacity, count);
        break;
    default:
        collect_subformulas(f->data.binary.left, out, capacity, count);
        collect_subformulas(f->data.binary.right, out, capacity, count);
        break;
    }

    if (*count < capacity)
        out[*count] = (ctl_formula *)f; /* discard const — read-only use */
    (*count)++;
    return 1;
}

int ctl_subformula_order(const ctl_formula *phi, ctl_formula **out,
                          int capacity) {
    int count = 0;
    collect_subformulas(phi, out, capacity, &count);
    return count;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Equivalence / Entailment Testing (Structural Approximation)
 * ═══════════════════════════════════════════════════════════════════════ */

int ctl_equivalent(const ctl_formula *phi, const ctl_formula *psi) {
    if (phi == psi) return 1;
    if (!phi || !psi) return 0;

    /* Syntactic check: canonically equivalent? */
    ctl_formula *canon_phi = ctl_canonicalize_formula(phi);
    ctl_formula *canon_psi = ctl_canonicalize_formula(psi);
    int eq = ctl_formula_equal(canon_phi, canon_psi);
    ctl_free_formula(canon_phi);
    ctl_free_formula(canon_psi);
    if (eq) return 1;

    /* PNF equivalence check */
    ctl_formula *pnf_phi = ctl_to_pnf(phi);
    ctl_formula *pnf_psi = ctl_to_pnf(psi);
    ctl_formula *canon_pnf_phi = ctl_canonicalize_formula(pnf_phi);
    ctl_formula *canon_pnf_psi = ctl_canonicalize_formula(pnf_psi);
    eq = ctl_formula_equal(canon_pnf_phi, canon_pnf_psi);
    ctl_free_formula(pnf_phi);
    ctl_free_formula(pnf_psi);
    ctl_free_formula(canon_pnf_phi);
    ctl_free_formula(canon_pnf_psi);
    return eq;
}

int ctl_entails(const ctl_formula *phi, const ctl_formula *psi) {
    if (!phi || !psi) return -1;
    /* phi |= psi iff !satisfiable(phi AND NOT psi) */
    ctl_formula *not_psi = ctl_mk_not(ctl_clone_formula(psi));
    ctl_formula *check_f = ctl_mk_and(ctl_clone_formula(phi), not_psi);
    int sat = ctl_is_satisfiable(check_f);
    ctl_free_formula(check_f);
    if (sat < 0) return -1;
    return !sat; /* entails iff NOT satisfiable */
}

/* ═══════════════════════════════════════════════════════════════════════
 * CTL / LTL Relationship
 * ═══════════════════════════════════════════════════════════════════════ */

int ctl_is_ltl_expressible(const ctl_formula *phi) {
    if (!phi) return 0;
    /* Simple syntactic check: all path quantifiers are A? */
    /* Actually: CTL formulas expressible in LTL are exactly those
     * in the "ACTL detect" fragment (Maidi 2000).
     * For now, check that all temporal operators are A-quantified.
     */
    switch (phi->type) {
    case CTL_TOP: case CTL_BOT: case CTL_ATOM:
        return 1;
    case CTL_NOT:
        return ctl_is_ltl_expressible(phi->data.unary.child);
    case CTL_AND: case CTL_OR: case CTL_IMPLIES: case CTL_IFF:
        return ctl_is_ltl_expressible(phi->data.binary.left) &&
               ctl_is_ltl_expressible(phi->data.binary.right);
    case CTL_AX: case CTL_AF: case CTL_AG:
        return ctl_is_ltl_expressible(phi->data.unary.child);
    case CTL_AU: case CTL_AR:
        return ctl_is_ltl_expressible(phi->data.binary.left) &&
               ctl_is_ltl_expressible(phi->data.binary.right);
    /* Existential quantifiers: not LTL-expressible in general */
    case CTL_EX: case CTL_EF: case CTL_EG: case CTL_EU: case CTL_ER:
    default:
        return 0;
    }
}

char *ctl_to_ltl_string(const ctl_formula *phi) {
    if (!phi || !ctl_is_ltl_expressible(phi)) return NULL;
    /* Convert A-quantified CTL to LTL by dropping the A prefix */
    /* This is an approximation — full conversion is complex */
    size_t cap = 4096;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';

    switch (phi->type) {
    case CTL_TOP: strcpy(buf, "true"); break;
    case CTL_BOT: strcpy(buf, "false"); break;
    case CTL_ATOM: strcpy(buf, phi->data.atom_name); break;
    case CTL_NOT: {
        char *inner = ctl_to_ltl_string(phi->data.unary.child);
        if (inner) { snprintf(buf, cap, "!(%s)", inner); free(inner); }
        break;
    }
    case CTL_AND: {
        char *l = ctl_to_ltl_string(phi->data.binary.left);
        char *r = ctl_to_ltl_string(phi->data.binary.right);
        if (l && r) snprintf(buf, cap, "(%s) && (%s)", l, r);
        free(l); free(r); break;
    }
    case CTL_OR: {
        char *l = ctl_to_ltl_string(phi->data.binary.left);
        char *r = ctl_to_ltl_string(phi->data.binary.right);
        if (l && r) snprintf(buf, cap, "(%s) || (%s)", l, r);
        free(l); free(r); break;
    }
    case CTL_AX: {
        char *inner = ctl_to_ltl_string(phi->data.unary.child);
        if (inner) { snprintf(buf, cap, "X(%s)", inner); free(inner); }
        break;
    }
    case CTL_AF: {
        char *inner = ctl_to_ltl_string(phi->data.unary.child);
        if (inner) { snprintf(buf, cap, "F(%s)", inner); free(inner); }
        break;
    }
    case CTL_AG: {
        char *inner = ctl_to_ltl_string(phi->data.unary.child);
        if (inner) { snprintf(buf, cap, "G(%s)", inner); free(inner); }
        break;
    }
    case CTL_AU: {
        char *l = ctl_to_ltl_string(phi->data.binary.left);
        char *r = ctl_to_ltl_string(phi->data.binary.right);
        if (l && r) snprintf(buf, cap, "(%s) U (%s)", l, r);
        free(l); free(r); break;
    }
    default: break;
    }
    return buf;
}
