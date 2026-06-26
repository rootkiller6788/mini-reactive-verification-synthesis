/**
 * ltl_parser.c �� LTL Formula Construction, Parsing, and Transformations
 *
 * Implements formula construction (smart constructors with simplification),
 * parsing from string notation, semantic evaluation on bounded traces,
 * formula transformations (NNF, expansion, Fischer-Ladner closure).
 *
 * Reference: Pnueli (1977); Lichtenstein & Pnueli (1985);
 *            De Giacomo & Vardi (2013) IJCAI 2013
 *
 * Knowledge Level: L1 Definitions, L5 Algorithms
 */

#include "ltl_syntax.h"
#include "reactive_types.h"
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

static int32_t g_node_counter = 0;
static ltl_formula_t *g_ltl_true  = NULL;
static ltl_formula_t *g_ltl_false = NULL;

static ltl_formula_t *ltl_node_alloc(ltl_node_type_t type) {
    ltl_formula_t *node = (ltl_formula_t *)calloc(1, sizeof(ltl_formula_t));
    assert(node != NULL);
    node->type = type;
    node->id = g_node_counter++;
    node->refcount = 1;
    node->is_canonical = false;
    return node;
}

ltl_formula_t *ltl_clone(ltl_formula_t *phi) {
    if (phi == NULL) return NULL;
    phi->refcount++;
    return phi;
}

void ltl_free(ltl_formula_t *phi) {
    if (phi == NULL) return;
    if (phi == g_ltl_true || phi == g_ltl_false) return;
    phi->refcount--;
    if (phi->refcount <= 0) {
        ltl_free(phi->left);
        ltl_free(phi->right);
        free(phi);
    }
}

ltl_formula_t *ltl_true(void) {
    if (g_ltl_true == NULL) {
        g_ltl_true = ltl_node_alloc(LTL_TRUE);
        g_ltl_true->refcount = 999999;
    }
    return g_ltl_true;
}

ltl_formula_t *ltl_false(void) {
    if (g_ltl_false == NULL) {
        g_ltl_false = ltl_node_alloc(LTL_FALSE);
        g_ltl_false->refcount = 999999;
    }
    return g_ltl_false;
}

ltl_formula_t *ltl_var(int32_t var_id) {
    assert(var_id >= 0);
    ltl_formula_t *node = ltl_node_alloc(LTL_VAR);
    node->var_id = var_id;
    return node;
}

ltl_formula_t *ltl_not(ltl_formula_t *phi) {
    assert(phi != NULL);
    if (phi->type == LTL_TRUE)  { return ltl_false(); }
    if (phi->type == LTL_FALSE) { return ltl_true(); }
    if (phi->type == LTL_NOT) {
        ltl_formula_t *inner = phi->left;
        ltl_clone(inner);
        return inner;
    }
    ltl_formula_t *node = ltl_node_alloc(LTL_NOT);
    node->left = phi;
    ltl_clone(phi);
    return node;
}

ltl_formula_t *ltl_and(ltl_formula_t *phi, ltl_formula_t *psi) {
    assert(phi != NULL && psi != NULL);
    if (phi->type == LTL_TRUE)  { return ltl_clone(psi); }
    if (psi->type == LTL_TRUE)  { return ltl_clone(phi); }
    if (phi->type == LTL_FALSE || psi->type == LTL_FALSE) { return ltl_false(); }
    if (phi == psi) { return ltl_clone(phi); }
    ltl_formula_t *node = ltl_node_alloc(LTL_AND);
    node->left  = phi;  node->right = psi;
    ltl_clone(phi); ltl_clone(psi);
    return node;
}

ltl_formula_t *ltl_or(ltl_formula_t *phi, ltl_formula_t *psi) {
    ltl_formula_t *nphi = ltl_not(phi);
    ltl_formula_t *npsi = ltl_not(psi);
    ltl_formula_t *and_n = ltl_and(nphi, npsi);
    ltl_formula_t *result = ltl_not(and_n);
    ltl_free(nphi); ltl_free(npsi); ltl_free(and_n);
    return result;
}

ltl_formula_t *ltl_implies(ltl_formula_t *phi, ltl_formula_t *psi) {
    ltl_formula_t *nphi = ltl_not(phi);
    ltl_formula_t *result = ltl_or(nphi, psi);
    ltl_free(nphi);
    return result;
}

