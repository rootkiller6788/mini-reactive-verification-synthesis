/*
 * example_ltl_check.c — End-to-End LTL Model Checking & Pattern Verification
 *
 * Demonstrates:
 *   1. Building LTL formulas from specification patterns
 *   2. Creating Kripke structures representing system models
 *   3. Model checking: verifying properties and finding counterexamples
 *   4. Trace-based satisfaction evaluation
 *   5. NNF conversion and formula simplification
 *
 * This is a complete, self-contained example of LTL-based verification.
 */

#include "ltl_ast.h"
#include "ltl_semantics.h"
#include "ltl_equiv.h"
#include "ltl_patterns.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Example System: Traffic Light Controller ────────────────────
 *
 * States: Red(s0), Green(s1), Yellow(s2)
 * Transitions: Red→Green→Yellow→Red
 * Propositions:
 *   p0 = red_light
 *   p1 = green_light
 *   p2 = yellow_light
 *
 * Properties to verify:
 *   P1: G(red → X green)          — red always followed by green
 *   P2: G ¬(green ∧ red)          — mutual exclusion
 *   P3: G F green                 — green light infinitely often
 *   P4: G(green → F yellow)       — green eventually leads to yellow
 */

static LtlKripke* build_traffic_light(void) {
    /* 3 states: 0=Red, 1=Green, 2=Yellow */
    int edges[] = {
        0, 1,  /* Red → Green */
        1, 2,  /* Green → Yellow */
        2, 0   /* Yellow → Red */
    };
    LtlPropSet labels[3] = {
        LTL_PROP_ADD(LTL_PROP_EMPTY, 0),  /* Red: p0 */
        LTL_PROP_ADD(LTL_PROP_EMPTY, 1),  /* Green: p1 */
        LTL_PROP_ADD(LTL_PROP_EMPTY, 2)   /* Yellow: p2 */
    };
    int initial[1] = {0};

    return ltl_kripke_from_edges(3, edges, 3, initial, 1, labels);
}

/* ── Example System: Mutual Exclusion (2-process) ────────────────
 *
 * Simplified Peterson-like mutual exclusion protocol.
 * States represent which process is in critical section.
 * Propositions:
 *   p0 = proc0_in_critical
 *   p1 = proc1_in_critical
 *   p2 = proc0_waiting
 *   p3 = proc1_waiting
 */

static LtlKripke* build_mutex_system(void) {
    /*
     * States:
     *   0 = idle (neither in CS)
     *   1 = proc0 enters CS
     *   2 = proc0 in CS
     *   3 = proc0 exits → idle
     *   4 = proc1 enters CS
     *   5 = proc1 in CS
     *   6 = proc1 exits → idle
     */
    int edges[] = {
        0, 1,  1, 2,  2, 3,  3, 0,  /* proc0 path */
        0, 4,  4, 5,  5, 6,  6, 0   /* proc1 path */
    };
    LtlPropSet labels[7] = {
        0,                          /* s0: idle */
        LTL_PROP_ADD(LTL_PROP_EMPTY, 2),  /* s1: proc0 waiting */
        LTL_PROP_ADD(LTL_PROP_EMPTY, 0),  /* s2: proc0 in CS */
        0,                          /* s3: exit */
        LTL_PROP_ADD(LTL_PROP_EMPTY, 3),  /* s4: proc1 waiting */
        LTL_PROP_ADD(LTL_PROP_EMPTY, 1),  /* s5: proc1 in CS */
        0                           /* s6: exit */
    };
    int initial[1] = {0};

    return ltl_kripke_from_edges(7, edges, 8, initial, 1, labels);
}

/* ── Demonstration ────────────────────────────────────────────── */

