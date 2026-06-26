/*
 * buchi_emptiness.c — Emptiness Checking for Büchi Automata
 *
 * L4 Fundamental Law: NBA emptiness is NLOGSPACE-complete
 *   (equivalently, decidable in linear time via SCC decomposition).
 *
 * L5 Algorithms:
 *   - SCC-based (Tarjan): decompose graph, check accepting non-trivial SCC
 *   - Nested DFS (Courcoubetis et al. 1992): on-the-fly counterexample
 *   - Generalized Büchi emptiness: SCC intersecting all acceptance sets
 *
 * Reference:
 *   Courcoubetis, Vardi, Wolper, Yannakakis (FMSD 1992)
 *   Tauriainen (2004) — Nested Emptiness Search for GBA
 *   Baier & Katoen 2008 — Principles of Model Checking, §4.4
 */

#include "buchi_emptiness.h"
#include "buchi.h"
#include "omega_operations.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ================================================================
 * SCC-based Emptiness
 * ================================================================ */

int buchi_is_empty(const BuchiAutomaton* A) {
    if (!A) return 1;
    BuchiSCCDecomp* scc = buchi_scc_decompose(A);
    if (!scc) return 1;
    int empty = !buchi_has_accepting_scc(scc);
    buchi_scc_free(scc);
    return empty;
}

/* BFS from q0 to find path, then BFS within SCC for cycle */
OmegaWord* buchi_find_accepting_lasso(const BuchiAutomaton* A) {
    if (!A || buchi_is_empty(A)) return NULL;

    int n = A->n_states;

    /* Phase 1: BFS distance from q0 */
    int* dist = (int*)malloc(n * sizeof(int));
    int* pred_state = (int*)malloc(n * sizeof(int));
    int* pred_sym = (int*)malloc(n * sizeof(int));
    int* queue = (int*)malloc(n * sizeof(int));
    if (!dist || !pred_state || !pred_sym || !queue) {
        free(dist); free(pred_state); free(pred_sym); free(queue);
        return NULL;
    }
    for (int i = 0; i < n; i++) { dist[i] = -1; pred_state[i] = -1; pred_sym[i] = -1; }

    int qhead = 0, qtail = 0;
    dist[A->q0] = 0;
    queue[qtail++] = A->q0;

    while (qhead < qtail) {
        int v = queue[qhead++];
        for (int a = 0; a < A->alphabet_size; a++) {
            BuchiTransitionSet* ts = A->delta[v][a];
            if (ts) {
                for (int i = 0; i < ts->count; i++) {
                    int w = ts->data[i].to;
                    if (dist[w] < 0) {
                        dist[w] = dist[v] + 1;
                        pred_state[w] = v;
                        pred_sym[w] = A->alphabet[a];
                        queue[qtail++] = w;
                    }
                }
            }
        }
    }

    /* Phase 2: Find accepting state reachable and on cycle */
    BuchiSCCDecomp* scc = buchi_scc_decompose(A);
    if (!scc) { free(dist); free(pred_state); free(pred_sym); free(queue); return NULL; }

    int target_scc = -1;
    for (int i = 0; i < scc->n_sccs; i++) {
        if (scc->scc_accepting[i] && scc->scc_nontrivial[i]) {
            target_scc = i;
            break;
        }
    }

    if (target_scc < 0) {
        buchi_scc_free(scc);
        free(dist); free(pred_state); free(pred_sym); free(queue);
        return NULL;
    }

    /* Find a reachable accepting state in target_scc */
    int acc_state = -1;
    for (int s = 0; s < n; s++) {
        if (scc->scc_id[s] == target_scc && A->is_accepting[s] && dist[s] >= 0) {
            acc_state = s;
            break;
        }
    }
    if (acc_state < 0) {
        buchi_scc_free(scc);
        free(dist); free(pred_state); free(pred_sym); free(queue);
        return NULL;
    }

    /* Phase 3: Reconstruct prefix path (reverse) */
    int prefix_len = dist[acc_state];
    BuchiSymbol* prefix = (BuchiSymbol*)malloc(prefix_len * sizeof(BuchiSymbol));
    int cur = acc_state;
    for (int i = prefix_len - 1; i >= 0; i--) {
        prefix[i] = pred_sym[cur];
        cur = pred_state[cur];
    }

    /* Phase 4: Find cycle from acc_state to acc_state */
    /* BFS within SCC */
    int* cycle_dist = (int*)malloc(n * sizeof(int));
    int* cycle_pred_state = (int*)malloc(n * sizeof(int));
    int* cycle_pred_sym = (int*)malloc(n * sizeof(int));
    int* cycle_queue = (int*)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) cycle_dist[i] = -1;

    cycle_dist[acc_state] = 0;
    int ch = 0, ct = 0;
    cycle_queue[ct++] = acc_state;

    int cycle_end = -1;
    while (ch < ct) {
        int v = cycle_queue[ch++];
        for (int a = 0; a < A->alphabet_size; a++) {
            BuchiTransitionSet* ts = A->delta[v][a];
            if (ts) {
                for (int i = 0; i < ts->count; i++) {
                    int w = ts->data[i].to;
                    if (scc->scc_id[w] != target_scc) continue;
                    if (cycle_dist[w] < 0) {
                        cycle_dist[w] = cycle_dist[v] + 1;
                        cycle_pred_state[w] = v;
                        cycle_pred_sym[w] = A->alphabet[a];
                        cycle_queue[ct++] = w;
                    }
                    if (w == acc_state && v != acc_state) {
                        /* Update cycle_end to capture the full path */
                        if (cycle_end < 0) {
                            cycle_end = v;
                            /* Record the transition back */
                            cycle_pred_state[acc_state] = v;
                            cycle_pred_sym[acc_state] = A->alphabet[a];
                            cycle_dist[acc_state] = cycle_dist[v] + 1;
                        }
                    }
                }
            }
        }
    }

    /* Handle self-loop case */
    if (cycle_dist[acc_state] == 0) {
        for (int a = 0; a < A->alphabet_size; a++) {
            BuchiTransitionSet* ts = A->delta[acc_state][a];
            if (ts) {
                for (int i = 0; i < ts->count; i++) {
                    if (ts->data[i].to == acc_state) {
                        cycle_pred_state[acc_state] = acc_state;
                        cycle_pred_sym[acc_state] = A->alphabet[a];
                        cycle_dist[acc_state] = 1;
                        cycle_end = acc_state;
                        break;
                    }
                }
            }
            if (cycle_end >= 0) break;
        }
    }

    int cycle_len = cycle_dist[acc_state];
    if (cycle_len < 1) {
        buchi_scc_free(scc);
        free(dist); free(pred_state); free(pred_sym); free(queue);
        free(cycle_dist); free(cycle_pred_state); free(cycle_pred_sym); free(cycle_queue);
        free(prefix);
        return NULL;
    }

    BuchiSymbol* cycle = (BuchiSymbol*)malloc(cycle_len * sizeof(BuchiSymbol));
    cur = acc_state;
    for (int i = cycle_len - 1; i >= 0; i--) {
        cycle[i] = cycle_pred_sym[cur];
        cur = cycle_pred_state[cur];
    }

    OmegaWord* result = omega_make_lasso(prefix, prefix_len, cycle, cycle_len);

    buchi_scc_free(scc);
    free(dist); free(pred_state); free(pred_sym); free(queue);
    free(cycle_dist); free(cycle_pred_state); free(cycle_pred_sym); free(cycle_queue);
    free(prefix);
    free(cycle);
    return result;
}