ltl_formula_t *ltl_equiv(ltl_formula_t *phi, ltl_formula_t *psi) {
    ltl_formula_t *imp1 = ltl_implies(phi, psi);
    ltl_formula_t *imp2 = ltl_implies(psi, phi);
    ltl_formula_t *result = ltl_and(imp1, imp2);
    ltl_free(imp1); ltl_free(imp2);
    return result;
}

ltl_formula_t *ltl_next(ltl_formula_t *phi) {
    assert(phi != NULL);
    ltl_formula_t *node = ltl_node_alloc(LTL_X);
    node->left = phi; ltl_clone(phi);
    return node;
}

ltl_formula_t *ltl_eventually(ltl_formula_t *phi) {
    return ltl_until(ltl_true(), phi);
}

ltl_formula_t *ltl_always(ltl_formula_t *phi) {
    ltl_formula_t *nphi = ltl_not(phi);
    ltl_formula_t *f_nphi = ltl_eventually(nphi);
    ltl_formula_t *result = ltl_not(f_nphi);
    ltl_free(nphi); ltl_free(f_nphi);
    return result;
}

ltl_formula_t *ltl_until(ltl_formula_t *phi, ltl_formula_t *psi) {
    assert(phi != NULL && psi != NULL);
    if (psi->type == LTL_TRUE) { return ltl_true(); }
    ltl_formula_t *node = ltl_node_alloc(LTL_U);
    node->left = phi; node->right = psi;
    ltl_clone(phi); ltl_clone(psi);
    return node;
}

ltl_formula_t *ltl_release(ltl_formula_t *phi, ltl_formula_t *psi) {
    ltl_formula_t *nphi = ltl_not(phi);
    ltl_formula_t *npsi = ltl_not(psi);
    ltl_formula_t *u_n_n = ltl_until(nphi, npsi);
    ltl_formula_t *result = ltl_not(u_n_n);
    ltl_free(nphi); ltl_free(npsi); ltl_free(u_n_n);
    return result;
}

ltl_formula_t *ltl_weak_until(ltl_formula_t *phi, ltl_formula_t *psi) {
    ltl_formula_t *until_part = ltl_until(phi, psi);
    ltl_formula_t *always_phi = ltl_always(phi);
    ltl_formula_t *result = ltl_or(until_part, always_phi);
    ltl_free(until_part); ltl_free(always_phi);
    return result;
}

ltl_formula_t *ltl_strong_release(ltl_formula_t *phi, ltl_formula_t *psi) {
    ltl_formula_t *phi_and_psi = ltl_and(phi, psi);
    ltl_formula_t *until_part = ltl_until(psi, phi_and_psi);
    ltl_formula_t *always_psi = ltl_always(psi);
    ltl_formula_t *result = ltl_or(until_part, always_psi);
    ltl_free(phi_and_psi); ltl_free(until_part); ltl_free(always_psi);
    return result;
}

/* ====================================================================
 * Formula Analysis
 * ==================================================================== */

int32_t ltl_size(const ltl_formula_t *phi) {
    if (phi == NULL) return 0;
    switch (phi->type) {
    case LTL_TRUE: case LTL_FALSE: case LTL_VAR: return 1;
    case LTL_NOT: case LTL_X: case LTL_F: case LTL_G: return 1 + ltl_size(phi->left);
    case LTL_AND: case LTL_OR: case LTL_IMPLIES: case LTL_EQUIV:
    case LTL_U: case LTL_R: case LTL_W: case LTL_M:
        return 1 + ltl_size(phi->left) + ltl_size(phi->right);
    default: return 0;
    }
}

int32_t ltl_temporal_depth(const ltl_formula_t *phi) {
    if (phi == NULL) return 0;
    switch (phi->type) {
    case LTL_TRUE: case LTL_FALSE: case LTL_VAR: return 0;
    case LTL_NOT: return ltl_temporal_depth(phi->left);
    case LTL_AND: case LTL_OR: case LTL_IMPLIES: case LTL_EQUIV:
    case LTL_U: case LTL_R: case LTL_W: case LTL_M: {
        int32_t dl = ltl_temporal_depth(phi->left);
        int32_t dr = ltl_temporal_depth(phi->right);
        return (dl > dr ? dl : dr) + 1;
    }
    case LTL_X: case LTL_F: case LTL_G: return 1 + ltl_temporal_depth(phi->left);
    default: return 0;
    }
}

