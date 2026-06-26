/**
 * ctl_sat.h - CTL Satisfiability and Validity Checking
 *
 * CTL-SAT: does there exist Kripke structure M and state s with M,s |= phi?
 * CTL satisfiability is EXPTIME-complete (Emerson, Jutla 1988).
 * LTL satisfiability is PSPACE-complete. This gap shows branching vs linear.
 *
 * Decision procedure: tableau with small model property.
 * If phi is satisfiable, it has a model of size at most 2^O(|phi|).
 *
 * Knowledge: L1 (Definitions), L5 (Algorithms)
 * Reference: Emerson, Jutla (FOCS 1988)
 *            Emerson, Halpern (STOC 1982)
 */

#ifndef CTL_SAT_H
#define CTL_SAT_H

#include "ctl_ast.h"
#include "ctl_kripke.h"

int ctl_is_valid(const ctl_formula *phi);
int ctl_is_satisfiable(const ctl_formula *phi);
int ctl_is_contradiction(const ctl_formula *phi);
ctl_kripke *ctl_generate_model(const ctl_formula *phi, ctl_state_id *witness_state);
int ctl_compare_strength(const ctl_formula *phi, const ctl_formula *psi);

typedef enum {
    CTL_FRAG_FULL, CTL_FRAG_ACTL, CTL_FRAG_ECTL,
    CTL_FRAG_CTL_X, CTL_FRAG_CTL_PLUS
} ctl_fragment;

ctl_fragment ctl_classify(const ctl_formula *phi);
const char *ctl_fragment_complexity(ctl_fragment frag);
int ctl_max_subformulas(ctl_fragment frag, int formula_size);

#endif /* CTL_SAT_H */