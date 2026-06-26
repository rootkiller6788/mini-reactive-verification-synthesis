/**
 * example_omega_regular.c -- Omega-Regular Expression Operations
 *
 * Demonstrates omega-regular expression construction, property
 * classification, and NBA translation.
 */

#include <stdio.h>
#include <assert.h>
#include "omega_regular.h"
#include "buchi_automaton.h"

int main(void)
{
    printf("=== Omega-Regular Expression Example ===\n\n");

    /* Example 1: Build expression (a)^omega (infinitely often a) */
    printf("1. Building expression: (a)^omega\n");
    /* First register DFA for symbol 'a' (= 0) */
    bool sym_set[2] = {true, false};
    int dfa_id = simple_dfa_symbol_set_dfa(sym_set, 2);
    assert(dfa_id >= 0);

    ore_node_t *e_omega = ore_make_omega(dfa_id);
    printf("   Expression tree size: %zu nodes\n", ore_size(e_omega));
    ore_print(e_omega, stdout);
    printf("\n");

    /* Example 2: Property classification */
    printf("\n2. Property classification\n");
    prop_type_t pt = ore_classify_property(e_omega);
    const char *type_names[] = {
        "Safety", "Liveness", "Guarantee", "Response",
        "Persistence", "Recurrence", "General"
    };
    printf("   Property type: %s (expected: Recurrence)\n", type_names[pt]);

    /* Example 3: Translate to NBA and check emptiness */
    printf("\n3. Translate expression to NBA\n");
    nba_t nba;
    bool ok = ore_to_nba(e_omega, 2, &nba);
    assert(ok);
    printf("   NBA has %d states, %d transitions\n", nba.nstates, nba.ntrans);

    bool empty = nba_is_empty(&nba);
    printf("   Emptiness: %s (expected: NON-EMPTY)\n",
           empty ? "EMPTY" : "NON-EMPTY");
    assert(!empty);

    /* Example 4: Build a safety property: letter 'a' at position 0 */
    printf("\n4. Building safety expression: a\n");
    ore_node_t *e_safety = ore_make_letter(0);
    pt = ore_classify_property(e_safety);
    printf("   Property type: %s (expected: Safety)\n", type_names[pt]);
    assert(pt == PROP_SAFETY);

    /* Example 5: Union of expressions */
    printf("\n5. Building union: (a)^omega U b\n");
    ore_node_t *e_b = ore_make_letter(1);
    ore_node_t *e_union = ore_make_union(e_omega, e_b);
    printf("   Union tree size: %zu\n", ore_size(e_union));

    nba_t nba_union;
    ok = ore_to_nba(e_union, 2, &nba_union);
    assert(ok);
    printf("   Union NBA: %d states\n", nba_union.nstates);

    /* Example 6: Syntactic equality of cloned expression */
    printf("\n6. Cloning and equality check\n");
    ore_node_t *cloned = ore_clone(e_omega);
    assert(ore_is_syntactically_equal(e_omega, cloned));
    printf("   Cloned expression equals original: YES\n");

    /* Cleanup */
    ore_free(e_omega);
    ore_free(e_safety);
    ore_free(e_b);
    /* Note: e_union already owns e_omega and e_b, so don't double-free */
    ore_free(cloned);

    printf("\n=== All omega-regular expression examples completed ===\n");
    return 0;
}
