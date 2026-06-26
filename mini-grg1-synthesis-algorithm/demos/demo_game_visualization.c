/**
 * demo_game_visualization.c — Visualize GR(1) game construction
 *
 * Demonstrates the step-by-step construction of a game arena from
 * a GR(1) specification: variable enumeration, state creation,
 * transition generation, and DOT export for Graphviz visualization.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "grg1_types.h"
#include "grg1_spec.h"
#include "grg1_game.h"
#include "grg1_synthesis.h"

static bool demo_env_init(const grg1_valuation_t* v,
                           const grg1_variable_t* vars, int nv) {
    (void)vars; (void)nv;
    return v->values[0] == 0; /* req starts idle */
}

static bool demo_sys_init(const grg1_valuation_t* v,
                           const grg1_variable_t* vars, int nv) {
    (void)vars; (void)nv;
    return v->values[1] == 0; /* ack starts idle */
}

static bool demo_env_trans(const grg1_valuation_t* pre,
                            const grg1_valuation_t* post,
                            const grg1_variable_t* vars, int nv) {
    (void)pre; (void)vars; (void)nv;
    int post_req = post->values[0];
    return post_req == 0 || post_req == 1;
}

static bool demo_sys_trans(const grg1_valuation_t* pre,
                            const grg1_valuation_t* post,
                            const grg1_variable_t* vars, int nv) {
    (void)vars; (void)nv;
    int pre_req = pre->values[0];
    int pre_ack = pre->values[1];
    int post_ack = post->values[1];
    /* Ack follows request exactly one step later */
    if (pre_req == 1) return post_ack == 1;
    return post_ack == 0;
}

static bool demo_justice(const grg1_valuation_t* v,
                          const grg1_variable_t* vars, int nv) {
    (void)vars; (void)nv;
    return v->values[1] == 1; /* ack=1 is the justice goal */
}

int main(void) {
    printf("========================================\n");
    printf("GR(1) Game Visualization Demo\n");
    printf("========================================\n\n");

    /* Build a simple request-acknowledge specification */
    grg1_variable_t vars[2];
    vars[0].var_id = 0;
    strcpy(vars[0].name, "req");
    vars[0].domain_size = 2;
    vars[0].is_system_controlled = false;
    vars[0].initial_value = 0;

    vars[1].var_id = 1;
    strcpy(vars[1].name, "ack");
    vars[1].domain_size = 2;
    vars[1].is_system_controlled = true;
    vars[1].initial_value = 0;

    grg1_spec_t* spec = grg1_spec_create("Req-Ack Demo", 2, vars);
    grg1_spec_set_env_init(spec, "req_idle", demo_env_init);
    grg1_spec_set_sys_init(spec, "ack_idle", demo_sys_init);
    grg1_spec_set_env_safety(spec, "req_valid", demo_env_trans);
    grg1_spec_set_sys_safety(spec, "ack_follows_req", demo_sys_trans);
    grg1_spec_add_sys_justice(spec, "always_eventually_ack", demo_justice);

    /* Validate and print */
    printf("Specification:\n");
    grg1_spec_print(spec);

    /* Build game */
    printf("\nBuilding game arena...\n");
    grg1_game_t* game = grg1_game_from_spec(spec);
    if (game) {
        grg1_game_print_summary(game);

        /* Export to DOT */
        printf("\nExporting DOT visualization...\n");
        grg1_game_export_dot(game, "demos/demo_game.dot");
        printf("  Exported to demos/demo_game.dot\n");
        printf("  Render with: dot -Tpdf demos/demo_game.dot -o demo_game.pdf\n");

        /* Export stats */
        grg1_game_export_stats(game, "demos/demo_stats.csv");
        printf("  Stats exported to demos/demo_stats.csv\n");

        /* Run synthesis */
        printf("\nRunning synthesis...\n");
        grg1_synthesis_config_t config = GRG1_SYNTHESIS_CONFIG_DEFAULT;
        config.verbose = false;
        grg1_synthesis_result_t* result = grg1_synthesize(spec, &config);
        if (result) {
            grg1_synthesis_print_report(result);
            grg1_synthesis_result_free(result);
        }

        grg1_game_free(game);
    } else {
        printf("  Game construction failed (state space may be too large)\n");
    }

    grg1_spec_free(spec);

    printf("\nDone.\n");
    return 0;
}
