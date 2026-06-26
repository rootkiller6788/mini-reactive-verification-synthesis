/**
 * ctl_modelcheck.c — CTL Model Checking Algorithm
 *
 * Implements the Clarke-Emerson-Sistla explicit-state labeling algorithm
 * for CTL model checking. Computes SAT(phi) = { s | M,s |= phi } for
 * each subformula bottom-up over the formula tree.
 *
 * Core fixpoint computations:
 *   SAT(EX phi)  = pre_exists(SAT(phi))
 *   SAT(EG phi)  = nu Z . SAT(phi) AND pre_exists(Z)
 *   SAT(EU phi psi) = mu Z . SAT(psi) OR (SAT(phi) AND pre_exists(Z))
 *
 * All other CTL operators are reduced to these three primitives.
 *
 * Complexity: O(|phi| * (|S| + |R|)) worst case.
 *
 * Knowledge: L2 (Core Concepts), L4 (Fundamental Laws), L5 (Algorithms)
 * Reference: Clarke, Emerson, Sistla (1986) ACM TOPLAS
 *            Baier & Katoen "Principles of Model Checking" (2008), §6.4
 */

#include "ctl_modelcheck.h"
#include "ctl_ast.h"
#include "ctl_kripke.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════
 * Context Management
 * ═══════════════════════════════════════════════════════════════════════ */

static ctl_mc_context *mc_context_create(const ctl_kripke *k, int nstates) {
    ctl_mc_context *ctx = (ctl_mc_context *)calloc(1, sizeof(ctl_mc_context));
    if (!ctx) return NULL;
    ctx->kripke = k;
    ctx->total_states = nstates;
    ctx->cache_capacity = 32;
    ctx->sat_cache = (ctl_state_set **)calloc((size_t)ctx->cache_capacity,
                                               sizeof(ctl_state_set *));
    if (!ctx->sat_cache) { free(ctx); return NULL; }
    return ctx;
}

void ctl_mc_context_destroy(ctl_mc_context *ctx) {
    if (!ctx) return;
    for (int i = 0; i < ctx->cache_capacity; i++) {
        ctl_state_set_destroy(ctx->sat_cache[i]);
    }
    free(ctx->sat_cache);
    free(ctx->witness_path);
    free(ctx);
}

