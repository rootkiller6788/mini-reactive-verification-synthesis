/**
 * @file ltl_to_buchi.c
 * @brief LTL-to-Buchi Translation - tableau and GPVW construction.
 *
 * Implements translation algorithms: tableau construction (L3-L5),
 * GPVW on-the-fly (L5), model checking (L7), and pattern translation (L6).
 *
 * Pipeline: NNF -> tableau/GPWV -> extract GBA -> degeneralize -> trim.
 *
 * References:
 *   - Vardi & Wolper, LICS 1986
 *   - Gerth, Peled, Vardi, Wolper, PSTV 1995
 *   - Baier & Katoen, Principles of Model Checking, Ch.5
 */

#include "ltl.h"
#include "buchi.h"
#include "ltl_to_buchi.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ==================================================================
 * L1: Translation statistics printing
 * ================================================================== */

void ltl_to_buchi_stats_print(const ltl_to_buchi_stats_t *stats)
{
    if (!stats) return;
    printf("LTL-to-Buchi Translation:\n");
    printf("  Method:           %s\n", stats->method_name);
    printf("  Formula nodes:    %u\n", stats->formula_nodes);
    printf("  Closure size:     %u\n", stats->closure_size);
    printf("  Automaton states: %u\n", stats->automaton_states);
    printf("  Transitions:      %u\n", stats->automaton_trans);
    printf("  Acceptance sets:  %u\n", stats->acceptance_sets);
}

/* ==================================================================
 * L3: Tableau graph management
 * ================================================================== */

static tableau_node_t *tableau_add_node(tableau_t *tab)
{
    if (tab->num_nodes >= tab->capacity) {
        tab->capacity = tab->capacity ? tab->capacity * 2 : 256;
        tab->nodes = realloc(tab->nodes,
                     tab->capacity * sizeof(tableau_node_t));
    }
    tableau_node_t *node = &tab->nodes[tab->num_nodes];
    memset(node, 0, sizeof(tableau_node_t));
    node->id = (uint32_t)tab->num_nodes;
    tab->num_nodes++;
    return node;
}

void tableau_free(tableau_t *tab)
{
    if (!tab) return;
    for (size_t i = 0; i < tab->num_nodes; i++) {
        free(tab->nodes[i].formulas);
        free(tab->nodes[i].old);
        free(tab->nodes[i].new);
        free(tab->nodes[i].next);
    }
    for (size_t i = 0; i < tab->num_nodes; i++)
        free(tab->edges[i]);
    free(tab->edges);
    free(tab->edge_counts);
    free(tab->edge_caps);
    free(tab->nodes);
    free(tab);
}

/* ==================================================================
 * Helper: formula vector for collecting formula sets
 * ================================================================== */

typedef struct {
    ltl_formula_t **items;
    size_t          count;
    size_t          capacity;
} fvec_t;

static void fv_init(fvec_t *v)
    { v->items = NULL; v->count = 0; v->capacity = 0; }

static void fv_add(fvec_t *v, ltl_formula_t *f)
{
    if (!f) return;
    for (size_t i = 0; i < v->count; i++)
        if (v->items[i] == f) return;
    if (v->count >= v->capacity) {
        v->capacity = v->capacity ? v->capacity * 2 : 64;
        v->items = realloc(v->items, v->capacity * sizeof(ltl_formula_t*));
    }
    v->items[v->count++] = f;
}

static void fv_free(fvec_t *v) { free(v->items); }

/* ==================================================================
 * Expand a formula set according to local consistency rules
 *
 * For each formula in the current set:
 *   AND:  add both conjuncts
 *   OR:   branching point (handled by caller)
 *   U:    expansion: psi OR (phi AND X(phi U psi)) -> branching
 *   R:    expansion: psi AND (phi OR X(phi R psi)) -> add psi
 *   G:    add phi (implicitly also X G phi for next)
 *   F:    branching: phi OR X(F phi)
 *   X:    add child to next-state obligations
 *   not:  handled by NNF preprocessing
 *
 * Returns expanded set + next-state obligations.
 * Returns false if the set is propositionally inconsistent.
 * ================================================================== */

