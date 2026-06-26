/**
 * @file ltl.c
 * @brief Linear Temporal Logic - core implementation
 *
 * Implements the full LTL formula algebra: constructors (L1), memory
 * management and metrics (L2), NNF conversion (L3), simplification (L4),
 * operator rewriting and Fischer-Ladner closure (L5), printing/analysis (L7).
 *
 * Key algorithms:
 *   - NNF via push-negation duality rules (Baier & Katoen Def 5.20)
 *   - Fixed-point expansion laws (Baier & Katoen Thm 5.18)
 *   - Fischer-Ladner closure (Fischer & Ladner, JCSS 1979)
 *   - Derived operator elimination to {X, U, not, and}
 *
 * References:
 *   - Pnueli, "The Temporal Logic of Programs" (FOCS 1977)
 *   - Baier & Katoen, "Principles of Model Checking" (2008), Ch.5
 *   - Manna & Pnueli, "Temporal Logic of Reactive Systems" (1992)
 */

#include "ltl.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static uint32_t g_id = 0;

static ltl_formula_t *alloc_node(ltl_op_t op, ltl_formula_t *l,
                                  ltl_formula_t *r, uint32_t aid)
{
    ltl_formula_t *f = malloc(sizeof(ltl_formula_t));
    if (!f) return NULL;
    f->op = op; f->atom_id = aid; f->left = l; f->right = r;
    f->ref_count = 1; f->id = g_id++; f->flag = false; f->closure_id = 0;
    return f;
}

/* ---------- L1: Constructors (Pnueli 1977 LTL syntax) ---------- */

ltl_formula_t *ltl_true(void)
    { return alloc_node(LTL_CONST_TRUE, NULL, NULL, 0); }
ltl_formula_t *ltl_false(void)
    { return alloc_node(LTL_CONST_FALSE, NULL, NULL, 0); }
ltl_formula_t *ltl_atom(uint32_t a)
    { return alloc_node(LTL_ATOM, NULL, NULL, a); }
ltl_formula_t *ltl_not(ltl_formula_t *p)
    { return p ? alloc_node(LTL_NOT, p, NULL, 0) : NULL; }
ltl_formula_t *ltl_and(ltl_formula_t *p, ltl_formula_t *q)
    { return (p&&q) ? alloc_node(LTL_AND, p, q, 0) : NULL; }
ltl_formula_t *ltl_or(ltl_formula_t *p, ltl_formula_t *q)
    { return (p&&q) ? alloc_node(LTL_OR, p, q, 0) : NULL; }
ltl_formula_t *ltl_implies(ltl_formula_t *p, ltl_formula_t *q)
    { return (p&&q) ? alloc_node(LTL_IMPLIES, p, q, 0) : NULL; }
ltl_formula_t *ltl_equiv(ltl_formula_t *p, ltl_formula_t *q)
    { return (p&&q) ? alloc_node(LTL_EQUIV, p, q, 0) : NULL; }
ltl_formula_t *ltl_xor(ltl_formula_t *p, ltl_formula_t *q)
    { return (p&&q) ? alloc_node(LTL_XOR, p, q, 0) : NULL; }
ltl_formula_t *ltl_next(ltl_formula_t *p)
    { return p ? alloc_node(LTL_NEXT, p, NULL, 0) : NULL; }
ltl_formula_t *ltl_finally(ltl_formula_t *p)
    { return p ? alloc_node(LTL_FINALLY, p, NULL, 0) : NULL; }
ltl_formula_t *ltl_globally(ltl_formula_t *p)
    { return p ? alloc_node(LTL_GLOBALLY, p, NULL, 0) : NULL; }
ltl_formula_t *ltl_until(ltl_formula_t *p, ltl_formula_t *q)
    { return (p&&q) ? alloc_node(LTL_UNTIL, p, q, 0) : NULL; }
ltl_formula_t *ltl_release(ltl_formula_t *p, ltl_formula_t *q)
    { return (p&&q) ? alloc_node(LTL_RELEASE, p, q, 0) : NULL; }
ltl_formula_t *ltl_weak_until(ltl_formula_t *p, ltl_formula_t *q)
    { return (p&&q) ? alloc_node(LTL_WEAK_UNTIL, p, q, 0) : NULL; }
ltl_formula_t *ltl_strong_release(ltl_formula_t *p, ltl_formula_t *q)
    { return (p&&q) ? alloc_node(LTL_STRONG_RELEASE, p, q, 0) : NULL; }

/* ---------- L2: Memory management (reference counting) ---------- */

ltl_formula_t *ltl_ref(ltl_formula_t *phi)
    { if (phi) phi->ref_count++; return phi; }

void ltl_free(ltl_formula_t *phi)
{
    if (!phi) return;
    if (--phi->ref_count <= 0) {
        ltl_free(phi->left);
        ltl_free(phi->right);
        free(phi);
    }
}

/* ---------- Formula metrics ---------- */

size_t ltl_size(const ltl_formula_t *phi)
{
    if (!phi) return 0;
    if (phi->op == LTL_CONST_TRUE || phi->op == LTL_CONST_FALSE ||
        phi->op == LTL_ATOM || phi->op == LTL_LITERAL_POS ||
        phi->op == LTL_LITERAL_NEG) return 1;
    return 1 + ltl_size(phi->left) + ltl_size(phi->right);
}

