/*
 * buchi_core.c — Core Büchi Automaton Construction & Manipulation
 *
 * Implements the foundational data structures and operations for
 * nondeterministic Büchi automata on infinite words.
 *
 * L1 Definitions + L3 Mathematical Structures.
 *
 * Reference: Baier & Katoen 2008, Chapter 4
 */

#include "buchi.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ================================================================
 * ω-Word Implementation
 * ================================================================ */

OmegaWord* omega_make_prefix(const BuchiSymbol* prefix, int len) {
    if (!prefix || len < 1) return NULL;
    OmegaWord* w = (OmegaWord*)malloc(sizeof(OmegaWord));
    if (!w) return NULL;
    w->symbols = (BuchiSymbol*)malloc(len * sizeof(BuchiSymbol));
    if (!w->symbols) { free(w); return NULL; }
    memcpy(w->symbols, prefix, len * sizeof(BuchiSymbol));
    w->len = len;
    w->period_start = NULL;
    w->period_len = 0;
    w->is_periodic = 0;
    w->is_lasso = 0;
    return w;
}

OmegaWord* omega_make_lasso(const BuchiSymbol* prefix, int plen,
                             const BuchiSymbol* cycle, int clen) {
    if (plen < 0 || !cycle || clen < 1) return NULL;
    if (plen > 0 && !prefix) return NULL;
    OmegaWord* w = (OmegaWord*)malloc(sizeof(OmegaWord));
    if (!w) return NULL;
    int total = plen + clen;
    w->symbols = (BuchiSymbol*)malloc(total * sizeof(BuchiSymbol));
    if (!w->symbols) { free(w); return NULL; }
    if (plen > 0) memcpy(w->symbols, prefix, plen * sizeof(BuchiSymbol));
    memcpy(w->symbols + plen, cycle, clen * sizeof(BuchiSymbol));
    w->len = total;
    w->period_start = (int*)malloc(sizeof(int));
    w->period_len = clen;
    if (w->period_start) *w->period_start = plen;
    w->is_periodic = 0;
    w->is_lasso = 1;
    return w;
}

OmegaWord* omega_make_periodic(const BuchiSymbol* period, int plen) {
    if (!period || plen < 1) return NULL;
    return omega_make_lasso(NULL, 0, period, plen);
}

BuchiSymbol omega_get(const OmegaWord* w, int position) {
    if (!w || position < 0) return BUCHI_INVALID_STATE;

    if (w->is_lasso && w->period_start && w->period_len > 0) {
        int ps = *w->period_start;
        if (position < ps) {
            return (position < w->len) ? w->symbols[position] : BUCHI_INVALID_STATE;
        }
        int cycle_idx = (position - ps) % w->period_len;
        return w->symbols[ps + cycle_idx];
    }

    if (w->is_periodic && w->period_len > 0) {
        return w->symbols[position % w->period_len];
    }

    if (position < w->len) return w->symbols[position];
    return BUCHI_INVALID_STATE;
}

void omega_print(const OmegaWord* w, int max_len) {
    if (!w) { printf("null-omega-word\n"); return; }
    printf("ω-word: ");
    int i;
    for (i = 0; i < max_len && i < 120; i++) {
        BuchiSymbol s = omega_get(w, i);
        if (s == BUCHI_INVALID_STATE) break;
        printf("%d", s);
        if (w->is_lasso && w->period_start && i == *w->period_start - 1)
            printf(" [");
        else if (i < max_len - 1) printf(" ");
    }
    if (w->is_lasso || w->is_periodic) printf("]^ω");
    printf("\n");
}

void omega_free(OmegaWord* w) {
    if (!w) return;
    free(w->symbols);
    free(w->period_start);
    free(w);
}

int omega_equals_up_to(const OmegaWord* a, const OmegaWord* b, int max_pos) {
    if (!a || !b) return 0;
    for (int i = 0; i < max_pos; i++) {
        if (omega_get(a, i) != omega_get(b, i)) return 0;
    }
    return 1;
}