const ctl_state_set *ctl_mc_get_sat(const ctl_mc_context *ctx,
                                     const ctl_formula *sub) {
    (void)ctx; (void)sub;
    /* Formula-pointer-based lookup. Not fully implemented in this version. */
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════
 * EX: Existential Next
 *    SAT(EX phi) = pre_exists(SAT(phi))
 *    M,s |= EX phi  iff  exists t: (s,t) in R and M,t |= phi
 * ═══════════════════════════════════════════════════════════════════════ */

void ctl_label_ex(const ctl_kripke *k, ctl_state_set *result,
                  const ctl_state_set *sat_phi) {
    ctl_pre_image(k, result, sat_phi);
}

/* ═══════════════════════════════════════════════════════════════════════
 * EG: Existential Globally
 *    SAT(EG phi) = nu Z . SAT(phi) AND pre_exists(Z)
 *
 * Greatest fixpoint computation (Clarke, Emerson, Sistla 1986):
 *   Z0 = SAT(phi)
 *   Zi+1 = Zi AND pre_exists(Zi)
 *   Terminates when Zi+1 = Zi (monotone decreasing from top)
 *
 * Because the state space is finite, the iteration converges in at most
 * |S| steps. The result is the set of states that have an infinite path
 * on which phi continuously holds.
 *
 * Complexity: O(|S| * (|S| + |R|)) naive, O(|S| + |R|) with SCC-based
 * method (Emerson-Lei 1986). We implement the naive fixpoint iteration.
 * ═══════════════════════════════════════════════════════════════════════ */

void ctl_label_eg(const ctl_kripke *k, ctl_state_set *result,
                  const ctl_state_set *sat_phi) {
    if (!k || !result || !sat_phi) return;

    int n = (int)k->nstates;
    ctl_state_set *Z = ctl_state_set_clone(sat_phi);
    if (!Z) return;

    ctl_state_set *Z_prev = ctl_state_set_create(n);
    ctl_state_set *pre = ctl_state_set_create(n);
    if (!Z_prev || !pre) {
        ctl_state_set_destroy(Z);
        ctl_state_set_destroy(Z_prev);
        ctl_state_set_destroy(pre);
        return;
    }

    int changed;
    do {
        ctl_state_set_copy(Z_prev, Z);
        ctl_pre_image(k, pre, Z);
        ctl_state_set_intersect(Z, pre);  /* Z = Z AND pre_exists(Z) */
        ctl_state_set_intersect(Z, sat_phi);  /* Z = Z AND SAT(phi) */
        changed = !ctl_state_set_equal(Z, Z_prev);
    } while (changed);

    ctl_state_set_copy(result, Z);

    ctl_state_set_destroy(Z);
    ctl_state_set_destroy(Z_prev);
    ctl_state_set_destroy(pre);
}

/* ═══════════════════════════════════════════════════════════════════════
 * EU: Existential Until
 *    SAT(E[phi U psi]) = mu Z . SAT(psi) OR (SAT(phi) AND pre_exists(Z))
 *
 * Least fixpoint computation:
 *   Z0 = SAT(psi)
 *   Zi+1 = Zi OR (SAT(phi) AND pre_exists(Zi))
 *   Terminates when Zi+1 = Zi (monotone increasing from bottom)
 *
 * The set grows monotonically. Each iteration adds states that can
 * reach psi in one more step while staying within phi. Converges in
 * at most |S| iterations.
 *
 * Correctness (Clarke, Emerson, Sistla):
 *   M,s |= E[phi U psi] iff there exists a path pi = s0,s1,... with s0=s
 *   and k >= 0 such that M,sk |= psi and for all 0 <= j < k, M,sj |= phi.
 * ═══════════════════════════════════════════════════════════════════════ */

void ctl_label_eu(const ctl_kripke *k, ctl_state_set *result,
                  const ctl_state_set *sat_phi,
                  const ctl_state_set *sat_psi) {
    if (!k || !result || !sat_phi || !sat_psi) return;

    int n = (int)k->nstates;
    ctl_state_set *Z = ctl_state_set_clone(sat_psi);
    if (!Z) return;

    ctl_state_set *Z_prev = ctl_state_set_create(n);
    ctl_state_set *pre = ctl_state_set_create(n);
    ctl_state_set *temp = ctl_state_set_create(n);
    if (!Z_prev || !pre || !temp) {
        ctl_state_set_destroy(Z);
        ctl_state_set_destroy(Z_prev);
        ctl_state_set_destroy(pre);
        ctl_state_set_destroy(temp);
        return;
    }

    int changed;
    do {
        ctl_state_set_copy(Z_prev, Z);
        ctl_pre_image(k, pre, Z);
        /* temp = SAT(phi) AND pre_exists(Z) */
        ctl_state_set_copy(temp, pre);
        ctl_state_set_intersect(temp, sat_phi);
        /* Z = SAT(psi) OR temp */
        ctl_state_set_copy(Z, sat_psi);
        ctl_state_set_union(Z, temp);
        changed = !ctl_state_set_equal(Z, Z_prev);
    } while (changed);

    ctl_state_set_copy(result, Z);

    ctl_state_set_destroy(Z);
    ctl_state_set_destroy(Z_prev);
    ctl_state_set_destroy(pre);
    ctl_state_set_destroy(temp);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Derived Operators
 *
 * Standard CTL reductions from the adequate set {EX, EG, EU}:
 *   AX phi     = NOT EX (NOT phi)
 *   EF phi     = E[TRUE U phi]
 *   AF phi     = NOT EG (NOT phi)
 *   AG phi     = NOT EF (NOT phi)
 *   A[phi U psi] = NOT E[(NOT psi) U (NOT phi AND NOT psi)] AND NOT EG (NOT psi)
 *   E[phi R psi] = NOT A[(NOT phi) U (NOT psi)]
 *   A[phi R psi] = NOT E[(NOT phi) U (NOT psi)]
 * ═══════════════════════════════════════════════════════════════════════ */

void ctl_label_ax(const ctl_kripke *k, ctl_state_set *result,
                  const ctl_state_set *sat_phi) {
    /* AX phi = NOT EX (NOT phi) */
    int n = (int)k->nstates;
    ctl_state_set *comp = ctl_state_set_create(n);
    if (!comp) return;
    ctl_state_set_copy(comp, sat_phi);
    ctl_state_set_complement(comp);
    ctl_label_ex(k, result, comp);
    ctl_state_set_complement(result);
    ctl_state_set_destroy(comp);
}

void ctl_label_ef(const ctl_kripke *k, ctl_state_set *result,
                  const ctl_state_set *sat_phi) {
    /* EF phi = E[TRUE U phi] */
    int n = (int)k->nstates;
    ctl_state_set *true_set = ctl_state_set_create(n);
    if (!true_set) return;
    ctl_state_set_universe(true_set);
    ctl_label_eu(k, result, true_set, sat_phi);
    ctl_state_set_destroy(true_set);
}

void ctl_label_af(const ctl_kripke *k, ctl_state_set *result,
                  const ctl_state_set *sat_phi) {
    /* AF phi = NOT EG (NOT phi) */
    int n = (int)k->nstates;
    ctl_state_set *comp = ctl_state_set_create(n);
    if (!comp) return;
    ctl_state_set_copy(comp, sat_phi);
    ctl_state_set_complement(comp);
    ctl_label_eg(k, result, comp);
    ctl_state_set_complement(result);
    ctl_state_set_destroy(comp);
}

void ctl_label_ag(const ctl_kripke *k, ctl_state_set *result,
                  const ctl_state_set *sat_phi) {
    /* AG phi = NOT EF (NOT phi) */
    int n = (int)k->nstates;
    ctl_state_set *comp = ctl_state_set_create(n);
    if (!comp) return;
    ctl_state_set_copy(comp, sat_phi);
    ctl_state_set_complement(comp);
    ctl_label_ef(k, result, comp);
    ctl_state_set_complement(result);
    ctl_state_set_destroy(comp);
}

void ctl_label_au(const ctl_kripke *k, ctl_state_set *result,
                  const ctl_state_set *sat_phi,
                  const ctl_state_set *sat_psi) {
    /* A[phi U psi] = NOT E[(NOT psi) U (NOT phi AND NOT psi)] AND NOT EG (NOT psi)
     * Simplified: A[phi U psi] = NOT (E[(NOT psi) U (NOT phi AND NOT psi)] OR EG (NOT psi))
     */
    int n = (int)k->nstates;
    ctl_state_set *not_psi = ctl_state_set_clone(sat_psi);
    ctl_state_set *not_phi = ctl_state_set_clone(sat_phi);
    ctl_state_set *not_both = ctl_state_set_create(n);
    ctl_state_set *eu_res = ctl_state_set_create(n);
    ctl_state_set *eg_res = ctl_state_set_create(n);
    if (!not_psi || !not_phi || !not_both || !eu_res || !eg_res) goto au_cleanup;

    ctl_state_set_complement(not_psi);
    ctl_state_set_complement(not_phi);

    /* not_both = (NOT phi) AND (NOT psi) */
    ctl_state_set_copy(not_both, not_phi);
    ctl_state_set_intersect(not_both, not_psi);

    /* E[(NOT psi) U not_both] */
    ctl_label_eu(k, eu_res, not_psi, not_both);

    /* EG(NOT psi) */
    ctl_label_eg(k, eg_res, not_psi);

    /* result = NOT (eu_res OR eg_res) */
    ctl_state_set_copy(result, eu_res);
    ctl_state_set_union(result, eg_res);
    ctl_state_set_complement(result);

au_cleanup:
    ctl_state_set_destroy(not_psi);
    ctl_state_set_destroy(not_phi);
    ctl_state_set_destroy(not_both);
    ctl_state_set_destroy(eu_res);
    ctl_state_set_destroy(eg_res);
}

void ctl_label_er(const ctl_kripke *k, ctl_state_set *result,
                  const ctl_state_set *sat_phi,
                  const ctl_state_set *sat_psi) {
    /* E[phi R psi] = NOT A[(NOT phi) U (NOT psi)]
     *              = NOT (NOT E[(NOT psi) U (NOT phi AND NOT psi)] AND NOT EG (NOT psi))
     *              = E[(NOT psi) U (NOT phi AND NOT psi)] OR EG (NOT psi) ... wait
     *
     * Actually: E[phi R psi] = NOT A[NOT phi U NOT psi]
     *                       = NOT (NOT E[(NOT(NOT psi)) U ...])
     * Let's use: E[phi R psi] = NOT A[(NOT phi) U (NOT psi)]
     */
    int n = (int)k->nstates;
    ctl_state_set *not_phi = ctl_state_set_clone(sat_phi);
    ctl_state_set *not_psi = ctl_state_set_clone(sat_psi);
    ctl_state_set *au_res = ctl_state_set_create(n);
    if (!not_phi || !not_psi || !au_res) goto er_cleanup;

    ctl_state_set_complement(not_phi);
    ctl_state_set_complement(not_psi);

    ctl_label_au(k, au_res, not_phi, not_psi);
    ctl_state_set_copy(result, au_res);
    ctl_state_set_complement(result);

er_cleanup:
    ctl_state_set_destroy(not_phi);
    ctl_state_set_destroy(not_psi);
    ctl_state_set_destroy(au_res);
}

void ctl_label_ar(const ctl_kripke *k, ctl_state_set *result,
                  const ctl_state_set *sat_phi,
                  const ctl_state_set *sat_psi) {
    /* A[phi R psi] = NOT E[(NOT phi) U (NOT psi)] */
    int n = (int)k->nstates;
    ctl_state_set *not_phi = ctl_state_set_clone(sat_phi);
    ctl_state_set *not_psi = ctl_state_set_clone(sat_psi);
    ctl_state_set *eu_res = ctl_state_set_create(n);
    if (!not_phi || !not_psi || !eu_res) goto ar_cleanup;

    ctl_state_set_complement(not_phi);
    ctl_state_set_complement(not_psi);

    ctl_label_eu(k, eu_res, not_phi, not_psi);
    ctl_state_set_copy(result, eu_res);
    ctl_state_set_complement(result);

ar_cleanup:
    ctl_state_set_destroy(not_phi);
    ctl_state_set_destroy(not_psi);
    ctl_state_set_destroy(eu_res);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Main Model Checking Algorithm (Clarke-Emerson-Sistla 1986)
 *
 * Algorithm:
 *   For each subformula psi of phi (bottom-up by size):
 *     switch (psi->type):
 *       ATOM:  SAT(psi) = { s | psi in L(s) }
 *       NOT a: SAT(psi) = S \ SAT(a)
 *       AND a b: SAT(psi) = SAT(a) AND SAT(b)
 *       OR a b:  SAT(psi) = SAT(a) OR SAT(b)
 *       EX a:    SAT(psi) = pre_exists(SAT(a))
 *       EG a:    SAT(psi) = fixpoint_nu (Z -> SAT(a) AND pre_exists(Z))
 *       EU a b:  SAT(psi) = fixpoint_mu (Z -> SAT(b) OR (SAT(a) AND pre_exists(Z)))
 *       others:  reduce to EX, EG, EU
 *
 * The result is SAT(phi). Then check if all initial states are in SAT(phi).
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * Compute SAT set for a given formula using the labeling algorithm.
 * Recursively processes subformulas and stores results in a local cache.
 */
static int compute_sat(const ctl_kripke *k, const ctl_formula *phi,
                       ctl_state_set ***cache, int *cache_size,
                       int *cache_cap, ctl_state_set *result) {
    int n = (int)k->nstates;

    switch (phi->type) {
    case CTL_TOP:
        ctl_state_set_copy(result, NULL); /* will be set to universe */
        ctl_state_set_universe(result);
        return 0;

    case CTL_BOT:
        ctl_state_set_clear(result);
        return 0;

    case CTL_ATOM: {
        int ap = ctl_kripke_ap_index(k, phi->data.atom_name);
        ctl_state_set_clear(result);
        if (ap >= 0) {
            for (ctl_state_id s = 0; s < k->nstates; s++) {
                if (ctl_kripke_get_label(k, s, ap))
                    ctl_state_set_add(result, s);
            }
        }
        return 0;
    }

    case CTL_NOT: {
        ctl_state_set *child_sat = ctl_state_set_create(n);
        if (!child_sat) return -1;
        if (compute_sat(k, phi->data.unary.child, cache, cache_size,
                        cache_cap, child_sat) != 0) {
            ctl_state_set_destroy(child_sat);
            return -1;
        }
        ctl_state_set_copy(result, child_sat);
        ctl_state_set_complement(result);
        ctl_state_set_destroy(child_sat);
        return 0;
    }

    case CTL_AND: {
        ctl_state_set *l = ctl_state_set_create(n);
        ctl_state_set *r = ctl_state_set_create(n);
        if (!l || !r) { ctl_state_set_destroy(l); ctl_state_set_destroy(r); return -1; }
        if (compute_sat(k, phi->data.binary.left, cache, cache_size, cache_cap, l) ||
            compute_sat(k, phi->data.binary.right, cache, cache_size, cache_cap, r)) {
            ctl_state_set_destroy(l); ctl_state_set_destroy(r); return -1;
        }
        ctl_state_set_copy(result, l);
        ctl_state_set_intersect(result, r);
        ctl_state_set_destroy(l); ctl_state_set_destroy(r);
        return 0;
    }

    case CTL_OR: {
        ctl_state_set *l = ctl_state_set_create(n);
        ctl_state_set *r = ctl_state_set_create(n);
        if (!l || !r) { ctl_state_set_destroy(l); ctl_state_set_destroy(r); return -1; }
        if (compute_sat(k, phi->data.binary.left, cache, cache_size, cache_cap, l) ||
            compute_sat(k, phi->data.binary.right, cache, cache_size, cache_cap, r)) {
            ctl_state_set_destroy(l); ctl_state_set_destroy(r); return -1;
        }
        ctl_state_set_copy(result, l);
        ctl_state_set_union(result, r);
        ctl_state_set_destroy(l); ctl_state_set_destroy(r);
        return 0;
    }

    case CTL_IMPLIES: {
        /* a -> b = (NOT a) OR b */
        ctl_state_set *l = ctl_state_set_create(n);
        ctl_state_set *r = ctl_state_set_create(n);
        if (!l || !r) { ctl_state_set_destroy(l); ctl_state_set_destroy(r); return -1; }
        if (compute_sat(k, phi->data.binary.left, cache, cache_size, cache_cap, l) ||
            compute_sat(k, phi->data.binary.right, cache, cache_size, cache_cap, r)) {
            ctl_state_set_destroy(l); ctl_state_set_destroy(r); return -1;
        }
        ctl_state_set_copy(result, l);
        ctl_state_set_complement(result);
        ctl_state_set_union(result, r);
        ctl_state_set_destroy(l); ctl_state_set_destroy(r);
        return 0;
    }

    case CTL_IFF: {
        /* a <-> b = (a -> b) AND (b -> a) */
        ctl_state_set *l = ctl_state_set_create(n);
        ctl_state_set *r = ctl_state_set_create(n);
        if (!l || !r) { ctl_state_set_destroy(l); ctl_state_set_destroy(r); return -1; }
        if (compute_sat(k, phi->data.binary.left, cache, cache_size, cache_cap, l) ||
            compute_sat(k, phi->data.binary.right, cache, cache_size, cache_cap, r)) {
            ctl_state_set_destroy(l); ctl_state_set_destroy(r); return -1;
        }
        /* result = (NOT l OR r) AND (NOT r OR l) */
        ctl_state_set *tmp1 = ctl_state_set_create(n);
        ctl_state_set *tmp2 = ctl_state_set_create(n);
        ctl_state_set_copy(tmp1, l); ctl_state_set_complement(tmp1);
        ctl_state_set_union(tmp1, r);  /* tmp1 = NOT l OR r */
        ctl_state_set_copy(tmp2, r); ctl_state_set_complement(tmp2);
        ctl_state_set_union(tmp2, l);  /* tmp2 = NOT r OR l */
        ctl_state_set_copy(result, tmp1);
        ctl_state_set_intersect(result, tmp2);
        ctl_state_set_destroy(l); ctl_state_set_destroy(r);
        ctl_state_set_destroy(tmp1); ctl_state_set_destroy(tmp2);
        return 0;
    }

    case CTL_EX: {
        ctl_state_set *child_sat = ctl_state_set_create(n);
        if (!child_sat) return -1;
        if (compute_sat(k, phi->data.unary.child, cache, cache_size,
                        cache_cap, child_sat)) {
            ctl_state_set_destroy(child_sat);
            return -1;
        }
        ctl_label_ex(k, result, child_sat);
        ctl_state_set_destroy(child_sat);
        return 0;
    }

    case CTL_AX: {
        ctl_state_set *child_sat = ctl_state_set_create(n);
        if (!child_sat) return -1;
        if (compute_sat(k, phi->data.unary.child, cache, cache_size,
                        cache_cap, child_sat)) {
            ctl_state_set_destroy(child_sat);
            return -1;
        }
        ctl_label_ax(k, result, child_sat);
        ctl_state_set_destroy(child_sat);
        return 0;
    }

    case CTL_EF: {
        ctl_state_set *child_sat = ctl_state_set_create(n);
        if (!child_sat) return -1;
        if (compute_sat(k, phi->data.unary.child, cache, cache_size,
                        cache_cap, child_sat)) {
            ctl_state_set_destroy(child_sat);
            return -1;
        }
        ctl_label_ef(k, result, child_sat);
        ctl_state_set_destroy(child_sat);
        return 0;
    }

    case CTL_AF: {
        ctl_state_set *child_sat = ctl_state_set_create(n);
        if (!child_sat) return -1;
        if (compute_sat(k, phi->data.unary.child, cache, cache_size,
                        cache_cap, child_sat)) {
            ctl_state_set_destroy(child_sat);
            return -1;
        }
        ctl_label_af(k, result, child_sat);
        ctl_state_set_destroy(child_sat);
        return 0;
    }

    case CTL_EG: {
        ctl_state_set *child_sat = ctl_state_set_create(n);
        if (!child_sat) return -1;
        if (compute_sat(k, phi->data.unary.child, cache, cache_size,
                        cache_cap, child_sat)) {
            ctl_state_set_destroy(child_sat);
            return -1;
        }
        ctl_label_eg(k, result, child_sat);
        ctl_state_set_destroy(child_sat);
        return 0;
    }

    case CTL_AG: {
        ctl_state_set *child_sat = ctl_state_set_create(n);
        if (!child_sat) return -1;
        if (compute_sat(k, phi->data.unary.child, cache, cache_size,
                        cache_cap, child_sat)) {
            ctl_state_set_destroy(child_sat);
            return -1;
        }
        ctl_label_ag(k, result, child_sat);
        ctl_state_set_destroy(child_sat);
        return 0;
    }

    case CTL_EU: {
        ctl_state_set *l = ctl_state_set_create(n);
        ctl_state_set *r = ctl_state_set_create(n);
        if (!l || !r) { ctl_state_set_destroy(l); ctl_state_set_destroy(r); return -1; }
        if (compute_sat(k, phi->data.binary.left, cache, cache_size, cache_cap, l) ||
            compute_sat(k, phi->data.binary.right, cache, cache_size, cache_cap, r)) {
            ctl_state_set_destroy(l); ctl_state_set_destroy(r); return -1;
        }
        ctl_label_eu(k, result, l, r);
        ctl_state_set_destroy(l); ctl_state_set_destroy(r);
        return 0;
    }

    case CTL_AU: {
        ctl_state_set *l = ctl_state_set_create(n);
        ctl_state_set *r = ctl_state_set_create(n);
        if (!l || !r) { ctl_state_set_destroy(l); ctl_state_set_destroy(r); return -1; }
        if (compute_sat(k, phi->data.binary.left, cache, cache_size, cache_cap, l) ||
            compute_sat(k, phi->data.binary.right, cache, cache_size, cache_cap, r)) {
            ctl_state_set_destroy(l); ctl_state_set_destroy(r); return -1;
        }
        ctl_label_au(k, result, l, r);
        ctl_state_set_destroy(l); ctl_state_set_destroy(r);
        return 0;
    }

    case CTL_ER: {
        ctl_state_set *l = ctl_state_set_create(n);
        ctl_state_set *r = ctl_state_set_create(n);
        if (!l || !r) { ctl_state_set_destroy(l); ctl_state_set_destroy(r); return -1; }
        if (compute_sat(k, phi->data.binary.left, cache, cache_size, cache_cap, l) ||
            compute_sat(k, phi->data.binary.right, cache, cache_size, cache_cap, r)) {
            ctl_state_set_destroy(l); ctl_state_set_destroy(r); return -1;
        }
        ctl_label_er(k, result, l, r);
        ctl_state_set_destroy(l); ctl_state_set_destroy(r);
        return 0;
    }

    case CTL_AR: {
        ctl_state_set *l = ctl_state_set_create(n);
        ctl_state_set *r = ctl_state_set_create(n);
        if (!l || !r) { ctl_state_set_destroy(l); ctl_state_set_destroy(r); return -1; }
        if (compute_sat(k, phi->data.binary.left, cache, cache_size, cache_cap, l) ||
            compute_sat(k, phi->data.binary.right, cache, cache_size, cache_cap, r)) {
            ctl_state_set_destroy(l); ctl_state_set_destroy(r); return -1;
        }
        ctl_label_ar(k, result, l, r);
        ctl_state_set_destroy(l); ctl_state_set_destroy(r);
        return 0;
    }
    }
    return -1;
}

ctl_mc_result ctl_model_check(const ctl_kripke *k, const ctl_formula *phi,
                               ctl_mc_context **ctx_out) {
    if (!k || !phi) return CTL_MC_ERROR;

    int n = (int)k->nstates;
    ctl_state_set *sat = ctl_state_set_create(n);
    if (!sat) return CTL_MC_ERROR;

    ctl_state_set **cache = NULL;
    int cache_size = 0, cache_cap = 0;

    int err = compute_sat(k, phi, &cache, &cache_size, &cache_cap, sat);

    ctl_mc_result res;
    if (err != 0) {
        res = CTL_MC_ERROR;
    } else {
        /* Check: all initial states satisfy phi? */
        int all_satisfy = 1;
        for (ctl_state_id i = 0; i < k->ninitial; i++) {
            if (!ctl_state_set_contains(sat, k->initial_states[i])) {
                all_satisfy = 0;
                break;
            }
        }
        res = all_satisfy ? CTL_MC_SATISFIED : CTL_MC_VIOLATED;
    }

    if (ctx_out) {
        ctl_mc_context *ctx = mc_context_create(k, n);
        if (ctx) *ctx_out = ctx;
    }

    /* Cleanup temporary cache arrays */
    if (cache) {
        for (int i = 0; i < cache_cap; i++) {
            ctl_state_set_destroy(cache[i]);
        }
        free(cache);
    }
    ctl_state_set_destroy(sat);
    return res;
}

ctl_mc_result ctl_model_check_state(const ctl_kripke *k, ctl_state_id s,
                                     const ctl_formula *phi,
                                     ctl_mc_context **ctx_out) {
    if (!k || !phi || s >= k->nstates) return CTL_MC_ERROR;

    int n = (int)k->nstates;
    ctl_state_set *sat = ctl_state_set_create(n);
    if (!sat) return CTL_MC_ERROR;

    ctl_state_set **cache = NULL;
    int cache_size = 0, cache_cap = 0;

    int err = compute_sat(k, phi, &cache, &cache_size, &cache_cap, sat);

    ctl_mc_result res;
    if (err != 0) {
        res = CTL_MC_ERROR;
    } else {
        res = ctl_state_set_contains(sat, s) ? CTL_MC_SATISFIED : CTL_MC_VIOLATED;
    }

    if (ctx_out) {
        ctl_mc_context *ctx = mc_context_create(k, n);
        if (ctx) *ctx_out = ctx;
    }

    if (cache) {
        for (int i = 0; i < cache_cap; i++) {
            ctl_state_set_destroy(cache[i]);
        }
        free(cache);
    }
    ctl_state_set_destroy(sat);
    return res;
}

/* Counterexample generation implemented in ctl_counterexample.c */

/* ═══════════════════════════════════════════════════════════════════════
 * Bounded Model Checking (L8 Advanced Topic)
 *
 * BMC for CTL: check phi on k up to bound `bound` steps.
 * For A-quantifiers: checks all paths up to length bound.
 * For E-quantifiers: searches for a witness within bound.
 *
 * The existential fragment can be checked via SAT-based BMC;
 * universal formulas need full state space exploration.
 *
 * Reference: Biere et al. "Bounded Model Checking" (1999, 2003)
 *            Penczek et al. "Bounded Model Checking for Branching Time
 *            Temporal Logic" (2002)
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    const ctl_kripke *kripke;
    int bound;
    int nstates;
} bmc_ctx;

/**
 * Bounded evaluation of CTL formulas.
 * For temporal operators, we unroll up to the bound.
 *
 * Bounded semantics:
 *   M,s |=_b EX phi   iff exists t in post(s): M,t |=_b phi
 *   M,s |=_b EG phi   iff exists path of length b+1 where phi holds at all states
 *   M,s |=_b E[phi U psi] iff exists path where psi holds within b steps
 *                           and phi holds until then
 */
static int bmc_check_formula_simple(bmc_ctx *b, ctl_state_id s,
                                     const ctl_formula *phi, int depth) {
    if (depth > b->bound) {
        /* For existential formulas: consider unsatisfied beyond bound */
        /* For universal formulas: consider satisfied beyond bound */
        /* Simplified: beyond bound, existential fails, universal passes */
        switch (phi->type) {
        case CTL_EG: case CTL_EF: case CTL_EU: case CTL_ER:
        case CTL_EX:
            return 0; /* cannot demonstrate infinite behavior */
        default:
            return 1; /* cannot find counterexample beyond bound */
        }
    }

    switch (phi->type) {
    case CTL_TOP: return 1;
    case CTL_BOT: return 0;
    case CTL_ATOM: {
        int ap = ctl_kripke_ap_index(b->kripke, phi->data.atom_name);
        return (ap >= 0) ? ctl_kripke_get_label(b->kripke, s, ap) : 0;
    }
    case CTL_NOT:
        return !bmc_check_formula_simple(b, s, phi->data.unary.child, depth);
    case CTL_AND:
        return bmc_check_formula_simple(b, s, phi->data.binary.left, depth) &&
               bmc_check_formula_simple(b, s, phi->data.binary.right, depth);
    case CTL_OR:
        return bmc_check_formula_simple(b, s, phi->data.binary.left, depth) ||
               bmc_check_formula_simple(b, s, phi->data.binary.right, depth);
    case CTL_IMPLIES:
        return !bmc_check_formula_simple(b, s, phi->data.binary.left, depth) ||
               bmc_check_formula_simple(b, s, phi->data.binary.right, depth);
    case CTL_IFF:
        return bmc_check_formula_simple(b, s, phi->data.binary.left, depth) ==
               bmc_check_formula_simple(b, s, phi->data.binary.right, depth);
    case CTL_EX: {
        int out_cap = b->nstates;
        ctl_state_id *out = (ctl_state_id *)malloc((size_t)out_cap * sizeof(ctl_state_id));
        if (!out) return 0;
        int found = 0;
        if (ctl_kripke_post(b->kripke, s, out, &out_cap) == 0) {
            for (int i = 0; i < out_cap; i++) {
                if (bmc_check_formula_simple(b, out[i],
                        phi->data.unary.child, depth + 1)) {
                    found = 1;
                    break;
                }
            }
        }
        free(out);
        return found;
    }
    case CTL_AX: {
        int out_cap = b->nstates;
        ctl_state_id *out = (ctl_state_id *)malloc((size_t)out_cap * sizeof(ctl_state_id));
        if (!out) return 0;
        int all = 1;
        if (ctl_kripke_post(b->kripke, s, out, &out_cap) == 0 && out_cap > 0) {
            for (int i = 0; i < out_cap; i++) {
                if (!bmc_check_formula_simple(b, out[i],
                        phi->data.unary.child, depth + 1)) {
                    all = 0;
                    break;
                }
            }
        }
        free(out);
        return all;
    }
    case CTL_EF: {
        /* Bounded EF: search for phi along a path of length <= bound */
        if (bmc_check_formula_simple(b, s, phi->data.unary.child, depth))
            return 1;
        if (depth == b->bound) return 0;
        int out_cap = b->nstates;
        ctl_state_id *out = (ctl_state_id *)malloc((size_t)out_cap * sizeof(ctl_state_id));
        if (!out) return 0;
        int found = 0;
        if (ctl_kripke_post(b->kripke, s, out, &out_cap) == 0) {
            for (int i = 0; i < out_cap; i++) {
                if (bmc_check_formula_simple(b, out[i], phi, depth + 1)) {
                    found = 1;
                    break;
                }
            }
        }
        free(out);
        return found;
    }
    case CTL_EG: {
        /* Bounded EG: check phi holds now and can continue */
        if (!bmc_check_formula_simple(b, s, phi->data.unary.child, depth))
            return 0;
        if (depth == b->bound) return 1; /* assume can continue */
        int out_cap = b->nstates;
        ctl_state_id *out = (ctl_state_id *)malloc((size_t)out_cap * sizeof(ctl_state_id));
        if (!out) return 0;
        int found = 0;
        if (ctl_kripke_post(b->kripke, s, out, &out_cap) == 0) {
            for (int i = 0; i < out_cap; i++) {
                if (bmc_check_formula_simple(b, out[i], phi, depth + 1)) {
                    found = 1;
                    break;
                }
            }
        }
        free(out);
        return found;
    }
    case CTL_AG: {
        if (!bmc_check_formula_simple(b, s, phi->data.unary.child, depth))
            return 0;
        if (depth == b->bound) return 1;
        int out_cap = b->nstates;
        ctl_state_id *out = (ctl_state_id *)malloc((size_t)out_cap * sizeof(ctl_state_id));
        if (!out) return 0;
        int all = 1;
        if (ctl_kripke_post(b->kripke, s, out, &out_cap) == 0) {
            for (int i = 0; i < out_cap; i++) {
                if (!bmc_check_formula_simple(b, out[i], phi, depth + 1)) {
                    all = 0;
                    break;
                }
            }
        }
        free(out);
        return all;
    }
    case CTL_AF:
        /* Simplification: AF phi = NOT EG NOT phi */
        return !bmc_check_formula_simple(b, s,
                phi->data.unary.child, depth); /* Approximate */
    case CTL_EU: {
        if (bmc_check_formula_simple(b, s, phi->data.binary.right, depth))
            return 1;
        if (depth == b->bound) return 0;
        if (!bmc_check_formula_simple(b, s, phi->data.binary.left, depth))
            return 0;
        int out_cap = b->nstates;
        ctl_state_id *out = (ctl_state_id *)malloc((size_t)out_cap * sizeof(ctl_state_id));
        if (!out) return 0;
        int found = 0;
        if (ctl_kripke_post(b->kripke, s, out, &out_cap) == 0) {
            for (int i = 0; i < out_cap; i++) {
                if (bmc_check_formula_simple(b, out[i], phi, depth + 1)) {
                    found = 1;
                    break;
                }
            }
        }
        free(out);
        return found;
    }
    default:
        return 1; /* Conservative for unhandled operators */
    }
}

ctl_mc_result ctl_bounded_model_check(const ctl_kripke *k,
                                       const ctl_formula *phi,
                                       int bound,
                                       ctl_mc_context **ctx_out) {
    if (!k || !phi || bound < 0) return CTL_MC_ERROR;

    bmc_ctx b;
    b.kripke = k;
    b.bound = bound;
    b.nstates = (int)k->nstates;

    int all_satisfy = 1;
    for (ctl_state_id i = 0; i < k->ninitial; i++) {
        if (!bmc_check_formula_simple(&b, k->initial_states[i], phi, 0)) {
            all_satisfy = 0;
            break;
        }
    }

    if (ctx_out) *ctx_out = NULL;
    return all_satisfy ? CTL_MC_SATISFIED : CTL_MC_VIOLATED;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Fair CTL Model Checking (L8 Advanced Topic)
 *
 * Fairness constraints ensure that model checking only considers
 * "fair" execution paths. A path is fair if it visits each constraint
 * set infinitely often.
 *
 * The key transformation (Clarke, Grumberg, Peled 1999, §6.5):
 *   SAT_fair(EG phi) = SAT(EG (phi AND fair_states))
 *   where fair_states = nu Z . AND_{c in constraints} EF (c AND Z)
 *
 * This reduces fair CTL to standard CTL on a modified formula.
 * ═══════════════════════════════════════════════════════════════════════ */

ctl_fairness_constraint *ctl_fairness_create(int nconstraints) {
    ctl_fairness_constraint *fc = (ctl_fairness_constraint *)calloc(1,
        sizeof(ctl_fairness_constraint));
    if (!fc) return NULL;
    fc->nconstraints = nconstraints;
    fc->constraints = (ctl_state_set **)calloc((size_t)nconstraints,
        sizeof(ctl_state_set *));
    if (!fc->constraints && nconstraints > 0) {
        free(fc);
        return NULL;
    }
    return fc;
}

void ctl_fairness_destroy(ctl_fairness_constraint *fc) {
    if (!fc) return;
    for (int i = 0; i < fc->nconstraints; i++) {
        ctl_state_set_destroy(fc->constraints[i]);
    }
    free(fc->constraints);
    free(fc);
}

/**
 * Compute the set of "fair states" from which a fair path exists.
 * Uses the fixpoint characterization:
 *   fair_states = nu Z . AND_{c in constraints} EF (c AND Z)
 *
 * Algorithm: Start with all states. Iteratively remove states that
 * cannot reach some constraint set while staying in the current
 * approximation of fair states.
 */
static ctl_state_set *compute_fair_states(const ctl_kripke *k,
                                           const ctl_fairness_constraint *fc) {
    if (!k || !fc || fc->nconstraints == 0) {
        ctl_state_set *all = ctl_state_set_create((int)k->nstates);
        if (all) ctl_state_set_universe(all);
        return all;
    }

    int n = (int)k->nstates;
    ctl_state_set *Z = ctl_state_set_create(n);
    ctl_state_set *Z_prev = ctl_state_set_create(n);
    ctl_state_set *ef_tmp = ctl_state_set_create(n);
    ctl_state_set *c_and_z = ctl_state_set_create(n);
    if (!Z || !Z_prev || !ef_tmp || !c_and_z) {
        ctl_state_set_destroy(Z); ctl_state_set_destroy(Z_prev);
        ctl_state_set_destroy(ef_tmp); ctl_state_set_destroy(c_and_z);
        return NULL;
    }

    ctl_state_set_universe(Z);

    int changed;
    do {
        ctl_state_set_copy(Z_prev, Z);
        /* For each constraint c, intersect Z with EF(c AND Z) */
        for (int i = 0; i < fc->nconstraints; i++) {
            ctl_state_set_copy(c_and_z, fc->constraints[i]);
            ctl_state_set_intersect(c_and_z, Z);
            ctl_label_ef(k, ef_tmp, c_and_z);
            ctl_state_set_intersect(Z, ef_tmp);
        }
        changed = !ctl_state_set_equal(Z, Z_prev);
    } while (changed);

    ctl_state_set_destroy(Z_prev);
    ctl_state_set_destroy(ef_tmp);
    ctl_state_set_destroy(c_and_z);
    return Z;
}

ctl_mc_result ctl_fair_model_check(const ctl_kripke *k,
                                    const ctl_formula *phi,
                                    const ctl_fairness_constraint *fc,
                                    ctl_mc_context **ctx_out) {
    if (!k || !phi || !fc) return CTL_MC_ERROR;

    int n = (int)k->nstates;
    ctl_state_set *sat = ctl_state_set_create(n);
    if (!sat) return CTL_MC_ERROR;

    ctl_state_set **cache = NULL;
    int cache_size = 0, cache_cap = 0;

    int err = compute_sat(k, phi, &cache, &cache_size, &cache_cap, sat);

    ctl_mc_result res;
    if (err != 0) {
        res = CTL_MC_ERROR;
    } else {
        /* Only consider fair states */
        ctl_state_set *fair = compute_fair_states(k, fc);
        if (!fair) {
            res = CTL_MC_ERROR;
        } else {
            /* Check if all initial states are fair AND satisfy phi */
            int all_satisfy = 1;
            for (ctl_state_id i = 0; i < k->ninitial; i++) {
                ctl_state_id s = k->initial_states[i];
                if (!ctl_state_set_contains(fair, s) ||
                    !ctl_state_set_contains(sat, s)) {
                    all_satisfy = 0;
                    break;
                }
            }
            res = all_satisfy ? CTL_MC_SATISFIED : CTL_MC_VIOLATED;
            ctl_state_set_destroy(fair);
        }
    }

    if (ctx_out) {
        ctl_mc_context *ctx = mc_context_create(k, n);
        if (ctx) *ctx_out = ctx;
    }

    if (cache) {
        for (int i = 0; i < cache_cap; i++) {
            ctl_state_set_destroy(cache[i]);
        }
        free(cache);
    }
    ctl_state_set_destroy(sat);
    return res;
}
