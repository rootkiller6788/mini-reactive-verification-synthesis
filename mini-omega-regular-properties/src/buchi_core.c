/**
 * buchi_core.c -- Nondeterministic Buechi Automata Core Operations
 *
 * Implements NBA construction, state/transition management,
 * and run simulation on ultimately periodic omega-words.
 *
 * NBA A = (Q, Sigma, delta, q0, F)
 * Run is accepting iff Inf(r) intersect F != empty.
 *
 * References: Buechi (1962), Vardi & Wolper (1986), Thomas (1990)
 * Knowledge: L1 NBA definition, L2 Buechi acceptance, L3 transition structure
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "buchi_automaton.h"
#include "omega_regular.h"

void nba_init(nba_t *nba, size_t alphabet_size)
{
    if (!nba) return;
    memset(nba, 0, sizeof(*nba));
    nba->nstates = 0;
    nba->ntrans = 0;
    nba->alphabet_size = (alphabet_size < NBA_MAX_ALPHABET)
                         ? alphabet_size : NBA_MAX_ALPHABET;
    nba->initial = -1;
    for (int i = 0; i < NBA_MAX_STATES; i++) {
        nba->trans_head[i] = -1;
        for (size_t j = 0; j < NBA_MAX_ALPHABET; j++)
            nba->trans_by_sym[i * NBA_MAX_ALPHABET + j] = -1;
    }
}

int nba_add_state(nba_t *nba, bool initial, bool accepting, const char *name)
{
    if (!nba || nba->nstates >= NBA_MAX_STATES) return -1;
    int id = nba->nstates++;
    nba->states[id].id = id;
    nba->states[id].is_initial = initial;
    nba->states[id].is_accepting = accepting;
    if (name) {
        strncpy(nba->states[id].name, name, 31);
        nba->states[id].name[31] = '\0';
    } else {
        snprintf(nba->states[id].name, 32, "q%d", id);
    }
    nba->trans_head[id] = -1;
    if (initial && nba->initial < 0)
        nba->initial = id;
    return id;
}

void nba_set_initial(nba_t *nba, int state_id)
{
    if (!nba || state_id < 0 || state_id >= nba->nstates) return;
    nba->initial = state_id;
    nba->states[state_id].is_initial = true;
}

bool nba_add_transition(nba_t *nba, int src, uint8_t sym, int dst)
{
    if (!nba) return false;
    if (src < 0 || src >= nba->nstates) return false;
    if (dst < 0 || dst >= nba->nstates) return false;
    if (sym >= nba->alphabet_size) return false;

    int max_trans = NBA_MAX_STATES * NBA_MAX_ALPHABET * NBA_MAX_TRANS;
    if (nba->ntrans >= max_trans) return false;

    int tid = nba->ntrans++;
    nba->transitions[tid].src = src;
    nba->transitions[tid].sym = sym;
    nba->transitions[tid].dst = dst;

    nba->trans_next[tid] = nba->trans_head[src];
    nba->trans_head[src] = tid;

    int idx = src * (int)nba->alphabet_size + sym;
    if (idx < NBA_MAX_STATES * NBA_MAX_ALPHABET) {
        nba->trans_next[tid] = nba->trans_by_sym[idx];
        nba->trans_by_sym[idx] = tid;
    }
    return true;
}

/* ---- Successor computation ---- */

static int nba_successors(const nba_t *nba, int src, uint8_t sym,
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

static bool nba_state_accepting(const nba_t *nba, int state)
{
    if (!nba || state < 0 || state >= nba->nstates) return false;
    return nba->states[state].is_accepting;
}