valuation_t ltl_collect_propositions(const ltl_formula_t *phi) {
    valuation_t result = VALUATION_EMPTY;
    if (phi == NULL) return result;
    switch (phi->type) {
    case LTL_VAR: result = valuation_set(result, phi->var_id, true); break;
    case LTL_NOT: case LTL_X: case LTL_F: case LTL_G:
        result = ltl_collect_propositions(phi->left); break;
    case LTL_AND: case LTL_OR: case LTL_IMPLIES: case LTL_EQUIV:
    case LTL_U: case LTL_R: case LTL_W: case LTL_M:
        result = valuation_or(ltl_collect_propositions(phi->left),
                              ltl_collect_propositions(phi->right));
        break;
    default: break;
    }
    return result;
}

bool ltl_equal(const ltl_formula_t *a, const ltl_formula_t *b) {
    if (a == b) return true;
    if (a == NULL || b == NULL) return false;
    return false;
}

/* ====================================================================
 * Subformula Collection
 * ==================================================================== */

static void collect_subs(const ltl_formula_t *phi, ltl_formula_t **arr,
                         int32_t *count, int32_t *capacity) {
    if (phi == NULL) return;
    for (int32_t i = 0; i < *count; i++) {
        if (arr[i] == phi) return;
    }
    if (*count >= *capacity) {
        *capacity *= 2;
        arr = (ltl_formula_t **)realloc(arr, (size_t)(*capacity) * sizeof(ltl_formula_t *));
    }
    arr[*count] = (ltl_formula_t *)phi;
    (*count)++;
    switch (phi->type) {
    case LTL_TRUE: case LTL_FALSE: case LTL_VAR: break;
    case LTL_NOT: case LTL_X: case LTL_F: case LTL_G:
        collect_subs(phi->left, arr, count, capacity); break;
    case LTL_AND: case LTL_OR: case LTL_IMPLIES: case LTL_EQUIV:
    case LTL_U: case LTL_R: case LTL_W: case LTL_M:
        collect_subs(phi->left, arr, count, capacity);
        collect_subs(phi->right, arr, count, capacity);
        break;
    }
}

ltl_formula_t **ltl_subformulas(const ltl_formula_t *phi, int32_t *count) {
    if (phi == NULL) { *count = 0; return NULL; }
    int32_t capacity = 16;
    ltl_formula_t **arr = (ltl_formula_t **)malloc((size_t)capacity * sizeof(ltl_formula_t *));
    assert(arr != NULL);
    *count = 0;
    collect_subs(phi, arr, count, &capacity);
    return arr;
}

/* ====================================================================
 * Fischer-Ladner Closure
 *
 * Reference: Fischer & Ladner (1979) "Propositional Dynamic Logic
 *            of Regular Programs", JCSS
 * ==================================================================== */

ltl_formula_t **ltl_fischer_ladner_closure(const ltl_formula_t *phi,
                                            int32_t *closure_size) {
    if (phi == NULL) { *closure_size = 0; return NULL; }
    ltl_formula_t *phi_nnf = ltl_to_nnf(phi);
    int32_t cnt = 0;
    int32_t cap = 64;
    ltl_formula_t **closure = (ltl_formula_t **)malloc((size_t)cap * sizeof(ltl_formula_t *));
    assert(closure != NULL);
    ltl_formula_t **subs = ltl_subformulas(phi_nnf, &cnt);
    if (cnt > cap) {
        cap = cnt;
        closure = (ltl_formula_t **)realloc(closure, (size_t)cap * sizeof(ltl_formula_t *));
    }
    memcpy(closure, subs, (size_t)cnt * sizeof(ltl_formula_t *));
    free(subs);
    *closure_size = cnt;
    ltl_free(phi_nnf);
    return closure;
}

/* ====================================================================
 * Negation Normal Form (NNF) Transformation
 *
 * Push negations inward to the atomic level.
 * Eliminates IMPLIES/EQUIV. Result has NOT only in front of VAR.
 * ==================================================================== */