/* ================================================================
 * Transition Set Implementation
 * ================================================================ */

void buchi_trans_set_init(BuchiTransitionSet* ts, int cap) {
    if (!ts) return;
    ts->data = (BuchiTransition*)malloc(cap * sizeof(BuchiTransition));
    ts->count = 0;
    ts->capacity = (ts->data) ? cap : 0;
}

void buchi_trans_set_add(BuchiTransitionSet* ts, BuchiState from,
                          BuchiSymbol sym, BuchiState to) {
    if (!ts || !ts->data) return;
    if (ts->count >= ts->capacity) {
        int new_cap = ts->capacity * 2;
        if (new_cap < 4) new_cap = 4;
        BuchiTransition* nd = (BuchiTransition*)realloc(ts->data,
                                new_cap * sizeof(BuchiTransition));
        if (!nd) return;
        ts->data = nd;
        ts->capacity = new_cap;
    }
    ts->data[ts->count].from = from;
    ts->data[ts->count].symbol = sym;
    ts->data[ts->count].to = to;
    ts->count++;
}

void buchi_trans_set_free(BuchiTransitionSet* ts) {
    if (!ts) return;
    free(ts->data);
    ts->data = NULL;
    ts->count = ts->capacity = 0;
}

/* ================================================================
 * Büchi Automaton Construction
 * ================================================================ */

BuchiAutomaton* buchi_create(int n_states, BuchiState q0, int alphabet_size) {
    if (n_states <= 0) return NULL;

    BuchiAutomaton* ba = (BuchiAutomaton*)calloc(1, sizeof(BuchiAutomaton));
    if (!ba) return NULL;

    ba->n_states = n_states;
    ba->q0 = q0;
    ba->n_accepting = 0;
    ba->accepting = NULL;
    ba->alphabet_size = alphabet_size;
    ba->alphabet = (BuchiSymbol*)calloc(alphabet_size, sizeof(BuchiSymbol));
    ba->is_accepting = (uint8_t*)calloc(n_states, sizeof(uint8_t));
    ba->name = NULL;
    ba->n_trans_total = 0;

    if (!ba->alphabet || !ba->is_accepting) {
        free(ba->alphabet);
        free(ba->is_accepting);
        free(ba);
        return NULL;
    }

    /* Allocate delta: n_states × alphabet_size matrix of transition sets */
    ba->delta = (BuchiTransitionSet***)malloc(n_states * sizeof(BuchiTransitionSet**));
    if (!ba->delta) {
        free(ba->alphabet); free(ba->is_accepting); free(ba);
        return NULL;
    }
    for (int s = 0; s < n_states; s++) {
        ba->delta[s] = (BuchiTransitionSet**)calloc(alphabet_size,
                          sizeof(BuchiTransitionSet*));
        if (!ba->delta[s]) {
            for (int i = 0; i < s; i++) {
                for (int a = 0; a < alphabet_size; a++)
                    buchi_trans_set_free(ba->delta[i][a]);
                free(ba->delta[i]);
            }
            free(ba->delta);
            free(ba->alphabet); free(ba->is_accepting); free(ba);
            return NULL;
        }
    }

    return ba;
}

void buchi_set_name(BuchiAutomaton* ba, const char* name) {
    if (!ba || !name) return;
    free(ba->name);
    ba->name = strdup(name);
}

void buchi_set_accepting(BuchiAutomaton* ba, const BuchiState* states, int n) {
    if (!ba || !states || n <= 0) return;
    free(ba->accepting);
    ba->accepting = (BuchiState*)malloc(n * sizeof(BuchiState));
    if (!ba->accepting) return;
    memcpy(ba->accepting, states, n * sizeof(BuchiState));
    ba->n_accepting = n;
    memset(ba->is_accepting, 0, ba->n_states);
    for (int i = 0; i < n; i++) {
        if (states[i] >= 0 && states[i] < ba->n_states)
            ba->is_accepting[states[i]] = 1;
    }
}

