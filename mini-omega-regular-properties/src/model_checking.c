/**
 * model_checking.c -- LTL Model Checking via NBA Emptiness
 *
 * The automata-theoretic approach to LTL model checking (Vardi & Wolper 1986):
 *   System |= phi  iff  L(A_sys) subset L(A_phi)
 *                 iff  L(A_sys) cap L(A_not_phi) = empty
 *                 iff  L(A_sys x A_not_phi) = empty
 *
 * Steps:
 *   1. Negate the property: psi = not phi
 *   2. Build NBA for psi: A_psi = ltl_to_nba(psi)
 *   3. Build NBA for system: A_sys (from Kripke structure)
 *   4. Build product NBA: A_product = nba_intersection(A_sys, A_psi)
 *   5. Check emptiness: nba_is_empty(A_product)
 *       - If empty: System |= phi (property holds)
 *       - If non-empty: counterexample found
 *
 * References:
 *   - Vardi & Wolper (1986) "An automata-theoretic approach..."
 *   - Clarke, Grumberg & Peled (1999) "Model Checking"
 *   - Baier & Katoen (2008) "Principles of Model Checking"
 *
 * Knowledge: L7 model checking (NBA-based), L5 emptiness as decision procedure.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ltl_formula.h"
#include "buchi_automaton.h"
#include "omega_regular.h"

/**
 * kripke_structure -- A simple Kripke structure M = (S, S0, R, L)
 * where:
 *   S:    finite set of states
 *   S0:   initial states (subset S)
 *   R:    S x S transition relation
 *   L:    S -> 2^AP labeling function (maps each state to set of atomic props)
 */
typedef struct {
    int      nstates;
    bool     initial[64];
    bool     trans[64][64];
    uint64_t labels[64];    /* Bitmask of atomic propositions true at each state */
    int      nprops;        /* Number of atomic propositions */
} kripke_t;

/**
 * kripke_to_nba -- Convert a Kripke structure to an NBA.
 *
 * The NBA has the same states as the Kripke structure, with the
 * same transitions, and all states are accepting (since we're
 * modeling the system's infinite behavior, every infinite path
 * through the Kripke structure is valid).
 *
 * The alphabet is 2^AP (represented as bitmask indices).
 * We use a simplified mapping: each symbol corresponds to
 * a specific subset of AP (power set, up to 2^MAX_ATOMIC).
 */
bool kripke_to_nba(const kripke_t *ks, nba_t *nba_out)
{
    if (!ks || !nba_out) return false;
    /* For simplicity, the alphabet is the label bitmask.
     * We use values 0..2^nprops-1 as symbols. */
    size_t al_size = (1ULL << ks->nprops);
    if (al_size > OR_MAX_ALPHABET) al_size = OR_MAX_ALPHABET;

    nba_init(nba_out, al_size);

    /* Copy states */
    for (int i = 0; i < ks->nstates; i++) {
        char name[32];
        snprintf(name, 32, "s%d", i);
        nba_add_state(nba_out, ks->initial[i], true, name);
    }

    /* Add transitions: the symbol read must match the label of the
     * target state (state-based labeling semantics). */
    for (int i = 0; i < ks->nstates; i++) {
        for (int j = 0; j < ks->nstates; j++) {
            if (!ks->trans[i][j]) continue;
            /* The symbol is the label of the target state j */
            uint8_t sym = (uint8_t)(ks->labels[j] & (al_size - 1));
            nba_add_transition(nba_out, i, sym, j);
        }
    }

    return true;
}

/**
 * model_check_ltl -- Check if a Kripke structure satisfies an LTL formula.
 *
 * Returns:
 *   true  if M, s0 |= phi (the property holds)
 *   false if M, s0 |/= phi (counterexample exists)
 *
 * Algorithm: NBA-based model checking (Vardi & Wolper 1986).
 */
bool model_check_ltl(const kripke_t *ks, const ltl_node_t *phi)
{
    if (!ks || !phi) return false;

    /* Step 1: Build NBA for the negation of phi */
    ltl_node_t *not_phi = ltl_make_not(ltl_clone(phi));
    nba_t a_not_phi;
    if (!ltl_to_nba(not_phi, &a_not_phi)) {
        ltl_free(not_phi);
        return false;
    }
    ltl_free(not_phi);

    /* Step 2: Build NBA for the Kripke structure */
    nba_t a_sys;
    if (!kripke_to_nba(ks, &a_sys)) return false;

    /* Step 3: Build product NBA */
    nba_t product;
    if (!nba_intersection(&a_sys, &a_not_phi, &product)) return false;

    /* Step 4: Check emptiness */
    bool empty = nba_is_empty(&product);

    /* If empty, no counterexample exists -> property holds */
    return empty;
}

/**
 * model_check_omega_regular -- Check if an NBA satisfies an
 * omega-regular property specified by another NBA.
 *
 * This is the general case: A_sys |= A_spec iff L(A_sys) subset L(A_spec).
 */
bool model_check_nba(const nba_t *sys, const nba_t *spec)
{
    if (!sys || !spec) return false;
    return nba_includes(sys, spec);
}