/* ================================================================
 * Nested DFS (Courcoubetis-Vardi-Wolper-Yannakakis 1992)
 * ================================================================ */

static void ndfs_blue_dfs(NestedDFS* nd, const BuchiAutomaton* A, int v) {
    nd->blue_visited[v] = 1;

    for (int a = 0; a < A->alphabet_size; a++) {
        BuchiTransitionSet* ts = A->delta[v][a];
        if (ts) {
            for (int i = 0; i < ts->count; i++) {
                int w = ts->data[i].to;
                if (!nd->blue_visited[w]) {
                    ndfs_blue_dfs(nd, A, w);
                }
            }
        }
    }

    /* If v is accepting, launch red DFS from v */
    if (A->is_accepting[v]) {
        nd->stack_red[nd->stack_red_top++] = v;
        int old_top = nd->stack_blue_top;
        nd->stack_blue[nd->stack_blue_top++] = v;

        /* Red DFS: search for cycle back to v */
        for (int a = 0; a < A->alphabet_size && !nd->found_cycle; a++) {
            BuchiTransitionSet* ts = A->delta[v][a];
            if (ts) {
                for (int i = 0; i < ts->count && !nd->found_cycle; i++) {
                    int w = ts->data[i].to;
                    if (!nd->red_visited[w]) {
                        nd->red_visited[w] = 1;
                        nd->stack_red[nd->stack_red_top++] = w;
                        /* Recursive red DFS: search through blue-visited states */
                        /* Use iterative approach for stack-safety */
                        int red_stack[1024];
                        int red_top = 0;
                        red_stack[red_top++] = w;
                        while (red_top > 0 && !nd->found_cycle) {
                            int u = red_stack[red_top - 1];
                            if (u == v) {
                                /* Found cycle! Save it */
                                nd->found_cycle = 1;
                                nd->accepting_state = v;
                                /* The red stack contains the cycle */
                                nd->cycle_len = nd->stack_red_top - 1; /* exclude v at start */
                                break;
                            }
                            /* Process one successor */
                            int found_next = 0;
                            for (int b = 0; b < A->alphabet_size && !found_next; b++) {
                                BuchiTransitionSet* ts2 = A->delta[u][b];
                                if (ts2) {
                                    for (int j = 0; j < ts2->count && !found_next; j++) {
                                        int x = ts2->data[j].to;
                                        if (!nd->red_visited[x] && nd->blue_visited[x]) {
                                            nd->red_visited[x] = 1;
                                            nd->stack_red[nd->stack_red_top++] = x;
                                            red_stack[red_top++] = x;
                                            found_next = 1;
                                        }
                                    }
                                }
                            }
                            if (!found_next) {
                                red_top--;
                                nd->stack_red_top--;
                            }
                        }
                        if (nd->found_cycle) break;
                    }
                }
            }
        }
        /* Reset red visited */
        for (int i = 0; i < nd->stack_red_top; i++)
            nd->red_visited[nd->stack_red[i]] = 0;
        nd->stack_red_top = 0;

        nd->stack_blue_top = old_top;
    }
}