ltl_formula_t *ltl_to_nnf(const ltl_formula_t *phi) {
    if (phi == NULL) return NULL;
    switch (phi->type) {
    case LTL_TRUE:  return ltl_true();
    case LTL_FALSE: return ltl_false();
    case LTL_VAR:   return ltl_var(phi->var_id);
    case LTL_NOT: {
        ltl_formula_t *child = phi->left;
        switch (child->type) {
        case LTL_TRUE:  return ltl_false();
        case LTL_FALSE: return ltl_true();
        case LTL_VAR: {
            ltl_formula_t *v = ltl_var(child->var_id);
            ltl_formula_t *n = ltl_not(v);
            ltl_free(v);
            return n;
        }
        case LTL_NOT: return ltl_to_nnf(child->left);
        case LTL_AND: {
            ltl_formula_t *l = ltl_to_nnf(ltl_not(child->left));
            ltl_formula_t *r = ltl_to_nnf(ltl_not(child->right));
            ltl_formula_t *res = ltl_or(l, r);
            ltl_free(l); ltl_free(r);
            return res;
        }
        case LTL_OR: {
            ltl_formula_t *l = ltl_to_nnf(ltl_not(child->left));
            ltl_formula_t *r = ltl_to_nnf(ltl_not(child->right));
            ltl_formula_t *res = ltl_and(l, r);
            ltl_free(l); ltl_free(r);
            return res;
        }
        case LTL_X: {
            ltl_formula_t *inner = ltl_to_nnf(ltl_not(child->left));
            ltl_formula_t *res = ltl_next(inner);
            ltl_free(inner);
            return res;
        }
        case LTL_U: {
            ltl_formula_t *l = ltl_to_nnf(ltl_not(child->left));
            ltl_formula_t *r = ltl_to_nnf(ltl_not(child->right));
            ltl_formula_t *res = ltl_release(l, r);
            ltl_free(l); ltl_free(r);
            return res;
        }
        case LTL_R: {
            ltl_formula_t *l = ltl_to_nnf(ltl_not(child->left));
            ltl_formula_t *r = ltl_to_nnf(ltl_not(child->right));
            ltl_formula_t *res = ltl_until(l, r);
            ltl_free(l); ltl_free(r);
            return res;
        }
        case LTL_F: {
            ltl_formula_t *inner = ltl_to_nnf(ltl_not(child->left));
            ltl_formula_t *res = ltl_always(inner);
            ltl_free(inner);
            return res;
        }
        case LTL_G: {
            ltl_formula_t *inner = ltl_to_nnf(ltl_not(child->left));
            ltl_formula_t *res = ltl_eventually(inner);
            ltl_free(inner);
            return res;
        }
        default: return ltl_false();
        }
    }
    case LTL_AND: case LTL_OR: {
        ltl_formula_t *l = ltl_to_nnf(phi->left);
        ltl_formula_t *r = ltl_to_nnf(phi->right);
        ltl_formula_t *res = (phi->type == LTL_AND) ? ltl_and(l, r) : ltl_or(l, r);
        ltl_free(l); ltl_free(r);
        return res;
    }
    case LTL_IMPLIES: {
        ltl_formula_t *l = ltl_to_nnf(ltl_not(phi->left));
        ltl_formula_t *r = ltl_to_nnf(phi->right);
        ltl_formula_t *res = ltl_or(l, r);
        ltl_free(l); ltl_free(r);
        return res;
    }
    case LTL_EQUIV: {
        ltl_formula_t *l1 = ltl_to_nnf(ltl_not(phi->left));
        ltl_formula_t *r1 = ltl_to_nnf(phi->right);
        ltl_formula_t *o1 = ltl_or(l1, r1);
        ltl_formula_t *l2 = ltl_to_nnf(phi->left);
        ltl_formula_t *r2 = ltl_to_nnf(ltl_not(phi->right));
        ltl_formula_t *o2 = ltl_or(l2, r2);
        ltl_formula_t *res = ltl_and(o1, o2);
        ltl_free(l1); ltl_free(r1); ltl_free(o1);
        ltl_free(l2); ltl_free(r2); ltl_free(o2);
        return res;
    }
    case LTL_X: case LTL_F: case LTL_G: {
        ltl_formula_t *inner = ltl_to_nnf(phi->left);
        ltl_formula_t *res;
        if (phi->type == LTL_X) res = ltl_next(inner);
        else if (phi->type == LTL_F) res = ltl_eventually(inner);
        else res = ltl_always(inner);
        ltl_free(inner);
        return res;
    }
    case LTL_U: case LTL_R: case LTL_W: case LTL_M: {
        ltl_formula_t *l = ltl_to_nnf(phi->left);
        ltl_formula_t *r = ltl_to_nnf(phi->right);
        ltl_formula_t *res;
        if (phi->type == LTL_U) res = ltl_until(l, r);
        else if (phi->type == LTL_R) res = ltl_release(l, r);
        else if (phi->type == LTL_W) res = ltl_weak_until(l, r);
        else res = ltl_strong_release(l, r);
        ltl_free(l); ltl_free(r);
        return res;
    }
    default: return ltl_false();
    }
}