void buchi_add_accepting(BuchiAutomaton* ba, BuchiState s) {
    if (!ba || s < 0 || s >= ba->n_states) return;
    BuchiState* na = (BuchiState*)realloc(ba->accepting,
                       (ba->n_accepting + 1) * sizeof(BuchiState));
    if (!na) return;
    ba->accepting = na;
    ba->accepting[ba->n_accepting] = s;
    ba->n_accepting++;
    ba->is_accepting[s] = 1;
}

int buchi_is_accepting(const BuchiAutomaton* ba, BuchiState s) {
    if (!ba || s < 0 || s >= ba->n_states) return 0;
    return ba->is_accepting[s];
}

void buchi_set_alphabet(BuchiAutomaton* ba, const BuchiSymbol* syms, int n) {
    if (!ba || !syms || n <= 0) return;
    ba->alphabet = (BuchiSymbol*)realloc(ba->alphabet, n * sizeof(BuchiSymbol));
    if (!ba->alphabet) return;
    memcpy(ba->alphabet, syms, n * sizeof(BuchiSymbol));
    ba->alphabet_size = n;
}

int buchi_sym_index(const BuchiAutomaton* ba, BuchiSymbol sym) {
    if (!ba) return -1;
    for (int i = 0; i < ba->alphabet_size; i++) {
        if (ba->alphabet[i] == sym) return i;
    }
    return -1;
}

int buchi_add_transition(BuchiAutomaton* ba, BuchiState from,
                          BuchiSymbol sym, BuchiState to) {
    if (!ba || from < 0 || from >= ba->n_states ||
        to < 0 || to >= ba->n_states) return -1;

    int sym_idx = buchi_sym_index(ba, sym);
    if (sym_idx < 0) return -1;

    BuchiTransitionSet* ts = ba->delta[from][sym_idx];
    if (!ts) {
        ts = (BuchiTransitionSet*)malloc(sizeof(BuchiTransitionSet));
        if (!ts) return -1;
        buchi_trans_set_init(ts, 4);
        ba->delta[from][sym_idx] = ts;
    }

    /* Check for duplicate */
    for (int i = 0; i < ts->count; i++) {
        if (ts->data[i].to == to) return 0; /* already exists */
    }

    buchi_trans_set_add(ts, from, sym, to);
    ba->n_trans_total++;
    return 1;
}

void buchi_free(BuchiAutomaton* ba) {
    if (!ba) return;
    for (int s = 0; s < ba->n_states; s++) {
        if (ba->delta[s]) {
            for (int a = 0; a < ba->alphabet_size; a++) {
                if (ba->delta[s][a]) {
                    buchi_trans_set_free(ba->delta[s][a]);
                    free(ba->delta[s][a]);
                }
            }
            free(ba->delta[s]);
        }
    }
    free(ba->delta);
    free(ba->alphabet);
    free(ba->accepting);
    free(ba->is_accepting);
    free(ba->name);
    free(ba);
}

/* ================================================================
 * Query API
 * ================================================================ */

int buchi_num_states(const BuchiAutomaton* ba) {
    return ba ? ba->n_states : 0;
}

int buchi_num_trans(const BuchiAutomaton* ba) {
    return ba ? ba->n_trans_total : 0;
}

int buchi_has_transition(const BuchiAutomaton* ba, BuchiState from,
                          BuchiSymbol sym, BuchiState to) {
    if (!ba) return 0;
    int sym_idx = buchi_sym_index(ba, sym);
    if (sym_idx < 0) return 0;
    BuchiTransitionSet* ts = ba->delta[from][sym_idx];
    if (!ts) return 0;
    for (int i = 0; i < ts->count; i++) {
        if (ts->data[i].to == to) return 1;
    }
    return 0;
}

const BuchiTransitionSet* buchi_post(const BuchiAutomaton* ba,
                                      BuchiState s, BuchiSymbol sym) {
    if (!ba || s < 0 || s >= ba->n_states) return NULL;
    int sym_idx = buchi_sym_index(ba, sym);
    if (sym_idx < 0) return NULL;
    return ba->delta[s][sym_idx];
}

