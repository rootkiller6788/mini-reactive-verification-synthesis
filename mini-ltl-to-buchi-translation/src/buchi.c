/**
 * @file buchi.c
 * @brief Buchi Automata - data structures and core operations.
 *
 * Implements Buchi automaton construction (L1), manipulation (L2),
 * label algebra (L3), emptiness/lasso finding (L4), degeneralization
 * and SCC (L5), NDFS emptiness (L6), and verification utilities (L7).
 *
 * References:
 *   - J.R. Buchi, "On a Decision Method in Restricted Second Order
 *     Arithmetic" (1962)
 *   - Baier & Katoen, "Principles of Model Checking" (2008), Ch.4
 *   - Vardi & Wolper, "An Automata-Theoretic Approach..." (LICS 1986)
 */

#include "buchi.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ==========================================================================
 * L1+L2: Construction and memory management
 * ========================================================================== */

buchi_t *buchi_new(uint32_t capacity, uint32_t num_atoms)
{
    buchi_t *ba = (buchi_t *)malloc(sizeof(buchi_t));
    if (!ba) return NULL;
    ba->num_states = 0;
    ba->capacity   = capacity > 0 ? capacity : 16;
    ba->states     = (buchi_state_t *)calloc(ba->capacity, sizeof(buchi_state_t));
    ba->num_accept_sets = 1;
    ba->accept_sets = (uint32_t *)calloc(ba->capacity, sizeof(uint32_t));
    ba->num_initial = 0;
    ba->initial     = NULL;
    ba->num_atoms   = num_atoms;
    ba->atom_names  = NULL;
    if (!ba->states || !ba->accept_sets) {
        buchi_free(ba); return NULL;
    }
    return ba;
}

void buchi_free(buchi_t *ba)
{
    if (!ba) return;
    if (ba->states) {
        for (uint32_t i = 0; i < ba->num_states; i++) {
            buchi_trans_t *t = ba->states[i].trans;
            while (t) {
                buchi_trans_t *next = t->next;
                free(t);
                t = next;
            }
            free(ba->states[i].name);
        }
        free(ba->states);
    }
    free(ba->accept_sets);
    free(ba->initial);
    if (ba->atom_names) {
        for (uint32_t i = 0; i < ba->num_atoms; i++)
            free(ba->atom_names[i]);
        free(ba->atom_names);
    }
    free(ba);
}

void buchi_ensure_capacity(buchi_t *ba, uint32_t needed)
{
    if (!ba || needed <= ba->capacity) return;
    uint32_t new_cap = ba->capacity;
    while (new_cap < needed) new_cap *= 2;
    ba->states = (buchi_state_t *)realloc(ba->states,
                    new_cap * sizeof(buchi_state_t));
    ba->accept_sets = (uint32_t *)realloc(ba->accept_sets,
                    new_cap * sizeof(uint32_t));
    memset(ba->states + ba->capacity, 0,
           (new_cap - ba->capacity) * sizeof(buchi_state_t));
    memset(ba->accept_sets + ba->capacity, 0,
           (new_cap - ba->capacity) * sizeof(uint32_t));
    ba->capacity = new_cap;
}

uint32_t buchi_add_state(buchi_t *ba, bool initial, bool accepting)
{
    if (!ba) return (uint32_t)-1;
    buchi_ensure_capacity(ba, ba->num_states + 1);
    uint32_t idx = ba->num_states++;
    ba->states[idx].id        = idx;
    ba->states[idx].initial   = initial;
    ba->states[idx].accepting = accepting;
    ba->states[idx].trans     = NULL;
    ba->states[idx].name      = NULL;
    ba->states[idx].prop_mask = 0;
    ba->states[idx].next_mask = 0;
    ba->states[idx].flags     = 0;
    if (accepting)
        ba->accept_sets[idx] = 1;
    if (initial) {
        ba->num_initial++;
        ba->initial = (uint32_t *)realloc(ba->initial,
                        ba->num_initial * sizeof(uint32_t));
        ba->initial[ba->num_initial - 1] = idx;
    }
    return idx;
}

