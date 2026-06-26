/**
 * buchi_intersection.c -- NBA Intersection (Product)
 *
 * L(A1 cap A2): 2-copy product construction.
 * A run is accepting iff both component runs visit accepting states
 * infinitely often. The 2-copy construction alternates flags 0->1
 * when seeing F1, and 1->0 when seeing F2.
 *
 * Theorem: omega-regular languages are closed under intersection.
 * Complexity: O(|A1| * |A2|) states.
 *
 * Reference: Vardi & Wolper (1986), Thomas (1990)
 * Knowledge: L2 closure under intersection, L5 product construction.
 */

#include <string.h>
#include <stdio.h>
#include "buchi_automaton.h"

bool nba_intersection(const nba_t *a1, const nba_t *a2, nba_t *result)
{
    if (!a1 || !a2 || !result) return false;
    size_t as = a1->alphabet_size > a2->alphabet_size
                ? a1->alphabet_size : a2->alphabet_size;
    nba_init(result, as);

    int total = a1->nstates * a2->nstates * 2;
    if (total > NBA_MAX_STATES) return false;

    int map_a[2][NBA_MAX_STATES][NBA_MAX_STATES];
    memset(map_a, -1, sizeof(map_a));

    /* Create states for all (q1, q2, flag) triples */
    for (int f = 0; f < 2; f++) {
        for (int i = 0; i < a1->nstates; i++) {
            for (int j = 0; j < a2->nstates; j++) {
                bool is_init = (f == 0 && i == a1->initial
                                && j == a2->initial);
                bool is_acc = (f == 1 && a2->states[j].is_accepting);
                char name[32];
                snprintf(name, 32, "p%d,%d,%d", i, j, f);
                map_a[f][i][j] = nba_add_state(result, is_init, is_acc, name);
            }
        }
    }

    /* Add product transitions */
    for (int f = 0; f < 2; f++) {
        for (int i = 0; i < a1->nstates; i++) {
            for (int j = 0; j < a2->nstates; j++) {
                int src = map_a[f][i][j];
                if (src < 0) continue;

                for (size_t s = 0; s < as; s++) {
                    int ti1 = i * (int)a1->alphabet_size + (int)s;
                    int ti2 = j * (int)a2->alphabet_size + (int)s;
                    if (ti1 < 0 || ti2 < 0) continue;
                    int t1 = a1->trans_by_sym[ti1];
                    int t2 = a2->trans_by_sym[ti2];

                    while (t1 >= 0) {
                        int i2 = a1->transitions[t1].dst;
                        int t2b = t2;
                        while (t2b >= 0) {
                            int j2 = a2->transitions[t2b].dst;

                            int new_f = f;
                            if (f == 0 && a1->states[i2].is_accepting)
                                new_f = 1;
                            else if (f == 1 && a2->states[j2].is_accepting)
                                new_f = 0;

                            int dst = map_a[new_f][i2][j2];
                            if (dst >= 0)
                                nba_add_transition(result, src,
                                                   (uint8_t)s, dst);
                            t2b = a2->trans_next[t2b];
                        }
                        t1 = a1->trans_next[t1];
                    }
                }
            }
        }
    }
    return true;
}
