/**
 * omega_closure.c -- Closure Properties of omega-Regular Languages
 *
 * Proves and implements the closure properties of omega-regular
 * languages under various operations.
 *
 * Theorem (Buechi 1962): The class of omega-regular languages is
 * closed under:
 *   - Union: L1 U L2 = L(A1 U A2)
 *   - Intersection: L1 cap L2 = L(A1 x A2)
 *   - Complement: not L = L(not A)  [requires Safra/Schewe]
 *   - Projection: exists sigma. L  [via alphabet projection]
 *   - Prefix closure: {w | exists v: w.v in L} is omega-regular
 *
 * Moreover, omega-regular languages are NOT closed under:
 *   - Limit: lim(L) = {w | infinitely many prefixes of w in L}
 *     [The limit of a regular language is always omega-regular,
 *      but the limit of an omega-regular language may not be]
 *
 * Knowledge: L2 closure properties, L4 closure theorems.
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "omega_regular.h"
#include "buchi_automaton.h"

/**
 * nba_prefix_closure -- Construct NBA accepting the prefix-closure
 * of L(A). The prefix-closure contains all omega-words that have
 * a finite prefix whose extension (any infinite continuation)
 * is in L(A).
 *
 * Since ultimately periodic words capture all omega-regular languages,
 * the prefix-closure corresponds to "eventually L".
 */
bool nba_prefix_closure(const nba_t *nba, nba_t *result)
{
    if (!nba || !result) return false;
    /* Prefix closure: make all states that can reach an accepting
     * state become initial states. Also keep original transitions. */

    /* Copy the NBA structure */
    nba_init(result, nba->alphabet_size);

    /* Find states that can reach an accepting state (reverse reachability) */
    bool can_reach_accept[NBA_MAX_STATES];
    memset(can_reach_accept, 0, sizeof(can_reach_accept));

    /* Build reverse adjacency */
    int rev_h[NBA_MAX_STATES], rev_n[NBA_MAX_STATES * 256], rev_t[NBA_MAX_STATES * 256];
    int rc = 0;
    memset(rev_h, -1, sizeof(rev_h));

    for (int ti = 0; ti < nba->ntrans; ti++) {
        int s = nba->transitions[ti].src;
        int d = nba->transitions[ti].dst;
        rev_t[rc] = s;
        rev_n[rc] = rev_h[d];
        rev_h[d] = rc++;
    }

    /* Backwards BFS from accepting states */
    int stack[NBA_MAX_STATES], sp = 0;
    for (int i = 0; i < nba->nstates; i++) {
        if (nba->states[i].is_accepting) {
            can_reach_accept[i] = true;
            stack[sp++] = i;
        }
    }

    while (sp > 0) {
        int v = stack[--sp];
        int e = rev_h[v];
        while (e >= 0) {
            int pred = rev_t[e];
            if (!can_reach_accept[pred]) {
                can_reach_accept[pred] = true;
                stack[sp++] = pred;
            }
            e = rev_n[e];
        }
    }

    /* Copy states: original initial + any state that can reach accept */
    for (int i = 0; i < nba->nstates; i++) {
        bool is_init = (i == nba->initial) || can_reach_accept[i];
        nba_add_state(result, is_init,
                      nba->states[i].is_accepting,
                      nba->states[i].name);
    }

    /* Copy all transitions */
    for (int ti = 0; ti < nba->ntrans; ti++) {
        nba_add_transition(result,
            nba->transitions[ti].src,
            nba->transitions[ti].sym,
            nba->transitions[ti].dst);
    }

    return true;
}

/**
 * nba_eventually -- Construct NBA accepting "eventually L".
 * F L = {w | there exists k such that w[k..] in L}.
 *
 * This is the omega-regular expression: Sigma*.L.
 */
bool nba_eventually(const nba_t *nba, nba_t *result)
{
    if (!nba || !result) return false;

    /* Sigma*.L: copy NBA, add self-loops on all symbols from all states.
     * Every state can nondeterministically decide to jump to the
     * original NBA's initial state. */
    nba_init(result, nba->alphabet_size);

    /* Copy original NBA states (shifted by 0) but none are initial */
    int offset = 0;
    for (int i = 0; i < nba->nstates; i++) {
        nba_add_state(result, (i == nba->initial),
                      nba->states[i].is_accepting,
                      nba->states[i].name);
    }

    /* Copy transitions from original NBA */
    for (int ti = 0; ti < nba->ntrans; ti++) {
        nba_add_transition(result,
            nba->transitions[ti].src + offset,
            nba->transitions[ti].sym,
            nba->transitions[ti].dst + offset);
    }

    /* Add self-loops: from every state, on every symbol, stay in same state.
     * And from every state, can also jump to NBA's initial. */
    for (int i = 0; i < nba->nstates; i++) {
        for (size_t s = 0; s < nba->alphabet_size; s++) {
            /* Self-loop */
            nba_add_transition(result, i, (uint8_t)s, i);
            /* Jump to NBA initial (if NBA has a transition from initial on s) */
            int ti_idx = nba->initial * (int)nba->alphabet_size + (int)s;
            if (ti_idx >= 0) {
                int tid = nba->trans_by_sym[ti_idx];
                while (tid >= 0) {
                    nba_add_transition(result, i, (uint8_t)s,
                                       nba->transitions[tid].dst);
                    tid = nba->trans_next[tid];
                }
            }
        }
    }

    return true;
}

/**
 * nba_always -- Construct NBA accepting "always L".
 * G L = {w | for all k, w[k..] in L}.
 *
 * This is the complement of eventually-not-L, but simpler direct
 * construction: make all states accepting and remove all transitions
 * that lead to non-accepting states in the original NBA.
 */
bool nba_always(const nba_t *nba, nba_t *result)
{
    if (!nba || !result) return false;

    nba_init(result, nba->alphabet_size);

    /* Copy all states, make them all accepting */
    for (int i = 0; i < nba->nstates; i++) {
        nba_add_state(result, (i == nba->initial),
                      true, nba->states[i].name);
    }

    /* Only copy transitions that lead to accepting states */
    for (int ti = 0; ti < nba->ntrans; ti++) {
        int dst = nba->transitions[ti].dst;
        if (!nba->states[dst].is_accepting) continue;
        nba_add_transition(result,
            nba->transitions[ti].src,
            nba->transitions[ti].sym,
            nba->transitions[ti].dst);
    }

    return true;
}