size_t ltl_depth(const ltl_formula_t *phi)
{
    if (!phi) return 0;
    if (phi->op == LTL_CONST_TRUE || phi->op == LTL_CONST_FALSE ||
        phi->op == LTL_ATOM || phi->op == LTL_LITERAL_POS ||
        phi->op == LTL_LITERAL_NEG) return 1;
    size_t dl = phi->left  ? ltl_depth(phi->left)  : 0;
    size_t dr = phi->right ? ltl_depth(phi->right) : 0;
    return 1 + ((dl > dr) ? dl : dr);
}

/* Pointer-set for subformula deduplication */
typedef struct { ltl_formula_t **v; size_t n, cap; } fset_t;

static int fset_has(fset_t *s, const ltl_formula_t *f) {
    for (size_t i = 0; i < s->n; i++) if (s->v[i] == f) return 1;
    return 0;
}
static void fset_put(fset_t *s, ltl_formula_t *f) {
    if (!f || fset_has(s, f)) return;
    if (s->n >= s->cap) { s->cap = s->cap ? s->cap*2 : 64;
        s->v = realloc(s->v, s->cap * sizeof(ltl_formula_t*)); }
    s->v[s->n++] = f;
}
static void fset_collect(const ltl_formula_t *phi, fset_t *s) {
    if (!phi) return;
    fset_put(s, (ltl_formula_t*)phi);
    fset_collect(phi->left, s); fset_collect(phi->right, s);
}

size_t ltl_subformula_count(const ltl_formula_t *phi) {
    fset_t s = {NULL,0,0}; fset_collect(phi, &s);
    size_t c = s.n; free(s.v); return c;
}

uint32_t ltl_atoms_used(const ltl_formula_t *phi)
{
    if (!phi) return 0;
    if (phi->op == LTL_ATOM || phi->op == LTL_LITERAL_POS ||
        phi->op == LTL_LITERAL_NEG)
        return 1u << phi->atom_id;
    return ltl_atoms_used(phi->left) | ltl_atoms_used(phi->right);
}

ltl_formula_t *ltl_clone(const ltl_formula_t *phi)
{
    if (!phi) return NULL;
    return alloc_node(phi->op, ltl_clone(phi->left),
                      ltl_clone(phi->right), phi->atom_id);
}

/* ==================================================================
 * L3: Negation Normal Form (NNF)
 *
 * Push negations inward using temporal duality laws.
 * not(not a)=>a, not(a&&b)=>not a||not b, not(a||b)=>not a&&not b,
 * not X a=>X not a, not F a=>G not a, not G a=>F not a,
 * not(a U b)=>not a R not b, not(a R b)=>not a U not b,
 * not(a W b)=>not a M not b, not(a M b)=>not a W not b.
 * O(|phi|) time, result at most 2x original size.
 * ================================================================== */

