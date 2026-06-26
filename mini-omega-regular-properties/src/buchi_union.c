/**
 * buchi_union.c -- NBA Union Operation
 *
 * L(A1 cup A2): disjoint union + new initial state.
 *
 * Theorem: omega-regular languages are closed under finite union.
 * Complexity: O(|A1| + |A2|) time and space.
 *
 * Knowledge: L2 closure under union, L5 union construction.
 */

#include <string.h>
#include "buchi_automaton.h"

bool nba_union(const nba_t *a1, const nba_t *a2, nba_t *result)
{
    if (!a1 || !a2 || !result) return false;
    size_t as = a1->alphabet_size > a2->alphabet_size
                ? a1->alphabet_size : a2->alphabet_size;
    nba_init(result, as);

    int off2 = a1->nstates;

    /* Copy a1 states */
    for (int i = 0; i < a1->nstates; i++)
        nba_add_state(result, (i == a1->initial),
                      a1->states[i].is_accepting, a1->states[i].name);

    /* Copy a2 states */
    for (int i = 0; i < a2->nstates; i++)
        nba_add_state(result, false,
                      a2->states[i].is_accepting, a2->states[i].name);

    /* New initial state with epsilon-like transitions */
    int new_init = nba_add_state(result, true, false, "union_init");
    for (size_t s = 0; s < as; s++) {
        if (a1->initial >= 0) {
            int ti = a1->initial * (int)a1->alphabet_size + (int)s;
            if (ti >= 0 && a1->trans_by_sym[ti] >= 0)
                nba_add_transition(result, new_init, (uint8_t)s, a1->initial);
        }
        if (a2->initial >= 0) {
            int ti = a2->initial * (int)a2->alphabet_size + (int)s;
            if (ti >= 0 && a2->trans_by_sym[ti] >= 0)
                nba_add_transition(result, new_init, (uint8_t)s,
                                   off2 + a2->initial);
        }
    }

    /* Copy a1 transitions */
    for (int t = 0; t < a1->ntrans; t++)
        nba_add_transition(result, a1->transitions[t].src,
                           a1->transitions[t].sym,
                           a1->transitions[t].dst);

    /* Copy a2 transitions (shifted) */
    for (int t = 0; t < a2->ntrans; t++)
        nba_add_transition(result, off2 + a2->transitions[t].src,
                           a2->transitions[t].sym,
                           off2 + a2->transitions[t].dst);

    return true;
}
