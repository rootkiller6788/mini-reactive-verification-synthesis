/**
 * omega_expression.c -- omega-Regular Expression Operations
 *
 * Implements syntactic manipulation of omega-regular expressions:
 * construction, tree traversal, cloning, and printing.
 * Also includes an internal DFA library for finite regular sub-expressions
 * used in r.L and r^omega constructs.
 *
 * References:
 *   - Buchi, J.R. (1962) "On a decision method in restricted second order arithmetic"
 *   - Vardi & Wolper (1986) "An automata-theoretic approach to program verification"
 *   - Alpern & Schneider (1987) "Defining Liveness"
 *
 * Knowledge Coverage:
 *   L1: omega-regular expression syntax tree
 *   L2: closure under Boolean operations
 *   L3: tree structure for expressions, DFA as tuple
 *   L4: Myhill-Nerode (minimal DFA unique)
 *   L6: property classification (safety/liveness/response/persistence)
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "omega_regular.h"

/* =========================================================================
 * Expression Tree Construction
 * ======================================================================== */

ore_node_t *ore_make_letter(uint8_t letter)
{
    ore_node_t *n = (ore_node_t *)malloc(sizeof(ore_node_t));
    if (!n) return NULL;
    n->op = ORE_LETTER;
    n->letter = letter;
    n->left = n->right = NULL;
    n->dfa_id = -1;
    return n;
}

ore_node_t *ore_make_union(ore_node_t *left, ore_node_t *right)
{
    if (!left || !right) return NULL;
    ore_node_t *n = (ore_node_t *)malloc(sizeof(ore_node_t));
    if (!n) return NULL;
    n->op = ORE_UNION; n->letter = 0;
    n->left = left; n->right = right; n->dfa_id = -1;
    return n;
}

ore_node_t *ore_make_intersection(ore_node_t *left, ore_node_t *right)
{
    if (!left || !right) return NULL;
    ore_node_t *n = (ore_node_t *)malloc(sizeof(ore_node_t));
    if (!n) return NULL;
    n->op = ORE_INTERSECTION; n->letter = 0;
    n->left = left; n->right = right; n->dfa_id = -1;
    return n;
}

ore_node_t *ore_make_complement(ore_node_t *left)
{
    if (!left) return NULL;
    ore_node_t *n = (ore_node_t *)malloc(sizeof(ore_node_t));
    if (!n) return NULL;
    n->op = ORE_COMPLEMENT; n->letter = 0;
    n->left = left; n->right = NULL; n->dfa_id = -1;
    return n;
}

ore_node_t *ore_make_omega(int dfa_id)
{
    ore_node_t *n = (ore_node_t *)malloc(sizeof(ore_node_t));
    if (!n) return NULL;
    n->op = ORE_OMEGA; n->letter = 0;
    n->left = n->right = NULL; n->dfa_id = dfa_id;
    return n;
}

ore_node_t *ore_make_concat(int dfa_id, ore_node_t *right)
{
    if (!right) return NULL;
    ore_node_t *n = (ore_node_t *)malloc(sizeof(ore_node_t));
    if (!n) return NULL;
    n->op = ORE_CONCAT; n->letter = 0;
    n->left = NULL; n->right = right; n->dfa_id = dfa_id;
    return n;
}

/* =========================================================================
 * Tree Operations
 * ======================================================================== */

void ore_free(ore_node_t *root)
{
    if (!root) return;
    ore_free(root->left);
    ore_free(root->right);
    free(root);
}

size_t ore_size(const ore_node_t *root)
{
    if (!root) return 0;
    return 1 + ore_size(root->left) + ore_size(root->right);
}

size_t ore_depth(const ore_node_t *root)
{
    if (!root) return 0;
    size_t ld = ore_depth(root->left);
    size_t rd = ore_depth(root->right);
    return 1 + (ld > rd ? ld : rd);
}

ore_node_t *ore_clone(const ore_node_t *root)
{
    if (!root) return NULL;
    ore_node_t *n = (ore_node_t *)malloc(sizeof(ore_node_t));
    if (!n) return NULL;
    n->op = root->op; n->letter = root->letter;
    n->dfa_id = root->dfa_id;
    n->left = ore_clone(root->left);
    n->right = ore_clone(root->right);
    return n;
}