ltl_formula_t *ltl_to_nnf(ltl_formula_t *phi)
{
    if (!phi) return NULL;
    switch (phi->op) {
    case LTL_CONST_TRUE: case LTL_CONST_FALSE: case LTL_ATOM:
    case LTL_LITERAL_POS: case LTL_LITERAL_NEG: return phi;

    case LTL_NOT: {
        ltl_formula_t *c = phi->left; if (!c) return phi;
        switch (c->op) {
        case LTL_CONST_TRUE:  ltl_free(phi); return ltl_false();
        case LTL_CONST_FALSE: ltl_free(phi); return ltl_true();
        case LTL_ATOM:
            phi->op = LTL_LITERAL_NEG; phi->atom_id = c->atom_id;
            phi->left = NULL; ltl_free(c); return phi;
        case LTL_NOT: {
            ltl_formula_t *in = c->left; if (in) ltl_ref(in);
            ltl_free(phi); return ltl_to_nnf(in);
        }
        case LTL_AND: {
            ltl_formula_t *a=c->left,*b=c->right; ltl_ref(a);ltl_ref(b);
            ltl_formula_t *na=ltl_to_nnf(ltl_not(a)),*nb=ltl_to_nnf(ltl_not(b));
            ltl_free(phi); return ltl_or(na,nb);
        }
        case LTL_OR: {
            ltl_formula_t *a=c->left,*b=c->right; ltl_ref(a);ltl_ref(b);
            ltl_formula_t *na=ltl_to_nnf(ltl_not(a)),*nb=ltl_to_nnf(ltl_not(b));
            ltl_free(phi); return ltl_and(na,nb);
        }
        case LTL_IMPLIES: {
            ltl_formula_t *a=c->left,*b=c->right; ltl_ref(a);ltl_ref(b);
            ltl_free(phi); return ltl_to_nnf(ltl_and(a,ltl_not(b)));
        }
        case LTL_EQUIV: {
            ltl_formula_t *a=c->left,*b=c->right; ltl_ref(a);ltl_ref(b);
            ltl_formula_t *anb=ltl_and(ltl_to_nnf(a),ltl_to_nnf(ltl_not(b)));
            ltl_formula_t *bna=ltl_and(ltl_to_nnf(b),ltl_to_nnf(ltl_not(a)));
            ltl_free(phi); return ltl_to_nnf(ltl_or(anb,bna));
        }
        case LTL_NEXT: {
            ltl_formula_t *a=c->left;ltl_ref(a);
            ltl_free(phi); return ltl_next(ltl_to_nnf(ltl_not(a)));
        }
        case LTL_FINALLY: {
            ltl_formula_t *a=c->left;ltl_ref(a);
            ltl_free(phi); return ltl_globally(ltl_to_nnf(ltl_not(a)));
        }
        case LTL_GLOBALLY: {
            ltl_formula_t *a=c->left;ltl_ref(a);
            ltl_free(phi); return ltl_finally(ltl_to_nnf(ltl_not(a)));
        }
        case LTL_UNTIL: {
            ltl_formula_t *a=c->left,*b=c->right; ltl_ref(a);ltl_ref(b);
            ltl_free(phi); return ltl_release(ltl_to_nnf(ltl_not(a)),ltl_to_nnf(ltl_not(b)));
        }
        case LTL_RELEASE: {
            ltl_formula_t *a=c->left,*b=c->right; ltl_ref(a);ltl_ref(b);
            ltl_free(phi); return ltl_until(ltl_to_nnf(ltl_not(a)),ltl_to_nnf(ltl_not(b)));
        }
        case LTL_WEAK_UNTIL: {
            ltl_formula_t *a=c->left,*b=c->right; ltl_ref(a);ltl_ref(b);
            ltl_free(phi); return ltl_strong_release(ltl_to_nnf(ltl_not(a)),ltl_to_nnf(ltl_not(b)));
        }
        case LTL_STRONG_RELEASE: {
            ltl_formula_t *a=c->left,*b=c->right; ltl_ref(a);ltl_ref(b);
            ltl_free(phi); return ltl_weak_until(ltl_to_nnf(ltl_not(a)),ltl_to_nnf(ltl_not(b)));
        }
        default: return phi;
        }
    }

    case LTL_AND: case LTL_OR: case LTL_UNTIL: case LTL_RELEASE:
    case LTL_WEAK_UNTIL: case LTL_STRONG_RELEASE:
        if (phi->left)  { ltl_formula_t *s=ltl_to_nnf(phi->left);
            if(s!=phi->left){ltl_free(phi->left);phi->left=s;} }
        if (phi->right) { ltl_formula_t *s=ltl_to_nnf(phi->right);
            if(s!=phi->right){ltl_free(phi->right);phi->right=s;} }
        return phi;

    case LTL_NEXT: case LTL_FINALLY: case LTL_GLOBALLY:
        if (phi->left)  { ltl_formula_t *s=ltl_to_nnf(phi->left);
            if(s!=phi->left){ltl_free(phi->left);phi->left=s;} }
        return phi;

    case LTL_IMPLIES: case LTL_EQUIV: case LTL_XOR:
        { ltl_formula_t *rw = ltl_remove_implies_equiv(phi); return ltl_to_nnf(rw); }

    default: return phi;
    }
}

bool ltl_is_nnf(const ltl_formula_t *phi)
{
    if (!phi) return true;
    switch (phi->op) {
    case LTL_CONST_TRUE: case LTL_CONST_FALSE: case LTL_ATOM:
    case LTL_LITERAL_POS: case LTL_LITERAL_NEG: return true;
    case LTL_NOT: return phi->left && phi->left->op == LTL_ATOM;
    case LTL_AND: case LTL_OR: case LTL_UNTIL: case LTL_RELEASE:
    case LTL_WEAK_UNTIL: case LTL_STRONG_RELEASE:
        return ltl_is_nnf(phi->left) && ltl_is_nnf(phi->right);
    case LTL_NEXT: case LTL_FINALLY: case LTL_GLOBALLY:
        return ltl_is_nnf(phi->left);
    case LTL_IMPLIES: case LTL_EQUIV: case LTL_XOR: return false;
    default: return false;
    }
}

/* ==================================================================
 * L4: Formula simplification
 *
 * Boolean identities + basic temporal simplifications.
 * Based on Somenzi & Bloem (CAV 2000) rewrite rules:
 *   true && phi = phi,  false && _ = false,  phi && phi = phi
 *   false || phi = phi,  true || _ = true,   phi || phi = phi
 *   phi U false = false, true U phi = F phi, phi U true = true
 *   false R phi = G phi, phi R true = true
 * ================================================================== */

