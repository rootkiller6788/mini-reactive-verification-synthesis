/**
 * ctl_kripke.c — Kripke Structure Implementation
 *
 * Implements the Kripke structure data type M = (S, S0, R, L)
 * fundamental to CTL model checking. Provides construction,
 * manipulation, graph analysis (reachability, SCC), and I/O.
 *
 * Knowledge: L1 (Definitions), L3 (Mathematical Structures), L5 (Algorithms)
 * Reference: Clarke, Grumberg, Peled "Model Checking" (1999), Ch 2
 *            Baier & Katoen "Principles of Model Checking" (2008), §2.1
 */

#include "ctl_kripke.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════
 * Label Set Implementation
 * ═══════════════════════════════════════════════════════════════════════ */

void ctl_label_set_init(ctl_label_set *ls, int nap) {
    ls->nap = nap;
    ls->bits = 0;
    ls->ext = NULL;
    if (nap > CTL_MAX_LABEL_BITS) {
        int nwords = (nap + 63) / 64;
        ls->ext = (unsigned long long *)calloc((size_t)nwords,
                                                sizeof(unsigned long long));
    }
}

void ctl_label_set_destroy(ctl_label_set *ls) {
    free(ls->ext);
    ls->ext = NULL;
    ls->nap = 0;
    ls->bits = 0;
}

void ctl_label_set_set(ctl_label_set *ls, int ap, int val) {
    if (ap < 0 || ap >= ls->nap) return;
    if (ls->nap <= CTL_MAX_LABEL_BITS) {
        if (val)
            ls->bits |= (1ULL << ap);
        else
            ls->bits &= ~(1ULL << ap);
    } else if (ls->ext) {
        int word = ap / 64;
        int bit  = ap % 64;
        if (val)
            ls->ext[word] |= (1ULL << bit);
        else
            ls->ext[word] &= ~(1ULL << bit);
    }
}