uint32_t buchi_add_named_state(buchi_t *ba, const char *name,
                                bool initial, bool accepting)
{
    uint32_t idx = buchi_add_state(ba, initial, accepting);
    if (name && idx != (uint32_t)-1) {
        ba->states[idx].name = strdup(name);
    }
    return idx;
}

void buchi_add_transition(buchi_t *ba, uint32_t from, uint32_t to,
                          const buchi_label_t *label)
{
    if (!ba || from >= ba->num_states) return;
    buchi_trans_t *t = (buchi_trans_t *)malloc(sizeof(buchi_trans_t));
    if (!t) return;
    t->target = to;
    t->label  = *label;
    t->next   = ba->states[from].trans;
    ba->states[from].trans = t;
}

void buchi_add_true_transition(buchi_t *ba, uint32_t from, uint32_t to)
{
    buchi_label_t lbl = buchi_label_true();
    buchi_add_transition(ba, from, to, &lbl);
}

void buchi_set_accept_sets(buchi_t *ba, uint32_t state_idx,
                            uint32_t accept_mask)
{
    if (ba && state_idx < ba->num_states)
        ba->accept_sets[state_idx] = accept_mask;
}

uint32_t buchi_state_count(const buchi_t *ba)
    { return ba ? ba->num_states : 0; }

uint32_t buchi_trans_count(const buchi_t *ba)
{
    if (!ba) return 0;
    uint32_t cnt = 0;
    for (uint32_t i = 0; i < ba->num_states; i++) {
        buchi_trans_t *t = ba->states[i].trans;
        while (t) { cnt++; t = t->next; }
    }
    return cnt;
}

/* ==========================================================================
 * L3: Label algebra
 *
 * Labels are conjunctions of literals. pos_mask = atoms that must be TRUE,
 * neg_mask = atoms that must be FALSE. care_mask = pos | neg.
 * Two labels are compatible iff no atom forced both TRUE and FALSE.
 * ========================================================================== */

buchi_label_t buchi_label_make(uint32_t pos, uint32_t neg)
{
    buchi_label_t l;
    l.pos_mask  = pos;
    l.neg_mask  = neg;
    l.care_mask = pos | neg;
    return l;
}

buchi_label_t buchi_label_true(void)
{
    buchi_label_t l = {0, 0, 0};
    return l;
}

buchi_label_t buchi_label_false(void)
{
    buchi_label_t l = {1, 1, 1};
    return l;
}

bool buchi_label_compatible(const buchi_label_t *a, const buchi_label_t *b)
{
    if (!a || !b) return false;
    return ((a->pos_mask & b->neg_mask) == 0) &&
           ((a->neg_mask & b->pos_mask) == 0);
}

buchi_label_t buchi_label_conjoin(const buchi_label_t *a,
                                   const buchi_label_t *b)
{
    if (!a || !b) return buchi_label_false();
    buchi_label_t r;
    r.pos_mask  = a->pos_mask | b->pos_mask;
    r.neg_mask  = a->neg_mask | b->neg_mask;
    r.care_mask = r.pos_mask | r.neg_mask;
    if (r.pos_mask & r.neg_mask)
        return buchi_label_false();
    return r;
}

bool buchi_label_implies(const buchi_label_t *a, const buchi_label_t *b)
{
    if (!a || !b) return false;
    return ((b->pos_mask & ~a->pos_mask) == 0) &&
           ((b->neg_mask & ~a->neg_mask) == 0);
}

bool buchi_label_satisfies(const buchi_label_t *label, uint32_t valuation)
{
    if (!label) return false;
    if ((label->pos_mask & ~valuation) != 0) return false;
    if ((label->neg_mask & valuation) != 0) return false;
    return true;
}

bool buchi_label_equal(const buchi_label_t *a, const buchi_label_t *b)
{
    if (!a || !b) return (a == b);
    return a->pos_mask == b->pos_mask && a->neg_mask == b->neg_mask;
}