bool ore_is_syntactically_equal(const ore_node_t *a, const ore_node_t *b)
{
    if (!a && !b) return true;
    if (!a || !b) return false;
    if (a->op != b->op) return false;
    if (a->op == ORE_LETTER && a->letter != b->letter) return false;
    if (a->dfa_id != b->dfa_id) return false;
    return ore_is_syntactically_equal(a->left, b->left)
        && ore_is_syntactically_equal(a->right, b->right);
}

void ore_print(const ore_node_t *root, FILE *fp)
{
    if (!root || !fp) return;
    switch (root->op) {
    case ORE_EMPTY:    fprintf(fp, "0"); break;
    case ORE_LETTER:   fprintf(fp, "'%c'", (char)root->letter); break;
    case ORE_UNION:
        fprintf(fp, "("); ore_print(root->left, fp);
        fprintf(fp, " U "); ore_print(root->right, fp);
        fprintf(fp, ")"); break;
    case ORE_INTERSECTION:
        fprintf(fp, "("); ore_print(root->left, fp);
        fprintf(fp, " & "); ore_print(root->right, fp);
        fprintf(fp, ")"); break;
    case ORE_COMPLEMENT:
        fprintf(fp, "~("); ore_print(root->left, fp);
        fprintf(fp, ")"); break;
    case ORE_OMEGA:
        fprintf(fp, "(dfa%d)^w", root->dfa_id); break;
    case ORE_CONCAT:
        fprintf(fp, "(dfa%d).", root->dfa_id);
        ore_print(root->right, fp); break;
    default: fprintf(fp, "?"); break;
    }
}

void ore_count_operators(const ore_node_t *root, int counts[8])
{
    if (!root || !counts) return;
    if (root->op >= 0 && root->op < 8) counts[root->op]++;
    ore_count_operators(root->left, counts);
    ore_count_operators(root->right, counts);
}

/* =========================================================================
 * Internal DFA Library -- L1/L3/L4 knowledge points
 *
 * DFA = (Q, Sigma, delta, q0, F)
 * Stored in a global table referenced by dfa_id.
 *
 * Struct definition and constants are in omega_regular.h
 * ======================================================================== */

static simple_dfa_t g_dfa_table[MAX_DFA_TABLE];
static int g_dfa_count = 0;

static void dfa_init(simple_dfa_t *dfa, size_t al_size)
{
    dfa->nstates = 0; dfa->initial = -1;
    dfa->alphabet_size = al_size;
    memset(dfa->accepting, 0, sizeof(dfa->accepting));
    for (int i = 0; i < MAX_DFA_STATES; i++)
        for (size_t j = 0; j < OR_MAX_ALPHABET; j++)
            dfa->trans[i][j] = -1;
}

static int dfa_add_state(simple_dfa_t *dfa, bool accepting)
{
    if (dfa->nstates >= MAX_DFA_STATES) return -1;
    int id = dfa->nstates++;
    dfa->accepting[id] = accepting;
    return id;
}

static void dfa_add_trans(simple_dfa_t *dfa, int src, uint8_t sym, int dst)
{
    if (src < 0 || src >= dfa->nstates) return;
    if (sym >= dfa->alphabet_size) return;
    dfa->trans[src][sym] = dst;
}

int simple_dfa_register(simple_dfa_t *dfa)
{
    if (g_dfa_count >= MAX_DFA_TABLE) return -1;
    int id = g_dfa_count++;
    g_dfa_table[id] = *dfa;
    return id;
}

const simple_dfa_t *simple_dfa_get(int dfa_id)
{
    if (dfa_id < 0 || dfa_id >= g_dfa_count) return NULL;
    return &g_dfa_table[dfa_id];
}

static void dfa_complete(simple_dfa_t *dfa)
{
    bool missing = false;
    for (int q = 0; q < dfa->nstates && !missing; q++)
        for (size_t s = 0; s < dfa->alphabet_size; s++)
            if (dfa->trans[q][s] < 0) { missing = true; break; }
    if (!missing) return;
    int sink = dfa_add_state(dfa, false);
    if (sink < 0) return;
    for (size_t s = 0; s < dfa->alphabet_size; s++)
        dfa_add_trans(dfa, sink, (uint8_t)s, sink);
    for (int q = 0; q < dfa->nstates - 1; q++)
        for (size_t s = 0; s < dfa->alphabet_size; s++)
            if (dfa->trans[q][s] < 0)
                dfa->trans[q][s] = sink;
}

