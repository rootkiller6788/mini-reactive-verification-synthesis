/**
 * model.h — Kripke Structures for Symbolic Model Checking
 *
 * Defines the finite-state transition system model (Kripke structure)
 * and its symbolic representation using OBDDs.
 *
 * Knowledge Coverage:
 *   L1: Kripke structure M = (S, S0, R, L, AP), state, transition, labeling
 *   L2: Symbolic representation: characteristic functions for S0, R, and
 *       each proposition p in AP encoded as boolean functions
 *   L3: State encoding: state -> boolean vector in {0,1}^n
 *       Transition encoding: prime variable convention (x, x')
 *   L6: Canonical problems: reachability, safety, liveness on Kripke structures
 *
 * References:
 *   Clarke, E.M., Grumberg, O., & Peled, D.A. (1999). "Model Checking."
 *     MIT Press. Chapters 2-3.
 *   McMillan, K.L. (1993). "Symbolic Model Checking." Kluwer.
 */

#ifndef MODEL_H
#define MODEL_H

#include <stdint.h>
#include <stdbool.h>
#include "obdd.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * L1: Kripke Structure Definition
 * ============================================================ */

/**
 * kripke_t — A Kripke structure for model checking.
 *
 * M = (S, S0, R, L, AP) where:
 *   S  is a finite set of states (encoded as {0,1}^n)
 *   S0 subset S is the set of initial states
 *   R subset S x S is the transition relation
 *   L: S -> 2^{AP} labels each state with atomic propositions
 *   AP is the set of atomic propositions
 *
 * Symbolic encoding:
 *   Each state s in S is encoded as an n-bit vector.
 *   Two sets of variables: current-state (x_1,...,x_n) and
 *   next-state (x'_1,...,x'_n), with prime variables at positions n..2n-1.
 */
typedef struct {
    int32_t        num_state_bits;
    int32_t        num_primed_bits;
    obdd_manager_t *mgr;
    obdd_ref_t      init_states;
    obdd_ref_t      trans_rel;
    int32_t        num_props;
    char          **prop_names;
    obdd_ref_t     *labels;
    int32_t         labels_capacity;
    int32_t         var_offset;
} kripke_t;

/* ============================================================
 * L3: Kripke Structure Construction
 * ============================================================ */

kripke_t *kripke_create(int32_t num_state_bits, int32_t num_props);
void kripke_destroy(kripke_t *K);

void kripke_add_initial_state(kripke_t *K, uint64_t state_bits);
void kripke_add_transition(kripke_t *K, uint64_t src, uint64_t dst);
void kripke_set_label(kripke_t *K, int32_t prop, const char *prop_name,
                      obdd_ref_t states);
void kripke_set_label_mask(kripke_t *K, int32_t prop, const char *prop_name,
                           uint64_t mask, int32_t num_states);

/* ============================================================
 * L3: Symbolic State Set Operations
 * ============================================================ */

obdd_ref_t kripke_encode_state(const kripke_t *K, uint64_t state_bits);
obdd_ref_t kripke_encode_prime_state(const kripke_t *K, uint64_t state_bits);
obdd_ref_t kripke_state_union(const kripke_t *K, obdd_ref_t A, obdd_ref_t B);
obdd_ref_t kripke_state_intersect(const kripke_t *K, obdd_ref_t A, obdd_ref_t B);
obdd_ref_t kripke_state_complement(const kripke_t *K, obdd_ref_t A);

/* ============================================================
 * L3: Transition Relation Operations
 * ============================================================ */

obdd_ref_t kripke_image(const kripke_t *K, obdd_ref_t A);
obdd_ref_t kripke_preimage(const kripke_t *K, obdd_ref_t B);
obdd_ref_t kripke_rename_to_prime(const kripke_t *K, obdd_ref_t f);
obdd_ref_t kripke_rename_to_unprime(const kripke_t *K, obdd_ref_t f);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_H */