static bool expand_set(fvec_t *current, fvec_t *next_obl)
{
    size_t pos = 0;
    while (pos < current->count) {
        ltl_formula_t *f = current->items[pos++];

        switch (f->op) {
        case LTL_AND:
            fv_add(current, f->left);
            fv_add(current, f->right);
            break;
        case LTL_RELEASE:
            fv_add(current, f->right);
            fv_add(next_obl, f);
            break;
        case LTL_GLOBALLY:
            fv_add(current, f->left);
            fv_add(next_obl, f);
            break;
        case LTL_NEXT:
            fv_add(next_obl, f->left);
            break;
        case LTL_UNTIL:
        case LTL_OR:
        case LTL_FINALLY:
            /* Branching point - handled by caller */
            break;
        default:
            break;
        }

        if (!ltl_set_consistent(current->items, current->count))
            return false;
    }
    return true;
}

/* ==================================================================
 * L4+L5: Classic tableau construction
 *
 * Builds the full tableau graph for formula phi (in NNF).
 *
 * Algorithm:
 *   1. Start with phi as the initial "current" formula set
 *   2. Expand the set using local consistency rules
 *   3. At branching points (OR, UNTIL, FINALLY), create child nodes
 *   4. For each node, extract X-formulas as next-state obligations
 *   5. Find or create successor nodes matching these obligations
 *   6. Repeat until fixpoint
 *
 * Acceptance: For each phi_i U psi_i in the closure, create
 * acceptance set F_i = {states where psi_i holds or phi_i U psi_i
 * is not present}.
 * ================================================================== */

