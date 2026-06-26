/**
 * omega_parity.c -- Parity and Other Acceptance Conditions
 *
 * While Buechi automata suffice for all omega-regular languages,
 * other acceptance conditions offer advantages for determinization
 * and complementation. This module introduces:
 *
 *   - Parity condition (Mostowski 1984): each state has a priority
 *     (color) in {0..k}. A run is accepting iff the minimum priority
 *     seen infinitely often is even.
 *   - Rabin condition: pairs (E_i, F_i); accepting iff exists i:
 *     Inf(r) cap E_i = empty AND Inf(r) cap F_i != empty
 *   - Streett condition: dual of Rabin
 *
 * Theorem (McNaughton 1966): Every NBA can be transformed into an
 * equivalent deterministic Muller automaton, which can be expressed
 * as a deterministic parity automaton.
 *
 * Theorem (Safra 1988): Deterministic parity automata are as
 * expressive as NBAs, with 2^O(n log n) states in the worst case.
 *
 * Knowledge: L8 acceptance conditions, L8 McNaughton's theorem,
 *            L8 parity games relationship.
 */

#include <stdlib.h>
#include <string.h>
#include "buchi_automaton.h"

/**
 * nba_to_parity_outline -- Outline the conversion from NBA to
 * deterministic parity automaton.
 *
 * This function does not perform the full Safra construction but
 * provides the signature and documentation for the conversion.
 *
 * A parity automaton is: (Q, Sigma, delta, q0, pr) where
 *   pr: Q -> {0, 1, ..., k} assigns a priority/color to each state.
 * A run is accepting iff liminf(pr(r(i))) is even.
 *
 * The conversion NBA -> DPA proceeds via:
 *   1. NBA -> Nondeterministic parity automaton (trivial: priority 1
 *      for non-accepting, 2 for accepting)
 *   2. NPA -> DPA via Safra's determinization
 */
bool nba_is_parity_equivalent(const nba_t *nba)
{
    /* Every NBA is equivalent to some parity automaton.
     * This function always returns true (Safra's theorem). */
    (void)nba;
    return true;
}

/**
 * acceptance_type -- Enumerate common acceptance conditions.
 */
typedef enum {
    ACC_BUCHI,      /* Inf(r) cap F != empty */
    ACC_COBUCHI,    /* Inf(r) subset F (finitely often outside F) */
    ACC_PARITY,     /* liminf(priority) is even */
    ACC_RABIN,      /* exists i: Inf(r) cap E_i = empty, Inf(r) cap F_i != empty */
    ACC_STREETT,    /* forall i: Inf(r) cap E_i != empty or Inf(r) cap F_i = empty */
    ACC_MULLER      /* Inf(r) in F where F is a family of state sets */
} acc_type_t;

/**
 * acceptance_type_name -- Return human-readable name for acceptance type.
 */
const char *acceptance_type_name(acc_type_t t)
{
    switch (t) {
    case ACC_BUCHI:   return "Buechi";
    case ACC_COBUCHI: return "Co-Buechi";
    case ACC_PARITY:  return "Parity (Mostowski)";
    case ACC_RABIN:   return "Rabin (Pairs)";
    case ACC_STREETT: return "Streett (Dual-Rabin)";
    case ACC_MULLER:  return "Muller (Family)";
    default:          return "Unknown";
    }
}

/**
 * acceptance_expressiveness -- Expressiveness hierarchy of acceptance
 * conditions on deterministic automata.
 *
 * DFA (finite words): DBA < DCA < DPA = DRabin = DStreett = DMuller
 * NFA (omega words): NBA = NCA = NPA = NRabin = NStreett = NMuller
 *
 * On deterministic automata:
 *   - DBA < DPA (strictly: L = (a|b)*.a^omega is not DBA-recognizable)
 *   - DPA = DMuller = DRabin = DStreett (all omega-regular)
 */
void acceptance_expressiveness(void)
{
    /* This function exists to document the expressiveness hierarchy.
     * Key facts:
     *   1. DBA cannot recognize "infinitely often a" over {a,b}
     *      (Landweber 1969)
     *   2. DPA can recognize all omega-regular languages (Safra 1988)
     *   3. DPA equivalent to DRabin/ DStreett / DMuller
     *      (polynomial translations between them)
     */
}
