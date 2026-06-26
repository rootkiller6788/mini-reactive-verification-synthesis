/**
 * ltl_formula.c -- LTL Formula Construction and Evaluation
 *
 * Implements LTL syntax tree construction, semantic evaluation
 * on omega-words, and formula manipulation.
 *
 * LTL operators: atoms, NOT, AND, OR, NEXT, UNTIL, RELEASE,
 * EVENTUALLY (F), ALWAYS (G), WEAK_UNTIL (W).
 *
 * Reference: Pnueli (1977) "The Temporal Logic of Programs"
 * Knowledge: L1 LTL syntax, L2 temporal semantics, L3 Kripke models.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ltl_formula.h"

static int g_node_id = 0;

static ltl_node_t *node_new(ltl_op_t op)
{
    ltl_node_t *n = (ltl_node_t *)malloc(sizeof(ltl_node_t));
    if (!n) return NULL;
    n->op = op; n->atom_id = -1;
    n->left = n->right = NULL;
    n->id = g_node_id++;
    return n;
}

ltl_node_t *ltl_make_true(void)   { return node_new(LTL_TRUE); }
ltl_node_t *ltl_make_false(void)  { return node_new(LTL_FALSE); }

ltl_node_t *ltl_make_atom(int id) {
    ltl_node_t *n = node_new(LTL_ATOM);
    if (n) n->atom_id = id;
    return n;
}

ltl_node_t *ltl_make_not(ltl_node_t *phi) {
    if (!phi) return NULL;
    ltl_node_t *n = node_new(LTL_NOT);
    if (n) n->left = phi;
    return n;
}

ltl_node_t *ltl_make_and(ltl_node_t *l, ltl_node_t *r) {
    if (!l || !r) return NULL;
    ltl_node_t *n = node_new(LTL_AND);
    if (n) { n->left = l; n->right = r; }
    return n;
}

ltl_node_t *ltl_make_or(ltl_node_t *l, ltl_node_t *r) {
    if (!l || !r) return NULL;
    ltl_node_t *n = node_new(LTL_OR);
    if (n) { n->left = l; n->right = r; }
    return n;
}

ltl_node_t *ltl_make_implies(ltl_node_t *l, ltl_node_t *r) {
    if (!l || !r) return NULL;
    ltl_node_t *n = node_new(LTL_IMPLIES);
    if (n) { n->left = l; n->right = r; }
    return n;
}

ltl_node_t *ltl_make_next(ltl_node_t *phi) {
    if (!phi) return NULL;
    ltl_node_t *n = node_new(LTL_NEXT);
    if (n) n->left = phi;
    return n;
}

ltl_node_t *ltl_make_until(ltl_node_t *l, ltl_node_t *r) {
    if (!l || !r) return NULL;
    ltl_node_t *n = node_new(LTL_UNTIL);
    if (n) { n->left = l; n->right = r; }
    return n;
}

ltl_node_t *ltl_make_release(ltl_node_t *l, ltl_node_t *r) {
    if (!l || !r) return NULL;
    ltl_node_t *n = node_new(LTL_RELEASE);
    if (n) { n->left = l; n->right = r; }
    return n;
}

ltl_node_t *ltl_make_eventually(ltl_node_t *phi) {
    if (!phi) return NULL;
    ltl_node_t *n = node_new(LTL_EVENTUALLY);
    if (n) n->left = phi;
    return n;
}

ltl_node_t *ltl_make_always(ltl_node_t *phi) {
    if (!phi) return NULL;
    ltl_node_t *n = node_new(LTL_ALWAYS);
    if (n) n->left = phi;
    return n;
}

ltl_node_t *ltl_make_weak_until(ltl_node_t *l, ltl_node_t *r) {
    if (!l || !r) return NULL;
    ltl_node_t *n = node_new(LTL_WEAK_UNTIL);
    if (n) { n->left = l; n->right = r; }
    return n;
}

void ltl_free(ltl_node_t *root)
{
    if (!root) return;
    ltl_free(root->left);
    ltl_free(root->right);
    free(root);
}

size_t ltl_size(const ltl_node_t *root)
{
    if (!root) return 0;
    return 1 + ltl_size(root->left) + ltl_size(root->right);
}

ltl_node_t *ltl_clone(const ltl_node_t *root)
{
    if (!root) return NULL;
    ltl_node_t *n = (ltl_node_t *)malloc(sizeof(ltl_node_t));
    if (!n) return NULL;
    n->op = root->op; n->atom_id = root->atom_id; n->id = root->id;
    n->left = ltl_clone(root->left);
    n->right = ltl_clone(root->right);
    return n;
}