void buchi_label_print(const buchi_label_t *label, uint32_t num_atoms,
                       char **atom_names)
{
    if (!label) { printf("null"); return; }
    if (label->care_mask == 0) { printf("true"); return; }
    if (label->pos_mask & label->neg_mask) { printf("false"); return; }
    bool first = true;
    for (uint32_t i = 0; i < num_atoms && i < 32; i++) {
        uint32_t bit = 1u << i;
        if (label->pos_mask & bit) {
            if (!first) printf(" && ");
            if (atom_names && atom_names[i])
                printf("%s", atom_names[i]);
            else printf("p%u", i);
            first = false;
        }
        if (label->neg_mask & bit) {
            if (!first) printf(" && ");
            if (atom_names && atom_names[i])
                printf("!%s", atom_names[i]);
            else printf("!p%u", i);
            first = false;
        }
    }
}

/* ==================================================================
 * L6: Nested DFS for emptiness checking (CVWY algorithm)
 *
 * Two DFS passes: outer (blue) explores states; inner (red)
 * starts from accepting states backtracked in outer DFS to
 * find a cycle back to the same accepting state.
 *
 * O(|Q| + |delta|) time, O(|Q|) space.
 * Reference: Courcoubetis, Vardi, Wolper, Yannakakis, CAV 1990
 * ================================================================== */

bool buchi_ndfs_empty(const buchi_t *ba, uint32_t **cycle, size_t *cycle_len)
{
    if (!ba || ba->num_states == 0) {
        if (cycle) *cycle = NULL;
        if (cycle_len) *cycle_len = 0;
        return false;
    }

    uint32_t n = ba->num_states;
    int *outer_visited = (int *)calloc(n, sizeof(int));
    int *inner_visited = (int *)calloc(n, sizeof(int));
    uint32_t *stack = (uint32_t *)malloc(n * sizeof(uint32_t));
    /* reserved */
    /* For each initial state, perform outer DFS */
    for (uint32_t ii = 0; ii < ba->num_initial; ii++) {
        uint32_t init = ba->initial[ii];
        if (outer_visited[init]) continue;

        /* Outer DFS stack */
        uint32_t *outer_stack = (uint32_t *)malloc(n * sizeof(uint32_t));
        uint32_t outer_top = 0;
        outer_stack[outer_top++] = init;

        while (outer_top > 0) {
            uint32_t s = outer_stack[outer_top - 1];

            if (!outer_visited[s]) {
                outer_visited[s] = 1;
            }

            /* If s is accepting and not yet inner-visited, launch inner DFS */
            if (ba->states[s].accepting && !inner_visited[s]) {
                /* Inner DFS from s looking for cycle back to s */
                uint32_t *inner_stack = (uint32_t *)malloc(n * sizeof(uint32_t));
                uint32_t inner_top = 0;
                int *in_stack = (int *)calloc(n, sizeof(int));

                inner_stack[inner_top++] = s;
                in_stack[s] = 1;

                while (inner_top > 0) {
                    uint32_t is = inner_stack[inner_top - 1];
                    int found = 0;

                    buchi_trans_t *t = ba->states[is].trans;
                    while (t) {
                        if (t->target == s) {
                            /* Found cycle back to accepting state */
                            uint32_t *cyc = malloc((inner_top + 1) * sizeof(uint32_t));
                            for (uint32_t j = 0; j < inner_top; j++)
                                cyc[j] = inner_stack[j];
                            cyc[inner_top] = s;
                            if (cycle) *cycle = cyc;
                            if (cycle_len) *cycle_len = inner_top + 1;

                            free(in_stack); free(inner_stack);
                            free(outer_stack);
                            free(outer_visited); free(inner_visited); free(stack);
                            return true;
                        }
                        if (!inner_visited[t->target] && !in_stack[t->target]) {
                            in_stack[t->target] = 1;
                            inner_stack[inner_top++] = t->target;
                            found = 1;
                            break;
                        }
                        t = t->next;
                    }
                    if (!found) {
                        inner_top--;
                        inner_visited[is] = 1;
                    }
                }
                free(in_stack); free(inner_stack);
            }

            /* Continue outer DFS */
            int found = 0;
            buchi_trans_t *t = ba->states[s].trans;
            while (t) {
                if (!outer_visited[t->target]) {
                    outer_stack[outer_top++] = t->target;
                    found = 1;
                    break;
                }
                t = t->next;
            }
            if (!found) {
                outer_top--;
            }
        }
        free(outer_stack);
    }

    if (cycle) *cycle = NULL;
    if (cycle_len) *cycle_len = 0;
    free(outer_visited); free(inner_visited); free(stack);
    return false;
}

