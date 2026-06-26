/**
 * ex_equiv.c — CTL Formula Equivalence and Transformation Demo
 *
 * Demonstrates CTL formula equivalences, duality, normal forms,
 * and the reduction to the Emerson adequate set {EX, EG, EU}.
 *
 * Shows:
 *   1. Duality laws: converting between existential and universal forms
 *   2. Negation Normal Form transformation
 *   3. Reduction to the Emerson core
 *   4. Formula simplification
 *
 * Knowledge: L4 (Fundamental Laws), L2 (Core Concepts)
 * Reference: Emerson "Temporal and Modal Logic" (1990)
 */

#include "ctl_ast.h"
#include "ctl_equiv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  CTL Formula Equivalences & Transformations      ║\n");
    printf("║  Reference: Emerson (1990)                       ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    /* ─── 1. Duality Demo ─── */
    printf("─── 1. Duality ───\n");
    ctl_formula *p = ctl_mk_atom("p");

    /* EX p */
    ctl_formula *ex_p = ctl_mk_ex(ctl_clone_formula(p));
    printf("Original:  "); ctl_print_formula(ex_p);

    /* dual(EX p) = AX !p — wait, dual(EX p) = AX(dual(p)) = AX(NOT p) */
    ctl_formula *d = ctl_dual(ex_p);
    printf("Dual:      "); ctl_print_formula(d);
    ctl_free_formula(ex_p);
    ctl_free_formula(d);

    /* EG p */
    ctl_formula *eg_p = ctl_mk_eg(ctl_clone_formula(p));
    printf("Original:  "); ctl_print_formula(eg_p);
    d = ctl_dual(eg_p);
    printf("Dual:      "); ctl_print_formula(d); /* AF NOT p */
    ctl_free_formula(eg_p);
    ctl_free_formula(d);

    /* E[p U q] */
    ctl_formula *q = ctl_mk_atom("q");
    ctl_formula *eu = ctl_mk_eu(ctl_clone_formula(p), ctl_clone_formula(q));
    printf("Original:  "); ctl_print_formula(eu);
    d = ctl_dual(eu);
    printf("Dual:      "); ctl_print_formula(d); /* A[NOT p R NOT q] — wait depends on dual def */
    ctl_free_formula(eu);
    ctl_free_formula(d);

    printf("\n");

    /* ─── 2. Negation Normal Form ─── */
    printf("─── 2. Negation Normal Form ───\n");

    /* !(E[p U q]) */
    ctl_formula *not_eu = ctl_mk_not(
        ctl_mk_eu(ctl_clone_formula(p), ctl_clone_formula(q)));
    printf("Original:  "); ctl_print_formula(not_eu);

    ctl_formula *nnf = ctl_to_nnf(not_eu);
    printf("NNF:       "); ctl_print_formula(nnf);
    ctl_free_formula(not_eu);
    ctl_free_formula(nnf);

    /* !(AG(p AND !q)) */
    ctl_formula *and_f = ctl_mk_and(ctl_clone_formula(p),
                                     ctl_mk_not(ctl_clone_formula(q)));
    ctl_formula *ag_f = ctl_mk_ag(and_f);
    ctl_formula *not_ag = ctl_mk_not(ag_f);
    printf("Original:  "); ctl_print_formula(not_ag);
    nnf = ctl_to_nnf(not_ag);
    printf("NNF:       "); ctl_print_formula(nnf);
    ctl_free_formula(not_ag);
    ctl_free_formula(nnf);

    printf("\n");

    /* ─── 3. Reduction to Core ─── */
    printf("─── 3. Reduction to Emerson Adequate Set {EX, EG, EU} ───\n");

    /* AF p = NOT EG (NOT p) */
    ctl_formula *af_p = ctl_mk_af(ctl_clone_formula(p));
    printf("Original:  "); ctl_print_formula(af_p);
    ctl_formula *core = ctl_reduce_to_core(af_p);
    printf("Core:      "); ctl_print_formula(core);
    ctl_free_formula(af_p);
    ctl_free_formula(core);

    /* AG p = NOT EF (NOT p) = NOT E[TRUE U (NOT p)] */
    ctl_formula *ag_p = ctl_mk_ag(ctl_clone_formula(p));
    printf("Original:  "); ctl_print_formula(ag_p);
    core = ctl_reduce_to_core(ag_p);
    printf("Core:      "); ctl_print_formula(core);
    ctl_free_formula(ag_p);
    ctl_free_formula(core);

    /* AX p = NOT EX (NOT p) */
    ctl_formula *ax_p = ctl_mk_ax(ctl_clone_formula(p));
    printf("Original:  "); ctl_print_formula(ax_p);
    core = ctl_reduce_to_core(ax_p);
    printf("Core:      "); ctl_print_formula(core);
    ctl_free_formula(ax_p);
    ctl_free_formula(core);

    printf("\n");

    /* ─── 4. Simplification ─── */
    printf("─── 4. Formula Simplification ───\n");

    /* !!(p) simplifies to p */
    ctl_formula *not_not_p = ctl_mk_not(ctl_mk_not(ctl_clone_formula(p)));
    printf("Original:  "); ctl_print_formula(not_not_p);
    ctl_formula *simp = ctl_simplify(not_not_p);
    printf("Simplified: "); ctl_print_formula(simp);
    ctl_free_formula(not_not_p);
    ctl_free_formula(simp);

    /* p AND (q OR p) — no simplification in current implementation */
    ctl_formula *or_f = ctl_mk_or(ctl_clone_formula(q), ctl_clone_formula(p));
    ctl_formula *and_f2 = ctl_mk_and(ctl_clone_formula(p), or_f);
    printf("Original:  "); ctl_print_formula(and_f2);
    simp = ctl_simplify(and_f2);
    printf("Simplified: "); ctl_print_formula(simp);
    ctl_free_formula(and_f2);
    ctl_free_formula(simp);

    /* EF(EF(p)) = EF(p) — idempotence */
    ctl_formula *ef_p = ctl_mk_ef(ctl_clone_formula(p));
    ctl_formula *ef_ef_p = ctl_mk_ef(ef_p);
    printf("Original:  "); ctl_print_formula(ef_ef_p);
    simp = ctl_simplify(ef_ef_p);
    printf("Simplified: "); ctl_print_formula(simp);
    ctl_free_formula(ef_ef_p);
    ctl_free_formula(simp);

    printf("\n");

    /* ─── 5. Expansion Demo ─── */
    printf("─── 5. One-Step Expansion ───\n");
    ctl_formula *ef_p2 = ctl_mk_ef(ctl_clone_formula(p));
    printf("Original:    "); ctl_print_formula(ef_p2);
    ctl_formula *exp1 = ctl_expand(ef_p2, 1);
    printf("Expand(1):   "); ctl_print_formula(exp1);
    ctl_formula *exp2 = ctl_expand(ef_p2, 2);
    printf("Expand(2):   "); ctl_print_formula(exp2);
    ctl_free_formula(ef_p2); ctl_free_formula(exp1); ctl_free_formula(exp2);

    printf("\n");

    /* ─── 6. LTL Expressibility Check ─── */
    printf("─── 6. CTL/LTL Expressibility ───\n");

    ctl_formula *ag_ef_p = ctl_mk_ag(ctl_mk_ef(ctl_clone_formula(p)));
    printf("Formula:    "); ctl_print_formula(ag_ef_p);
    printf("LTL-expressible: %s (expected: NO)\n",
           ctl_is_ltl_expressible(ag_ef_p) ? "YES" : "NO");

    ctl_formula *ag_p2 = ctl_mk_ag(ctl_clone_formula(p));
    printf("Formula:    "); ctl_print_formula(ag_p2);
    printf("LTL-expressible: %s (expected: YES, as G p)\n",
           ctl_is_ltl_expressible(ag_p2) ? "YES" : "NO");

    char *ltl = ctl_to_ltl_string(ag_p2);
    if (ltl) { printf("As LTL: %s\n", ltl); free(ltl); }

    ctl_free_formula(p); ctl_free_formula(q);
    ctl_free_formula(ag_ef_p); ctl_free_formula(ag_p2);

    printf("\n─── All equivalence demonstrations complete ───\n");
    return 0;
}