ltl_formula_t *ltl_simplify(ltl_formula_t *phi)
{
    if (!phi) return NULL;
    ltl_formula_t *ls = phi->left  ? ltl_simplify(phi->left)  : NULL;
    ltl_formula_t *rs = phi->right ? ltl_simplify(phi->right) : NULL;
    if (ls != phi->left)  { ltl_free(phi->left);  phi->left  = ls; }
    if (rs != phi->right) { ltl_free(phi->right); phi->right = rs; }

    switch (phi->op) {
    case LTL_AND:
        if (phi->left->op == LTL_CONST_TRUE)
            { ltl_ref(phi->right); ltl_free(phi); return phi->right; }
        if (phi->right->op == LTL_CONST_TRUE)
            { ltl_ref(phi->left);  ltl_free(phi); return phi->left; }
        if (phi->left->op == LTL_CONST_FALSE || phi->right->op == LTL_CONST_FALSE)
            { ltl_free(phi); return ltl_false(); }
        if (ltl_equal(phi->left, phi->right))
            { ltl_ref(phi->left); ltl_free(phi); return phi->left; }
        break;
    case LTL_OR:
        if (phi->left->op == LTL_CONST_FALSE)
            { ltl_ref(phi->right); ltl_free(phi); return phi->right; }
        if (phi->right->op == LTL_CONST_FALSE)
            { ltl_ref(phi->left);  ltl_free(phi); return phi->left; }
        if (phi->left->op == LTL_CONST_TRUE || phi->right->op == LTL_CONST_TRUE)
            { ltl_free(phi); return ltl_true(); }
        if (ltl_equal(phi->left, phi->right))
            { ltl_ref(phi->left); ltl_free(phi); return phi->left; }
        break;
    case LTL_IMPLIES:
        if (phi->left->op == LTL_CONST_FALSE)  { ltl_free(phi); return ltl_true(); }
        if (phi->left->op == LTL_CONST_TRUE)
            { ltl_ref(phi->right); ltl_free(phi); return phi->right; }
        if (phi->right->op == LTL_CONST_TRUE)  { ltl_free(phi); return ltl_true(); }
        break;
    case LTL_UNTIL:
        if (phi->right->op == LTL_CONST_FALSE) { ltl_free(phi); return ltl_false(); }
        if (phi->left->op == LTL_CONST_TRUE) {
            ltl_ref(phi->right); ltl_formula_t *r=ltl_finally(phi->right);
            ltl_free(phi); return r; }
        if (phi->right->op == LTL_CONST_TRUE)  { ltl_free(phi); return ltl_true(); }
        break;
    case LTL_RELEASE:
        if (phi->right->op == LTL_CONST_TRUE)  { ltl_free(phi); return ltl_true(); }
        if (phi->left->op == LTL_CONST_FALSE) {
            ltl_ref(phi->right); ltl_formula_t *r=ltl_globally(phi->right);
            ltl_free(phi); return r; }
        break;
    case LTL_GLOBALLY:
        if (phi->left->op == LTL_CONST_TRUE)   { ltl_free(phi); return ltl_true(); }
        break;
    case LTL_FINALLY:
        if (phi->left->op == LTL_CONST_FALSE)  { ltl_free(phi); return ltl_false(); }
        break;
    default: break;
    }
    return phi;
}

/* ==================================================================
 * Fixed-point expansion laws (Baier & Katoen Thm 5.18)
 *
 * phi U psi == psi || (phi && X(phi U psi))
 * phi R psi == psi && (phi || X(phi R psi))
 *
 * These are the foundation of all tableau-based LTL-to-Buchi translations.
 * ================================================================== */

ltl_formula_t *ltl_unfold_until(const ltl_formula_t *phi, const ltl_formula_t *psi)
{
    if (!phi || !psi) return NULL;
    ltl_ref((ltl_formula_t*)phi); ltl_ref((ltl_formula_t*)psi);
    ltl_formula_t *u = ltl_until((ltl_formula_t*)phi, (ltl_formula_t*)psi);
    return ltl_or((ltl_formula_t*)psi, ltl_and((ltl_formula_t*)phi, ltl_next(u)));
}

ltl_formula_t *ltl_unfold_release(const ltl_formula_t *phi, const ltl_formula_t *psi)
{
    if (!phi || !psi) return NULL;
    ltl_ref((ltl_formula_t*)phi); ltl_ref((ltl_formula_t*)psi);
    ltl_formula_t *r = ltl_release((ltl_formula_t*)phi, (ltl_formula_t*)psi);
    return ltl_and((ltl_formula_t*)psi, ltl_or((ltl_formula_t*)phi, ltl_next(r)));
}

bool ltl_implies_syntactic(const ltl_formula_t *phi, const ltl_formula_t *psi)
{
    if (!phi || !psi) return false;
    if (phi->op == LTL_CONST_FALSE) return true;
    if (psi->op == LTL_CONST_TRUE) return true;
    if (phi->op == LTL_CONST_TRUE) return psi->op == LTL_CONST_TRUE;
    if (psi->op == LTL_CONST_FALSE) return phi->op == LTL_CONST_FALSE;
    return ltl_equal(phi, psi);
}

/* ==================================================================
 * L5: Derived operator elimination to core set {X, U, not, and}
 *
 * F a   => true U a
 * G a   => false R a
 * a R b => not((not a) U (not b))
 * a W b => (G a) || (a U b)
 * a M b => not((not a) W (not b))
 * a||b  => not(not a && not b)
 * a->b  => (not a) || b
 * ================================================================== */