/* ====================================================================
 * LTL Expansion (Tableau Decomposition)
 *
 * Separates "now" from "next". Key step in automaton construction.
 * Reference: Lichtenstein & Pnueli (1985) POPL 1985
 * ==================================================================== */

ltl_formula_t *ltl_expand(const ltl_formula_t *phi) {
    if (phi == NULL) return NULL;
    switch (phi->type) {
    case LTL_TRUE:  return ltl_true();
    case LTL_FALSE: return ltl_false();
    case LTL_VAR:   return ltl_var(phi->var_id);
    case LTL_NOT: {
        ltl_formula_t *inner = ltl_expand(phi->left);
        ltl_formula_t *res = ltl_not(inner);
        ltl_free(inner); return res;
    }
    case LTL_AND: case LTL_OR: {
        ltl_formula_t *l = ltl_expand(phi->left);
        ltl_formula_t *r = ltl_expand(phi->right);
        ltl_formula_t *res = (phi->type == LTL_AND) ? ltl_and(l, r) : ltl_or(l, r);
        ltl_free(l); ltl_free(r); return res;
    }
    case LTL_X:
        return ltl_next(ltl_clone((ltl_formula_t *)phi->left));
    case LTL_U: {
        /* phi U psi -> psi OR (phi AND X(phi U psi)) */
        ltl_formula_t *psi = ltl_clone((ltl_formula_t *)phi->right);
        ltl_formula_t *phi_c = ltl_clone((ltl_formula_t *)phi->left);
        ltl_formula_t *x_until = ltl_next(ltl_clone((ltl_formula_t *)phi));
        ltl_formula_t *phi_and_x = ltl_and(phi_c, x_until);
        ltl_formula_t *res = ltl_or(psi, phi_and_x);
        ltl_free(phi_and_x); return res;
    }
    case LTL_R: {
        /* phi R psi -> psi AND (phi OR X(phi R psi)) */
        ltl_formula_t *psi = ltl_clone((ltl_formula_t *)phi->right);
        ltl_formula_t *phi_c = ltl_clone((ltl_formula_t *)phi->left);
        ltl_formula_t *x_rel = ltl_next(ltl_clone((ltl_formula_t *)phi));
        ltl_formula_t *phi_or_x = ltl_or(phi_c, x_rel);
        ltl_formula_t *res = ltl_and(psi, phi_or_x);
        ltl_free(phi_or_x); return res;
    }
    case LTL_F: {
        /* F phi -> phi OR X(F phi) */
        ltl_formula_t *phi_c = ltl_clone((ltl_formula_t *)phi->left);
        ltl_formula_t *x_f = ltl_next(ltl_clone((ltl_formula_t *)phi));
        ltl_formula_t *res = ltl_or(phi_c, x_f);
        return res;
    }
    case LTL_G: {
        /* G phi -> phi AND X(G phi) */
        ltl_formula_t *phi_c = ltl_clone((ltl_formula_t *)phi->left);
        ltl_formula_t *x_g = ltl_next(ltl_clone((ltl_formula_t *)phi));
        ltl_formula_t *res = ltl_and(phi_c, x_g);
        return res;
    }
    default: return ltl_true();
    }
}

/* ====================================================================
 * Bounded Semantic Evaluation on Finite Traces
 *
 * Reference: De Giacomo & Vardi (2013) IJCAI
 * ==================================================================== */

