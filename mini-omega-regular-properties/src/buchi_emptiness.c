/**
 * buchi_emptiness.c -- NBA Emptiness via Tarjan SCC
 *
 * Emptiness is the core decision procedure for omega-regular
 * model checking: System |= phi iff L(A_sys x A_not_phi) = empty.
 *
 * Theorem (Buechi 1962): L(A) != empty iff there exists a reachable
 * accepting state lying on a cycle reachable from itself.
 *
 * Algorithm: Tarjan's SCC decomposition in O(|Q| + |delta|).
 *
 * Knowledge: L4 Buechi's theorem, L5 emptiness algorithm, L7 model checking.
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "buchi_automaton.h"

typedef struct {
    int index;
    int lowlink;
    bool onstack;
} ts_t;

static void tarjan_visit(int v, const nba_t *nba, ts_t *ts,
                          int *idx, int *stack, int *sp,
                          bool *found)
{
    ts[v].index = ts[v].lowlink = (*idx)++;
    ts[v].onstack = true;
    stack[(*sp)++] = v;

    for (size_t sym = 0; sym < nba->alphabet_size && !*found; sym++) {
        int ti = v * (int)nba->alphabet_size + (int)sym;
        if (ti < 0 || ti >= NBA_MAX_STATES * NBA_MAX_ALPHABET) continue;
        int tid = nba->trans_by_sym[ti];
        while (tid >= 0) {
            int w = nba->transitions[tid].dst;
            if (ts[w].index < 0) {
                tarjan_visit(w, nba, ts, idx, stack, sp, found);
                if (ts[w].lowlink < ts[v].lowlink)
                    ts[v].lowlink = ts[w].lowlink;
            } else if (ts[w].onstack) {
                if (ts[w].index < ts[v].lowlink)
                    ts[v].lowlink = ts[w].index;
            }
            tid = nba->trans_next[tid];
        }
    }

    if (ts[v].lowlink == ts[v].index && !*found) {
        int scc_sz = 0;
        bool has_acc = false;
        int w;
        do {
            w = stack[--(*sp)];
            ts[w].onstack = false;
            scc_sz++;
            if (nba->states[w].is_accepting) has_acc = true;
        } while (w != v);

        if (has_acc && scc_sz >= 1) {
            if (scc_sz == 1) {
                /* Check for self-loop */
                bool sl = false;
                for (size_t sym = 0; sym < nba->alphabet_size && !sl; sym++) {
                    int ti2 = v * (int)nba->alphabet_size + (int)sym;
                    if (ti2 < 0) continue;
                    int t2 = nba->trans_by_sym[ti2];
                    while (t2 >= 0) {
                        if (nba->transitions[t2].dst == v) { sl = true; break; }
                        t2 = nba->trans_next[t2];
                    }
                }
                if (!sl) has_acc = false;
            }
            if (has_acc) *found = true;
        }
    }
}

bool nba_is_empty(const nba_t *nba)
{
    if (!nba || nba->initial < 0) return true;

    ts_t ts[NBA_MAX_STATES];
    int stack[NBA_MAX_STATES];
    int sp = 0, idx = 0;

    for (int i = 0; i < nba->nstates; i++) {
        ts[i].index = -1;
        ts[i].lowlink = 0;
        ts[i].onstack = false;
    }

    bool found = false;
    tarjan_visit(nba->initial, nba, ts, &idx, stack, &sp, &found);

    for (int i = 0; i < nba->nstates && !found; i++)
        if (ts[i].index < 0)
            tarjan_visit(i, nba, ts, &idx, stack, &sp, &found);

    return !found;
}

bool nba_is_universal(const nba_t *nba)
{
    if (!nba || nba->initial < 0) return false;
    /* Sufficient condition: all states accepting + total transitions */
    for (int i = 0; i < nba->nstates; i++) {
        if (!nba->states[i].is_accepting) return false;
        for (size_t s = 0; s < nba->alphabet_size; s++) {
            int ti = i * (int)nba->alphabet_size + (int)s;
            if (ti < 0 || ti >= NBA_MAX_STATES * NBA_MAX_ALPHABET) return false;
            if (nba->trans_by_sym[ti] < 0) return false;
        }
    }
    return true;
}
