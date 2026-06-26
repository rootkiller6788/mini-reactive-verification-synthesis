/*
 * ltl_demo.c — Interactive LTL Demonstration
 *
 * Demonstrates LTL formula construction, transformation, and evaluation
 * on sample traces. Useful for teaching and visualization.
 *
 * Compile: gcc -I../include -o ltl_demo ltl_demo.c -L../build -lltl -lm
 */

#include "ltl_ast.h"
#include "ltl_semantics.h"
#include "ltl_equiv.h"
#include "ltl_patterns.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("╔═══════════════════════════════════════════╗\n");
    printf("║     LTL Interactive Demo                  ║\n");
    printf("╚═══════════════════════════════════════════╝\n\n");

    const char* names[] = {"req", "ack", "busy", "done", "error"};
    ltl_set_atom_names(names, 5);

    /* Demonstrate key LTL equivalences */
    printf("─── LTL Duality Demonstrations ───\n\n");

    /* Duality: ¬F φ ≡ G ¬φ */
    LtlNode* f_req = ltl_mk_finally(ltl_mk_atom(0));
    LtlNode* neg_f_req = ltl_mk_not(ltl_clone(f_req));
    LtlNode* g_not_req = ltl_mk_globally(ltl_mk_not(ltl_mk_atom(0)));

    LtlNode* nnf_neg_f = ltl_to_nnf(neg_f_req);
    printf("Duality check: ¬F req ≡ G ¬req\n");
    printf("  ¬F req NNF:  ");
    ltl_print(nnf_neg_f);
    printf("  G ¬req:      ");
    ltl_print(g_not_req);
    printf("  Equal: %s\n\n", ltl_equals(nnf_neg_f, g_not_req) ? "YES ✓" : "NO");

    ltl_free(f_req);
    ltl_free(neg_f_req);
    ltl_free(g_not_req);
    ltl_free(nnf_neg_f);

    /* Demonstrate the response pattern evaluation */
    printf("─── Response Pattern Evaluation ───\n\n");

    LtlTrace* trace = ltl_trace_create_lasso(
        (LtlPropSet[]){LTL_PROP_ADD(0,0), LTL_PROP_ADD(0,0), LTL_PROP_ADD(LTL_PROP_ADD(0,0),1)},
        3,
        (LtlPropSet[]){LTL_PROP_ADD(LTL_PROP_ADD(0,0),1)},
        1);

    LtlNode* resp = ltl_pattern_response_global(0, 1);
    printf("Property: G(req → F ack)\n");
    printf("Trace: req, req, req∧ack, (req∧ack)^ω\n");
    printf("Satisfied: %s\n", ltl_satisfies(trace, resp) ? "YES ✓" : "NO");

    ltl_trace_free(trace);
    ltl_free(resp);

    /* Demonstrate safety vs liveness */
    printf("\n─── Safety vs Liveness ───\n\n");
    LtlNode* safety = ltl_mk_globally(ltl_mk_atom(0));
    LtlNode* liveness = ltl_mk_finally(ltl_mk_atom(0));
    printf("G req:  safety=%s, liveness=%s\n",
           ltl_is_safety_fragment(safety) ? "yes" : "no",
           ltl_is_liveness_fragment(safety) ? "yes" : "no");
    printf("F req:  safety=%s, liveness=%s\n",
           ltl_is_safety_fragment(liveness) ? "yes" : "no",
           ltl_is_liveness_fragment(liveness) ? "yes" : "no");
    ltl_free(safety);
    ltl_free(liveness);

    printf("\n─── Done ───\n");
    ltl_set_atom_names(NULL, 0);
    return 0;
}
