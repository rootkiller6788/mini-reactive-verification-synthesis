/**
 * grg1_fixpoint.c — Fixpoint computation for state-set transformers
 *
 * Implements least and greatest fixpoint computation via Kleene iteration,
 * nested fixpoints (ν.μ) for GR(1) synthesis, and acceleration techniques
 * including widening/narrowing and worklist-based algorithms.
 *
 * The core mathematical foundation is the Knaster-Tarski theorem:
 * in a complete lattice (P(S), ⊆), every monotone function F has a
 * least fixpoint μF and greatest fixpoint νF.
 *
 * For finite state spaces, fixpoints are computable by iteration:
 *   μF = Fⁿ(∅) for n ≤ |S| (since ∅ ⊆ F(∅) ⊆ F²(∅) ⊆ ... ⊆ S)
 *   νF = Fⁿ(S) for n ≤ |S| (since S ⊇ F(S) ⊇ F²(S) ⊇ ... ⊇ ∅)
 *
 * Knowledge coverage:
 *   L1: Fixpoint definitions (Kleene, Knaster-Tarski)
 *   L2: Monotonicity, lattice theory
 *   L3: Complete lattice (P(S), ⊆), ascending/descending chains
 *   L4: Knaster-Tarski theorem, computational adequacy of iteration
 *   L5: Kleene iteration, nested fixpoint, worklist algorithms
 *   L8: Widening/narrowing (Abstract Interpretation)
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "grg1_fixpoint.h"
#include "grg1_types.h"

/* =========================================================================
 * Least Fixpoint (LFP) — Kleene iteration from ∅
 * ========================================================================*/

bool grg1_fixpoint_lfp(const grg1_region_t* initial,
                        void (*transformer)(const grg1_region_t* in,
                                             grg1_region_t* out,
                                             void* context),
                        void* context,
                        grg1_region_t* result,
                        int max_iter,
                        int* iterations) {
    if (!transformer || !result) return false;

    int n = result->num_states;
    int w = result->words_needed;

    /* Allocate working regions */
    grg1_region_t* current = grg1_region_alloc(n);
    grg1_region_t* next = grg1_region_alloc(n);
    if (!current || !next) {
        grg1_region_free(current);
        grg1_region_free(next);
        return false;
    }

    /* Start from initial (typically empty) */
    if (initial) {
        grg1_region_copy(current, initial);
    } else {
        grg1_region_clear(current);
    }

    int iter = 0;
    bool converged = false;

    while (iter < max_iter && !converged) {
        /* Compute F(current) */
        transformer(current, next, context);

        /* Check if next ⊆ current (we're looking for current ⊇ next,
         * i.e., current = next since we're ascending) */
        /* Actually for LFP: we go UP (∅ ⊆ F(∅) ⊆ F²(∅) ⊆ ...).
         * So we check if next ⊆ current (meaning Fⁱ⁺¹ ⊆ Fⁱ, i.e. fixpoint reached) */
        bool all_contained = true;
        for (int i = 0; i < w; i++) {
            if ((next->bits[i] & ~current->bits[i]) != 0) {
                all_contained = false;
                break;
            }
        }

        if (all_contained) {
            converged = true;
        } else {
            /* current = current ∪ next (monotonic ascent) */
            for (int i = 0; i < w; i++) {
                current->bits[i] |= next->bits[i];
            }
        }
        iter++;
    }

    grg1_region_copy(result, current);

    if (iterations) *iterations = iter;

    grg1_region_free(current);
    grg1_region_free(next);

    return converged;
}

/* =========================================================================
 * Greatest Fixpoint (GFP) — Kleene iteration from S
 * ========================================================================*/