ltl_formula_t *ltl_to_core(ltl_formula_t *phi)
{
    if (!phi) return NULL;
    switch (phi->op) {
    case LTL_CONST_TRUE: case LTL_CONST_FALSE: case LTL_ATOM:
    case LTL_LITERAL_POS: case LTL_LITERAL_NEG: return phi;
    case LTL_NOT: {
        ltl_formula_t *c=ltl_to_core(phi->left);
        if(c!=phi->left){ltl_free(phi->left);phi->left=c;} return phi;
    }
    case LTL_AND: case LTL_UNTIL: {
        ltl_formula_t *l=ltl_to_core(phi->left),*r=ltl_to_core(phi->right);
        if(l!=phi->left){ltl_free(phi->left);phi->left=l;}
        if(r!=phi->right){ltl_free(phi->right);phi->right=r;}
        return phi;
    }
    case LTL_OR: {
        ltl_formula_t *a=phi->left,*b=phi->right; ltl_ref(a);ltl_ref(b);
        ltl_formula_t *r=ltl_to_core(ltl_not(ltl_and(ltl_not(a),ltl_not(b))));
        ltl_free(phi); return r;
    }
    case LTL_IMPLIES: {
        ltl_formula_t *a=phi->left,*b=phi->right; ltl_ref(a);ltl_ref(b);
        ltl_free(phi); return ltl_to_core(ltl_or(ltl_not(a),b));
    }
    case LTL_NEXT: {
        ltl_formula_t *c=ltl_to_core(phi->left);
        if(c!=phi->left){ltl_free(phi->left);phi->left=c;} return phi;
    }
    case LTL_FINALLY: {
        ltl_formula_t *a=phi->left;ltl_ref(a);
        ltl_free(phi); return ltl_until(ltl_true(),ltl_to_core(a));
    }
    case LTL_GLOBALLY: {
        ltl_formula_t *a=phi->left;ltl_ref(a);
        ltl_free(phi); return ltl_release(ltl_false(),ltl_to_core(a));
    }
    case LTL_RELEASE: {
        ltl_formula_t *a=phi->left,*b=phi->right; ltl_ref(a);ltl_ref(b);
        ltl_formula_t *ca=ltl_to_core(a),*cb=ltl_to_core(b);
        ltl_free(phi); return ltl_to_core(ltl_not(ltl_until(ltl_not(ca),ltl_not(cb))));
    }
    case LTL_WEAK_UNTIL: {
        ltl_formula_t *a=phi->left,*b=phi->right; ltl_ref(a);ltl_ref(b);
        ltl_free(phi); return ltl_to_core(ltl_or(ltl_globally(ltl_to_core(a)),ltl_until(ltl_to_core(a),ltl_to_core(b))));
    }
    case LTL_STRONG_RELEASE: {
        ltl_formula_t *a=phi->left,*b=phi->right; ltl_ref(a);ltl_ref(b);
        ltl_free(phi); return ltl_to_core(ltl_not(ltl_weak_until(ltl_not(ltl_to_core(a)),ltl_not(ltl_to_core(b)))));
    }
    case LTL_EQUIV: {
        ltl_formula_t *a=phi->left,*b=phi->right; ltl_ref(a);ltl_ref(b);
        ltl_free(phi); return ltl_to_core(ltl_and(ltl_or(ltl_not(a),b),ltl_or(ltl_not(b),a)));
    }
    default: return phi;
    }
}

/* Selective operator removal for preprocessing */

ltl_formula_t *ltl_remove_release(ltl_formula_t *phi)
{
    if (!phi) return NULL;
    if (phi->op == LTL_RELEASE) {
        ltl_formula_t *a=phi->left,*b=phi->right; ltl_ref(a);ltl_ref(b);
        ltl_free(phi); return ltl_not(ltl_until(ltl_not(a),ltl_not(b)));
    }
    if (phi->left)  { ltl_formula_t *s=ltl_remove_release(phi->left);
        if(s!=phi->left){ltl_free(phi->left);phi->left=s;} }
    if (phi->right) { ltl_formula_t *s=ltl_remove_release(phi->right);
        if(s!=phi->right){ltl_free(phi->right);phi->right=s;} }
    return phi;
}

ltl_formula_t *ltl_remove_weak_until(ltl_formula_t *phi)
{
    if (!phi) return NULL;
    if (phi->op == LTL_WEAK_UNTIL) {
        ltl_formula_t *a=phi->left,*b=phi->right; ltl_ref(a);ltl_ref(b);
        ltl_free(phi); return ltl_or(ltl_globally(a),ltl_until(a,b));
    }
    if (phi->op == LTL_STRONG_RELEASE) {
        ltl_formula_t *a=phi->left,*b=phi->right; ltl_ref(a);ltl_ref(b);
        ltl_free(phi); return ltl_and(ltl_finally(a),ltl_release(a,b));
    }
    if (phi->left)  { ltl_formula_t *s=ltl_remove_weak_until(phi->left);
        if(s!=phi->left){ltl_free(phi->left);phi->left=s;} }
    if (phi->right) { ltl_formula_t *s=ltl_remove_weak_until(phi->right);
        if(s!=phi->right){ltl_free(phi->right);phi->right=s;} }
    return phi;
}

ltl_formula_t *ltl_remove_implies_equiv(ltl_formula_t *phi)
{
    if (!phi) return NULL;
    if (phi->op == LTL_IMPLIES) {
        ltl_formula_t *a=phi->left,*b=phi->right; ltl_ref(a);ltl_ref(b);
        ltl_free(phi); return ltl_or(ltl_not(a),b);
    }
    if (phi->op == LTL_EQUIV) {
        ltl_formula_t *a=phi->left,*b=phi->right; ltl_ref(a);ltl_ref(b);
        ltl_free(phi); return ltl_and(ltl_or(ltl_not(a),b),ltl_or(ltl_not(b),a));
    }
    if (phi->op == LTL_XOR) {
        ltl_formula_t *a=phi->left,*b=phi->right; ltl_ref(a);ltl_ref(b);
        ltl_free(phi); return ltl_or(ltl_and(a,ltl_not(b)),ltl_and(ltl_not(a),b));
    }
    if (phi->left)  { ltl_formula_t *s=ltl_remove_implies_equiv(phi->left);
        if(s!=phi->left){ltl_free(phi->left);phi->left=s;} }
    if (phi->right) { ltl_formula_t *s=ltl_remove_implies_equiv(phi->right);
        if(s!=phi->right){ltl_free(phi->right);phi->right=s;} }
    return phi;
}