/* ==================================================================
 * L5: Degeneralization (TGBA to NBA)
 *
 * Converts generalized Buchi with k acceptance sets to standard Buchi
 * with single acceptance set. Standard construction: create k copies,
 * cycle through acceptance sets. O(k * |Q|) states.
 * Reference: Choueka, "Theories of Automata on omega-Tapes" (1974)
 * ================================================================== */

buchi_t *buchi_degeneralize(const buchi_t *tgba)
{
    if (!tgba || tgba->num_accept_sets <= 1)
        return buchi_clone(tgba);

    uint32_t k = tgba->num_accept_sets;
    uint32_t n = tgba->num_states;
    buchi_t *result = buchi_new(n * k, tgba->num_atoms);
    if (!result) return NULL;

    /* Create k copies of each state */
    for (uint32_t q = 0; q < n; q++) {
        for (uint32_t i = 0; i < k; i++) {
            bool init = (tgba->states[q].initial && i == 0);
            buchi_add_state(result, init, false);
        }
    }

    /* Transitions: (q,i) -> (q',i') where i' = i if q not in set i,
     * else i' = (i+1)%k */
    for (uint32_t q = 0; q < n; q++) {
        buchi_trans_t *t = tgba->states[q].trans;
        while (t) {
            for (uint32_t i = 0; i < k; i++) {
                uint32_t from = q * k + i;
                bool in_set = (tgba->accept_sets[q] & (1u << i)) != 0;
                uint32_t next_i = in_set ? ((i + 1) % k) : i;
                uint32_t to = t->target * k + next_i;
                buchi_add_transition(result, from, to, &t->label);
            }
            t = t->next;
        }
    }

    /* Acceptance: copy 0 is accepting */
    for (uint32_t q = 0; q < n; q++)
        result->states[q * k].accepting = true;

    buchi_remove_unreachable(result);
    return result;
}

/* ==================================================================
 * L6: Trace construction from lasso
 * ================================================================== */

buchi_trace_t *buchi_trace_from_lasso(const buchi_t *ba,
                                       uint32_t *prefix, size_t prefix_len,
                                       uint32_t *cycle, size_t cycle_len)
{
    if (!ba || !prefix || !cycle) return NULL;

    buchi_trace_t *trace = malloc(sizeof(buchi_trace_t));
    trace->length = prefix_len + cycle_len;
    trace->states = malloc(trace->length * sizeof(uint32_t));
    trace->valuations = malloc(trace->length * sizeof(uint32_t));

    for (size_t i = 0; i < prefix_len; i++) {
        trace->states[i] = prefix[i];
        trace->valuations[i] = ba->states[prefix[i]].prop_mask;
    }
    for (size_t i = 0; i < cycle_len; i++) {
        size_t pos = prefix_len + i;
        trace->states[pos] = cycle[i];
        trace->valuations[pos] = ba->states[cycle[i]].prop_mask;
    }

    return trace;
}

void buchi_trace_free(buchi_trace_t *trace)
{
    if (trace) {
        free(trace->states);
        free(trace->valuations);
        free(trace);
    }
}

/* ==================================================================
 * L5: SCC computation (Tarjan's algorithm)
 * ================================================================== */