bool grg1_fixpoint_gfp(const grg1_region_t* initial,
                        void (*transformer)(const grg1_region_t* in,
                                             grg1_region_t* out,
                                             void* context),
                        void* context,
                        grg1_region_t* result,
                        int max_iter,
                        int* iterations) {
    if (!transformer || !result) return false;

    int n = result->num_states;
    int w = result->words_needed;

    grg1_region_t* current = grg1_region_alloc(n);
    grg1_region_t* next = grg1_region_alloc(n);
    if (!current || !next) {
        grg1_region_free(current);
        grg1_region_free(next);
        return false;
    }

    /* Start from initial (typically full set) */
    if (initial) {
        grg1_region_copy(current, initial);
    } else {
        grg1_region_fill(current);
    }

    int iter = 0;
    bool converged = false;

    while (iter < max_iter && !converged) {
        /* Compute F(current) */
        transformer(current, next, context);

        /* For GFP: we go DOWN. Check if current ⊆ next
         * (meaning Fⁱ ⊆ Fⁱ⁺¹, i.e. post-fixpoint reached) */
        bool all_contained = true;
        for (int i = 0; i < w; i++) {
            if ((current->bits[i] & ~next->bits[i]) != 0) {
                all_contained = false;
                break;
            }
        }

        if (all_contained) {
            converged = true;
        } else {
            /* current = current ∩ next (monotonic descent) */
            for (int i = 0; i < w; i++) {
                current->bits[i] &= next->bits[i];
            }
        }
        iter++;
    }

    grg1_region_copy(result, current);

    if (iterations) *iterations = iter;

    grg1_region_free(current);
    grg1_region_free(next);

    return converged;
}

/* =========================================================================
 * Nested Fixpoint — νX. μY. F(X, Y)
 * ========================================================================*/

bool grg1_fixpoint_nested(
    const grg1_region_t* full_set,
    void (*inner_transformer)(const grg1_region_t* outer_approx,
                               const grg1_region_t* inner_approx,
                               grg1_region_t* out,
                               void* context),
    void* context,
    grg1_region_t* result,
    int outer_max_iter,
    int inner_max_iter,
    int* outer_iters,
    int* total_inner_iters) {
    if (!full_set || !inner_transformer || !result) return false;

    int n = result->num_states;
    int w = result->words_needed;

    /* Allocate regions */
    grg1_region_t* X = grg1_region_alloc(n);      /* Outer approximation */
    grg1_region_t* X_prev = grg1_region_alloc(n);
    grg1_region_t* Y = grg1_region_alloc(n);      /* Inner iteration */
    grg1_region_t* Y_next = grg1_region_alloc(n);

    if (!X || !X_prev || !Y || !Y_next) {
        grg1_region_free(X); grg1_region_free(X_prev);
        grg1_region_free(Y); grg1_region_free(Y_next);
        return false;
    }

    /* Outer initialization: X = S (full set for GFP) */
    grg1_region_fill(X);
    int outer_iter = 0;
    int total_inner = 0;
    bool outer_converged = false;

    while (outer_iter < outer_max_iter && !outer_converged) {
        /* Save previous X */
        grg1_region_copy(X_prev, X);

        /* Inner LFP: Y = μY. F(X, Y), starting from ∅ */
        grg1_region_clear(Y);
        int inner_iter = 0;
        bool inner_converged = false;

        while (inner_iter < inner_max_iter && !inner_converged) {
            /* Compute F(X, Y) */
            inner_transformer(X, Y, Y_next, context);

            /* Check if Y_next ⊆ Y (LFP convergence: ascending chain) */
            bool subset = true;
            for (int i = 0; i < w; i++) {
                if ((Y_next->bits[i] & ~Y->bits[i]) != 0) {
                    subset = false;
                    break;
                }
            }

            if (subset) {
                inner_converged = true;
            } else {
                /* Y = Y ∪ Y_next */
                for (int i = 0; i < w; i++) {
                    Y->bits[i] |= Y_next->bits[i];
                }
            }
            inner_iter++;
        }
        total_inner += inner_iter;

        /* X = Y (outer step: X shrinks toward fixpoint) */
        grg1_region_copy(X, Y);

        /* Check outer convergence: X = X_prev */
        bool outer_eq = true;
        for (int i = 0; i < w; i++) {
            if (X->bits[i] != X_prev->bits[i]) {
                outer_eq = false;
                break;
            }
        }

        if (outer_eq) {
            outer_converged = true;
        }
        outer_iter++;
    }

    grg1_region_copy(result, X);

    if (outer_iters) *outer_iters = outer_iter;
    if (total_inner_iters) *total_inner_iters = total_inner;

    grg1_region_free(X);
    grg1_region_free(X_prev);
    grg1_region_free(Y);
    grg1_region_free(Y_next);

    return outer_converged;
}