/* ==================================================================
 * Substitution and elementary set
 * ================================================================== */

ltl_formula_t *ltl_substitute(ltl_formula_t *phi, uint32_t from, ltl_formula_t *to)
{
    if (!phi) return NULL;
    if (phi->op == LTL_ATOM && phi->atom_id == from) { ltl_ref(to); ltl_free(phi); return to; }
    if (phi->left)  { ltl_formula_t *s=ltl_substitute(phi->left,from,to);
        if(s!=phi->left){ltl_free(phi->left);phi->left=s;} }
    if (phi->right) { ltl_formula_t *s=ltl_substitute(phi->right,from,to);
        if(s!=phi->right){ltl_free(phi->right);phi->right=s;} }
    return phi;
}

size_t ltl_elementary_set(const ltl_formula_t *phi, ltl_formula_t ***out)
{
    if (!phi || !out) return 0;
    fset_t s={NULL,0,0}; fset_collect(phi,&s);
    *out = malloc(s.n * sizeof(ltl_formula_t*));
    for (size_t i=0;i<s.n;i++) { (*out)[i]=s.v[i]; ltl_ref(s.v[i]); }
    size_t c=s.n; free(s.v); return c;
}

/* ==================================================================
 * Fischer-Ladner closure
 *
 * FL(phi) = smallest set containing phi closed under:
 *   1. Subformulas
 *   2. X(f) for Until and Release nodes
 *   3. Single negation
 * |FL(phi)| = O(|phi|). Foundation for tableau-based Buchi construction.
 * Reference: Fischer & Ladner, JCSS 1979
 * ================================================================== */

static void fl_add(ltl_closure_t *cl, ltl_formula_t *f)
{
    if (!f) return;
    for (size_t i=0;i<cl->count;i++) if(cl->formulas[i]==f) return;
    if (cl->count>=cl->capacity) {
        cl->capacity=cl->capacity?cl->capacity*2:128;
        cl->formulas=realloc(cl->formulas,cl->capacity*sizeof(ltl_formula_t*));
    }
    cl->formulas[cl->count]=f; f->closure_id=(uint32_t)cl->count; cl->count++;
}

static void fl_recurse(ltl_formula_t *f, ltl_closure_t *cl)
{
    if (!f) return;
    fl_add(cl,f);
    /* Single negation: add not(f) if f not a NOT or literal */
    if (f->op != LTL_NOT && f->op != LTL_LITERAL_POS && f->op != LTL_LITERAL_NEG) {
        ltl_formula_t *nf = ltl_not(f); fl_add(cl,nf);
    }
    if (f->left)  fl_recurse(f->left,cl);
    if (f->right) fl_recurse(f->right,cl);
    /* Fischer-Ladner rule: for Until and Release, add X(f) */
    if (f->op == LTL_UNTIL || f->op == LTL_RELEASE) {
        ltl_formula_t *xf = ltl_next(f); fl_add(cl,xf); fl_recurse(xf,cl);
    }
}

ltl_closure_t *ltl_fischer_ladner_closure(const ltl_formula_t *phi)
{
    if (!phi) return NULL;
    ltl_closure_t *cl = malloc(sizeof(ltl_closure_t));
    cl->formulas=NULL; cl->count=0; cl->capacity=0;
    fl_recurse((ltl_formula_t*)phi, cl);
    return cl;
}

void ltl_closure_free(ltl_closure_t *cl)
{
    if (!cl) return;
    for (size_t i=0;i<cl->count;i++)
        if (cl->formulas[i] && cl->formulas[i]->op==LTL_NOT && !cl->formulas[i]->left)
            free(cl->formulas[i]);
    free(cl->formulas); free(cl);
}

/* ==================================================================
 * Propositional consistency check
 * ================================================================== */

bool ltl_set_consistent(ltl_formula_t **formulas, size_t count)
{
    if (!formulas || count==0) return true;
    uint32_t tmask=0, fmask=0;
    for (size_t i=0;i<count;i++) {
        ltl_formula_t *f=formulas[i]; if(!f) continue;
        switch (f->op) {
        case LTL_CONST_FALSE: return false;
        case LTL_ATOM: case LTL_LITERAL_POS: tmask |= 1u<<f->atom_id; break;
        case LTL_LITERAL_NEG: fmask |= 1u<<f->atom_id; break;
        case LTL_NOT:
            if(f->left && f->left->op==LTL_ATOM) { fmask |= 1u<<f->left->atom_id; } break;
        default: break;
        }
    }
    return (tmask & fmask) == 0;
}

/* ==================================================================
 * Structural equality (pointer equality + recursive) and FNV-1a hashing
 * ================================================================== */

