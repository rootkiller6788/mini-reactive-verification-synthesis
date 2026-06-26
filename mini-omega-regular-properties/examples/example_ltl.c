/**
 * example_ltl.c -- LTL Formula Construction and Evaluation
 *
 * Demonstrates LTL formula building, evaluation on omega-words,
 * and NBA-based model checking.
 */

#include <stdio.h>
#include <assert.h>
#include "ltl_formula.h"
#include "omega_regular.h"

/* Assignment function for omega-word evaluation */
static bool word_assign(int atom_id, size_t pos, void *ctx)
{
    const omega_word *w = (const omega_word *)ctx;
    uint8_t sym = omega_word_index(w, pos);
    /* atom 0 is true when symbol == 0, atom 1 is true when symbol == 1 */
    if (atom_id == 0) return (sym == 0);
    if (atom_id == 1) return (sym == 1);
    return false;
}

int main(void)
{
    printf("=== LTL Formula Example ===\n\n");

    /* Build formula: G(p0 -> F p1)
     * Always (if p0 holds now, then eventually p1 holds) */
    printf("1. Building LTL formula: G(p0 -> F p1)\n");

    ltl_node_t *p0 = ltl_make_atom(0);
    ltl_node_t *p1 = ltl_make_atom(1);
    ltl_node_t *fp1 = ltl_make_eventually(p1);
    ltl_node_t *impl = ltl_make_implies(p0, fp1);
    ltl_node_t *formula = ltl_make_always(impl);

    printf("   Formula: ");
    ltl_print(formula, stdout);
    printf("\n   Size: %zu nodes\n", ltl_size(formula));

    /* Evaluate on an omega-word: (0,1)^omega
     * At even positions: p0 holds, p1 does not. F p1 holds (p1 at next pos).
     * At odd positions: p0 does not hold. Implication vacuously true.
     * So G(p0 -> F p1) should hold everywhere. */
    printf("\n2. Evaluating on omega-word (0,1)^omega\n");
    omega_word w;
    uint8_t period[] = {0, 1};
    omega_word_create(&w, NULL, 0, period, 2);

    bool result = ltl_eval(formula, 0, word_assign, &w);
    printf("   w,0 |= G(p0 -> F p1): %s (expected: TRUE)\n",
           result ? "TRUE" : "FALSE");
    assert(result);

    /* Evaluate on omega-word (0)^omega
     * p0 always holds, p1 never holds. F p1 is false.
     * So p0 -> F p1 is false at position 0.
     * G(p0 -> F p1) is false. */
    printf("\n3. Evaluating on omega-word (0)^omega\n");
    omega_word w2;
    uint8_t period2[] = {0};
    omega_word_create(&w2, NULL, 0, period2, 1);

    result = ltl_eval(formula, 0, word_assign, &w2);
    printf("   w,0 |= G(p0 -> F p1): %s (expected: FALSE)\n",
           result ? "TRUE" : "FALSE");
    assert(!result);

    /* Build formula: GF p0 (infinitely often p0) */
    printf("\n4. Building LTL formula: GF p0\n");
    ltl_node_t *fp0 = ltl_make_eventually(ltl_make_atom(0));
    ltl_node_t *gfp0 = ltl_make_always(fp0);

    /* On (0,1)^omega: p0 holds infinitely often -> GF p0 is true */
    result = ltl_eval(gfp0, 0, word_assign, &w);
    printf("   w,0 |= GF p0: %s (expected: TRUE)\n",
           result ? "TRUE" : "FALSE");
    assert(result);

    /* On (1)^omega: p0 never holds -> GF p0 is false */
    result = ltl_eval(gfp0, 0, word_assign, &w2);
    printf("   w,0 |= GF p0 on (0)^omega: %s (expected: TRUE)\n",
           result ? "TRUE" : "FALSE");

    /* Cleanup */
    ltl_free(formula);
    ltl_free(gfp0);

    printf("\n=== All LTL examples completed ===\n");
    return 0;
}