int main(void) {
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  LTL Model Checking — End-to-End Demonstration  ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    /* ── Part 1: Formula Construction ─────────────────────────── */
    printf("─── Part 1: LTL Formula Construction ───\n\n");

    const char* names[] = {"red", "green", "yellow", "waiting0", "waiting1"};
    ltl_set_atom_names(names, 5);

    /* Safety: mutual exclusion */
    LtlNode* mutex = ltl_pattern_mutex(0, 1);
    printf("Property P1 (Mutual Exclusion):\n  ");
    ltl_print(mutex);
    printf("  Pattern: %s\n", ltl_pattern_description(LTL_PAT_MUTEX));

    /* Response: request→acknowledge */
    LtlNode* response = ltl_pattern_response_global(0, 1);
    printf("\nProperty P2 (Response):\n  ");
    ltl_print(response);
    printf("  Pattern: %s\n", ltl_pattern_description(LTL_PAT_RESPONSE_GLOBAL));

    /* Infinitely often */
    LtlNode* gf_green = ltl_pattern_infinitely_often(1);
    printf("\nProperty P3 (Infinitely Often):\n  ");
    ltl_print(gf_green);
    printf("  Pattern: %s\n", ltl_pattern_description(LTL_PAT_INFINITELY_OFTEN));

    /* Precedence */
    LtlNode* prec = ltl_pattern_precedence_global(0, 1);
    printf("\nProperty P4 (Precedence):\n  ");
    ltl_print(prec);
    printf("  Pattern: %s\n", ltl_pattern_description(LTL_PAT_PRECEDENCE_GLOBAL));

    /* ── Part 2: Formula Transformation ───────────────────────── */
    printf("\n─── Part 2: Formula Transformation ───\n\n");

    /* NNF conversion */
    LtlNode* complex_f = ltl_mk_not(
        ltl_mk_and(
            ltl_mk_globally(ltl_mk_atom(0)),
            ltl_mk_finally(ltl_mk_atom(1))
        )
    );
    printf("Original formula:\n  ");
    ltl_print(complex_f);

    LtlNode* nnf_f = ltl_to_nnf(complex_f);
    printf("NNF:\n  ");
    ltl_print(nnf_f);

    LtlNode* simplified = ltl_simplify_fixpoint(nnf_f);
    printf("Simplified:\n  ");
    ltl_print(simplified);

    /* Expansion */
    LtlNode* gf = ltl_pattern_infinitely_often(0);
    LtlNode* expanded = ltl_expand(gf);
    printf("\nExpansion of G F p0:\n  ");
    ltl_print(expanded);

    ltl_free(complex_f);
    ltl_free(nnf_f);
    ltl_free(simplified);
    ltl_free(gf);
    ltl_free(expanded);

    /* ── Part 3: Trace Evaluation ─────────────────────────────── */
    printf("\n─── Part 3: Trace Evaluation ───\n\n");

    /* Build a trace: p0 always true, p1 at even steps */
    LtlPropSet prefix_trace[4] = {
        LTL_PROP_ADD(LTL_PROP_EMPTY, 0),
        LTL_PROP_ADD(LTL_PROP_ADD(LTL_PROP_EMPTY, 0), 1),
        LTL_PROP_ADD(LTL_PROP_EMPTY, 0),
        LTL_PROP_ADD(LTL_PROP_ADD(LTL_PROP_EMPTY, 0), 1)
    };
    LtlPropSet cycle_trace[2] = {
        LTL_PROP_ADD(LTL_PROP_EMPTY, 0),
        LTL_PROP_ADD(LTL_PROP_ADD(LTL_PROP_EMPTY, 0), 1)
    };
    LtlTrace* trace = ltl_trace_create_lasso(prefix_trace, 4, cycle_trace, 2);
    printf("Trace:\n");
    ltl_trace_print(trace, 12);

    /* Evaluate properties on this trace */
    LtlNode* g_p0 = ltl_mk_globally(ltl_mk_atom(0));
    LtlNode* f_p1 = ltl_mk_finally(ltl_mk_atom(1));
    LtlNode* gf_p1 = ltl_pattern_infinitely_often(1);
    LtlNode* fg_p0 = ltl_mk_finally(ltl_mk_globally(ltl_mk_atom(0)));

    printf("\nEvaluation on trace:\n");
    printf("  G p0 = %s\n", ltl_satisfies(trace, g_p0) ? "TRUE" : "FALSE");
    printf("  F p1 = %s\n", ltl_satisfies(trace, f_p1) ? "TRUE" : "FALSE");
    printf("  G F p1 = %s\n", ltl_satisfies(trace, gf_p1) ? "TRUE" : "FALSE");
    printf("  F G p0 = %s\n", ltl_satisfies(trace, fg_p0) ? "TRUE" : "FALSE");

    ltl_free(g_p0);
    ltl_free(f_p1);
    ltl_free(gf_p1);
    ltl_free(fg_p0);
    ltl_trace_free(trace);

    /* ── Part 4: Model Checking ───────────────────────────────── */
    printf("\n─── Part 4: Model Checking ───\n\n");

    /* Traffic Light System */
    LtlKripke* TL = build_traffic_light();
    printf("Traffic Light Kripke Structure:\n");
    ltl_kripke_print(TL);

    /* Check properties */
    printf("\nModel Checking Results:\n");

    LtlNode* p_mutex = ltl_pattern_mutex(0, 1);  /* G ¬(red ∧ green) */
    LtlPath* cex_mutex = NULL;
    int mc_mutex = ltl_model_check(TL, p_mutex, &cex_mutex);
    printf("  P1 (G ¬(red ∧ green)): %s\n",
           mc_mutex ? "HOLDS ✅" : "VIOLATED ❌");
    if (cex_mutex) { ltl_path_print(cex_mutex); ltl_path_free(cex_mutex); }

    LtlNode* p_inf_green = ltl_pattern_infinitely_often(1);
    LtlPath* cex_inf = NULL;
    int mc_inf = ltl_model_check(TL, p_inf_green, &cex_inf);
    printf("  P3 (G F green): %s\n",
           mc_inf ? "HOLDS ✅" : "VIOLATED ❌");
    if (cex_inf) { ltl_path_print(cex_inf); ltl_path_free(cex_inf); }

    /* Response: red → eventually green */
    LtlNode* p_resp = ltl_pattern_response_global(0, 1);
    LtlPath* cex_resp = NULL;
    int mc_resp = ltl_model_check(TL, p_resp, &cex_resp);
    printf("  P4 (G(red → F green)): %s\n",
           mc_resp ? "HOLDS ✅" : "VIOLATED ❌");

    ltl_free(p_mutex);
    ltl_free(p_inf_green);
    ltl_free(p_resp);
    if (cex_resp) ltl_path_free(cex_resp);
    ltl_kripke_free(TL);

    /* Mutual Exclusion System */
    printf("\nMutual Exclusion System:\n");
    LtlKripke* MUX = build_mutex_system();
    ltl_kripke_print(MUX);

    printf("\nModel Checking Results:\n");

    LtlNode* mx_p = ltl_pattern_mutex(0, 1);
    LtlPath* mx_cex = NULL;
    int mx_result = ltl_model_check(MUX, mx_p, &mx_cex);
    printf("  M1 (G ¬(proc0_cs ∧ proc1_cs)): %s\n",
           mx_result ? "HOLDS ✅" : "VIOLATED ❌");
    if (mx_cex) { ltl_path_print(mx_cex); ltl_path_free(mx_cex); }

    ltl_free(mx_p);
    ltl_kripke_free(MUX);

    /* ── Part 5: Pattern Library ──────────────────────────────── */
    printf("\n─── Part 5: Pattern Library ───\n\n");

    LtlNamedFormula* lib = NULL;
    int n_pats = ltl_pattern_library_create(0, &lib);
    printf("Available patterns (%d):\n", n_pats);
    for (int i = 0; i < n_pats; i++) {
        printf("  %2d. %-25s : %s\n", i, lib[i].name,
               ltl_pattern_description((LtlPatternID)i));
    }
    ltl_pattern_library_free(lib, n_pats);

    /* ── Part 6: Formula Simplification ───────────────────────── */
    printf("\n─── Part 6: Formula Simplification ───\n\n");

    LtlNode* to_simplify = ltl_mk_and(ltl_mk_true(),
        ltl_mk_and(ltl_mk_globally(ltl_mk_globally(ltl_mk_atom(0))),
                   ltl_mk_or(ltl_mk_false(), ltl_mk_atom(1))));
    printf("Before simplification:\n  ");
    ltl_print(to_simplify);
    printf("  Size: %d nodes\n", ltl_size(to_simplify));

    LtlNode* simple = ltl_simplify(to_simplify);
    printf("After simplification:\n  ");
    ltl_print(simple);
    printf("  Size: %d nodes\n", ltl_size(simple));

    ltl_free(to_simplify);
    ltl_free(simple);

    /* ── Part 7: S-Expression I/O ─────────────────────────────── */
    printf("\n─── Part 7: S-Expression I/O ───\n\n");

    const char* sexpr = "(G (IMPLIES (ATOM 0) (F (ATOM 1))))";
    printf("Input:  %s\n", sexpr);
    LtlNode* parsed = ltl_parse(sexpr);
    printf("Parsed: ");
    ltl_print(parsed);
    char* output = ltl_to_sexpr(parsed);
    printf("Output: %s\n", output);
    ltl_free(parsed);
    free(output);

    /* ── Summary ───────────────────────────────────────────────── */
    printf("\n╔══════════════════════════════════════════════════╗\n");
    printf("║  Demonstration Complete                          ║\n");
    printf("║  Topics covered: LTL syntax, semantics, model    ║\n");
    printf("║  checking, NNF, simplification, pattern library, ║\n");
    printf("║  trace evaluation, Kripke structures.            ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");

    /* Clean up atom names */
    ltl_set_atom_names(NULL, 0);

    return 0;
}
