/**
 * ctlsmc.h — CTL Symbolic Model Checking
 *
 * Implements symbolic model checking for Computation Tree Logic (CTL)
 * on Kripke structures using OBDDs for state set representation.
 *
 * Knowledge Coverage:
 *   L1: CTL syntax: state formulas and path formulas
 *       CTL operators: EX, EF, EG, EU, AX, AF, AG, AU
 *   L2: Fixpoint characterization of CTL operators
 *       Least fixpoint (mu): EU, EF, AF
 *       Greatest fixpoint (nu): EG, AG, AU
 *   L4: CTL model checking is in O(|Phi| * (|S| + |R|)) for explicit,
 *       O(|Phi| * |BDD|) for symbolic (Clarke, Emerson, Sistla 1986)
 *   L5: Symbolic CTL algorithm (Burch, Clarke, McMillan, Dill 1992)
 *       using OBDD-based fixpoint computation
 *   L6: Mutual exclusion, safety, liveness verification
 *   L7: Hardware verification: counter circuits, bus protocols
 *
 * References:
 *   Clarke, E.M., Emerson, E.A., & Sistla, A.P. (1986).
 *     "Automatic verification of finite-state concurrent systems
 *      using temporal logic specifications." ACM TOPLAS, 8(2), 244-263.
 *   Burch, J.R., Clarke, E.M., McMillan, K.L., Dill, D.L., & Hwang, L.J. (1992).
 *     "Symbolic model checking: 10^20 states and beyond."
 *     Information and Computation, 98(2), 142-170.
 */

#ifndef CTLSMC_H
#define CTLSMC_H

#include <stdint.h>
#include <stdbool.h>
#include "obdd.h"
#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * L1: CTL Formula AST
 * ============================================================ */

/**
 * ctl_node_type_t — CTL formula node types.
 *
 * CTL syntax (Clarke, Emerson, Sistla 1986):
 *   State formulas ::= p | !Phi | Phi && Phi | EX Phi | EG Phi |
 *                      E[Phi U Phi] | AX Phi | AG Phi | A[Phi U Phi]
 */
typedef enum {
    CTL_TRUE,           /* constant true */
    CTL_FALSE,          /* constant false */
    CTL_ATOM,           /* atomic proposition (by index) */
    CTL_NOT,            /* logical negation */
    CTL_AND,            /* logical conjunction */
    CTL_OR,             /* logical disjunction */
    CTL_EX,             /* exists next state */
    CTL_EF,             /* exists future (reachable) */
    CTL_EG,             /* exists globally (always) */
    CTL_EU,             /* exists until */
    CTL_AX,             /* forall next state */
    CTL_AF,             /* forall future (inevitable) */
    CTL_AG,             /* forall globally */
    CTL_AU              /* forall until */
} ctl_node_type_t;

/**
 * ctl_formula_t — A CTL formula as a syntax tree.
 *
 * Leaf nodes (TRUE, FALSE, ATOM) have no children.
 * Unary nodes (NOT, EX, EF, EG, AX, AF, AG) have one child.
 * Binary nodes (AND, OR, EU, AU) have two children.
 */
typedef struct ctl_formula_s {
    ctl_node_type_t     type;
    int32_t             atom_index;
    struct ctl_formula_s *left;
    struct ctl_formula_s *right;
    obdd_ref_t          cached_result;
    bool                evaluated;
} ctl_formula_t;

/* ============================================================
 * L1: CTL Formula Construction
 * ============================================================ */

ctl_formula_t *ctl_true(void);
ctl_formula_t *ctl_false(void);
ctl_formula_t *ctl_atom(int32_t prop_index);
ctl_formula_t *ctl_not(ctl_formula_t *phi);
ctl_formula_t *ctl_and(ctl_formula_t *phi, ctl_formula_t *psi);
ctl_formula_t *ctl_or(ctl_formula_t *phi, ctl_formula_t *psi);
ctl_formula_t *ctl_ex(ctl_formula_t *phi);
ctl_formula_t *ctl_ef(ctl_formula_t *phi);
ctl_formula_t *ctl_eg(ctl_formula_t *phi);
ctl_formula_t *ctl_eu(ctl_formula_t *phi, ctl_formula_t *psi);
ctl_formula_t *ctl_ax(ctl_formula_t *phi);
ctl_formula_t *ctl_af(ctl_formula_t *phi);
ctl_formula_t *ctl_ag(ctl_formula_t *phi);
ctl_formula_t *ctl_au(ctl_formula_t *phi, ctl_formula_t *psi);

void ctl_free(ctl_formula_t *phi);

/* ============================================================
 * L5: CTL Symbolic Model Checking
 * ============================================================ */

/**
 * ctl_model_check — Evaluate a CTL formula on a Kripke structure.
 *
 * Returns the characteristic function (as OBDD) of the set of states
 * satisfying the formula.
 *
 * Algorithm (Clarke, Emerson, Sistla 1986 / Burch et al. 1992):
 *   For each subformula psi of phi (bottom-up):
 *     - Atomic prop p: use precomputed label set
 *     - Boolean connectives: OBDD boolean operations
 *     - EX psi: PreImage(psi_set, R)
 *     - EG psi: nu Z. psi_set && EX Z
 *     - EU(psi1, psi2): mu Z. psi2_set || (psi1_set && EX Z)
 *     - Others: derived from above
 *
 * Complexity: O(|Phi| * |BDD_ops|)
 *
 * @param K    Kripke structure
 * @param phi  CTL formula to check
 * @return     OBDD representing states satisfying phi
 */
obdd_ref_t ctl_model_check(const kripke_t *K, ctl_formula_t *phi);

/**
 * ctl_check_initial — Check if all initial states satisfy phi.
 */
bool ctl_check_initial(const kripke_t *K, ctl_formula_t *phi);

/**
 * ctl_print_counterexample — Print a counterexample trace if one exists.
 */
void ctl_print_counterexample(const kripke_t *K, ctl_formula_t *phi);

/**
 * ctl_satisfying_states_count — Return number of states satisfying phi.
 */
int32_t ctl_satisfying_states_count(const kripke_t *K, ctl_formula_t *phi);

#ifdef __cplusplus
}
#endif

#endif /* CTLSMC_H */