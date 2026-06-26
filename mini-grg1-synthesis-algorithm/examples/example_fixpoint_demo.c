/**
 * example_fixpoint_demo.c — Fixpoint Computation Demonstration
 *
 * Demonstrates the fixpoint computation library by solving
 * reachability and safety games on constructed game arenas.
 * Shows LFP/GFP iteration, convergence monitoring, and
 * worklist-based acceleration.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "grg1_types.h"
#include "grg1_game.h"
#include "grg1_fixpoint.h"

/* =========================================================================
 * Context for a simple reachability transformer
 * ========================================================================*/

typedef struct {
    const grg1_game_t* game;
    const grg1_region_t* target;
} reach_ctx_t;

static void reach_transformer(const grg1_region_t* in,
                               grg1_region_t* out,
                               void* context) {
    reach_ctx_t* ctx = (reach_ctx_t*)context;

    /* F(X) = target ∪ CPre_sys(X) */
    grg1_region_t* cpre = grg1_region_alloc(in->num_states);
    if (!cpre) { grg1_region_copy(out, in); return; }

    grg1_game_cpre_sys(ctx->game, in, cpre);
    grg1_region_union(out, ctx->target, cpre);
    grg1_region_free(cpre);
}

/* Simple decision function for worklist: a state should be in the fixpoint
 * if it has a successor already in the fixpoint (for LFP backward reach) */
static bool worklist_decide(int state_id, const grg1_region_t* current,
                             const grg1_game_t* g, void* context) {
    (void)context;
    grg1_adj_node_t* succ = g->successors[state_id].head;
    while (succ) {
        if (grg1_region_contains(current, succ->target_state_id)) {
            return true;
        }
        succ = succ->next;
    }
    return false;
}

int main(void) {
    printf("========================================\n");
    printf("Fixpoint Computation Demo\n");
    printf("========================================\n\n");

    /* Build a simple 5-state chain game:
     *   0 → 1 → 2 → 3 → 4
     * All states are environment-turn (for simplicity). */
    int num_states = 5;
    grg1_game_t* game = grg1_game_alloc(num_states);

    grg1_valuation_t* v = grg1_valuation_alloc(1);
    for (int i = 0; i < num_states; i++) {
        v->values[0] = i;
        grg1_game_add_state(game, v, GRG1_PLAYER_ENVIRONMENT,
                             i == 0 /* state 0 is initial */);
    }
    for (int i = 0; i < num_states - 1; i++) {
        grg1_game_add_transition(game, i, i + 1, GRG1_PLAYER_ENVIRONMENT, 0);
    }
    grg1_valuation_free(v);

    printf("Game: %d-state chain (all env-turn)\n", game->num_states);

    /* -- LFP demo: compute reachability to state 4 -- */
    printf("\n--- Least Fixpoint: Reachability to State 4 ---\n");

    grg1_region_t* target = grg1_region_alloc(num_states);
    grg1_region_t* lfp_result = grg1_region_alloc(num_states);
    grg1_region_add(target, 4);

    reach_ctx_t ctx = { .game = game, .target = target };

    int lfp_iters;
    bool lfp_ok = grg1_fixpoint_lfp(
        target, /* start from target set */
        reach_transformer, &ctx,
        lfp_result, 100, &lfp_iters);

    printf("LFP converged: %s\n", lfp_ok ? "yes" : "no");
    printf("LFP iterations: %d\n", lfp_iters);
    printf("LFP region size: %d\n", grg1_region_count(lfp_result));
    printf("States in LFP: {");
    for (int i = 0; i < num_states; i++) {
        if (grg1_region_contains(lfp_result, i)) printf(" %d", i);
    }
    printf(" }\n");

    /* Verify LFP property: F(lfp) ⊆ lfp */
    bool is_postfix = grg1_fixpoint_is_postfixpoint(
        lfp_result, reach_transformer, &ctx);
    printf("LFP is post-fixpoint: %s\n", is_postfix ? "yes" : "no");

    /* Verify F(lfp) = lfp */
    bool is_fixpoint = grg1_fixpoint_is_fixpoint(
        lfp_result, reach_transformer, &ctx);
    printf("LFP is exact fixpoint: %s\n", is_fixpoint ? "yes" : "no");

    /* -- GFP demo: compute states that can always stay in {0,1,2,3} -- */
    printf("\n--- Greatest Fixpoint: Safety in {0,1,2,3} ---\n");

    grg1_region_t* safe_set = grg1_region_alloc(num_states);
    grg1_region_t* gfp_result = grg1_region_alloc(num_states);
    for (int i = 0; i < 4; i++) grg1_region_add(safe_set, i);

    /* GFP starting from the safe set itself (shrinks) */
    grg1_region_fill(gfp_result);
    int gfp_iters;
    bool gfp_ok = grg1_fixpoint_gfp(
        safe_set,
        reach_transformer, &ctx,
        gfp_result, 100, &gfp_iters);

    printf("GFP converged: %s\n", gfp_ok ? "yes" : "no");
    printf("GFP iterations: %d\n", gfp_iters);
    printf("GFP region size: %d\n", grg1_region_count(gfp_result));

    /* -- Worklist demo -- */
    printf("\n--- Worklist Fixpoint Demo ---\n");

    /* Worklist decision function defined at file scope below */

    grg1_region_t* wl_result = grg1_region_alloc(num_states);
    grg1_region_add(wl_result, 4); /* Seed with state 4 */

    int wl_iters;
    grg1_fixpoint_worklist_lfp(game, wl_result,
                                worklist_decide, NULL,
                                wl_result, &wl_iters);

    printf("Worklist iterations (state evaluations): %d\n", wl_iters);
    printf("Worklist result size: %d\n", grg1_region_count(wl_result));

    /* -- Dual transform demo -- */
    printf("\n--- Dual Transform Demo ---\n");
    grg1_region_t* full_set = grg1_region_alloc(num_states);
    grg1_region_fill(full_set);

    grg1_region_t* dual_result = grg1_region_alloc(num_states);
    grg1_fixpoint_dual_transform(target, reach_transformer, &ctx,
                                  full_set, dual_result);
    printf("Dual transform region size: %d\n",
           grg1_region_count(dual_result));

    /* -- Cleanup -- */
    grg1_region_free(target);
    grg1_region_free(lfp_result);
    grg1_region_free(safe_set);
    grg1_region_free(gfp_result);
    grg1_region_free(wl_result);
    grg1_region_free(full_set);
    grg1_region_free(dual_result);
    grg1_game_free(game);

    printf("\nDone.\n");
    return 0;
}