NestedDFS* nested_dfs_create(int n_states) {
    NestedDFS* nd = (NestedDFS*)calloc(1, sizeof(NestedDFS));
    if (!nd) return NULL;
    nd->blue_visited = (uint8_t*)calloc(n_states, sizeof(uint8_t));
    nd->red_visited = (uint8_t*)calloc(n_states, sizeof(uint8_t));
    nd->stack_blue = (int*)malloc(n_states * sizeof(int));
    nd->stack_red = (int*)malloc(n_states * sizeof(int));
    nd->prefix = (int*)malloc(n_states * sizeof(int));
    nd->cycle = (int*)malloc(n_states * sizeof(int));
    if (!nd->blue_visited || !nd->red_visited || !nd->stack_blue ||
        !nd->stack_red || !nd->prefix || !nd->cycle) {
        nested_dfs_free(nd);
        return NULL;
    }
    return nd;
}

void nested_dfs_free(NestedDFS* nd) {
    if (!nd) return;
    free(nd->blue_visited);
    free(nd->red_visited);
    free(nd->stack_blue);
    free(nd->stack_red);
    free(nd->prefix);
    free(nd->cycle);
    free(nd);
}

int nested_dfs_check(const BuchiAutomaton* A, NestedDFS* nd) {
    if (!A || !nd) return 1;
    memset(nd->blue_visited, 0, A->n_states);
    memset(nd->red_visited, 0, A->n_states);
    nd->stack_blue_top = 0;
    nd->stack_red_top = 0;
    nd->found_cycle = 0;
    nd->prefix_len = 0;
    nd->cycle_len = 0;

    ndfs_blue_dfs(nd, A, A->q0);

    return nd->found_cycle ? 0 : 1;
}