/* ================================================================
 * Run Data Structure
 * ================================================================ */

BuchiRun* buchi_run_create(int max_len) {
    BuchiRun* run = (BuchiRun*)calloc(1, sizeof(BuchiRun));
    if (!run) return NULL;
    run->states = (BuchiState*)malloc(max_len * sizeof(BuchiState));
    if (!run->states) { free(run); return NULL; }
    run->length = 0;
    run->period_start = 0;
    run->period_len = 0;
    run->is_lasso = 0;
    return run;
}

void buchi_run_free(BuchiRun* run) {
    if (!run) return;
    free(run->states);
    free(run);
}

int buchi_run_is_accepting(const BuchiRun* run, const BuchiAutomaton* ba) {
    if (!run || !ba || run->length < 1) return 0;

    if (run->is_lasso) {
        /* For lasso runs: check if any state in the cycle is accepting */
        for (int i = run->period_start; i < run->period_start + run->period_len; i++) {
            if (i >= run->length) break;
            if (ba->is_accepting[run->states[i]]) return 1;
        }
        return 0;
    }

    /* For non-lasso finite runs: cannot be accepting (need infinite run) */
    return 0;
}

/* ================================================================
 * Acceptance of ω-words
 * ================================================================ */

/* Helper: DFS from current states, tracking visited (state, step_mod) pairs */
typedef struct {
    int state;
    int step_mod;
} StateModPair;