tableau_t *tableau_build(const ltl_formula_t *phi)
{
    if (!phi) return NULL;

    tableau_t *tab = malloc(sizeof(tableau_t));
    tab->nodes = NULL; tab->num_nodes = 0; tab->capacity = 0;
    tab->edges = NULL; tab->edge_counts = NULL; tab->edge_caps = NULL;
    tab->num_accept_sets = 0;
    tab->num_atoms = 0;

    /* Count atoms used */
    uint32_t atoms = ltl_atoms_used(phi);
    tab->num_atoms = 0;
    while (atoms) { tab->num_atoms++; atoms >>= 1; }

    /* Compute Fischer-Ladner closure */
    ltl_closure_t *closure = ltl_fischer_ladner_closure(phi);
    if (!closure) { free(tab); return NULL; }

    /* Count Until subformulas for acceptance sets */
    for (size_t i = 0; i < closure->count; i++) {
        if (closure->formulas[i]->op == LTL_UNTIL)
            tab->num_accept_sets++;
    }

    /* Initialize: create node for phi */
    tableau_node_t *init_node = tableau_add_node(tab);
    init_node->new_count = 1;
    init_node->new = malloc(sizeof(ltl_formula_t*));
    init_node->new[0] = (ltl_formula_t*)phi;
    init_node->is_initial = true;

    /* Worklist-based expansion */
    size_t *worklist = malloc(sizeof(size_t));
    worklist[0] = 0;
    size_t wl_len = 1;

    while (wl_len > 0) {
        size_t node_idx = worklist[--wl_len];
        tableau_node_t *node = &tab->nodes[node_idx];

        /* Skip already expanded nodes */
        if (node->formulas) continue;

        /* Expand the "new" formulas of this node */
        fvec_t current, next;
        fv_init(&current); fv_init(&next);

        for (size_t i = 0; i < node->new_count; i++)
            fv_add(&current, node->new[i]);

        if (!expand_set(&current, &next)) {
            /* Node is inconsistent - mark as dead */
            node->formulas = NULL;
            node->count = 0;
            fv_free(&current); fv_free(&next);
            continue;
        }

        /* Store expanded formulas */
        node->formulas = malloc(current.count * sizeof(ltl_formula_t*));
        memcpy(node->formulas, current.items, current.count * sizeof(ltl_formula_t*));
        node->count = current.count;

        /* Store next obligations */
        node->next_count = next.count;
        node->next = malloc(next.count * sizeof(ltl_formula_t*));
        memcpy(node->next, next.items, next.count * sizeof(ltl_formula_t*));

        /* Compute acceptance mask */
        node->accept_mask = 0;
        uint32_t acc_idx = 0;
        for (size_t i = 0; i < closure->count && acc_idx < tab->num_accept_sets; i++) {
            if (closure->formulas[i]->op == LTL_UNTIL) {
                ltl_formula_t *until_f = closure->formulas[i];
                /* Check if psi (right) holds, or Until is not present */
                bool psi_holds = false;
                for (size_t j = 0; j < current.count; j++) {
                    if (current.items[j] == until_f->right) { psi_holds = true; break; }
                }
                bool until_present = false;
                for (size_t j = 0; j < current.count; j++) {
                    if (current.items[j] == until_f) { until_present = true; break; }
                }
                if (psi_holds || !until_present)
                    node->accept_mask |= (1u << acc_idx);
                acc_idx++;
            }
        }

        /* Find or create successor nodes */
        if (next.count > 0) {
            /* Look for existing node with same next obligations */
            int32_t found = -1;
            for (size_t i = 0; i < tab->num_nodes; i++) {
                if (tab->nodes[i].new && tab->nodes[i].new_count == next.count) {
                    bool match = true;
                    for (size_t j = 0; j < next.count; j++) {
                        bool found_f = false;
                        for (size_t k = 0; k < tab->nodes[i].new_count; k++) {
                            if (tab->nodes[i].new[k] == next.items[j]) {
                                found_f = true; break;
                            }
                        }
                        if (!found_f) { match = false; break; }
                    }
                    if (match) { found = (int32_t)i; break; }
                }
            }

            if (found < 0) {
                /* Create new node from next obligations */
                tableau_node_t *succ = tableau_add_node(tab);
                succ->new_count = next.count;
                succ->new = malloc(next.count * sizeof(ltl_formula_t*));
                memcpy(succ->new, next.items,
                       next.count * sizeof(ltl_formula_t*));
                succ->is_initial = false;
                found = (int32_t)(tab->num_nodes - 1);

                /* Add to worklist */
                wl_len++;
                worklist = realloc(worklist, wl_len * sizeof(size_t));
                worklist[wl_len - 1] = found;
            }

            /* Add edge from node to successor */
            if (tab->num_nodes > (size_t)node_idx) {
                /* Allocate edges array if needed */
                if (!tab->edges) {
                    tab->edges = calloc(tab->num_nodes, sizeof(uint32_t*));
                    tab->edge_counts = calloc(tab->num_nodes, sizeof(size_t));
                    tab->edge_caps = calloc(tab->num_nodes, sizeof(size_t));
                }
                size_t ec = tab->edge_counts[node_idx];
                if (ec >= tab->edge_caps[node_idx]) {
                    tab->edge_caps[node_idx] = tab->edge_caps[node_idx] ?
                        tab->edge_caps[node_idx] * 2 : 4;
                    tab->edges[node_idx] = realloc(tab->edges[node_idx],
                        tab->edge_caps[node_idx] * sizeof(uint32_t));
                }
                tab->edges[node_idx][ec] = (uint32_t)found;
                tab->edge_counts[node_idx] = ec + 1;
            }
        }

        fv_free(&current); fv_free(&next);
    }

    free(worklist);
    ltl_closure_free(closure);
    return tab;
}

/* ==================================================================
 * Extract generalized Buchi automaton from tableau
 * ================================================================== */

buchi_t *tableau_to_buchi(const tableau_t *tab, uint32_t num_atoms)
{
    if (!tab || tab->num_nodes == 0) return NULL;

    buchi_t *ba = buchi_new((uint32_t)tab->num_nodes, num_atoms);
    if (!ba) return NULL;

    ba->num_accept_sets = tab->num_accept_sets;

    /* Add states */
    for (size_t i = 0; i < tab->num_nodes; i++) {
        uint32_t idx = buchi_add_state(ba,
            tab->nodes[i].is_initial,
            tab->nodes[i].accept_mask != 0);
        ba->states[idx].prop_mask = 0;
        ba->states[idx].next_mask = 0;

        /* Compute propositional mask from node formulas */
        if (tab->nodes[i].formulas) {
            for (size_t j = 0; j < tab->nodes[i].count; j++) {
                ltl_formula_t *f = tab->nodes[i].formulas[j];
                if (f->op == LTL_ATOM)
                    ba->states[idx].prop_mask |= (1u << f->atom_id);
            }
        }

        ba->accept_sets[idx] = tab->nodes[i].accept_mask;
    }

    /* Add transitions */
    for (size_t i = 0; i < tab->num_nodes; i++) {
        uint32_t from = (uint32_t)i;
        for (size_t j = 0; j < tab->edge_counts[i]; j++) {
            uint32_t to = tab->edges[i][j];

            /* Build label: propositional mask must match at target */
            buchi_label_t label;
            label.pos_mask = 0;
            label.neg_mask = 0;
            label.care_mask = label.pos_mask;

            buchi_add_transition(ba, from, to, &label);
        }
    }

    return ba;
}
/* ==================================================================
 * L5: GPVW on-the-fly construction
 *
 * Gerth-Peled-Vardi-Wolper algorithm builds the tableau
 * on-the-fly. Each node has: old (processed), new (to process),
 * next (X-obligations). The algorithm expands nodes by repeatedly
 * taking a formula from new and applying expansion rules.
 *
 * Reference: GPVW, PSTV 1995
 * ================================================================== */