bool ltl_equal(const ltl_formula_t *a, const ltl_formula_t *b)
{
    if (a==b) return true;
    if (!a||!b) return false;
    if (a->op!=b->op||a->atom_id!=b->atom_id) return false;
    return ltl_equal(a->left,b->left) && ltl_equal(a->right,b->right);
}

uint32_t ltl_hash(const ltl_formula_t *phi)
{
    if (!phi) return 0;
    uint32_t h=2166136261u;
    h^=(uint32_t)phi->op; h*=16777619u;
    h^=phi->atom_id;       h*=16777619u;
    if (phi->left)  { h^=ltl_hash(phi->left);  h*=16777619u; }
    if (phi->right) { h^=ltl_hash(phi->right); h*=16777619u; }
    return h;
}

/* ==================================================================
 * L7: Pretty printing
 * ================================================================== */

static int ltl_snprint_rec(char *b, size_t sz, const ltl_formula_t *phi, int pr)
{
    if (!phi || sz < 2) return 0;
    int p = 0;
    switch (phi->op) {
    case LTL_CONST_TRUE:  p = snprintf(b, sz, "true"); break;
    case LTL_CONST_FALSE: p = snprintf(b, sz, "false"); break;
    case LTL_ATOM: case LTL_LITERAL_POS:
        p = snprintf(b, sz, "p%u", phi->atom_id); break;
    case LTL_LITERAL_NEG:
        p = snprintf(b, sz, "!p%u", phi->atom_id); break;
    case LTL_NOT:
        p = snprintf(b, sz, "!(");
        p += ltl_snprint_rec(b + p, sz - p, phi->left, 0);
        p += snprintf(b + p, sz - p, ")"); break;
    case LTL_AND:
        if (pr > 3) p += snprintf(b + p, sz - p, "(");
        p += ltl_snprint_rec(b + p, sz - p, phi->left, 3);
        p += snprintf(b + p, sz - p, " && ");
        p += ltl_snprint_rec(b + p, sz - p, phi->right, 3);
        if (pr > 3) { p += snprintf(b + p, sz - p, ")"); } break;
    case LTL_OR:
        if (pr > 2) p += snprintf(b + p, sz - p, "(");
        p += ltl_snprint_rec(b + p, sz - p, phi->left, 2);
        p += snprintf(b + p, sz - p, " || ");
        p += ltl_snprint_rec(b + p, sz - p, phi->right, 2);
        if (pr > 2) { p += snprintf(b + p, sz - p, ")"); } break;
    case LTL_IMPLIES:
        if (pr > 1) p += snprintf(b + p, sz - p, "(");
        p += ltl_snprint_rec(b + p, sz - p, phi->left, 1);
        p += snprintf(b + p, sz - p, " -> ");
        p += ltl_snprint_rec(b + p, sz - p, phi->right, 1);
        if (pr > 1) { p += snprintf(b + p, sz - p, ")"); } break;
    case LTL_EQUIV:
        if (pr > 0) p += snprintf(b + p, sz - p, "(");
        p += ltl_snprint_rec(b + p, sz - p, phi->left, 0);
        p += snprintf(b + p, sz - p, " <-> ");
        p += ltl_snprint_rec(b + p, sz - p, phi->right, 0);
        if (pr > 0) { p += snprintf(b + p, sz - p, ")"); } break;
    case LTL_XOR:
        if (pr > 1) p += snprintf(b + p, sz - p, "(");
        p += ltl_snprint_rec(b + p, sz - p, phi->left, 1);
        p += snprintf(b + p, sz - p, " XOR ");
        p += ltl_snprint_rec(b + p, sz - p, phi->right, 1);
        if (pr > 1) { p += snprintf(b + p, sz - p, ")"); } break;
    case LTL_NEXT:
        p = snprintf(b, sz, "X ");
        p += ltl_snprint_rec(b + p, sz - p, phi->left, 10); break;
    case LTL_FINALLY:
        p = snprintf(b, sz, "F ");
        p += ltl_snprint_rec(b + p, sz - p, phi->left, 10); break;
    case LTL_GLOBALLY:
        p = snprintf(b, sz, "G ");
        p += ltl_snprint_rec(b + p, sz - p, phi->left, 10); break;
    case LTL_UNTIL:
        if (pr > 4) p += snprintf(b + p, sz - p, "(");
        p += ltl_snprint_rec(b + p, sz - p, phi->left, 4);
        p += snprintf(b + p, sz - p, " U ");
        p += ltl_snprint_rec(b + p, sz - p, phi->right, 4);
        if (pr > 4) { p += snprintf(b + p, sz - p, ")"); } break;
    case LTL_RELEASE:
        if (pr > 4) p += snprintf(b + p, sz - p, "(");
        p += ltl_snprint_rec(b + p, sz - p, phi->left, 4);
        p += snprintf(b + p, sz - p, " R ");
        p += ltl_snprint_rec(b + p, sz - p, phi->right, 4);
        if (pr > 4) { p += snprintf(b + p, sz - p, ")"); } break;
    case LTL_WEAK_UNTIL:
        if (pr > 4) p += snprintf(b + p, sz - p, "(");
        p += ltl_snprint_rec(b + p, sz - p, phi->left, 4);
        p += snprintf(b + p, sz - p, " W ");
        p += ltl_snprint_rec(b + p, sz - p, phi->right, 4);
        if (pr > 4) { p += snprintf(b + p, sz - p, ")"); } break;
    case LTL_STRONG_RELEASE:
        if (pr > 4) p += snprintf(b + p, sz - p, "(");
        p += ltl_snprint_rec(b + p, sz - p, phi->left, 4);
        p += snprintf(b + p, sz - p, " M ");
        p += ltl_snprint_rec(b + p, sz - p, phi->right, 4);
        if (pr > 4) { p += snprintf(b + p, sz - p, ")"); } break;
    default: p = snprintf(b, sz, "<?>"); break;
    }
    return p;
}

