/**
 * ctl_modelcheck.h - CTL Model Checking Algorithm
 *
 * Computes SAT_M(phi) = { s in S | M, s |= phi } for CTL state formula phi
 * on Kripke structure M, using the explicit-state labeling algorithm of
 * Clarke, Emerson, and Sistla (1986).
 *
 * Algorithm: process subformulas bottom-up
 *   - Atomic props p: SAT(p) = { s | p in L(s) }
 *   - Boolean connectives: set operations on SAT sets
 *   - EX phi: pre_exists(SAT(phi))
 *   - EG phi: nu Z. SAT(phi) AND pre_exists(Z)  (greatest fixpoint)
 *   - EU phi psi: mu Z. SAT(psi) OR (SAT(phi) AND pre_exists(Z))
 *   - Other temporal ops reduced to EX, EG, EU
 *
 * Complexity: O(|phi| * (|S| + |R|))
 *
 * Knowledge: L2 (Core Concepts), L4 (Fundamental Laws), L5 (Algorithms)
 * Reference: Clarke, Emerson, Sistla (1986) ACM TOPLAS
 *            Baier & Katoen (2008), Chapter 6.4
 */

#ifndef CTL_MODELCHECK_H
#define CTL_MODELCHECK_H

#include "ctl_ast.h"
#include "ctl_kripke.h"

/* --- Model Checking Result --- */

typedef enum {
    CTL_MC_SATISFIED = 0,
    CTL_MC_VIOLATED = 1,
    CTL_MC_ERROR   = -1
} ctl_mc_result;

/* --- Model Checking Context --- */

typedef struct {
    const ctl_kripke *kripke;
    ctl_state_set **sat_cache;
    int cache_capacity;
    int total_states;
    ctl_state_id *witness_path;
    int witness_length;
} ctl_mc_context;

/* --- Main API --- */

/** Model check phi on Kripke structure k. */
ctl_mc_result ctl_model_check(const ctl_kripke *k, const ctl_formula *phi,
                               ctl_mc_context **ctx_out);

/** Model check from a specific state s. */
ctl_mc_result ctl_model_check_state(const ctl_kripke *k, ctl_state_id s,
                                     const ctl_formula *phi,
                                     ctl_mc_context **ctx_out);

/** Get SAT set for a subformula from context. */
const ctl_state_set *ctl_mc_get_sat(const ctl_mc_context *ctx,
                                     const ctl_formula *sub);

/** Destroy model checking context. */
void ctl_mc_context_destroy(ctl_mc_context *ctx);

/* --- Label Computation (Exposed for Testing) --- */

void ctl_label_ex(const ctl_kripke *k, ctl_state_set *result,
                  const ctl_state_set *sat_phi);

void ctl_label_eg(const ctl_kripke *k, ctl_state_set *result,
                  const ctl_state_set *sat_phi);

void ctl_label_eu(const ctl_kripke *k, ctl_state_set *result,
                  const ctl_state_set *sat_phi,
                  const ctl_state_set *sat_psi);

void ctl_label_ax(const ctl_kripke *k, ctl_state_set *result,
                  const ctl_state_set *sat_phi);

void ctl_label_ef(const ctl_kripke *k, ctl_state_set *result,
                  const ctl_state_set *sat_phi);

void ctl_label_af(const ctl_kripke *k, ctl_state_set *result,
                  const ctl_state_set *sat_phi);

void ctl_label_ag(const ctl_kripke *k, ctl_state_set *result,
                  const ctl_state_set *sat_phi);

void ctl_label_au(const ctl_kripke *k, ctl_state_set *result,
                  const ctl_state_set *sat_phi,
                  const ctl_state_set *sat_psi);

void ctl_label_er(const ctl_kripke *k, ctl_state_set *result,
                  const ctl_state_set *sat_phi,
                  const ctl_state_set *sat_psi);

void ctl_label_ar(const ctl_kripke *k, ctl_state_set *result,
                  const ctl_state_set *sat_phi,
                  const ctl_state_set *sat_psi);

/* --- Counterexample / Witness --- */

typedef enum {
    CTL_CEX_NONE = 0, CTL_CEX_PATH, CTL_CEX_LOOP, CTL_CEX_PREFIX
} ctl_cex_type;

typedef struct {
    ctl_cex_type type;
    ctl_state_id *states;
    int length;
    int loop_start;
    char *description;
} ctl_counterexample;

ctl_counterexample *ctl_generate_counterexample(const ctl_mc_context *ctx,
                                                 const ctl_formula *phi);
void ctl_counterexample_destroy(ctl_counterexample *cex);
void ctl_counterexample_print(const ctl_counterexample *cex);
ctl_counterexample *ctl_generate_witness(const ctl_mc_context *ctx,
                                          const ctl_formula *phi);

/* --- Bounded Model Checking (L8) --- */

ctl_mc_result ctl_bounded_model_check(const ctl_kripke *k,
                                       const ctl_formula *phi,
                                       int bound,
                                       ctl_mc_context **ctx_out);

/* --- Fair CTL Model Checking (L8) --- */

typedef struct {
    int nconstraints;
    ctl_state_set **constraints;
} ctl_fairness_constraint;

ctl_fairness_constraint *ctl_fairness_create(int nconstraints);
void ctl_fairness_destroy(ctl_fairness_constraint *fc);
ctl_mc_result ctl_fair_model_check(const ctl_kripke *k,
                                    const ctl_formula *phi,
                                    const ctl_fairness_constraint *fc,
                                    ctl_mc_context **ctx_out);

#endif /* CTL_MODELCHECK_H */