typedef struct {
    ltl_formula_t **old;    size_t old_cnt;
    ltl_formula_t **new;    size_t new_cnt;
    ltl_formula_t **next;   size_t next_cnt;
    uint32_t        id;
    uint32_t        accept_mask;
    bool            initial;
    bool            expanded;
} gpvw_node_t;

typedef struct {
    gpvw_node_t *nodes;
    size_t       num_nodes;
    size_t       capacity;
    size_t       num_accept;
} gpvw_tab_t;

static gpvw_tab_t *gpvw_create(void)
{
    gpvw_tab_t *t = malloc(sizeof(gpvw_tab_t));
    t->nodes = NULL; t->num_nodes = 0;
    t->capacity = 0; t->num_accept = 0;
    return t;
}

static uint32_t gpvw_add(gpvw_tab_t *t, ltl_formula_t **nf, size_t nfc,
                          ltl_formula_t **nx, size_t nxc)
{
    if (t->num_nodes >= t->capacity) {
        t->capacity = t->capacity ? t->capacity * 2 : 256;
        t->nodes = realloc(t->nodes, t->capacity * sizeof(gpvw_node_t));
    }
    gpvw_node_t *n = &t->nodes[t->num_nodes];
    n->id = (uint32_t)t->num_nodes;
    n->old = NULL; n->old_cnt = 0;
    n->new = malloc(nfc * sizeof(ltl_formula_t*));
    memcpy(n->new, nf, nfc * sizeof(ltl_formula_t*)); n->new_cnt = nfc;
    n->next = malloc(nxc * sizeof(ltl_formula_t*));
    memcpy(n->next, nx, nxc * sizeof(ltl_formula_t*)); n->next_cnt = nxc;
    n->accept_mask = 0; n->initial = false; n->expanded = false;
    return (uint32_t)t->num_nodes++;
}

static void gpvw_free(gpvw_tab_t *t)
{
    if (!t) return;
    for (size_t i = 0; i < t->num_nodes; i++) {
        free(t->nodes[i].old); free(t->nodes[i].new);
        free(t->nodes[i].next);
    }
    free(t->nodes); free(t);
}

/**
 * GPVW expand step: process one formula from the new set.
 *
 * Rules (from GPVW 1995):
 *   - AND: move both to new
 *   - OR: split into two successor nodes
 *   - UNTIL: split: psi holds OR (phi holds + X(phi U psi) in next)
 *   - RELEASE: psi holds + split: phi OR X(phi R psi) in next
 *   - NEXT: move child to next
 *   - GLOBALLY: phi holds + X(G phi) in next
 *   - FINALLY: split: phi OR X(F phi) in next
 *
 * Returns number of successor nodes generated (0 if inconsistent).
 */