/* =========================================================================
 * Widen / Narrow (Abstract Interpretation acceleration)
 * ========================================================================*/

void grg1_fixpoint_widen(const grg1_region_t* prev,
                          const grg1_region_t* current,
                          grg1_region_t* result) {
    if (!prev || !current || !result) return;

    int w = result->words_needed;
    /* Simple union-based widening: X ∇ Y = X ∪ Y */
    for (int i = 0; i < w; i++) {
        result->bits[i] = prev->bits[i] | current->bits[i];
    }
}

bool grg1_fixpoint_narrow(const grg1_region_t* overapprox,
                           void (*transformer)(const grg1_region_t* in,
                                                grg1_region_t* out,
                                                void* context),
                           void* context,
                           grg1_region_t* result,
                           int max_iter,
                           int* iterations) {
    if (!overapprox || !transformer || !result) return false;

    int n = result->num_states;
    int w = result->words_needed;

    grg1_region_t* current = grg1_region_alloc(n);
    grg1_region_t* next = grg1_region_alloc(n);
    if (!current || !next) {
        grg1_region_free(current);
        grg1_region_free(next);
        return false;
    }

    grg1_region_copy(current, overapprox);
    int iter = 0;
    bool converged = false;

    while (iter < max_iter && !converged) {
        transformer(current, next, context);

        /* For narrowing: F(X) ⊆ X (post-fixpoint), we iterate down.
         * Check if next ⊆ current (decreasing chain) */
        bool equal = true;
        bool not_subset = false;
        for (int i = 0; i < w; i++) {
            if (current->bits[i] != next->bits[i]) equal = false;
            if ((next->bits[i] & ~current->bits[i]) != 0) not_subset = true;
        }

        if (equal) {
            converged = true;
        } else if (not_subset) {
            /* This shouldn't happen for a true post-fixpoint.
             * Take intersection to ensure we stay in the descending chain. */
            for (int i = 0; i < w; i++) {
                current->bits[i] &= next->bits[i];
            }
        } else {
            grg1_region_copy(current, next);
        }
        iter++;
    }

    grg1_region_copy(result, current);

    if (iterations) *iterations = iter;

    grg1_region_free(current);
    grg1_region_free(next);

    return converged;
}

/* =========================================================================
 * Worklist-based fixpoint computation
 * ========================================================================*/

grg1_worklist_t* grg1_worklist_alloc(int num_states) {
    if (num_states <= 0) return NULL;

    grg1_worklist_t* wl = (grg1_worklist_t*)malloc(sizeof(grg1_worklist_t));
    if (!wl) return NULL;

    wl->capacity = num_states;
    wl->head = 0;
    wl->tail = 0;
    wl->worklist = (int*)malloc((size_t)num_states * sizeof(int));
    wl->in_queue = (bool*)calloc((size_t)num_states, sizeof(bool));
    wl->game = NULL;

    if (!wl->worklist || !wl->in_queue) {
        free(wl->worklist);
        free(wl->in_queue);
        free(wl);
        return NULL;
    }
    return wl;
}

void grg1_worklist_free(grg1_worklist_t* wl) {
    if (!wl) return;
    free(wl->worklist);
    free(wl->in_queue);
    free(wl);
}

void grg1_worklist_push(grg1_worklist_t* wl, int state_id) {
    if (!wl || state_id < 0 || state_id >= wl->capacity) return;
    if (wl->in_queue[state_id]) return; /* Already in queue */

    wl->worklist[wl->tail] = state_id;
    wl->tail = (wl->tail + 1) % wl->capacity;
    wl->in_queue[state_id] = true;
}

int grg1_worklist_pop(grg1_worklist_t* wl) {
    if (!wl || wl->head == wl->tail) return -1;

    int state_id = wl->worklist[wl->head];
    wl->head = (wl->head + 1) % wl->capacity;
    wl->in_queue[state_id] = false;
    return state_id;
}

bool grg1_worklist_is_empty(const grg1_worklist_t* wl) {
    if (!wl) return true;
    return wl->head == wl->tail;
}