static bool dfa_accepts(const simple_dfa_t *dfa,
                         const uint8_t *word, size_t len)
{
    int cur = dfa->initial;
    for (size_t i = 0; i < len; i++) {
        if (cur < 0) return false;
        if (word[i] >= dfa->alphabet_size) return false;
        cur = dfa->trans[cur][word[i]];
    }
    return (cur >= 0 && dfa->accepting[cur]);
}

uint64_t dfa_accepting_mask(const simple_dfa_t *dfa)
{
    uint64_t mask = 0;
    for (int i = 0; i < dfa->nstates && i < 64; i++)
        if (dfa->accepting[i]) mask |= (1ULL << i);
    return mask;
}

int simple_dfa_symbol_set_dfa(const bool *symbol_set, size_t alphabet_size)
{
    simple_dfa_t dfa; dfa_init(&dfa, alphabet_size);
    int q0 = dfa_add_state(&dfa, false);
    int q1 = dfa_add_state(&dfa, true);
    if (q0 < 0 || q1 < 0) return -1;
    dfa.initial = q0;
    for (size_t s = 0; s < alphabet_size; s++)
        if (symbol_set && symbol_set[s])
            dfa_add_trans(&dfa, q0, (uint8_t)s, q1);
    for (size_t s = 0; s < alphabet_size; s++)
        dfa_add_trans(&dfa, q1, (uint8_t)s, q1);
    return simple_dfa_register(&dfa);
}

int simple_dfa_all_dfa(size_t alphabet_size)
{
    simple_dfa_t dfa; dfa_init(&dfa, alphabet_size);
    int q = dfa_add_state(&dfa, true);
    if (q < 0) return -1;
    dfa.initial = q;
    for (size_t s = 0; s < alphabet_size; s++)
        dfa_add_trans(&dfa, q, (uint8_t)s, q);
    return simple_dfa_register(&dfa);
}

int simple_dfa_empty_dfa(size_t alphabet_size)
{
    simple_dfa_t dfa; dfa_init(&dfa, alphabet_size);
    (void)dfa_add_state(&dfa, false);
    dfa.initial = 0;
    return simple_dfa_register(&dfa);
}

/* =========================================================================
 * Property Classification -- L6: Alpern & Schneider
 *
 * Every linear-time property = safety intersect liveness.
 * Safety: prefix-closed + limit-closed ("bad thing never happens")
 * Liveness: dense in Sigma^omega ("good thing eventually happens")
 * ======================================================================== */

prop_type_t ore_classify_property(const ore_node_t *expr)
{
    if (!expr) return PROP_SAFETY;
    switch (expr->op) {
    case ORE_EMPTY:     return PROP_SAFETY;
    case ORE_LETTER:    return PROP_SAFETY;
    case ORE_OMEGA:     return PROP_RECURRENCE;
    case ORE_COMPLEMENT: {
        prop_type_t inner = ore_classify_property(expr->left);
        if (inner == PROP_SAFETY) return PROP_LIVENESS;
        if (inner == PROP_LIVENESS) return PROP_SAFETY;
        return PROP_GENERAL;
    }
    case ORE_UNION:
    case ORE_INTERSECTION: {
        prop_type_t l = ore_classify_property(expr->left);
        prop_type_t r = ore_classify_property(expr->right);
        if (l == PROP_SAFETY && r == PROP_SAFETY) return PROP_SAFETY;
        if (l == PROP_LIVENESS && r == PROP_LIVENESS
            && expr->op == ORE_UNION) return PROP_LIVENESS;
        return PROP_GENERAL;
    }
    case ORE_CONCAT:
        return ore_classify_property(expr->right);
    default:
        return PROP_GENERAL;
    }
}

bool ore_is_safety_syntactic(const ore_node_t *expr)
{
    return ore_classify_property(expr) == PROP_SAFETY;
}