static size_t gpvw_expand(gpvw_tab_t *t, uint32_t node_id,
                           ltl_closure_t *closure)
{
    gpvw_node_t *n = &t->nodes[node_id];
    if (n->expanded) return 1; /* Already expanded */
    n->expanded = true;

    if (n->new_cnt == 0) {
        /* Node is a state - store formulas */
        n->old_cnt = 0;
        return 1;
    }

    /* Take first formula from new set */
    ltl_formula_t *f = n->new[0];
    /* Remove it from new */
    n->new_cnt--;
    if (n->new_cnt > 0)
        memmove(n->new, n->new + 1, n->new_cnt * sizeof(ltl_formula_t*));

    /* Add to old */
    n->old_cnt++;
    n->old = realloc(n->old, n->old_cnt * sizeof(ltl_formula_t*));
    n->old[n->old_cnt - 1] = f;

    switch (f->op) {
    case LTL_CONST_TRUE:
    case LTL_ATOM:
    case LTL_LITERAL_POS:
    case LTL_LITERAL_NEG:
        /* Nothing to expand; continue with remaining new */
        return gpvw_expand(t, node_id, closure);

    case LTL_CONST_FALSE:
        return 0; /* Inconsistent */

    case LTL_AND: {
        /* Add both conjuncts to new */
        n->new_cnt += 2;
        n->new = realloc(n->new, n->new_cnt * sizeof(ltl_formula_t*));
        n->new[n->new_cnt - 2] = f->left;
        n->new[n->new_cnt - 1] = f->right;
        return gpvw_expand(t, node_id, closure);
    }

    case LTL_OR: {
        /* Split: create two successor nodes */
        /* Branch 1: left holds */
        ltl_formula_t *b1_new[1] = {f->left};
        gpvw_add(t, b1_new, 1, n->next, n->next_cnt);

        /* Branch 2: right holds */
        ltl_formula_t *b2_new[1] = {f->right};
        gpvw_add(t, b2_new, 1, n->next, n->next_cnt);

        return 2;
    }

    case LTL_NEXT: {
        /* Move child to next */
        n->next_cnt++;
        n->next = realloc(n->next, n->next_cnt * sizeof(ltl_formula_t*));
        n->next[n->next_cnt - 1] = f->left;
        return gpvw_expand(t, node_id, closure);
    }

    case LTL_UNTIL: {
        /* Split: psi OR (phi and X(phi U psi)) */
        /* Branch 1: psi holds */
        ltl_formula_t *b1_new[1] = {f->right};
        gpvw_add(t, b1_new, 1, n->next, n->next_cnt);

        /* Branch 2: phi holds, X(phi U psi) in next */
        n->new_cnt++;
        n->new = realloc(n->new, n->new_cnt * sizeof(ltl_formula_t*));
        n->new[n->new_cnt - 1] = f->left;
        n->next_cnt++;
        n->next = realloc(n->next, n->next_cnt * sizeof(ltl_formula_t*));
        n->next[n->next_cnt - 1] = f;

        return 2;
    }

    case LTL_RELEASE: {
        /* psi holds, and split: phi OR X(phi R psi) in next */
        n->new_cnt++;
        n->new = realloc(n->new, n->new_cnt * sizeof(ltl_formula_t*));
        n->new[n->new_cnt - 1] = f->right;

        /* Split for the OR part */
        ltl_formula_t *b1_new[1] = {f->left};
        gpvw_add(t, b1_new, 1, n->next, n->next_cnt);

        /* Branch: X(phi R psi) in next */
        ltl_formula_t *b2_next_set = f;
        gpvw_add(t, NULL, 0, &b2_next_set, 1);

        return 2 + gpvw_expand(t, node_id, closure);
    }

    case LTL_GLOBALLY: {
        /* phi holds, X(G phi) in next */
        n->new_cnt++;
        n->new = realloc(n->new, n->new_cnt * sizeof(ltl_formula_t*));
        n->new[n->new_cnt - 1] = f->left;
        n->next_cnt++;
        n->next = realloc(n->next, n->next_cnt * sizeof(ltl_formula_t*));
        n->next[n->next_cnt - 1] = f;
        return gpvw_expand(t, node_id, closure);
    }

    case LTL_FINALLY: {
        /* Split: phi OR X(F phi) */
        ltl_formula_t *b1_new[1] = {f->left};
        gpvw_add(t, b1_new, 1, n->next, n->next_cnt);

        ltl_formula_t *b2_next_set = f;
        gpvw_add(t, NULL, 0, &b2_next_set, 1);
        return 2;
    }

    default:
        return gpvw_expand(t, node_id, closure);
    }
}