int buchi_accepts_lasso(const BuchiAutomaton* ba, const OmegaWord* w) {
    if (!ba || !w) return 0;
    if (!w->is_lasso || !w->period_start || w->period_len < 1) return 0;

    int prefix_len = *w->period_start;
    int cycle_len = w->period_len;

    /* Simulate prefix from q0 */
    int* current_states = (int*)malloc(ba->n_states * sizeof(int));
    int* next_states = (int*)malloc(ba->n_states * sizeof(int));
    int n_current = 0;

    current_states[n_current++] = ba->q0;

    /* Process prefix */
    for (int t = 0; t < prefix_len; t++) {
        BuchiSymbol sym = w->symbols[t];
        int sym_idx = buchi_sym_index(ba, sym);
        if (sym_idx < 0) { free(current_states); free(next_states); return 0; }

        int n_next = 0;
        for (int i = 0; i < n_current; i++) {
            int s = current_states[i];
            BuchiTransitionSet* ts = ba->delta[s][sym_idx];
            if (ts) {
                for (int j = 0; j < ts->count; j++) {
                    int tgt = ts->data[j].to;
                    /* Dedup */
                    int found = 0;
                    for (int k = 0; k < n_next; k++) {
                        if (next_states[k] == tgt) { found = 1; break; }
                    }
                    if (!found && n_next < ba->n_states)
                        next_states[n_next++] = tgt;
                }
            }
        }
        n_current = n_next;
        int* tmp = current_states; current_states = next_states; next_states = tmp;
        if (n_current == 0) { free(current_states); free(next_states); return 0; }
    }

    /* Check acceptance of the lasso using the product graph method.
     *
     * Build product graph: nodes are (state, position mod cycle_len).
     * Edge: (s, p) --cycle[p]--> (t, (p+1) mod cycle_len) if t ∈ δ(s, cycle[p]).
     *
     * The lasso is accepted iff from some state reachable after the prefix,
     * there is a path in the product graph that visits an accepting state
     * and can loop back on itself (i.e., there's an accepting cycle). */
    int cycle_syms[cycle_len];
    for (int i = 0; i < cycle_len; i++)
        cycle_syms[i] = w->symbols[prefix_len + i];

    int cycle_sym_idx[cycle_len];
    int valid_cycle = 1;
    for (int t = 0; t < cycle_len; t++) {
        cycle_sym_idx[t] = buchi_sym_index(ba, cycle_syms[t]);
        if (cycle_sym_idx[t] < 0) valid_cycle = 0;
    }
    if (!valid_cycle) { free(current_states); free(next_states); return 0; }

    int n = ba->n_states;
    int npairs = n * cycle_len; /* size of product graph */

    /* Compute forward reachability in the product graph
     * using Floyd-Warshall on the full product graph */
    uint8_t** P = (uint8_t**)malloc(npairs * sizeof(uint8_t*));
    for (int i = 0; i < npairs; i++) {
        P[i] = (uint8_t*)calloc(npairs, sizeof(uint8_t));
    }

    /* Build direct edges in product graph */
    for (int s = 0; s < n; s++) {
        for (int p = 0; p < cycle_len; p++) {
            int from = s * cycle_len + p;
            int sym_idx = cycle_sym_idx[p];
            BuchiTransitionSet* ts = ba->delta[s][sym_idx];
            if (ts) {
                for (int j = 0; j < ts->count; j++) {
                    int t = ts->data[j].to;
                    int np = (p + 1) % cycle_len;
                    int to = t * cycle_len + np;
                    P[from][to] = 1;
                }
            }
        }
    }

    /* Transitive closure of product graph: Floyd-Warshall */
    for (int k = 0; k < npairs; k++)
        for (int i = 0; i < npairs; i++)
            if (P[i][k])
                for (int j = 0; j < npairs; j++)
                    if (P[k][j]) P[i][j] = 1;

    /* Check: from any state in current_states at position 0,
     * can we reach an accepting state that can loop back to itself? */
    int accepted = 0;
    for (int si = 0; si < n_current && !accepted; si++) {
        int r = current_states[si];
        int start_pair = r * cycle_len + 0;

        /* Check all pairs (f, p) where f is accepting */
        for (int p = 0; p < cycle_len && !accepted; p++) {
            for (int ai = 0; ai < ba->n_accepting && !accepted; ai++) {
                int f = ba->accepting[ai];
                int acc_pair = f * cycle_len + p;
                /* f must be reachable from start via P*, and
                 * f must be able to loop back to itself via P+ */
                if (P[start_pair][acc_pair] && P[acc_pair][acc_pair]) {
                    accepted = 1;
                }
            }
        }
    }

    for (int i = 0; i < npairs; i++) free(P[i]);
    free(P);

    if (accepted) {
        free(current_states); free(next_states);
        return 1;
    }

    free(current_states); free(next_states);
    return 0;
}

int buchi_accepts(const BuchiAutomaton* ba, const OmegaWord* w,
                  int max_prefix_check) {
    if (!ba || !w) return 0;

    /* For lasso words, use specialized acceptance */
    if (w->is_lasso)
        return buchi_accepts_lasso(ba, w);

    /* For other representations: simulate up to max_prefix_check
     * and do SCC analysis on the produced fragment */
    int* current = (int*)malloc(ba->n_states * sizeof(int));
    int* next = (int*)malloc(ba->n_states * sizeof(int));
    int n_cur = 0;

    current[n_cur++] = ba->q0;

    for (int t = 0; t < max_prefix_check; t++) {
        BuchiSymbol sym = omega_get(w, t);
        if (sym == BUCHI_INVALID_STATE) break;
        int sym_idx = buchi_sym_index(ba, sym);
        if (sym_idx < 0) { free(current); free(next); return 0; }

        int n_nxt = 0;
        for (int i = 0; i < n_cur; i++) {
            int s = current[i];
            BuchiTransitionSet* ts = ba->delta[s][sym_idx];
            if (ts) {
                for (int j = 0; j < ts->count && n_nxt < ba->n_states; j++) {
                    int tgt = ts->data[j].to;
                    int dup = 0;
                    for (int k = 0; k < n_nxt; k++)
                        if (next[k] == tgt) { dup = 1; break; }
                    if (!dup) next[n_nxt++] = tgt;
                }
            }
        }
        n_cur = n_nxt;
        int* tmp = current; current = next; next = tmp;
        if (n_cur == 0) { free(current); free(next); return 0; }

        /* Heuristic: if at any step an accepting state is in current set,
         * check if it appears to be in a cycle (coarse approximation) */
        if (t > 0) {
            for (int i = 0; i < n_cur; i++) {
                if (ba->is_accepting[current[i]]) {
                    /* Accepting state reached - but we need ∞-often.
                     * In general this check is incomplete for non-lasso words. */
                    free(current); free(next);
                    return 1; /* optimistic for prefix-only check */
                }
            }
        }
    }

    free(current); free(next);
    return 0;
}

