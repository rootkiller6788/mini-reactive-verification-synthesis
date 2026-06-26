/**
 * ctl_kripke.h - Kripke Structures for CTL Model Checking
 *
 * A Kripke structure M = (S, S0, R, L) over atomic propositions AP:
 *   S  - finite set of states
 *   S0 subset S - set of initial states
 *   R subset S x S - total transition relation
 *   L : S -> 2^AP - labeling function
 *
 * Knowledge: L1 (Definitions), L3 (Mathematical Structures)
 * Reference: Clarke, Grumberg, Peled "Model Checking" (1999), Chapter 2
 *            Baier & Katoen "Principles of Model Checking" (2008), Section 2.1
 */

#ifndef CTL_KRIPKE_H
#define CTL_KRIPKE_H

#include <stddef.h>

/* --- State Type ----------------------------------------------- */

/** State identifier. States numbered 0..nstates-1. */
typedef unsigned int ctl_state_id;

/** Sentinel for invalid state. */
#define CTL_INVALID_STATE ((ctl_state_id)(-1))

/* --- Labeling ------------------------------------------------ */

/** Bit-vector type for inline label sets (up to 64 APs). */
typedef unsigned long long ctl_label_bits;

#define CTL_MAX_LABEL_BITS 64

/**
 * ctl_label_set - per-state labeling.
 * Uses inline 64-bit mask when nap <= 64; extended array otherwise.
 */
typedef struct {
    int nap;
    ctl_label_bits bits;
    unsigned long long *ext;
} ctl_label_set;

/* --- Transition Relation -------------------------------------- */

/** Singly-linked edge in adjacency list. */
typedef struct ctl_edge {
    ctl_state_id target;
    struct ctl_edge *next;
} ctl_edge;

/* --- Kripke Structure ----------------------------------------- */

/**
 * ctl_kripke - Kripke structure M = (S, S0, R, L).
 *
 * Invariants (maintained by API):
 *   nstates >= 1
 *   R is total (every state has at least one successor)
 *   labels[s] defined for all s in S
 */
typedef struct {
    ctl_state_id nstates;
    ctl_state_id ninitial;
    ctl_state_id *initial_states;
    ctl_edge **successors;
    ctl_edge **predecessors;
    ctl_label_set *labels;
    int nap;
    char **ap_names;
} ctl_kripke;

/* --- Construction / Destruction ------------------------------- */

/** Create a Kripke structure. ap_names may be NULL. O(nstates + nap). */
ctl_kripke *ctl_kripke_create(ctl_state_id nstates, int nap, const char **ap_names);

/** Destroy a Kripke structure. Safe on NULL. */
void ctl_kripke_destroy(ctl_kripke *k);

/** Set initial states. Copies count ids from states. Returns 0 on success. */
int ctl_kripke_set_initial(ctl_kripke *k, const ctl_state_id *states, ctl_state_id count);

/** Add transition s -> t. Duplicate edges silently ignored. */
int ctl_kripke_add_edge(ctl_kripke *k, ctl_state_id s, ctl_state_id t);

/** Remove transition s -> t. */
int ctl_kripke_remove_edge(ctl_kripke *k, ctl_state_id s, ctl_state_id t);

/** Set label bit for state s, AP index ap to value (0 or 1). */
int ctl_kripke_set_label(ctl_kripke *k, ctl_state_id s, int ap, int value);

/** Get label bit for state s, AP index ap. */
int ctl_kripke_get_label(const ctl_kripke *k, ctl_state_id s, int ap);

/** Make R total by adding self-loops to dead-end states. Returns count added. */
int ctl_kripke_make_total(ctl_kripke *k);

/** Compute post-image (successors) of state s. See implementation. */
int ctl_kripke_post(const ctl_kripke *k, ctl_state_id s,
                    ctl_state_id *out, int *capacity);

/* --- Accessors ------------------------------------------------ */