bool ltl_evaluate_bounded(const ltl_formula_t *phi,
                           const valuation_t *trace,
                           int32_t trace_len,
                           int32_t pos) {
    if (phi == NULL || trace == NULL) return false;
    if (pos < 0) return false;
    if (pos >= trace_len) return (phi->type == LTL_TRUE);

    switch (phi->type) {
    case LTL_TRUE:  return true;
    case LTL_FALSE: return false;
    case LTL_VAR:   return valuation_get(trace[pos], phi->var_id);
    case LTL_NOT:
        return !ltl_evaluate_bounded(phi->left, trace, trace_len, pos);
    case LTL_AND:
        return ltl_evaluate_bounded(phi->left, trace, trace_len, pos) &&
               ltl_evaluate_bounded(phi->right, trace, trace_len, pos);
    case LTL_OR:
        return ltl_evaluate_bounded(phi->left, trace, trace_len, pos) ||
               ltl_evaluate_bounded(phi->right, trace, trace_len, pos);
    case LTL_IMPLIES:
        return !ltl_evaluate_bounded(phi->left, trace, trace_len, pos) ||
               ltl_evaluate_bounded(phi->right, trace, trace_len, pos);
    case LTL_EQUIV:
        return ltl_evaluate_bounded(phi->left, trace, trace_len, pos) ==
               ltl_evaluate_bounded(phi->right, trace, trace_len, pos);
    case LTL_X:
        if (pos + 1 >= trace_len) return false;
        return ltl_evaluate_bounded(phi->left, trace, trace_len, pos + 1);
    case LTL_F:
        for (int32_t k = pos; k < trace_len; k++)
            if (ltl_evaluate_bounded(phi->left, trace, trace_len, k)) return true;
        return false;
    case LTL_G:
        for (int32_t k = pos; k < trace_len; k++)
            if (!ltl_evaluate_bounded(phi->left, trace, trace_len, k)) return false;
        return true;
    case LTL_U:
        for (int32_t k = pos; k < trace_len; k++) {
            if (ltl_evaluate_bounded(phi->right, trace, trace_len, k)) {
                bool all_left = true;
                for (int32_t j = pos; j < k; j++)
                    if (!ltl_evaluate_bounded(phi->left, trace, trace_len, j))
                        { all_left = false; break; }
                if (all_left) return true;
            }
        }
        return false;
    case LTL_R: {
        for (int32_t k = pos; k < trace_len; k++) {
            if (!ltl_evaluate_bounded(phi->right, trace, trace_len, k)) {
                bool phi_found = false;
                for (int32_t j = pos; j < k; j++)
                    if (ltl_evaluate_bounded(phi->left, trace, trace_len, j))
                        { phi_found = true; break; }
                return phi_found;
            }
        }
        return true;
    }
    case LTL_W: {
        /* W is like U but psi not required to eventually hold */
        for (int32_t k = pos; k < trace_len; k++) {
            if (ltl_evaluate_bounded(phi->right, trace, trace_len, k)) {
                bool all_left = true;
                for (int32_t j = pos; j < k; j++)
                    if (!ltl_evaluate_bounded(phi->left, trace, trace_len, j))
                        { all_left = false; break; }
                if (all_left) return true;
            }
        }
        /* Check if phi holds forever */
        for (int32_t k = pos; k < trace_len; k++)
            if (!ltl_evaluate_bounded(phi->left, trace, trace_len, k)) return false;
        return true;
    }
    case LTL_M: {
        for (int32_t k = pos; k < trace_len; k++) {
            if (ltl_evaluate_bounded(phi->right, trace, trace_len, k) &&
                ltl_evaluate_bounded(phi->left, trace, trace_len, k)) {
                bool all_psi = true;
                for (int32_t j = pos; j < k; j++)
                    if (!ltl_evaluate_bounded(phi->right, trace, trace_len, j))
                        { all_psi = false; break; }
                if (all_psi) return true;
            }
        }
        /* Check if psi holds forever */
        for (int32_t k = pos; k < trace_len; k++)
            if (!ltl_evaluate_bounded(phi->right, trace, trace_len, k)) return false;
        return true;
    }
    default: return false;
    }
}

/* ====================================================================
 * LTL Formula Printing
 * ==================================================================== */