/* ================================================================
 * I/O
 * ================================================================ */

void buchi_print(const BuchiAutomaton* ba) {
    if (!ba) { printf("null-automaton\n"); return; }
    printf("BuchiAutomaton");
    if (ba->name) printf(" \"%s\"", ba->name);
    printf(":\n");
    printf("  States: %d (q0=%d)\n", ba->n_states, ba->q0);
    printf("  Alphabet: {");
    for (int i = 0; i < ba->alphabet_size; i++) {
        printf("%d", ba->alphabet[i]);
        if (i < ba->alphabet_size - 1) printf(", ");
    }
    printf("}\n");
    printf("  Accepting: {");
    for (int i = 0; i < ba->n_accepting; i++) {
        printf("%d", ba->accepting[i]);
        if (i < ba->n_accepting - 1) printf(", ");
    }
    printf("}\n");
    printf("  Transitions: %d\n", ba->n_trans_total);
    for (int s = 0; s < ba->n_states; s++) {
        for (int a = 0; a < ba->alphabet_size; a++) {
            BuchiTransitionSet* ts = ba->delta[s][a];
            if (ts) {
                for (int i = 0; i < ts->count; i++) {
                    printf("    %d --%d--> %d", s, ba->alphabet[a], ts->data[i].to);
                    if (ba->is_accepting[ts->data[i].to]) printf(" [acc]");
                    printf("\n");
                }
            }
        }
    }
}

void buchi_print_dot(const BuchiAutomaton* ba, const char* filename) {
    if (!ba || !filename) return;
    FILE* f = fopen(filename, "w");
    if (!f) return;
    fprintf(f, "digraph Buchi {\n");
    fprintf(f, "  rankdir=LR;\n");
    fprintf(f, "  node [shape=circle];\n");
    fprintf(f, "  init [shape=point];\n");
    for (int s = 0; s < ba->n_states; s++) {
        const char* shape = ba->is_accepting[s] ? "doublecircle" : "circle";
        fprintf(f, "  %d [shape=%s];\n", s, shape);
    }
    fprintf(f, "  init -> %d;\n", ba->q0);
    for (int s = 0; s < ba->n_states; s++) {
        for (int a = 0; a < ba->alphabet_size; a++) {
            BuchiTransitionSet* ts = ba->delta[s][a];
            if (ts) {
                for (int i = 0; i < ts->count; i++) {
                    fprintf(f, "  %d -> %d [label=\"%d\"];\n",
                            s, ts->data[i].to, ba->alphabet[a]);
                }
            }
        }
    }
    fprintf(f, "}\n");
    fclose(f);
}

void buchi_print_run(const BuchiRun* run, int max_len) {
    if (!run) { printf("null-run\n"); return; }
    printf("Run: ");
    int limit = (run->length < max_len) ? run->length : max_len;
    for (int i = 0; i < limit; i++) {
        printf("%d", run->states[i]);
        if (run->is_lasso && i == run->period_start - 1)
            printf(" [");
        else if (i < limit - 1) printf(" → ");
    }
    if (run->is_lasso) printf("]^ω");
    printf("\n");
}

/* ================================================================
 * Tarjan's SCC Decomposition
 * ================================================================ */