/* Build GPVW tableau from formula phi (in NNF) */
tableau_t *tableau_build_gpvw(const ltl_formula_t *phi)
{
    if (!phi) return NULL;

    gpvw_tab_t *gt = gpvw_create();
    ltl_closure_t *closure = ltl_fischer_ladner_closure(phi);

    /* Count Until subformulas */
    gt->num_accept = 0;
    for (size_t i = 0; i < closure->count; i++)
        if (closure->formulas[i]->op == LTL_UNTIL) gt->num_accept++;

    /* Initialize with phi */
    ltl_formula_t *init[1] = {(ltl_formula_t*)phi};
    uint32_t root = gpvw_add(gt, init, 1, NULL, 0);
    gt->nodes[root].initial = true;

    /* Expand all nodes */
    for (size_t i = 0; i < gt->num_nodes; i++) {
        gpvw_expand(gt, (uint32_t)i, closure);
    }

    /* Convert to tableau_t */
    tableau_t *tab = malloc(sizeof(tableau_t));
    tab->num_nodes = gt->num_nodes;
    tab->capacity = gt->num_nodes;
    tab->nodes = calloc(gt->num_nodes, sizeof(tableau_node_t));
    tab->num_accept_sets = (uint32_t)gt->num_accept;
    tab->num_atoms = 0;
    uint32_t atoms = ltl_atoms_used(phi);
    while (atoms) { tab->num_atoms++; atoms >>= 1; }

    for (size_t i = 0; i < gt->num_nodes; i++) {
        tab->nodes[i].id = (uint32_t)i;
        tab->nodes[i].is_initial = gt->nodes[i].initial;
        tab->nodes[i].accept_mask = gt->nodes[i].accept_mask;
        /* Copy formulas */
        tab->nodes[i].count = gt->nodes[i].old_cnt + gt->nodes[i].new_cnt;
        if (tab->nodes[i].count > 0) {
            tab->nodes[i].formulas = malloc(tab->nodes[i].count *
                                            sizeof(ltl_formula_t*));
            size_t pos = 0;
            for (size_t j = 0; j < gt->nodes[i].old_cnt; j++)
                tab->nodes[i].formulas[pos++] = gt->nodes[i].old[j];
            for (size_t j = 0; j < gt->nodes[i].new_cnt; j++)
                tab->nodes[i].formulas[pos++] = gt->nodes[i].new[j];
        }
    }

    /* Build edges: for each node, successors are nodes with matching
     * next obligations */
    tab->edges = calloc(gt->num_nodes, sizeof(uint32_t*));
    tab->edge_counts = calloc(gt->num_nodes, sizeof(size_t));
    tab->edge_caps = calloc(gt->num_nodes, sizeof(size_t));

    for (size_t i = 0; i < gt->num_nodes; i++) {
        for (size_t j = 0; j < gt->num_nodes; j++) {
            /* Check if j's old+new contain all of i's next obligations */
            bool all_in = true;
            for (size_t k = 0; k < gt->nodes[i].next_cnt && all_in; k++) {
                ltl_formula_t *nf = gt->nodes[i].next[k];
                bool found_f = false;
                for (size_t m = 0; m < gt->nodes[j].old_cnt; m++)
                    if (gt->nodes[j].old[m] == nf) { found_f = true; break; }
                if (!found_f) {
                    for (size_t m = 0; m < gt->nodes[j].new_cnt; m++)
                        if (gt->nodes[j].new[m] == nf) { found_f = true; break; }
                }
                if (!found_f) { all_in = false; }
            }
            if (all_in) {
                size_t ec = tab->edge_counts[i];
                if (ec >= tab->edge_caps[i]) {
                    tab->edge_caps[i] = tab->edge_caps[i] ?
                        tab->edge_caps[i] * 2 : 4;
                    tab->edges[i] = realloc(tab->edges[i],
                        tab->edge_caps[i] * sizeof(uint32_t));
                }
                tab->edges[i][ec] = (uint32_t)j;
                tab->edge_counts[i] = ec + 1;
            }
        }
    }

    gpvw_free(gt);
    ltl_closure_free(closure);
    return tab;
}

/* ==================================================================
 * L5: Translation drivers
 * ================================================================== */