char *ltl_to_string(const ltl_formula_t *phi, char *buf, int32_t buf_size) {
    if (phi == NULL || buf == NULL || buf_size < 2) { if (buf) buf[0] = '\0'; return buf; }
    buf[0] = '\0';
    switch (phi->type) {
    case LTL_TRUE:  snprintf(buf, buf_size, "true"); break;
    case LTL_FALSE: snprintf(buf, buf_size, "false"); break;
    case LTL_VAR:   snprintf(buf, buf_size, "p%d", phi->var_id); break;
    case LTL_NOT: { char s[256]; ltl_to_string(phi->left, s, 256); snprintf(buf, buf_size, "!%s", s); break; }
    case LTL_X: { char s[256]; ltl_to_string(phi->left, s, 256); snprintf(buf, buf_size, "X(%s)", s); break; }
    case LTL_F: { char s[256]; ltl_to_string(phi->left, s, 256); snprintf(buf, buf_size, "F(%s)", s); break; }
    case LTL_G: { char s[256]; ltl_to_string(phi->left, s, 256); snprintf(buf, buf_size, "G(%s)", s); break; }
    case LTL_AND: { char l[256],r[256]; ltl_to_string(phi->left,l,256); ltl_to_string(phi->right,r,256); snprintf(buf, buf_size, "(%s & %s)", l, r); break; }
    case LTL_OR:  { char l[256],r[256]; ltl_to_string(phi->left,l,256); ltl_to_string(phi->right,r,256); snprintf(buf, buf_size, "(%s | %s)", l, r); break; }
    case LTL_U:   { char l[256],r[256]; ltl_to_string(phi->left,l,256); ltl_to_string(phi->right,r,256); snprintf(buf, buf_size, "(%s U %s)", l, r); break; }
    case LTL_R:   { char l[256],r[256]; ltl_to_string(phi->left,l,256); ltl_to_string(phi->right,r,256); snprintf(buf, buf_size, "(%s R %s)", l, r); break; }
    default: snprintf(buf, buf_size, "(?%d)", phi->id); break;
    }
    return buf;
}

void ltl_print(const ltl_formula_t *phi) {
    char buf[1024];
    ltl_to_string(phi, buf, sizeof(buf));
    printf("%s", buf);
}

/* ====================================================================
 * Simple Recursive Descent LTL Parser
 *
 * Grammar: expr := iff_expr ; iff_expr := imp_expr ("<->" imp_expr)*
 *   imp_expr := until_expr ("->" until_expr)*
 *   until_expr := release_expr ("U" release_expr)*
 *   release_expr := or_expr ("R" or_expr)*
 *   or_expr := and_expr ("|" and_expr)*
 *   and_expr := unary_expr ("&" unary_expr)*
 *   unary_expr := "!" unary_expr | "X" unary_expr | "F" unary_expr
 *               | "G" unary_expr | atomic | "(" expr ")"
 *   atomic := "true" | "false" | VARNAME
 * ==================================================================== */

#define MAX_VARS 128
static char *g_var_names[MAX_VARS];
static int32_t g_var_count = 0;

static void parser_reset(void) {
    for (int32_t i = 0; i < g_var_count; i++) free(g_var_names[i]);
    g_var_count = 0;
}

static int32_t parser_get_var_id(const char *name) {
    for (int32_t i = 0; i < g_var_count; i++)
        if (strcmp(g_var_names[i], name) == 0) return i;
    if (g_var_count >= MAX_VARS) return -1;
    g_var_names[g_var_count] = strdup(name);
    return g_var_count++;
}

static void skip_ws(const char **s) {
    while (**s == ' ' || **s == '\t' || **s == '\n' || **s == '\r') (*s)++;
}

static ltl_formula_t *parse_expr(const char **s);
static ltl_formula_t *parse_iff(const char **s);
static ltl_formula_t *parse_implies(const char **s);
static ltl_formula_t *parse_until(const char **s);
static ltl_formula_t *parse_release(const char **s);
static ltl_formula_t *parse_or(const char **s);
static ltl_formula_t *parse_and(const char **s);
static ltl_formula_t *parse_unary(const char **s);
static ltl_formula_t *parse_atomic(const char **s);

static ltl_formula_t *parse_atomic(const char **s) {
    skip_ws(s);
    if (**s == '(') {
        (*s)++;
        ltl_formula_t *inner = parse_expr(s);
        skip_ws(s);
        if (**s == ')') { (*s)++; return inner; }
        ltl_free(inner); return NULL;
    }
    if (strncmp(*s, "true", 4) == 0 && !isalnum((*s)[4])) { *s += 4; return ltl_true(); }
    if (strncmp(*s, "false", 5) == 0 && !isalnum((*s)[5])) { *s += 5; return ltl_false(); }
    if (isalpha(**s)) {
        char vname[64]; int32_t len = 0;
        while (isalnum(**s) || **s == '_') { if (len < 63) vname[len++] = **s; (*s)++; }
        vname[len] = '\0';
        int32_t id = parser_get_var_id(vname);
        if (id < 0) return NULL;
        return ltl_var(id);
    }
    return NULL;
}