int ltl_snprint(char *buf, size_t sz, const ltl_formula_t *phi)
    { return ltl_snprint_rec(buf,sz,phi,100); }

void ltl_print(const ltl_formula_t *phi)
    { char b[4096]; ltl_snprint(b,sizeof(b),phi); printf("%s",b); }

/* Spin/Promela LTL syntax: [] = G, <> = F, X = next, U = until, V = release */
void ltl_print_spin(const ltl_formula_t *phi)
{
    if (!phi) { printf("null"); return; }
    switch (phi->op) {
    case LTL_CONST_TRUE: printf("1"); break;
    case LTL_CONST_FALSE: printf("0"); break;
    case LTL_ATOM: case LTL_LITERAL_POS: printf("p%u",phi->atom_id); break;
    case LTL_LITERAL_NEG: printf("!p%u",phi->atom_id); break;
    case LTL_NOT: printf("!("); ltl_print_spin(phi->left); printf(")"); break;
    case LTL_AND:
        printf("("); ltl_print_spin(phi->left);
        printf(" && "); ltl_print_spin(phi->right); printf(")"); break;
    case LTL_OR:
        printf("("); ltl_print_spin(phi->left);
        printf(" || "); ltl_print_spin(phi->right); printf(")"); break;
    case LTL_IMPLIES:
        printf("("); ltl_print_spin(phi->left);
        printf(" -> "); ltl_print_spin(phi->right); printf(")"); break;
    case LTL_NEXT: printf("X "); ltl_print_spin(phi->left); break;
    case LTL_FINALLY: printf("<> "); ltl_print_spin(phi->left); break;
    case LTL_GLOBALLY: printf("[] "); ltl_print_spin(phi->left); break;
    case LTL_UNTIL:
        printf("("); ltl_print_spin(phi->left);
        printf(" U "); ltl_print_spin(phi->right); printf(")"); break;
    case LTL_RELEASE:
        printf("("); ltl_print_spin(phi->left);
        printf(" V "); ltl_print_spin(phi->right); printf(")"); break;
    default: printf("<?>"); break;
    }
}

/* ==================================================================
 * L7: Formula analysis utilities
 *
 * temporal_depth: max nesting depth of temporal operators
 * operator_count: frequency distribution of temporal operators
 * is_propositional: true iff no temporal operators present
 * ================================================================== */

size_t ltl_temporal_depth(const ltl_formula_t *phi)
{
    if (!phi) return 0;
    switch (phi->op) {
    case LTL_NEXT: case LTL_FINALLY: case LTL_GLOBALLY:
        return 1 + ltl_temporal_depth(phi->left);
    case LTL_UNTIL: case LTL_RELEASE:
    case LTL_WEAK_UNTIL: case LTL_STRONG_RELEASE: {
        size_t dl=ltl_temporal_depth(phi->left), dr=ltl_temporal_depth(phi->right);
        return 1 + ((dl>dr)?dl:dr);
    }
    case LTL_AND: case LTL_OR: case LTL_IMPLIES:
    case LTL_EQUIV: case LTL_XOR: {
        size_t dl=phi->left?ltl_temporal_depth(phi->left):0;
        size_t dr=phi->right?ltl_temporal_depth(phi->right):0;
        return (dl>dr)?dl:dr;
    }
    case LTL_NOT: return ltl_temporal_depth(phi->left);
    default: return 0;
    }
}

void ltl_operator_count(const ltl_formula_t *phi,
                         size_t *xc, size_t *fc, size_t *gc,
                         size_t *uc, size_t *rc, size_t *wc, size_t *mc)
{
    if (!phi) return;
    switch (phi->op) {
    case LTL_NEXT: (*xc)++; break;
    case LTL_FINALLY: (*fc)++; break;
    case LTL_GLOBALLY: (*gc)++; break;
    case LTL_UNTIL: (*uc)++; break;
    case LTL_RELEASE: (*rc)++; break;
    case LTL_WEAK_UNTIL: (*wc)++; break;
    case LTL_STRONG_RELEASE: (*mc)++; break;
    default: break;
    }
    if (phi->left)  ltl_operator_count(phi->left,xc,fc,gc,uc,rc,wc,mc);
    if (phi->right) ltl_operator_count(phi->right,xc,fc,gc,uc,rc,wc,mc);
}

bool ltl_is_propositional(const ltl_formula_t *phi)
{
    if (!phi) return true;
    switch (phi->op) {
    case LTL_NEXT: case LTL_FINALLY: case LTL_GLOBALLY:
    case LTL_UNTIL: case LTL_RELEASE:
    case LTL_WEAK_UNTIL: case LTL_STRONG_RELEASE:
        return false;
    default:
        return ltl_is_propositional(phi->left) && ltl_is_propositional(phi->right);
    }
}
