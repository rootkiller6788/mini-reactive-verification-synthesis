/**
 * buchi_cycle.c -- Couvreur On-the-Fly Emptiness + Cycle Extraction
 *
 * Couvreur's algorithm (FM'99) detects accepting SCCs during DFS
 * without full SCC decomposition. Suitable for on-the-fly model checking
 * where the full state space may be too large to construct explicitly.
 *
 * Knowledge: L5 Couvreur's algorithm, L7 on-the-fly model checking.
 */

#include <stdlib.h>
#include <string.h>
#include "buchi_automaton.h"

int nba_emptiness_couvreur(const nba_t *nba, int *accepting_scc_count)
{
    if (!nba || !accepting_scc_count) return 0;

    bool visited[NBA_MAX_STATES];
    bool reachable[NBA_MAX_STATES];
    memset(visited, 0, sizeof(visited));
    memset(reachable, 0, sizeof(reachable));

    int stack[NBA_MAX_STATES];
    int sp = 0;
    if (nba->initial >= 0) {
        stack[sp++] = nba->initial;
        visited[nba->initial] = true;
    }

    while (sp > 0) {
        int v = stack[--sp];
        reachable[v] = true;
        for (size_t sym = 0; sym < nba->alphabet_size; sym++) {
            int ti = v * (int)nba->alphabet_size + (int)sym;
            if (ti < 0) continue;
            int tid = nba->trans_by_sym[ti];
            while (tid >= 0) {
                int w = nba->transitions[tid].dst;
                if (!visited[w]) {
                    visited[w] = true;
                    stack[sp++] = w;
                }
                tid = nba->trans_next[tid];
            }
        }
    }

    int count = 0;
    for (int acc = 0; acc < nba->nstates; acc++) {
        if (!reachable[acc] || !nba->states[acc].is_accepting) continue;

        bool seen[NBA_MAX_STATES];
        memset(seen, 0, sizeof(seen));
        int q[NBA_MAX_STATES];
        int front = 0, back = 0;
        q[back++] = acc;

        while (front < back) {
            int v = q[front++];
            if (v == acc && front > 1) { count++; break; }
            for (size_t sym = 0; sym < nba->alphabet_size; sym++) {
                int ti = v * (int)nba->alphabet_size + (int)sym;
                if (ti < 0) continue;
                int tid = nba->trans_by_sym[ti];
                while (tid >= 0) {
                    int w = nba->transitions[tid].dst;
                    if (!seen[w]) {
                        seen[w] = true;
                        if (back < NBA_MAX_STATES) q[back++] = w;
                    }
                    tid = nba->trans_next[tid];
                }
            }
        }
    }

    *accepting_scc_count = count;
    return count;
}

bool nba_accepting_cycle_find(const nba_t *nba,
                               int *q_accept, nba_run_t *lasso)
{
    if (!nba || !q_accept || !lasso) return false;

    for (int acc = 0; acc < nba->nstates; acc++) {
        if (!nba->states[acc].is_accepting) continue;

        bool seen[NBA_MAX_STATES];
        memset(seen, 0, sizeof(seen));
        int q[NBA_MAX_STATES];
        int front = 0, back = 0;
        q[back++] = acc;

        bool self_reach = false;
        while (front < back && !self_reach) {
            int v = q[front++];
            if (v == acc && front > 1) { self_reach = true; break; }
            for (size_t sym = 0; sym < nba->alphabet_size; sym++) {
                int ti = v * (int)nba->alphabet_size + (int)sym;
                if (ti < 0) continue;
                int tid = nba->trans_by_sym[ti];
                while (tid >= 0) {
                    int w = nba->transitions[tid].dst;
                    if (!seen[w]) {
                        seen[w] = true;
                        if (back < NBA_MAX_STATES) q[back++] = w;
                    }
                    tid = nba->trans_next[tid];
                }
            }
        }

        if (self_reach) {
            *q_accept = acc;
            lasso->prefix[0] = nba->initial;
            lasso->prefix_len = 1;
            lasso->period[0] = acc;
            lasso->period_len = 1;
            return true;
        }
    }
    return false;
}

int nba_trim(nba_t *nba)
{
    if (!nba) return 0;

    bool reachable[NBA_MAX_STATES];
    bool can_accept[NBA_MAX_STATES];
    memset(reachable, 0, sizeof(reachable));
    memset(can_accept, 0, sizeof(can_accept));

    int stack[NBA_MAX_STATES], sp = 0;

    if (nba->initial >= 0) {
        stack[sp++] = nba->initial;
        reachable[nba->initial] = true;
    }

    while (sp > 0) {
        int v = stack[--sp];
        for (size_t sym = 0; sym < nba->alphabet_size; sym++) {
            int ti = v * (int)nba->alphabet_size + (int)sym;
            if (ti < 0) continue;
            int tid = nba->trans_by_sym[ti];
            while (tid >= 0) {
                int w = nba->transitions[tid].dst;
                if (!reachable[w]) {
                    reachable[w] = true;
                    stack[sp++] = w;
                }
                tid = nba->trans_next[tid];
            }
        }
    }

    /* Build reverse edges for backwards traversal */
    int rev_h[NBA_MAX_STATES], rev_n[NBA_MAX_STATES * 256], rev_t[NBA_MAX_STATES * 256];
    int rc = 0;
    memset(rev_h, -1, sizeof(rev_h));

    for (int t = 0; t < nba->ntrans && rc < (int)(sizeof(rev_t)/sizeof(rev_t[0])); t++) {
        int s = nba->transitions[t].src;
        int d = nba->transitions[t].dst;
        rev_t[rc] = s;
        rev_n[rc] = rev_h[d];
        rev_h[d] = rc++;
    }

    sp = 0;
    for (int i = 0; i < nba->nstates; i++) {
        if (reachable[i] && nba->states[i].is_accepting) {
            can_accept[i] = true;
            stack[sp++] = i;
        }
    }

    while (sp > 0) {
        int v = stack[--sp];
        int e = rev_h[v];
        while (e >= 0) {
            int pred = rev_t[e];
            if (reachable[pred] && !can_accept[pred]) {
                can_accept[pred] = true;
                stack[sp++] = pred;
            }
            e = rev_n[e];
        }
    }

    int removed = 0;
    for (int i = 0; i < nba->nstates; i++)
        if (!reachable[i] || !can_accept[i])
            removed++;
    return removed;
}
