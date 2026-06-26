/**
 * buchi_run.c -- NBA Run Simulation on omega-Words
 *
 * An accepting run on an ultimately periodic omega-word u.v^omega
 * must have a "lasso" shape: finite prefix of states followed by
 * a cycle that contains at least one Buechi accepting state.
 *
 * Knowledge: L2 Buechi acceptance, L5 run-finding algorithm.
 */

#include <stdlib.h>
#include <string.h>
#include "buchi_automaton.h"
#include "omega_regular.h"

/* Duplicate the successor function to avoid cross-file dependency issues */
static int get_succ(const nba_t *nba, int src, uint8_t sym,
                     int *dsts, int max_dsts)
{
    if (!nba || !dsts || src < 0 || src >= nba->nstates) return 0;
    if (sym >= nba->alphabet_size) return 0;
    int count = 0;
    int idx = src * (int)nba->alphabet_size + sym;
    if (idx < 0 || idx >= NBA_MAX_STATES * NBA_MAX_ALPHABET) return 0;
    int tid = nba->trans_by_sym[idx];
    while (tid >= 0 && count < max_dsts) {
        if (nba->transitions[tid].src == src
            && nba->transitions[tid].sym == sym)
            dsts[count++] = nba->transitions[tid].dst;
        tid = nba->trans_next[tid];
    }
    return count;
}

static bool is_acc(const nba_t *nba, int state)
{
    return (nba && state >= 0 && state < nba->nstates
            && nba->states[state].is_accepting);
}

bool nba_accepts(const nba_t *nba, const omega_word *w)
{
    if (!nba || !w) return false;

    /* Track reachable states during prefix processing */
    int cur[64], ncur;
    if (nba->initial < 0) return false;
    cur[0] = nba->initial;
    ncur = 1;

    /* Process prefix of omega-word */
    for (size_t i = 0; i < w->prefix_len && ncur > 0; i++) {
        uint8_t sym = omega_word_index(w, i);
        int next[64], nnext = 0;
        for (int j = 0; j < ncur; j++) {
            int succ[16];
            int ns = get_succ(nba, cur[j], sym, succ, 16);
            for (int k = 0; k < ns && nnext < 64; k++) {
                bool dup = false;
                for (int d = 0; d < nnext; d++)
                    if (next[d] == succ[k]) { dup = true; break; }
                if (!dup) next[nnext++] = succ[k];
            }
        }
        ncur = nnext;
        memcpy(cur, next, ncur * sizeof(int));
    }

    if (ncur == 0) return false;

    /* Search for accepting cycle in periodic suffix */
    size_t check_len = w->period_len * (nba->nstates + 1);
    if (check_len > NBA_MAX_STATES * 3) check_len = NBA_MAX_STATES * 3;

    for (int si = 0; si < ncur; si++) {
        int q = cur[si];
        int hist[256], hlen = 1;
        hist[0] = q;

        for (size_t step = 0; step < check_len && hlen < 255; step++) {
            uint8_t sym = omega_word_index(w, w->prefix_len + step);
            int succ[16];
            int ns = get_succ(nba, q, sym, succ, 16);
            if (ns == 0) break;
            q = succ[0];
            hist[hlen++] = q;

            /* Look for cycle in history */
            for (int p = 0; p < hlen - 1; p++) {
                if (hist[p] != q) continue;
                /* Found cycle hist[p..hlen-1] */
                bool has_acc = false;
                for (int c = p; c < hlen; c++)
                    if (is_acc(nba, hist[c])) { has_acc = true; break; }
                if (has_acc) return true;
                break;
            }
        }
    }
    return false;
}

bool nba_run_find(const nba_t *nba, const omega_word *w, nba_run_t *run)
{
    if (!nba || !w || !run) return false;

    int cur[64], ncur;
    if (nba->initial < 0) return false;
    cur[0] = nba->initial;
    ncur = 1;

    for (size_t i = 0; i < w->prefix_len && ncur > 0; i++) {
        uint8_t sym = omega_word_index(w, i);
        int next[64], nnext = 0;
        for (int j = 0; j < ncur; j++) {
            int succ[16];
            int ns = get_succ(nba, cur[j], sym, succ, 16);
            for (int k = 0; k < ns && nnext < 64; k++) {
                bool dup = false;
                for (int d = 0; d < nnext; d++)
                    if (next[d] == succ[k]) { dup = true; break; }
                if (!dup) next[nnext++] = succ[k];
            }
        }
        ncur = nnext;
        memcpy(cur, next, ncur * sizeof(int));
    }

    if (ncur == 0) return false;

    size_t check_len = w->period_len * (nba->nstates + 1);
    if (check_len > NBA_MAX_STATES * 3) check_len = NBA_MAX_STATES * 3;

    for (int si = 0; si < ncur; si++) {
        int q = cur[si];
        int hist[256], hlen = 1;
        hist[0] = q;

        for (size_t step = 0; step < check_len && hlen < 255; step++) {
            uint8_t sym = omega_word_index(w, w->prefix_len + step);
            int succ[16];
            int ns = get_succ(nba, q, sym, succ, 16);
            if (ns == 0) break;
            q = succ[0];
            hist[hlen++] = q;

            for (int p = 0; p < hlen - 1; p++) {
                if (hist[p] != q) continue;
                bool has_acc = false;
                for (int c = p; c < hlen; c++)
                    if (is_acc(nba, hist[c])) { has_acc = true; break; }
                if (has_acc && p <= NBA_MAX_STATES * 2
                    && (hlen - p) <= NBA_MAX_STATES * 2) {
                    memcpy(run->prefix, hist, p * sizeof(int));
                    run->prefix_len = p;
                    memcpy(run->period, &hist[p], (hlen - p) * sizeof(int));
                    run->period_len = hlen - p;
                    return true;
                }
                break;
            }
        }
    }
    return false;
}