void grg1_fixpoint_worklist_lfp(
    const grg1_game_t* game,
    const grg1_region_t* initial,
    bool (*decide_fn)(int state_id, const grg1_region_t* current,
                       const grg1_game_t* game, void* context),
    void* context,
    grg1_region_t* result,
    int* iterations) {
    if (!game || !decide_fn || !result) return;

    int n = game->num_states;
    grg1_worklist_t* wl = grg1_worklist_alloc(n);
    if (!wl) return;

    /* Initialize result from initial region */
    if (initial) {
        grg1_region_copy(result, initial);
    } else {
        grg1_region_clear(result);
    }

    /* Seed worklist: states that may need re-evaluation.
     * For LFP from ∅, we start with all states whose predecessors
     * are initially in the set (typically only initial seeds). */
    for (int i = 0; i < n; i++) {
        if (!grg1_region_contains(result, i)) {
            grg1_worklist_push(wl, i);
        }
    }

    int iter = 0;

    while (!grg1_worklist_is_empty(wl)) {
        int s = grg1_worklist_pop(wl);
        iter++;

        /* Decide if this state should now be in the fixpoint */
        if (decide_fn(s, result, game, context)) {
            /* State s is added — its predecessors may now qualify */
            grg1_region_add(result, s);

            /* Add all predecessors to worklist (they may now qualify) */
            grg1_adj_node_t* pred = game->predecessors[s].head;
            while (pred) {
                if (!grg1_region_contains(result, pred->target_state_id)) {
                    grg1_worklist_push(wl, pred->target_state_id);
                }
                pred = pred->next;
            }
        }
    }

    if (iterations) *iterations = iter;

    grg1_worklist_free(wl);
}

/* =========================================================================
 * Fixpoint property checking
 * ========================================================================*/

bool grg1_fixpoint_is_fixpoint(const grg1_region_t* region,
                                void (*transformer)(const grg1_region_t* in,
                                                     grg1_region_t* out,
                                                     void* context),
                                void* context) {
    if (!region || !transformer) return false;

    grg1_region_t* f_x = grg1_region_alloc(region->num_states);
    if (!f_x) return false;

    transformer(region, f_x, context);
    bool result = grg1_region_equal(region, f_x);

    grg1_region_free(f_x);
    return result;
}

bool grg1_fixpoint_is_postfixpoint(const grg1_region_t* region,
                                    void (*transformer)(const grg1_region_t* in,
                                                         grg1_region_t* out,
                                                         void* context),
                                    void* context) {
    if (!region || !transformer) return false;

    grg1_region_t* f_x = grg1_region_alloc(region->num_states);
    if (!f_x) return false;

    transformer(region, f_x, context);
    /* Post-fixpoint: F(X) ⊆ X */
    bool result = grg1_region_subset(f_x, region);

    grg1_region_free(f_x);
    return result;
}

bool grg1_fixpoint_is_prefixpoint(const grg1_region_t* region,
                                   void (*transformer)(const grg1_region_t* in,
                                                        grg1_region_t* out,
                                                        void* context),
                                   void* context) {
    if (!region || !transformer) return false;

    grg1_region_t* f_x = grg1_region_alloc(region->num_states);
    if (!f_x) return false;

    transformer(region, f_x, context);
    /* Pre-fixpoint: X ⊆ F(X) */
    bool result = grg1_region_subset(region, f_x);

    grg1_region_free(f_x);
    return result;
}

void grg1_fixpoint_dual_transform(
    const grg1_region_t* X,
    void (*transformer)(const grg1_region_t* in, grg1_region_t* out,
                         void* context),
    void* context,
    const grg1_region_t* full_set,
    grg1_region_t* result) {
    if (!X || !transformer || !full_set || !result) return;

    int n = X->num_states;

    /* Compute S \ X */
    grg1_region_t* not_X = grg1_region_alloc(n);
    if (!not_X) return;
    grg1_region_complement(not_X, X);

    /* Compute F(S \ X) */
    grg1_region_t* f_not_X = grg1_region_alloc(n);
    if (!f_not_X) {
        grg1_region_free(not_X);
        return;
    }
    transformer(not_X, f_not_X, context);

    /* Result = S \ F(S \ X) */
    grg1_region_complement(result, f_not_X);

    grg1_region_free(not_X);
    grg1_region_free(f_not_X);
}