static ltl_formula_t *parse_unary(const char **s) {
    skip_ws(s);
    if (**s == '!') { (*s)++; ltl_formula_t *inner = parse_unary(s); return inner ? ltl_not(inner) : NULL; }
    if (**s == 'X') { (*s)++; ltl_formula_t *inner = parse_unary(s); return inner ? ltl_next(inner) : NULL; }
    if (**s == 'F') { (*s)++; ltl_formula_t *inner = parse_unary(s); return inner ? ltl_eventually(inner) : NULL; }
    if (**s == 'G') { (*s)++; ltl_formula_t *inner = parse_unary(s); return inner ? ltl_always(inner) : NULL; }
    return parse_atomic(s);
}

static ltl_formula_t *parse_and(const char **s) {
    ltl_formula_t *left = parse_unary(s);
    if (!left) return NULL;
    skip_ws(s);
    while (**s == '&') {
        (*s)++;
        ltl_formula_t *right = parse_unary(s);
        if (!right) { ltl_free(left); return NULL; }
        ltl_formula_t *nl = ltl_and(left, right);
        ltl_free(left); ltl_free(right); left = nl;
        skip_ws(s);
    }
    return left;
}

static ltl_formula_t *parse_or(const char **s) {
    ltl_formula_t *left = parse_and(s);
    if (!left) return NULL;
    skip_ws(s);
    while (**s == '|') {
        (*s)++;
        ltl_formula_t *right = parse_and(s);
        if (!right) { ltl_free(left); return NULL; }
        ltl_formula_t *nl = ltl_or(left, right);
        ltl_free(left); ltl_free(right); left = nl;
        skip_ws(s);
    }
    return left;
}

static ltl_formula_t *parse_release(const char **s) {
    ltl_formula_t *left = parse_or(s);
    if (!left) return NULL;
    skip_ws(s);
    while (**s == 'R') {
        (*s)++;
        ltl_formula_t *right = parse_or(s);
        if (!right) { ltl_free(left); return NULL; }
        ltl_formula_t *nl = ltl_release(left, right);
        ltl_free(left); ltl_free(right); left = nl;
        skip_ws(s);
    }
    return left;
}

static ltl_formula_t *parse_until(const char **s) {
    ltl_formula_t *left = parse_release(s);
    if (!left) return NULL;
    skip_ws(s);
    while (**s == 'U') {
        (*s)++;
        ltl_formula_t *right = parse_release(s);
        if (!right) { ltl_free(left); return NULL; }
        ltl_formula_t *nl = ltl_until(left, right);
        ltl_free(left); ltl_free(right); left = nl;
        skip_ws(s);
    }
    return left;
}

static ltl_formula_t *parse_implies(const char **s) {
    ltl_formula_t *left = parse_until(s);
    if (!left) return NULL;
    skip_ws(s);
    if ((*s)[0] == '-' && (*s)[1] == '>') {
        *s += 2;
        ltl_formula_t *right = parse_implies(s);
        if (!right) { ltl_free(left); return NULL; }
        ltl_formula_t *res = ltl_implies(left, right);
        ltl_free(left); ltl_free(right);
        return res;
    }
    return left;
}

static ltl_formula_t *parse_iff(const char **s) {
    ltl_formula_t *left = parse_implies(s);
    if (!left) return NULL;
    skip_ws(s);
    if ((*s)[0] == '<' && (*s)[1] == '-' && (*s)[2] == '>') {
        *s += 3;
        ltl_formula_t *right = parse_iff(s);
        if (!right) { ltl_free(left); return NULL; }
        ltl_formula_t *res = ltl_equiv(left, right);
        ltl_free(left); ltl_free(right);
        return res;
    }
    return left;
}

static ltl_formula_t *parse_expr(const char **s) {
    return parse_iff(s);
}

ltl_formula_t *ltl_parse(const char *str, int32_t *var_count) {
    if (str == NULL) return NULL;
    parser_reset();
    const char *s = str;
    ltl_formula_t *result = parse_expr(&s);
    skip_ws(&s);
    if (*s != '\0') { ltl_free(result); parser_reset(); return NULL; }
    if (var_count) *var_count = g_var_count;
    return result;
}