OmegaWord* nested_dfs_extract_lasso(const NestedDFS* nd, const BuchiAutomaton* A) {
    if (!nd || !A || !nd->found_cycle) return NULL;

    /* Build prefix: path from q0 to accepting_state */
    /* We need to reconstruct this from the blue DFS */
    int* prefix_syms = (int*)malloc(A->n_states * sizeof(int));
    int* prefix_states = (int*)malloc(A->n_states * sizeof(int));
    int plen = 0;

    /* BFS to reconstruct prefix */
    int* pred = (int*)malloc(A->n_states * sizeof(int));
    int* pred_sym = (int*)malloc(A->n_states * sizeof(int));
    int* dist = (int*)malloc(A->n_states * sizeof(int));
    int* queue = (int*)malloc(A->n_states * sizeof(int));
    for (int i = 0; i < A->n_states; i++) dist[i] = -1;

    int head = 0, tail = 0;
    dist[A->q0] = 0;
    queue[tail++] = A->q0;
    while (head < tail) {
        int v = queue[head++];
        if (v == nd->accepting_state) break;
        for (int a = 0; a < A->alphabet_size; a++) {
            BuchiTransitionSet* ts = A->delta[v][a];
            if (ts) {
                for (int i = 0; i < ts->count; i++) {
                    int w = ts->data[i].to;
                    if (dist[w] < 0 && nd->blue_visited[w]) {
                        dist[w] = dist[v] + 1;
                        pred[w] = v;
                        pred_sym[w] = A->alphabet[a];
                        queue[tail++] = w;
                    }
                }
            }
        }
    }

    if (dist[nd->accepting_state] >= 0) {
        plen = dist[nd->accepting_state];
        int cur = nd->accepting_state;
        for (int i = plen - 1; i >= 0; i--) {
            prefix_syms[i] = pred_sym[cur];
            cur = pred[cur];
        }
    }

    /* Cycle symbols from red DFS stack */
    int clen = nd->cycle_len;
    BuchiSymbol* cycle_syms = (BuchiSymbol*)malloc(clen * sizeof(BuchiSymbol));

    /* Reconstruct cycle transitions */
    for (int i = 0; i < clen; i++) {
        /* Find the symbol for edge stack_red[i+1] -> stack_red[i+2]
         * (wrapping to stack_red[1] for last edge) */
        int s_from = nd->stack_red[i + 1];
        int s_to = (i + 2 <= nd->stack_red_top) ? nd->stack_red[i + 2] : nd->stack_red[1];
        if (i == clen - 1) s_to = nd->stack_red[1]; /* back to start */

        for (int a = 0; a < A->alphabet_size; a++) {
            BuchiTransitionSet* ts = A->delta[s_from][a];
            if (ts) {
                for (int j = 0; j < ts->count; j++) {
                    if (ts->data[j].to == s_to) {
                        cycle_syms[i] = A->alphabet[a];
                        break;
                    }
                }
            }
        }
    }

    /* Fallback: use known transitions */
    int found = 0;
    for (int i = 0; i < clen; i++) {
        int s = nd->stack_red[i + 1];
        int t = (i + 2 <= nd->stack_red_top) ? nd->stack_red[i + 2] : nd->stack_red[1];
        for (int a = 0; a < A->alphabet_size; a++) {
            if (buchi_has_transition(A, s, A->alphabet[a], t)) {
                cycle_syms[i] = A->alphabet[a];
                found++;
                break;
            }
        }
    }

    OmegaWord* result = omega_make_lasso(prefix_syms, plen,
                                          cycle_syms, (clen > 0) ? clen : 1);

    free(prefix_syms); free(prefix_states);
    free(pred); free(pred_sym); free(dist); free(queue);
    free(cycle_syms);
    return result;
}

/* ================================================================
 * Generalized Büchi Emptiness
 * ================================================================ */

/* Simple SCC-based check: find SCC that intersects all F sets */
int gba_is_empty(const BuchiAutomaton* A, const BuchiState** F_sets,
                 const int* F_sizes, int n_F_sets) {
    if (!A) return 1;

    BuchiSCCDecomp* scc = buchi_scc_decompose(A);
    if (!scc) return 1;

    /* For each SCC, check if it's nontrivial and intersects all F sets */
    int found = 0;
    for (int scc_id = 0; scc_id < scc->n_sccs; scc_id++) {
        if (!scc->scc_nontrivial[scc_id]) continue;

        int hits_all = 1;
        for (int fi = 0; fi < n_F_sets && hits_all; fi++) {
            int hits_this = 0;
            for (int s = 0; s < A->n_states && !hits_this; s++) {
                if (scc->scc_id[s] == scc_id) {
                    for (int j = 0; j < F_sizes[fi]; j++) {
                        if (F_sets[fi][j] == s) { hits_this = 1; break; }
                    }
                }
            }
            if (!hits_this) hits_all = 0;
        }
        if (hits_all) { found = 1; break; }
    }

    buchi_scc_free(scc);
    return found ? 0 : 1;
}

/* ================================================================
 * Degeneralization
 * ================================================================ */

GBA* gba_create(const BuchiAutomaton* base, int n_F_sets) {
    if (!base || n_F_sets < 1) return NULL;
    GBA* gba = (GBA*)calloc(1, sizeof(GBA));
    if (!gba) return NULL;
    gba->base = (BuchiAutomaton*)base;
    gba->n_F_sets = n_F_sets;
    gba->F_sets = (BuchiState**)calloc(n_F_sets, sizeof(BuchiState*));
    gba->F_sizes = (int*)calloc(n_F_sets, sizeof(int));
    if (!gba->F_sets || !gba->F_sizes) {
        free(gba->F_sets); free(gba->F_sizes); free(gba);
        return NULL;
    }
    return gba;
}

void gba_add_accepting_set(GBA* gba, int set_idx,
                            const BuchiState* states, int n) {
    if (!gba || set_idx < 0 || set_idx >= gba->n_F_sets || !states || n <= 0) return;
    gba->F_sets[set_idx] = (BuchiState*)malloc(n * sizeof(BuchiState));
    if (!gba->F_sets[set_idx]) return;
    memcpy(gba->F_sets[set_idx], states, n * sizeof(BuchiState));
    gba->F_sizes[set_idx] = n;
}