buchi_t *ltl_to_buchi_tableau(const ltl_formula_t *phi,
                               ltl_to_buchi_stats_t *stats)
{
    if (!phi) return NULL;

    /* Convert to NNF first */
    ltl_formula_t *nnf = ltl_to_nnf(ltl_clone(phi));

    if (stats) {
        stats->method_name = "Tableau";
        stats->formula_nodes = (uint32_t)ltl_size(phi);
        stats->formula_depth = (uint32_t)ltl_depth(phi);
    }

    tableau_t *tab = tableau_build(nnf);

    if (stats) {
        stats->closure_size = (uint32_t)(tab ? tab->num_nodes : 0);
    }

    buchi_t *ba = tableau_to_buchi(tab, (uint32_t)tab->num_atoms);

    if (ba) {
        buchi_remove_unreachable(ba);
        buchi_remove_nonaccepting(ba);
    }

    if (stats && ba) {
        stats->automaton_states = buchi_state_count(ba);
        stats->automaton_trans = buchi_trans_count(ba);
        stats->acceptance_sets = ba->num_accept_sets;
        stats->construction_time_ms = 0.0;
    }

    tableau_free(tab);
    ltl_free(nnf);
    return ba;
}

buchi_t *ltl_to_buchi_gpvw(const ltl_formula_t *phi,
                            ltl_to_buchi_stats_t *stats)
{
    if (!phi) return NULL;

    ltl_formula_t *nnf = ltl_to_nnf(ltl_clone(phi));

    if (stats) {
        stats->method_name = "GPVW";
        stats->formula_nodes = (uint32_t)ltl_size(phi);
        stats->formula_depth = (uint32_t)ltl_depth(phi);
    }

    tableau_t *tab = tableau_build_gpvw(nnf);

    if (stats) {
        stats->closure_size = (uint32_t)(tab ? tab->num_nodes : 0);
    }

    buchi_t *ba = tableau_to_buchi(tab, (uint32_t)tab->num_atoms);

    if (ba) {
        buchi_remove_unreachable(ba);
        buchi_remove_nonaccepting(ba);
        if (ba->num_accept_sets > 1) {
            buchi_t *deg = buchi_degeneralize(ba);
            buchi_free(ba);
            ba = deg;
        }
    }

    if (stats && ba) {
        stats->automaton_states = buchi_state_count(ba);
        stats->automaton_trans = buchi_trans_count(ba);
        stats->acceptance_sets = ba->num_accept_sets;
        stats->construction_time_ms = 0.0;
    }

    tableau_free(tab);
    ltl_free(nnf);
    return ba;
}

buchi_t *ltl_to_buchi_simplified(const ltl_formula_t *phi,
                                  ltl_to_buchi_stats_t *stats)
{
    if (!phi) return NULL;

    /* Simplify then NNF then tableau */
    ltl_formula_t *cloned = ltl_clone(phi);
    ltl_formula_t *simplified = ltl_simplify(ltl_remove_implies_equiv(cloned));
    ltl_formula_t *nnf = ltl_to_nnf(simplified);

    return ltl_to_buchi_tableau(nnf, stats);
}

buchi_t *ltl_to_buchi(const ltl_formula_t *phi,
                       ltl_to_buchi_method_t method,
                       ltl_to_buchi_stats_t *stats_out)
{
    ltl_to_buchi_stats_t local_stats;
    ltl_to_buchi_stats_t *st = stats_out ? stats_out : &local_stats;
    memset(st, 0, sizeof(ltl_to_buchi_stats_t));

    switch (method) {
    case TRANSLATE_TABLEAU:
        return ltl_to_buchi_tableau(phi, st);
    case TRANSLATE_GPVW:
        return ltl_to_buchi_gpvw(phi, st);
    case TRANSLATE_SIMPLIFIED:
        return ltl_to_buchi_simplified(phi, st);
    default:
        return ltl_to_buchi_tableau(phi, st);
    }
}

buchi_t *ltl_negation_to_buchi(const ltl_formula_t *phi,
                                ltl_to_buchi_method_t method,
                                ltl_to_buchi_stats_t *stats_out)
{
    if (!phi) return NULL;

    /* Negate: not(phi) */
    ltl_formula_t *negated = ltl_not(ltl_clone(phi));
    ltl_formula_t *nnf = ltl_to_nnf(negated);

    buchi_t *result = ltl_to_buchi(nnf, method, stats_out);
    ltl_free(nnf);
    return result;
}

/* ==================================================================
 * L7: Model checking via automata-theoretic approach
 *
 * Sys |= phi  iff  L(B_Sys) intersect L(B_not_phi) = empty
 *
 * We check emptiness of the product automaton using NDFS.
 * ================================================================== */

