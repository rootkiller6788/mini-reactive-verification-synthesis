/**
 * ltl_formula.h -- Linear Temporal Logic (LTL) Definitions
 *
 * LTL is the standard specification language for omega-regular properties.
 * Every LTL formula defines an omega-regular language (Wolper, Vardi & Wolper).
 *
 * Syntax (Pnueli 1977):
 *   phi ::= p | !phi | phi & phi | phi | phi | X phi | phi U phi
 *
 * Derived operators:
 *   F phi = true U phi        (eventually)
 *   G phi = !F !phi           (always)
 *   phi R psi = !(!phi U !psi) (release)
 *   phi W psi = (phi U psi) | G phi (weak until)
 *
 * Semantics over omega-words w in Sigma^omega:
 *   w,i |= p         iff p in w(i)
 *   w,i |= !phi       iff w,i |/= phi
 *   w,i |= phi & psi iff w,i |= phi and w,i |= psi
 *   w,i |= X phi     iff w,i+1 |= phi
 *   w,i |= phi U psi iff exists k>=i: w,k |= psi and forall j in [i,k): w,j |= phi
 *
 * Reference: Pnueli (1977) "The Temporal Logic of Programs"
 *            Vardi & Wolper (1986) "An Automata-Theoretic Approach..."
 */

#ifndef LTL_FORMULA_H
#define LTL_FORMULA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#define LTL_MAX_SUBFORMULAS 128
#define LTL_MAX_ATOMIC      32

/* ---- LTL Syntax ---- */

typedef enum {
    LTL_TRUE,
    LTL_FALSE,
    LTL_ATOM,       /* atomic proposition p (index into prop_names) */
    LTL_NOT,        /* !phi */
    LTL_AND,        /* phi & psi */
    LTL_OR,         /* phi | psi */
    LTL_IMPLIES,    /* phi -> psi */
    LTL_NEXT,       /* X phi */
    LTL_UNTIL,      /* phi U psi */
    LTL_RELEASE,    /* phi R psi */
    LTL_EVENTUALLY, /* F phi */
    LTL_ALWAYS,     /* G phi */
    LTL_WEAK_UNTIL  /* phi W psi */
} ltl_op_t;

typedef struct ltl_node ltl_node_t;
struct ltl_node {
    ltl_op_t    op;
    int         atom_id;     /* For LTL_ATOM: index into atom table */
    ltl_node_t *left;
    ltl_node_t *right;
    int         id;          /* Unique node id for hashing */
};

/* ---- LTL Formula API ---- */

ltl_node_t *ltl_make_atom(int atom_id);
ltl_node_t *ltl_make_not(ltl_node_t *phi);
ltl_node_t *ltl_make_and(ltl_node_t *left, ltl_node_t *right);
ltl_node_t *ltl_make_or(ltl_node_t *left, ltl_node_t *right);
ltl_node_t *ltl_make_implies(ltl_node_t *left, ltl_node_t *right);
ltl_node_t *ltl_make_next(ltl_node_t *phi);
ltl_node_t *ltl_make_until(ltl_node_t *left, ltl_node_t *right);
ltl_node_t *ltl_make_release(ltl_node_t *left, ltl_node_t *right);
ltl_node_t *ltl_make_eventually(ltl_node_t *phi);
ltl_node_t *ltl_make_always(ltl_node_t *phi);
ltl_node_t *ltl_make_weak_until(ltl_node_t *left, ltl_node_t *right);
ltl_node_t *ltl_make_true(void);
ltl_node_t *ltl_make_false(void);

void ltl_free(ltl_node_t *root);

/**
 * ltl_eval -- Evaluate LTL formula on an omega-word at position i.
 *
 * This is the standard Kripke semantics: w,i |= phi.
 * The assignment function maps positions to sets of atomic propositions.
 *
 * @param assignment: function returning true if prop p holds at position pos
 * @param ctx: user context passed to assignment function
 */
typedef bool (*ltl_assign_fn)(int atom_id, size_t pos, void *ctx);
bool ltl_eval(const ltl_node_t *phi, size_t pos,
              ltl_assign_fn assign, void *ctx);

/**
 * ltl_size -- Number of nodes in the formula tree.
 */
size_t ltl_size(const ltl_node_t *root);

/**
 * ltl_print -- Print LTL formula in human-readable form.
 */
void ltl_print(const ltl_node_t *root, FILE *fp);

/**
 * ltl_clone -- Deep copy of LTL formula.
 */
ltl_node_t *ltl_clone(const ltl_node_t *root);

/**
 * ltl_to_nba -- Translate LTL formula to an equivalent NBA.
 *
 * Theorem (Vardi & Wolper 1986): For every LTL formula phi over AP,
 * there exists an NBA A_phi such that L(A_phi) = {w | w,0 |= phi}.
 * The NBA has at most 2^O(|phi|) states.
 *
 * Note: The nba_t type is defined in buchi_automaton.h.
 * Include that header before calling this function.
 */
#include "buchi_automaton.h"
bool ltl_to_nba(const ltl_node_t *phi, nba_t *nba);

#endif /* LTL_FORMULA_H */