uint32_t *buchi_scc(const buchi_t *ba, uint32_t *num_scc_out)
{
    if (!ba || ba->num_states == 0) {
        if (num_scc_out) *num_scc_out = 0;
        return NULL;
    }

    uint32_t n = ba->num_states;
    uint32_t *scc_id = (uint32_t *)malloc(n * sizeof(uint32_t));
    uint32_t *index  = (uint32_t *)malloc(n * sizeof(uint32_t));
    uint32_t *lowlink = (uint32_t *)malloc(n * sizeof(uint32_t));
    int *onstack = (int *)calloc(n, sizeof(int));
    uint32_t *stack = (uint32_t *)malloc(n * sizeof(uint32_t));
    uint32_t stack_top = 0;
    uint32_t cur_index = 1;
    uint32_t scc_count = 0;

    /* Iterative Tarjan using explicit stack */
    typedef struct { uint32_t state; uint32_t next_trans; } dfs_frame_t;
    dfs_frame_t *dfs_stack = (dfs_frame_t *)malloc(n * sizeof(dfs_frame_t));
    uint32_t dfs_top = 0;

    for (uint32_t s = 0; s < n; s++) {
        index[s] = 0;
    }

    for (uint32_t start = 0; start < n; start++) {
        if (index[start] > 0) continue;

        dfs_top = 0;
        dfs_stack[dfs_top].state = start;
        dfs_stack[dfs_top].next_trans = 0;
        dfs_top++;

        while (dfs_top > 0) {
            dfs_frame_t *frame = &dfs_stack[dfs_top - 1];
            uint32_t v = frame->state;

            if (frame->next_trans == 0) {
                /* First visit to v */
                index[v] = lowlink[v] = cur_index++;
                stack[stack_top++] = v;
                onstack[v] = 1;
            }

            /* Process edges */
            buchi_trans_t *t = ba->states[v].trans;
            uint32_t edge_idx = 0;
            int found = 0;

            while (t && edge_idx < frame->next_trans) {
                t = t->next; edge_idx++;
            }

            while (t) {
                uint32_t w = t->target;
                if (index[w] == 0) {
                    /* Not visited yet */
                    frame->next_trans = edge_idx + 1;
                    dfs_stack[dfs_top].state = v;
                    dfs_stack[dfs_top].next_trans = frame->next_trans;
                    dfs_top++;
                    dfs_stack[dfs_top - 1].state = w;
                    dfs_stack[dfs_top - 1].next_trans = 0;
                    found = 1;
                    break;
                } else if (onstack[w]) {
                    if (index[w] < lowlink[v])
                        lowlink[v] = index[w];
                }
                t = t->next; edge_idx++;
            }

            if (!found) {
                /* Finished processing v */
                dfs_top--;

                if (lowlink[v] == index[v]) {
                    /* v is root of an SCC */
                    uint32_t w;
                    do {
                        w = stack[--stack_top];
                        onstack[w] = 0;
                        scc_id[w] = scc_count;
                    } while (w != v);
                    scc_count++;
                }

                /* Update parent lowlink */
                if (dfs_top > 0) {
                    uint32_t parent = dfs_stack[dfs_top - 1].state;
                    if (lowlink[v] < lowlink[parent])
                        lowlink[parent] = lowlink[v];
                }
            }
        }
    }

    free(index); free(lowlink); free(onstack); free(stack); free(dfs_stack);
    if (num_scc_out) *num_scc_out = scc_count;
    return scc_id;
}

bool buchi_scc_is_accepting(const buchi_t *ba, const uint32_t *scc_ids,
                             uint32_t scc_id)
{
    if (!ba || !scc_ids) return false;
    for (uint32_t i = 0; i < ba->num_states; i++) {
        if (scc_ids[i] == scc_id && ba->states[i].accepting)
            return true;
    }
    return false;
}

buchi_scc_info_t *buchi_accepting_scc_analysis(buchi_t *ba)
{
    if (!ba) return NULL;
    uint32_t num_scc = 0;
    uint32_t *scc_ids = buchi_scc(ba, &num_scc);

    buchi_scc_info_t *info = malloc(sizeof(buchi_scc_info_t));
    info->scc_ids = scc_ids;
    info->total_sccs = num_scc;
    info->num_accepting_sccs = 0;

    for (uint32_t i = 0; i < num_scc; i++) {
        if (buchi_scc_is_accepting(ba, scc_ids, i))
            info->num_accepting_sccs++;
    }
    return info;
}

void buchi_scc_info_free(buchi_scc_info_t *info)
{
    if (info) { free(info->scc_ids); free(info); }
}