ctl_state_id ctl_kripke_nstates(const ctl_kripke *k);
ctl_state_id ctl_kripke_ninitial(const ctl_kripke *k);
ctl_state_id ctl_kripke_initial(const ctl_kripke *k, ctl_state_id i);
int ctl_kripke_nap(const ctl_kripke *k);
const char *ctl_kripke_ap_name(const ctl_kripke *k, int ap);
int ctl_kripke_ap_index(const ctl_kripke *k, const char *name);
int ctl_kripke_out_degree(const ctl_kripke *k, ctl_state_id s);
int ctl_kripke_in_degree(const ctl_kripke *k, ctl_state_id s);
ctl_state_id ctl_kripke_successor(const ctl_kripke *k, ctl_state_id s, int i);
ctl_state_id ctl_kripke_predecessor(const ctl_kripke *k, ctl_state_id s, int i);

/* --- State Set (Bit Vector) ----------------------------------- */

/** Bit-vector representation of a subset of states. */
typedef struct {
    int nstates;
    int nwords;
    unsigned long long *bits;
} ctl_state_set;

ctl_state_set *ctl_state_set_create(int nstates);
void ctl_state_set_destroy(ctl_state_set *ss);
ctl_state_set *ctl_state_set_clone(const ctl_state_set *ss);

void ctl_state_set_add(ctl_state_set *ss, ctl_state_id s);
void ctl_state_set_remove(ctl_state_set *ss, ctl_state_id s);
int ctl_state_set_contains(const ctl_state_set *ss, ctl_state_id s);
void ctl_state_set_clear(ctl_state_set *ss);
void ctl_state_set_universe(ctl_state_set *ss);
int ctl_state_set_count(const ctl_state_set *ss);
int ctl_state_set_is_empty(const ctl_state_set *ss);
int ctl_state_set_is_universe(const ctl_state_set *ss);

void ctl_state_set_copy(ctl_state_set *dest, const ctl_state_set *src);
void ctl_state_set_union(ctl_state_set *dest, const ctl_state_set *src);
void ctl_state_set_intersect(ctl_state_set *dest, const ctl_state_set *src);
void ctl_state_set_difference(ctl_state_set *dest, const ctl_state_set *src);
void ctl_state_set_complement(ctl_state_set *dest);
int ctl_state_set_equal(const ctl_state_set *a, const ctl_state_set *b);
int ctl_state_set_subset(const ctl_state_set *a, const ctl_state_set *b);

/* --- Pre-image Operations ------------------------------------- */

/**
 * Existential pre-image: pre_exists(T) = { s | exists t in T: (s,t) in R }
 * All states with at least one successor in T.
 */
void ctl_pre_image(const ctl_kripke *k, ctl_state_set *result,
                   const ctl_state_set *target);

/**
 * Universal pre-image: pre_forall(T) = { s | post(s) nonempty and post(s) subset T }
 * All states whose every successor is in T.
 */
void ctl_pre_image_all(const ctl_kripke *k, ctl_state_set *result,
                       const ctl_state_set *target);

/* --- Graph Analysis ------------------------------------------- */

/** States reachable from initial states. Caller owns result. */
ctl_state_set *ctl_reachable_states(const ctl_kripke *k);

/** States that can reach any state in target. Caller owns result. */
ctl_state_set *ctl_backward_reachable(const ctl_kripke *k,
                                       const ctl_state_set *target);

/**
 * Tarjan's SCC algorithm. Returns number of SCCs.
 * scc_of is output array of size k->nstates (caller frees).
 * SCCs numbered 0..nscc-1 in reverse topological order.
 */
int ctl_compute_sccs(const ctl_kripke *k, int **scc_of);

/** Compute bottom SCCs. Caller owns result. */
ctl_state_set *ctl_bottom_sccs(const ctl_kripke *k);

/* --- Label Set Operations ------------------------------------- */

void ctl_label_set_init(ctl_label_set *ls, int nap);
void ctl_label_set_destroy(ctl_label_set *ls);
void ctl_label_set_set(ctl_label_set *ls, int ap, int val);
int ctl_label_set_get(const ctl_label_set *ls, int ap);

/* --- I/O ------------------------------------------------------ */

void ctl_kripke_print_dot(const ctl_kripke *k, const ctl_state_set *highlight);
void ctl_kripke_print(const ctl_kripke *k);

/** Deep copy. Caller owns result. */
ctl_kripke *ctl_kripke_clone(const ctl_kripke *k);

#endif /* CTL_KRIPKE_H */