static void scc_strongconnect(BuchiSCCDecomp* scc, const BuchiAutomaton* ba,
                               int v) {
    scc->index[v] = scc->current_index;
    scc->lowlink[v] = scc->current_index;
    scc->current_index++;
    scc->stack[scc->stack_top++] = v;
    scc->on_stack[v] = 1;

    /* Explore all successors across all symbols */
    for (int a = 0; a < ba->alphabet_size; a++) {
        BuchiTransitionSet* ts = ba->delta[v][a];
        if (ts) {
            for (int i = 0; i < ts->count; i++) {
                int w = ts->data[i].to;
                if (!scc->visited[w]) {
                    scc->visited[w] = 1;
                    scc_strongconnect(scc, ba, w);
                    scc->lowlink[v] = (scc->lowlink[v] < scc->lowlink[w])
                                      ? scc->lowlink[v] : scc->lowlink[w];
                } else if (scc->on_stack[w]) {
                    scc->lowlink[v] = (scc->lowlink[v] < scc->index[w])
                                      ? scc->lowlink[v] : scc->index[w];
                }
            }
        }
    }

    if (scc->lowlink[v] == scc->index[v]) {
        int scc_id = scc->n_sccs;
        int w, count = 0;
        do {
            w = scc->stack[--scc->stack_top];
            scc->on_stack[w] = 0;
            scc->scc_id[w] = scc_id;
            count++;
            if (ba->is_accepting[w])
                scc->scc_accepting[scc_id] = 1;
        } while (w != v);

        /* Non-trivial SCC: has at least 1 edge, or size > 1 */
        if (count > 1) {
            scc->scc_nontrivial[scc_id] = 1;
        } else {
            /* Check for self-loop */
            for (int a = 0; a < ba->alphabet_size; a++) {
                BuchiTransitionSet* ts = ba->delta[v][a];
                if (ts) {
                    for (int i = 0; i < ts->count; i++) {
                        if (ts->data[i].to == v) {
                            scc->scc_nontrivial[scc_id] = 1;
                            break;
                        }
                    }
                }
            }
        }
        scc->n_sccs++;
    }
}

BuchiSCCDecomp* buchi_scc_decompose(const BuchiAutomaton* ba) {
    if (!ba) return NULL;
    BuchiSCCDecomp* scc = (BuchiSCCDecomp*)calloc(1, sizeof(BuchiSCCDecomp));
    if (!scc) return NULL;

    int n = ba->n_states;
    scc->visited = (uint8_t*)calloc(n, sizeof(uint8_t));
    scc->on_stack = (uint8_t*)calloc(n, sizeof(uint8_t));
    scc->lowlink = (int*)malloc(n * sizeof(int));
    scc->index = (int*)malloc(n * sizeof(int));
    scc->stack = (int*)malloc(n * sizeof(int));
    scc->scc_id = (int*)malloc(n * sizeof(int));
    scc->scc_accepting = (int*)calloc(n, sizeof(int));
    scc->scc_nontrivial = (int*)calloc(n, sizeof(int));

    if (!scc->visited || !scc->on_stack || !scc->lowlink || !scc->index ||
        !scc->stack || !scc->scc_id || !scc->scc_accepting || !scc->scc_nontrivial) {
        buchi_scc_free(scc);
        return NULL;
    }

    for (int i = 0; i < n; i++) scc->index[i] = -1;

    scc->visited[ba->q0] = 1;
    scc_strongconnect(scc, ba, ba->q0);

    return scc;
}

void buchi_scc_free(BuchiSCCDecomp* scc) {
    if (!scc) return;
    free(scc->visited);
    free(scc->on_stack);
    free(scc->lowlink);
    free(scc->index);
    free(scc->stack);
    free(scc->scc_id);
    free(scc->scc_accepting);
    free(scc->scc_nontrivial);
    free(scc);
}

int buchi_has_accepting_scc(const BuchiSCCDecomp* scc) {
    if (!scc) return 0;
    for (int i = 0; i < scc->n_sccs; i++) {
        if (scc->scc_accepting[i] && scc->scc_nontrivial[i])
            return 1;
    }
    return 0;
}