void gba_free(GBA* gba) {
    if (!gba) return;
    for (int i = 0; i < gba->n_F_sets; i++)
        free(gba->F_sets[i]);
    free(gba->F_sets);
    free(gba->F_sizes);
    free(gba);
}

BuchiAutomaton* gba_degeneralize(const GBA* gba) {
    if (!gba) return NULL;
    const BuchiAutomaton* base = gba->base;
    int k = gba->n_F_sets;

    /* States: Q × {0,...,k-1} */
    int n_new = base->n_states * k;
    BuchiAutomaton* result = buchi_create(n_new, base->q0 * k + 0,
                                           base->alphabet_size);

    /* Set alphabet */
    buchi_set_alphabet(result, base->alphabet, base->alphabet_size);

    /* Set name */
    char name[256];
    snprintf(name, sizeof(name), "%s-degeneralized(x%d)",
             base->name ? base->name : "A", k);
    buchi_set_name(result, name);

    /* Transitions: (q,i) --a--> (q',i') where q--a-->q' in base
     * i' = i if q ∉ F_i, else i' = (i+1)%k */
    for (int s = 0; s < base->n_states; s++) {
        for (int a = 0; a < base->alphabet_size; a++) {
            BuchiTransitionSet* ts = base->delta[s][a];
            if (!ts) continue;
            for (int ti = 0; ti < ts->count; ti++) {
                int tgt = ts->data[ti].to;
                for (int i = 0; i < k; i++) {
                    int from = s * k + i;
                    /* Check if s is in F_i */
                    int in_Fi = 0;
                    for (int j = 0; j < gba->F_sizes[i]; j++) {
                        if (gba->F_sets[i][j] == s) { in_Fi = 1; break; }
                    }
                    int next_i = in_Fi ? (i + 1) % k : i;
                    int to = tgt * k + next_i;
                    buchi_add_transition(result, from,
                                          base->alphabet[a], to);
                }
            }
        }
    }

    /* Accepting: states where counter = k-1 and
     * original state is in F_{k-1} */
    for (int s = 0; s < base->n_states; s++) {
        int in_Fk1 = 0;
        for (int j = 0; j < gba->F_sizes[k-1]; j++) {
            if (gba->F_sets[k-1][j] == s) { in_Fk1 = 1; break; }
        }
        if (in_Fk1) {
            buchi_add_accepting(result, s * k + (k - 1));
        }
    }

    return result;
}

BuchiAutomaton* gba_degeneralize_optimized(const GBA* gba) {
    /* The optimized version avoids creating states that are
     * unreachable or never needed. For now, delegate to standard
     * degeneralization with post-processing trim. */
    BuchiAutomaton* result = gba_degeneralize(gba);
    if (result) {
        BuchiAutomaton* trimmed = buchi_trim(result);
        buchi_free(result);
        return trimmed;
    }
    return NULL;
}

/* ================================================================
 * Emptiness Report
 * ================================================================ */

EmptinessReport buchi_emptiness_detailed(const BuchiAutomaton* A) {
    EmptinessReport r;
    memset(&r, 0, sizeof(r));

    if (!A) { r.found_empty = 1; return r; }

    r.n_states = A->n_states;
    r.n_trans = A->n_trans_total;

    BuchiSCCDecomp* scc = buchi_scc_decompose(A);
    if (scc) {
        r.n_sccs = scc->n_sccs;
        for (int i = 0; i < scc->n_sccs; i++) {
            if (scc->scc_nontrivial[i]) r.n_nontrivial_sccs++;
            if (scc->scc_accepting[i] && scc->scc_nontrivial[i])
                r.n_accepting_sccs++;
        }
        r.found_empty = (r.n_accepting_sccs == 0);
        buchi_scc_free(scc);
    } else {
        r.found_empty = 1;
    }

    return r;
}

void buchi_emptiness_report_print(const EmptinessReport* r) {
    if (!r) return;
    printf("Emptiness Report:\n");
    printf("  States: %d\n", r->n_states);
    printf("  Transitions: %d\n", r->n_trans);
    printf("  SCCs: %d (non-trivial: %d, accepting: %d)\n",
           r->n_sccs, r->n_nontrivial_sccs, r->n_accepting_sccs);
    printf("  Result: %s\n", r->found_empty ? "EMPTY" : "NON-EMPTY");
    if (r->time_ms > 0) printf("  Time: %.3f ms\n", r->time_ms);
}
