/**
 * ore_to_nba.c -- omega-Regular Expression to NBA Translation
 *
 * Constructive proof of Buechi's Theorem (1962):
 *   Every omega-regular expression can be translated to an equivalent NBA.
 *
 * The translation is compositional by cases on the expression operator:
 *   ORE_EMPTY      -> single non-accepting state, no transitions
 *   ORE_LETTER(a)  -> 2-state NBA accepting Sigma^omega starting with a
 *   ORE_UNION      -> nba_union(A1, A2)
 *   ORE_INTERSECT  -> nba_intersection(A1, A2)
 *   ORE_COMPLEMENT -> nba_complement(A)
 *   ORE_OMEGA(r)   -> NBA where accepting = initial, transitions from DFA r
 *                      with additional loops to initial on DFA-accepting transitions
 *   ORE_CONCAT(r,L)-> start in DFA r, bridge to NBA L on DFA-accepting
 *
 * This module also provides decision procedures:
 *   ore_membership:  w in L(E)?
 *   ore_is_empty:    L(E) = empty?
 *   ore_is_universal: L(E) = Sigma^omega?
 *   ore_language_equal: L(E1) = L(E2)?
 *
 * Knowledge:
 *   L4: Buechi's Theorem (constructive proof)
 *   L5: Compositional NBA construction algorithm
 *   L6: Decision procedures (membership, emptiness, universality, equality)
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "omega_regular.h"
#include "buchi_automaton.h"

/* DFA types and externs are now declared in omega_regular.h */

/**
 * ore_to_nba -- Translate an omega-regular expression to an equivalent NBA.
 *
 * This is the constructive proof of Buechi's Theorem (1962).
 * The construction is compositional by cases.
 *
 * Complexity:
 *   - Letter, empty: O(1)
 *   - Union, intersection: O(|A1| + |A2|) or O(|A1| * |A2|)
 *   - Complement: exponential in worst case
 *   - Omega: O(|r|) where r is the DFA for the regular sub-expression
 *   - Concat: O(|r| * |L|)
 */
bool ore_to_nba(const struct ore_node *expr, size_t alphabet_size, nba_t *out)
{
    if (!expr || !out) return false;
    nba_init(out, alphabet_size);

    switch (expr->op) {
    case ORE_EMPTY:
        nba_add_state(out, true, false, "q0");
        return true;

    case ORE_LETTER: {
        int q0 = nba_add_state(out, true, false, "q0");
        int q1 = nba_add_state(out, false, true, "q1");
        nba_add_transition(out, q0, expr->letter, q1);
        for (size_t s = 0; s < alphabet_size; s++)
            nba_add_transition(out, q1, (uint8_t)s, q1);
        return true;
    }

    case ORE_UNION: {
        nba_t left, right;
        if (!ore_to_nba(expr->left, alphabet_size, &left)) return false;
        if (!ore_to_nba(expr->right, alphabet_size, &right)) return false;
        return nba_union(&left, &right, out);
    }

    case ORE_INTERSECTION: {
        nba_t left, right;
        if (!ore_to_nba(expr->left, alphabet_size, &left)) return false;
        if (!ore_to_nba(expr->right, alphabet_size, &right)) return false;
        return nba_intersection(&left, &right, out);
    }

    case ORE_COMPLEMENT: {
        nba_t inner;
        if (!ore_to_nba(expr->left, alphabet_size, &inner)) return false;
        return nba_complement(&inner, out);
    }

    case ORE_OMEGA: {
        const simple_dfa_t *r_dfa = simple_dfa_get(expr->dfa_id);
        if (!r_dfa) return false;

        int state_map[MAX_DFA_STATES];
        for (int i = 0; i < r_dfa->nstates; i++) {
            state_map[i] = nba_add_state(out,
                (i == r_dfa->initial),
                (i == r_dfa->initial),
                "r");
        }

        for (int q = 0; q < r_dfa->nstates; q++) {
            for (size_t s = 0; s < alphabet_size; s++) {
                int t = r_dfa->trans[q][s];
                if (t < 0) continue;
                nba_add_transition(out, state_map[q],
                                   (uint8_t)s, state_map[t]);
                if (r_dfa->accepting[t]) {
                    nba_add_transition(out, state_map[q],
                                       (uint8_t)s,
                                       state_map[r_dfa->initial]);
                }
            }
        }
        return true;
    }

    case ORE_CONCAT: {
        const simple_dfa_t *r_dfa = simple_dfa_get(expr->dfa_id);
        if (!r_dfa) return false;

        nba_t suffix;
        if (!ore_to_nba(expr->right, alphabet_size, &suffix)) return false;

        int dfa_map[MAX_DFA_STATES];
        for (int i = 0; i < r_dfa->nstates; i++) {
            dfa_map[i] = nba_add_state(out,
                (i == r_dfa->initial), false, "cd");
        }

        int sf_off = out->nstates;
        for (int i = 0; i < suffix.nstates; i++) {
            nba_add_state(out, false,
                suffix.states[i].is_accepting,
                suffix.states[i].name);
        }

        for (int ti = 0; ti < suffix.ntrans; ti++) {
            nba_add_transition(out,
                sf_off + suffix.transitions[ti].src,
                suffix.transitions[ti].sym,
                sf_off + suffix.transitions[ti].dst);
        }

        for (int q = 0; q < r_dfa->nstates; q++) {
            for (size_t s = 0; s < alphabet_size; s++) {
                int t = r_dfa->trans[q][s];
                if (t < 0) continue;
                nba_add_transition(out, dfa_map[q], (uint8_t)s, dfa_map[t]);
                if (r_dfa->accepting[t]) {
                    nba_add_transition(out, dfa_map[q], (uint8_t)s,
                                       sf_off + suffix.initial);
                }
            }
        }
        return true;
    }

    default:
        return false;
    }
}

/* ---- Decision Procedures ---- */

bool ore_membership(const ore_node_t *expr, const omega_word *w)
{
    if (!expr || !w) return false;
    nba_t nba;
    if (!ore_to_nba(expr, OR_MAX_ALPHABET, &nba)) return false;
    return nba_accepts(&nba, w);
}

bool ore_is_empty(const ore_node_t *expr)
{
    if (!expr) return true;
    nba_t nba;
    if (!ore_to_nba(expr, OR_MAX_ALPHABET, &nba)) return true;
    return nba_is_empty(&nba);
}

bool ore_is_universal(const ore_node_t *expr, size_t alphabet_size)
{
    if (!expr) return false;
    nba_t nba;
    if (!ore_to_nba(expr, alphabet_size, &nba)) return false;
    return nba_is_universal(&nba);
}

bool ore_language_equal(const ore_node_t *e1, const ore_node_t *e2,
                         size_t alphabet_size)
{
    if (!e1 || !e2) return false;
    nba_t a1, a2;
    if (!ore_to_nba(e1, alphabet_size, &a1)) return false;
    if (!ore_to_nba(e2, alphabet_size, &a2)) return false;
    return nba_includes(&a1, &a2) && nba_includes(&a2, &a1);
}