int ctl_label_set_get(const ctl_label_set *ls, int ap) {
    if (ap < 0 || ap >= ls->nap) return 0;
    if (ls->nap <= CTL_MAX_LABEL_BITS)
        return (ls->bits >> ap) & 1;
    if (ls->ext) {
        int word = ap / 64;
        int bit  = ap % 64;
        return (int)((ls->ext[word] >> bit) & 1);
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Kripke Structure Construction / Destruction
 * ═══════════════════════════════════════════════════════════════════════ */

ctl_kripke *ctl_kripke_create(ctl_state_id nstates, int nap,
                               const char **ap_names) {
    if (nstates == 0) return NULL;

    ctl_kripke *k = (ctl_kripke *)calloc(1, sizeof(ctl_kripke));
    if (!k) return NULL;

    k->nstates = nstates;
    k->nap = nap;

    /* Allocate adjacency lists */
    k->successors = (ctl_edge **)calloc(nstates, sizeof(ctl_edge *));
    k->predecessors = (ctl_edge **)calloc(nstates, sizeof(ctl_edge *));
    if (!k->successors || !k->predecessors) {
        ctl_kripke_destroy(k);
        return NULL;
    }

    /* Allocate label sets */
    k->labels = (ctl_label_set *)calloc(nstates, sizeof(ctl_label_set));
    if (!k->labels) {
        ctl_kripke_destroy(k);
        return NULL;
    }
    for (ctl_state_id s = 0; s < nstates; s++) {
        ctl_label_set_init(&k->labels[s], nap);
    }

    /* Copy AP names */
    if (nap > 0 && ap_names) {
        k->ap_names = (char **)calloc((size_t)nap, sizeof(char *));
        if (!k->ap_names) {
            ctl_kripke_destroy(k);
            return NULL;
        }
        for (int i = 0; i < nap; i++) {
            if (ap_names[i]) {
                k->ap_names[i] = (char *)malloc(strlen(ap_names[i]) + 1);
                if (k->ap_names[i])
                    strcpy(k->ap_names[i], ap_names[i]);
            }
        }
    }

    return k;
}

void ctl_kripke_destroy(ctl_kripke *k) {
    if (!k) return;

    /* Free edges */
    for (ctl_state_id s = 0; s < k->nstates; s++) {
        ctl_edge *e = k->successors[s];
        while (e) {
            ctl_edge *next = e->next;
            free(e);
            e = next;
        }
        /* Note: predecessors share edges with successors, don't double-free */
    }
    free(k->successors);
    free(k->predecessors);

    /* Free labels */
    if (k->labels) {
        for (ctl_state_id s = 0; s < k->nstates; s++) {
            ctl_label_set_destroy(&k->labels[s]);
        }
        free(k->labels);
    }

    /* Free AP names */
    if (k->ap_names) {
        for (int i = 0; i < k->nap; i++) {
            free(k->ap_names[i]);
        }
        free(k->ap_names);
    }

    free(k->initial_states);
    free(k);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Initial States
 * ═══════════════════════════════════════════════════════════════════════ */

int ctl_kripke_set_initial(ctl_kripke *k, const ctl_state_id *states,
                            ctl_state_id count) {
    if (!k) return -1;
    free(k->initial_states);
    k->initial_states = NULL;
    k->ninitial = 0;

    if (count == 0) return 0;

    k->initial_states = (ctl_state_id *)malloc(count * sizeof(ctl_state_id));
    if (!k->initial_states) return -1;

    for (ctl_state_id i = 0; i < count; i++) {
        if (states[i] >= k->nstates) {
            free(k->initial_states);
            k->initial_states = NULL;
            return -1;
        }
        k->initial_states[i] = states[i];
    }
    k->ninitial = count;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Edge Management
 * ═══════════════════════════════════════════════════════════════════════ */

static ctl_edge *edge_find(ctl_edge *list, ctl_state_id target) {
    for (ctl_edge *e = list; e; e = e->next) {
        if (e->target == target) return e;
    }
    return NULL;
}

int ctl_kripke_add_edge(ctl_kripke *k, ctl_state_id s, ctl_state_id t) {
    if (!k || s >= k->nstates || t >= k->nstates) return -1;

    /* Check for duplicate */
    if (edge_find(k->successors[s], t)) return 0;

    /* Create forward edge */
    ctl_edge *fwd = (ctl_edge *)malloc(sizeof(ctl_edge));
    if (!fwd) return -1;
    fwd->target = t;
    fwd->next = k->successors[s];
    k->successors[s] = fwd;

    /* Create backward edge */
    ctl_edge *rev = (ctl_edge *)malloc(sizeof(ctl_edge));
    if (!rev) return -1;
    rev->target = s;
    rev->next = k->predecessors[t];
    k->predecessors[t] = rev;

    return 0;
}

int ctl_kripke_remove_edge(ctl_kripke *k, ctl_state_id s, ctl_state_id t) {
    if (!k || s >= k->nstates || t >= k->nstates) return -1;

    /* Remove from forward list */
    ctl_edge **prev = &k->successors[s];
    int removed = 0;
    while (*prev) {
        if ((*prev)->target == t) {
            ctl_edge *del = *prev;
            *prev = del->next;
            free(del);
            removed = 1;
            break;
        }
        prev = &(*prev)->next;
    }

    /* Remove from backward list */
    prev = &k->predecessors[t];
    while (*prev) {
        if ((*prev)->target == s) {
            ctl_edge *del = *prev;
            *prev = del->next;
            free(del);
            break;
        }
        prev = &(*prev)->next;
    }

    return removed ? 0 : -1;
}

int ctl_kripke_set_label(ctl_kripke *k, ctl_state_id s, int ap, int value) {
    if (!k || s >= k->nstates || ap < 0 || ap >= k->nap) return -1;
    ctl_label_set_set(&k->labels[s], ap, value);
    return 0;
}

int ctl_kripke_get_label(const ctl_kripke *k, ctl_state_id s, int ap) {
    if (!k || s >= k->nstates || ap < 0 || ap >= k->nap) return 0;
    return ctl_label_set_get(&k->labels[s], ap);
}

int ctl_kripke_make_total(ctl_kripke *k) {
    if (!k) return -1;
    int added = 0;
    for (ctl_state_id s = 0; s < k->nstates; s++) {
        if (!k->successors[s]) {
            ctl_kripke_add_edge(k, s, s);
            added++;
        }
    }
    return added;
}

int ctl_kripke_post(const ctl_kripke *k, ctl_state_id s,
                    ctl_state_id *out, int *capacity) {
    if (!k || s >= k->nstates || !capacity) return -1;
    int count = 0;
    for (ctl_edge *e = k->successors[s]; e; e = e->next) count++;
    if (out) {
        int i = 0;
        for (ctl_edge *e = k->successors[s]; e && i < *capacity; e = e->next)
            out[i++] = e->target;
        *capacity = (i < count) ? i : count;
    } else {
        *capacity = count;
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Accessors
 * ═══════════════════════════════════════════════════════════════════════ */

ctl_state_id ctl_kripke_nstates(const ctl_kripke *k) { return k ? k->nstates : 0; }
ctl_state_id ctl_kripke_ninitial(const ctl_kripke *k) { return k ? k->ninitial : 0; }
int ctl_kripke_nap(const ctl_kripke *k) { return k ? k->nap : 0; }

ctl_state_id ctl_kripke_initial(const ctl_kripke *k, ctl_state_id i) {
    if (!k || i >= k->ninitial) return CTL_INVALID_STATE;
    return k->initial_states[i];
}

const char *ctl_kripke_ap_name(const ctl_kripke *k, int ap) {
    if (!k || ap < 0 || ap >= k->nap) return NULL;
    return k->ap_names ? k->ap_names[ap] : NULL;
}

int ctl_kripke_ap_index(const ctl_kripke *k, const char *name) {
    if (!k || !name) return -1;
    for (int i = 0; i < k->nap; i++) {
        if (k->ap_names[i] && strcmp(k->ap_names[i], name) == 0) return i;
    }
    return -1;
}

int ctl_kripke_out_degree(const ctl_kripke *k, ctl_state_id s) {
    if (!k || s >= k->nstates) return 0;
    int count = 0;
    for (ctl_edge *e = k->successors[s]; e; e = e->next) count++;
    return count;
}

int ctl_kripke_in_degree(const ctl_kripke *k, ctl_state_id s) {
    if (!k || s >= k->nstates) return 0;
    int count = 0;
    for (ctl_edge *e = k->predecessors[s]; e; e = e->next) count++;
    return count;
}

ctl_state_id ctl_kripke_successor(const ctl_kripke *k, ctl_state_id s, int i) {
    if (!k || s >= k->nstates || i < 0) return CTL_INVALID_STATE;
    ctl_edge *e = k->successors[s];
    while (e && i > 0) { e = e->next; i--; }
    return e ? e->target : CTL_INVALID_STATE;
}

ctl_state_id ctl_kripke_predecessor(const ctl_kripke *k, ctl_state_id s, int i) {
    if (!k || s >= k->nstates || i < 0) return CTL_INVALID_STATE;
    ctl_edge *e = k->predecessors[s];
    while (e && i > 0) { e = e->next; i--; }
    return e ? e->target : CTL_INVALID_STATE;
}

/* ═══════════════════════════════════════════════════════════════════════
 * State Set Implementation
 * ═══════════════════════════════════════════════════════════════════════ */

ctl_state_set *ctl_state_set_create(int nstates) {
    ctl_state_set *ss = (ctl_state_set *)calloc(1, sizeof(ctl_state_set));
    if (!ss) return NULL;
    ss->nstates = nstates;
    ss->nwords = (nstates + 63) / 64;
    ss->bits = (unsigned long long *)calloc((size_t)ss->nwords,
                                             sizeof(unsigned long long));
    if (!ss->bits) { free(ss); return NULL; }
    return ss;
}

void ctl_state_set_destroy(ctl_state_set *ss) {
    if (!ss) return;
    free(ss->bits);
    free(ss);
}

ctl_state_set *ctl_state_set_clone(const ctl_state_set *ss) {
    if (!ss) return NULL;
    ctl_state_set *clone = ctl_state_set_create(ss->nstates);
    if (!clone) return NULL;
    memcpy(clone->bits, ss->bits, (size_t)ss->nwords * sizeof(unsigned long long));
    return clone;
}

void ctl_state_set_add(ctl_state_set *ss, ctl_state_id s) {
    if (!ss || (int)s >= ss->nstates) return;
    ss->bits[s / 64] |= (1ULL << (s % 64));
}

void ctl_state_set_remove(ctl_state_set *ss, ctl_state_id s) {
    if (!ss || (int)s >= ss->nstates) return;
    ss->bits[s / 64] &= ~(1ULL << (s % 64));
}

int ctl_state_set_contains(const ctl_state_set *ss, ctl_state_id s) {
    if (!ss || (int)s >= ss->nstates) return 0;
    return (int)((ss->bits[s / 64] >> (s % 64)) & 1);
}

void ctl_state_set_clear(ctl_state_set *ss) {
    if (!ss) return;
    memset(ss->bits, 0, (size_t)ss->nwords * sizeof(unsigned long long));
}

void ctl_state_set_universe(ctl_state_set *ss) {
    if (!ss) return;
    for (int i = 0; i < ss->nwords - 1; i++)
        ss->bits[i] = ~0ULL;
    int rem = ss->nstates % 64;
    if (rem == 0)
        ss->bits[ss->nwords - 1] = ~0ULL;
    else
        ss->bits[ss->nwords - 1] = (1ULL << rem) - 1;
}

int ctl_state_set_count(const ctl_state_set *ss) {
    if (!ss) return 0;
    int count = 0;
    for (int i = 0; i < ss->nwords; i++) {
        unsigned long long v = ss->bits[i];
        while (v) { count++; v &= v - 1; }  /* Kernighan's method */
    }
    return count;
}

int ctl_state_set_is_empty(const ctl_state_set *ss) {
    if (!ss) return 1;
    for (int i = 0; i < ss->nwords; i++)
        if (ss->bits[i]) return 0;
    return 1;
}

int ctl_state_set_is_universe(const ctl_state_set *ss) {
    if (!ss) return 0;
    for (int i = 0; i < ss->nwords - 1; i++)
        if (ss->bits[i] != ~0ULL) return 0;
    int rem = ss->nstates % 64;
    unsigned long long last_mask = (rem == 0) ? ~0ULL : (1ULL << rem) - 1;
    return ss->bits[ss->nwords - 1] == last_mask;
}

void ctl_state_set_copy(ctl_state_set *dest, const ctl_state_set *src) {
    if (!dest || !src) return;
    int n = (dest->nwords < src->nwords) ? dest->nwords : src->nwords;
    memcpy(dest->bits, src->bits, (size_t)n * sizeof(unsigned long long));
}

void ctl_state_set_union(ctl_state_set *dest, const ctl_state_set *src) {
    if (!dest || !src) return;
    int n = (dest->nwords < src->nwords) ? dest->nwords : src->nwords;
    for (int i = 0; i < n; i++)
        dest->bits[i] |= src->bits[i];
}

void ctl_state_set_intersect(ctl_state_set *dest, const ctl_state_set *src) {
    if (!dest || !src) return;
    int n = (dest->nwords < src->nwords) ? dest->nwords : src->nwords;
    for (int i = 0; i < n; i++)
        dest->bits[i] &= src->bits[i];
}

void ctl_state_set_difference(ctl_state_set *dest, const ctl_state_set *src) {
    if (!dest || !src) return;
    int n = (dest->nwords < src->nwords) ? dest->nwords : src->nwords;
    for (int i = 0; i < n; i++)
        dest->bits[i] &= ~src->bits[i];
}

void ctl_state_set_complement(ctl_state_set *dest) {
    if (!dest) return;
    for (int i = 0; i < dest->nwords - 1; i++)
        dest->bits[i] = ~dest->bits[i];
    int rem = dest->nstates % 64;
    unsigned long long mask = (rem == 0) ? ~0ULL : (1ULL << rem) - 1;
    dest->bits[dest->nwords - 1] = (~dest->bits[dest->nwords - 1]) & mask;
}

int ctl_state_set_equal(const ctl_state_set *a, const ctl_state_set *b) {
    if (!a || !b) return (a == b);
    int n = (a->nwords < b->nwords) ? a->nwords : b->nwords;
    for (int i = 0; i < n; i++)
        if (a->bits[i] != b->bits[i]) return 0;
    return 1;
}

int ctl_state_set_subset(const ctl_state_set *a, const ctl_state_set *b) {
    if (!a || !b) return 0;
    int n = (a->nwords < b->nwords) ? a->nwords : b->nwords;
    for (int i = 0; i < n; i++)
        if ((a->bits[i] & ~b->bits[i]) != 0) return 0;
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Pre-Image Operations
 * ═══════════════════════════════════════════════════════════════════════ */

void ctl_pre_image(const ctl_kripke *k, ctl_state_set *result,
                   const ctl_state_set *target) {
    if (!k || !result || !target) return;
    ctl_state_set_clear(result);
    for (ctl_state_id s = 0; s < k->nstates; s++) {
        for (ctl_edge *e = k->successors[s]; e; e = e->next) {
            if (ctl_state_set_contains(target, e->target)) {
                ctl_state_set_add(result, s);
                break;
            }
        }
    }
}

void ctl_pre_image_all(const ctl_kripke *k, ctl_state_set *result,
                       const ctl_state_set *target) {
    if (!k || !result || !target) return;
    ctl_state_set_clear(result);
    for (ctl_state_id s = 0; s < k->nstates; s++) {
        int out_deg = 0;
        int all_in = 1;
        for (ctl_edge *e = k->successors[s]; e; e = e->next) {
            out_deg++;
            if (!ctl_state_set_contains(target, e->target)) {
                all_in = 0;
                break;
            }
        }
        if (out_deg > 0 && all_in)
            ctl_state_set_add(result, s);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * Reachability (BFS)
 * ═══════════════════════════════════════════════════════════════════════ */

ctl_state_set *ctl_reachable_states(const ctl_kripke *k) {
    if (!k) return NULL;
    ctl_state_set *visited = ctl_state_set_create((int)k->nstates);
    if (!visited) return NULL;

    /* BFS queue (simple array) */
    ctl_state_id *queue = (ctl_state_id *)malloc(k->nstates * sizeof(ctl_state_id));
    if (!queue) { ctl_state_set_destroy(visited); return NULL; }
    int head = 0, tail = 0;

    for (ctl_state_id i = 0; i < k->ninitial; i++) {
        ctl_state_id s = k->initial_states[i];
        if (!ctl_state_set_contains(visited, s)) {
            ctl_state_set_add(visited, s);
            queue[tail++] = s;
        }
    }

    while (head < tail) {
        ctl_state_id cur = queue[head++];
        for (ctl_edge *e = k->successors[cur]; e; e = e->next) {
            if (!ctl_state_set_contains(visited, e->target)) {
                ctl_state_set_add(visited, e->target);
                queue[tail++] = e->target;
            }
        }
    }

    free(queue);
    return visited;
}

ctl_state_set *ctl_backward_reachable(const ctl_kripke *k,
                                       const ctl_state_set *target) {
    if (!k || !target) return NULL;
    ctl_state_set *visited = ctl_state_set_clone(target);
    if (!visited) return NULL;

    ctl_state_id *queue = (ctl_state_id *)malloc(k->nstates * sizeof(ctl_state_id));
    if (!queue) { ctl_state_set_destroy(visited); return NULL; }
    int head = 0, tail = 0;

    for (ctl_state_id s = 0; s < k->nstates; s++) {
        if (ctl_state_set_contains(target, s)) queue[tail++] = s;
    }

    while (head < tail) {
        ctl_state_id cur = queue[head++];
        for (ctl_edge *e = k->predecessors[cur]; e; e = e->next) {
            if (!ctl_state_set_contains(visited, e->target)) {
                ctl_state_set_add(visited, e->target);
                queue[tail++] = e->target;
            }
        }
    }

    free(queue);
    return visited;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Tarjan's SCC Algorithm
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    int *index;
    int *lowlink;
    int *onstack;
    int *stack;
    int stack_top;
    int idx;
    int *scc_of;
    int nscc;
} scc_state;

static void strongconnect(const ctl_kripke *k, ctl_state_id v, scc_state *st) {
    st->index[v] = st->idx;
    st->lowlink[v] = st->idx;
    st->idx++;
    st->stack[st->stack_top++] = (int)v;
    st->onstack[v] = 1;

    for (ctl_edge *e = k->successors[v]; e; e = e->next) {
        ctl_state_id w = e->target;
        if (st->index[w] == -1) {
            strongconnect(k, w, st);
            st->lowlink[v] = (st->lowlink[v] < st->lowlink[w]) ?
                              st->lowlink[v] : st->lowlink[w];
        } else if (st->onstack[w]) {
            st->lowlink[v] = (st->lowlink[v] < st->index[w]) ?
                              st->lowlink[v] : st->index[w];
        }
    }

    if (st->lowlink[v] == st->index[v]) {
        int w;
        do {
            w = st->stack[--st->stack_top];
            st->onstack[w] = 0;
            st->scc_of[w] = st->nscc;
        } while (w != (int)v);
        st->nscc++;
    }
}

int ctl_compute_sccs(const ctl_kripke *k, int **scc_of) {
    if (!k || !scc_of) return -1;
    int n = (int)k->nstates;

    scc_state st;
    st.index = (int *)malloc((size_t)n * sizeof(int));
    st.lowlink = (int *)malloc((size_t)n * sizeof(int));
    st.onstack = (int *)calloc((size_t)n, sizeof(int));
    st.stack = (int *)malloc((size_t)n * sizeof(int));
    st.scc_of = (int *)malloc((size_t)n * sizeof(int));
    st.stack_top = 0;
    st.idx = 0;
    st.nscc = 0;

    if (!st.index || !st.lowlink || !st.onstack || !st.stack || !st.scc_of) {
        free(st.index); free(st.lowlink); free(st.onstack);
        free(st.stack); free(st.scc_of);
        return -1;
    }

    for (int i = 0; i < n; i++) {
        st.index[i] = -1;
        st.lowlink[i] = -1;
        st.scc_of[i] = -1;
    }

    for (ctl_state_id v = 0; v < k->nstates; v++) {
        if (st.index[v] == -1) {
            strongconnect(k, v, &st);
        }
    }

    *scc_of = st.scc_of;
    free(st.index);
    free(st.lowlink);
    free(st.onstack);
    free(st.stack);
    return st.nscc;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Bottom SCCs
 * ═══════════════════════════════════════════════════════════════════════ */

ctl_state_set *ctl_bottom_sccs(const ctl_kripke *k) {
    if (!k) return NULL;
    int *scc_of = NULL;
    int nscc = ctl_compute_sccs(k, &scc_of);
    if (nscc < 0) return NULL;

    /* A BSCC has no edges to states in other SCCs */
    int *is_bscc = (int *)calloc((size_t)nscc, sizeof(int));
    if (!is_bscc) { free(scc_of); return NULL; }
    for (int i = 0; i < nscc; i++) is_bscc[i] = 1;

    for (ctl_state_id s = 0; s < k->nstates; s++) {
        for (ctl_edge *e = k->successors[s]; e; e = e->next) {
            if (scc_of[s] != scc_of[e->target]) {
                is_bscc[scc_of[s]] = 0;
            }
        }
    }

    ctl_state_set *result = ctl_state_set_create((int)k->nstates);
    if (!result) { free(is_bscc); free(scc_of); return NULL; }

    for (ctl_state_id s = 0; s < k->nstates; s++) {
        if (is_bscc[scc_of[s]]) ctl_state_set_add(result, s);
    }

    free(is_bscc);
    free(scc_of);
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Cloning
 * ═══════════════════════════════════════════════════════════════════════ */

ctl_kripke *ctl_kripke_clone(const ctl_kripke *k) {
    if (!k) return NULL;
    ctl_kripke *clone = ctl_kripke_create(k->nstates, k->nap,
                                            (const char **)k->ap_names);
    if (!clone) return NULL;

    /* Copy initial states */
    if (k->ninitial > 0) {
        ctl_kripke_set_initial(clone, k->initial_states, k->ninitial);
    }

    /* Copy edges */
    for (ctl_state_id s = 0; s < k->nstates; s++) {
        for (ctl_edge *e = k->successors[s]; e; e = e->next) {
            ctl_kripke_add_edge(clone, s, e->target);
        }
    }

    /* Copy labels */
    for (ctl_state_id s = 0; s < k->nstates; s++) {
        for (int ap = 0; ap < k->nap; ap++) {
            ctl_kripke_set_label(clone, s, ap,
                                  ctl_kripke_get_label(k, s, ap));
        }
    }

    return clone;
}

/* ═══════════════════════════════════════════════════════════════════════
 * I/O
 * ═══════════════════════════════════════════════════════════════════════ */

void ctl_kripke_print_dot(const ctl_kripke *k,
                           const ctl_state_set *highlight) {
    if (!k) return;
    printf("digraph Kripke {\n");
    printf("  rankdir=LR;\n");
    printf("  node [shape=circle];\n");

    for (ctl_state_id s = 0; s < k->nstates; s++) {
        printf("  s%u [label=\"s%u", s, s);
        /* Print atomic propositions true at this state */
        int first = 1;
        for (int ap = 0; ap < k->nap; ap++) {
            if (ctl_kripke_get_label(k, s, ap)) {
                printf("%s%s", first ? "\\n" : ",", k->ap_names[ap]);
                first = 0;
            }
        }
        printf("\"");
        if (highlight && ctl_state_set_contains(highlight, s))
            printf(", style=filled, fillcolor=green");
        /* Mark initial states with double border */
        for (ctl_state_id i = 0; i < k->ninitial; i++) {
            if (k->initial_states[i] == s) {
                printf(", peripheries=2");
                break;
            }
        }
        printf("];\n");
    }

    for (ctl_state_id s = 0; s < k->nstates; s++) {
        for (ctl_edge *e = k->successors[s]; e; e = e->next) {
            printf("  s%u -> s%u;\n", s, e->target);
        }
    }

    printf("}\n");
}

void ctl_kripke_print(const ctl_kripke *k) {
    if (!k) { printf("Kripke(NULL)\n"); return; }
    printf("Kripke structure: %u states, %u initial, %d APs\n",
           k->nstates, k->ninitial, k->nap);
    printf("Initial states:");
    for (ctl_state_id i = 0; i < k->ninitial; i++)
        printf(" s%u", k->initial_states[i]);
    printf("\n");
    for (ctl_state_id s = 0; s < k->nstates; s++) {
        printf("  s%u: {", s);
        int first = 1;
        for (int ap = 0; ap < k->nap; ap++) {
            if (ctl_kripke_get_label(k, s, ap)) {
                printf("%s%s", first ? "" : ",", k->ap_names[ap]);
                first = 0;
            }
        }
        printf("} -> {");
        first = 1;
        for (ctl_edge *e = k->successors[s]; e; e = e->next) {
            printf("%ss%u", first ? "" : ",", e->target);
            first = 0;
        }
        printf("}\n");
    }
}
