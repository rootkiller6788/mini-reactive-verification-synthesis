/**
 * buchi_complement.c -- NBA Complementation
 *
 * NBAs are closed under complement, but the construction is nontrivial.
 * Deterministic Buechi automata (DBA) are NOT closed under complement
 * (unlike DFAs on finite words). The complement of a DBA-recognizable
 * language may require nondeterminism.
 *
 * This implements:
 *   1. DBA complement: flip accepting states (works for deterministic NBAs)
 *   2. For nondeterministic NBAs: return false (full Safra/Schewe construction
 *      requires 2^O(n log n) states and is beyond this implementation)
 *
 * Reference: Schewe (2009) "Tighter Bounds for the Complementation of
 *            Buechi Automata"
 * Knowledge: L2 complement closure, L8 Safra/Schewe complementation.
 */

#include <string.h>
#include "buchi_automaton.h"

bool nba_complement(const nba_t *nba, nba_t *result)
{
    if (!nba || !result) return false;

    /* Check if the NBA is deterministic */
    bool is_det = true;
    for (int i = 0; i < nba->nstates && is_det; i++) {
        for (size_t s = 0; s < nba->alphabet_size && is_det; s++) {
            int ti = i * (int)nba->alphabet_size + (int)s;
            if (ti < 0) continue;
            int tid = nba->trans_by_sym[ti];
            int count = 0;
            while (tid >= 0) { count++; tid = nba->trans_next[tid]; }
            if (count > 1) is_det = false;
        }
    }

    if (is_det) {
        /* DBA complementation: copy, flip accept, add universal sink */
        nba_init(result, nba->alphabet_size);

        for (int i = 0; i < nba->nstates; i++) {
            nba_add_state(result, i == nba->initial,
                          !nba->states[i].is_accepting,
                          nba->states[i].name);
        }

        /* Add sink state that accepts everything missing */
        int sink = nba_add_state(result, false, true, "complement_sink");
        for (size_t s = 0; s < nba->alphabet_size; s++)
            nba_add_transition(result, sink, (uint8_t)s, sink);

        for (int i = 0; i < nba->nstates; i++) {
            for (size_t s = 0; s < nba->alphabet_size; s++) {
                int ti = i * (int)nba->alphabet_size + (int)s;
                if (ti < 0) {
                    nba_add_transition(result, i, (uint8_t)s, sink);
                    continue;
                }
                int tid = nba->trans_by_sym[ti];
                if (tid < 0) {
                    nba_add_transition(result, i, (uint8_t)s, sink);
                } else {
                    nba_add_transition(result, i, (uint8_t)s,
                                       nba->transitions[tid].dst);
                }
            }
        }
        return true;
    }

    /* Nondeterministic: Safra/Schewe required, not implemented */
    return false;
}

bool nba_includes(const nba_t *a1, const nba_t *a2)
{
    if (!a1 || !a2) return false;
    nba_t neg_a2;
    if (!nba_complement(a2, &neg_a2)) return false;
    nba_t product;
    if (!nba_intersection(a1, &neg_a2, &product)) return false;
    return nba_is_empty(&product);
}