bool ltl_model_check(const buchi_t *sys, const ltl_formula_t *phi,
                      buchi_trace_t **counterexample_out)
{
    if (!sys || !phi) return false;
    if (counterexample_out) *counterexample_out = NULL;

    /* Build automaton for not(phi) */
    buchi_t *b_not_phi = ltl_negation_to_buchi(phi, TRANSLATE_TABLEAU, NULL);
    if (!b_not_phi) return false;

    /* Product of system and not(phi) automaton */
    buchi_t *product = buchi_product(sys, b_not_phi);
    buchi_free(b_not_phi);
    if (!product) return false;

    /* Check emptiness */
    uint32_t *cycle = NULL;
    size_t cycle_len = 0;
    bool empty = !buchi_ndfs_empty(product, &cycle, &cycle_len);

    if (!empty && counterexample_out) {
        /* Build counterexample trace */
        buchi_trace_t *trace = malloc(sizeof(buchi_trace_t));
        trace->states = cycle;
        trace->length = cycle_len;
        trace->valuations = malloc(cycle_len * sizeof(uint32_t));
        /* Extract valuations from product states */
        for (size_t i = 0; i < cycle_len; i++) {
            trace->valuations[i] = product->states[cycle[i]].prop_mask;
        }
        *counterexample_out = trace;
    }

    buchi_free(product);
    return empty;
}

bool ltl_satisfiable(const ltl_formula_t *phi)
{
    if (!phi) return false;
    buchi_t *ba = ltl_to_buchi(phi, TRANSLATE_TABLEAU, NULL);
    if (!ba) return false;
    bool sat = !buchi_is_empty(ba);
    buchi_free(ba);
    return sat;
}

bool ltl_valid(const ltl_formula_t *phi)
{
    if (!phi) return false;
    buchi_t *ba = ltl_negation_to_buchi(phi, TRANSLATE_TABLEAU, NULL);
    if (!ba) return false;
    bool valid = buchi_is_empty(ba);
    buchi_free(ba);
    return valid;
}

/* ==================================================================
 * L6: Common LTL patterns to Buchi automata
 *
 * Pre-built translators for standard specification patterns.
 * ================================================================== */

buchi_t *ltl_pattern_to_buchi(const char *pattern,
                               ltl_to_buchi_stats_t *stats)
{
    if (!pattern) return NULL;

    /* Build formula from pattern using a simple recursive descent parser
     * for the compact pattern syntax:
     *   "G(p -> F q)"   response pattern
     *   "G F p"         recurrence
     *   "F G p"         persistence
     *   "G(p -> X q)"   next response
     *   "p U q"         until
     *   "G(!p || !q)"   mutual exclusion
     */

    /* For now, handle a few hardcoded patterns */
    ltl_formula_t *phi = NULL;

    if (strcmp(pattern, "G(p -> F q)") == 0) {
        ltl_formula_t *p = ltl_atom(0);
        ltl_formula_t *q = ltl_atom(1);
        ltl_formula_t *fq = ltl_finally(q);
        ltl_formula_t *imp = ltl_implies(p, fq);
        phi = ltl_globally(imp);
    } else if (strcmp(pattern, "G F p") == 0) {
        ltl_formula_t *p = ltl_atom(0);
        phi = ltl_globally(ltl_finally(p));
    } else if (strcmp(pattern, "F G p") == 0) {
        ltl_formula_t *p = ltl_atom(0);
        phi = ltl_finally(ltl_globally(p));
    } else if (strcmp(pattern, "p U q") == 0) {
        phi = ltl_until(ltl_atom(0), ltl_atom(1));
    } else if (strcmp(pattern, "G(!p || !q)") == 0) {
        ltl_formula_t *np = ltl_not(ltl_atom(0));
        ltl_formula_t *nq = ltl_not(ltl_atom(1));
        phi = ltl_globally(ltl_or(np, nq));
    } else if (strcmp(pattern, "G(p -> X q)") == 0) {
        ltl_formula_t *p = ltl_atom(0);
        ltl_formula_t *q = ltl_atom(1);
        phi = ltl_globally(ltl_implies(p, ltl_next(q)));
    } else {
        return NULL;
    }

    buchi_t *result = ltl_to_buchi(phi, TRANSLATE_SIMPLIFIED, stats);
    ltl_free(phi);
    return result;
}
