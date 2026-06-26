/**
 * example_bdd_synthesis.c — BDD-based Symbolic Operations Demo
 *
 * Demonstrates the BDD library for symbolic state-space representation,
 * showing variable creation, boolean operations, satisfiability counting,
 * and conversion between BDD and explicit regions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "grg1_types.h"

int main(void) {
    printf("========================================\n");
    printf("BDD Symbolic Operations Demo\n");
    printf("========================================\n\n");

    /* Create a BDD manager with 4 variables */
    grg1_bdd_manager_t* mgr = grg1_bdd_manager_alloc(4, 1024);
    assert(mgr != NULL);
    printf("BDD manager: %d variables, table size %d\n",
           mgr->num_vars, mgr->table_size);

    /* Create BDD variables v0, v1, v2, v3 */
    grg1_bdd_node_t* v0 = grg1_bdd_create_variable(mgr, 0);
    grg1_bdd_node_t* v1 = grg1_bdd_create_variable(mgr, 1);
    grg1_bdd_node_t* v2 = grg1_bdd_create_variable(mgr, 2);
    grg1_bdd_node_t* v3 = grg1_bdd_create_variable(mgr, 3);
    printf("Created BDD variables: v0, v1, v2, v3\n");

    /* Build formula: v0 AND v1 (conjunction) */
    grg1_bdd_node_t* f_and = grg1_bdd_and(mgr, v0, v1);
    printf("f_and = v0 ∧ v1: nodes = %d, satcount = %lld (expect 4)\n",
           grg1_bdd_node_count(f_and),
           (long long)grg1_bdd_satcount(mgr, f_and));

    /* Build formula: v0 OR v1 (disjunction) */
    grg1_bdd_node_t* f_or = grg1_bdd_or(mgr, v0, v1);
    printf("f_or = v0 ∨ v1: nodes = %d, satcount = %lld (expect 12)\n",
           grg1_bdd_node_count(f_or),
           (long long)grg1_bdd_satcount(mgr, f_or));

    /* Build formula: v0 XOR v1 */
    grg1_bdd_node_t* f_xor = grg1_bdd_xor(mgr, v0, v1);
    printf("f_xor = v0 ⊕ v1: satcount = %lld (expect 8)\n",
           (long long)grg1_bdd_satcount(mgr, f_xor));

    /* Build formula: v0 → v1 (implication) */
    grg1_bdd_node_t* f_imp = grg1_bdd_imp(mgr, v0, v1);
    printf("f_imp = v0 → v1: satcount = %lld (expect 12)\n",
           (long long)grg1_bdd_satcount(mgr, f_imp));

    /* Build formula: NOT(v0) */
    grg1_bdd_node_t* f_not = grg1_bdd_not(mgr, v0);
    printf("f_not = ¬v0: satcount = %lld (expect 8)\n",
           (long long)grg1_bdd_satcount(mgr, f_not));

    /* Quantification demo: exists v2 . (v0 AND v1 AND v2) */
    printf("\n--- Quantification Demo ---\n");
    grg1_bdd_node_t* f3 = grg1_bdd_and(mgr, f_and, v2); /* v0 ∧ v1 ∧ v2 */
    printf("v0 ∧ v1 ∧ v2: satcount = %lld\n",
           (long long)grg1_bdd_satcount(mgr, f3));

    grg1_bdd_node_t* f_exists = grg1_bdd_exists(mgr, f3, 2);
    printf("∃v2.(v0∧v1∧v2) = v0∧v1: satcount = %lld\n",
           (long long)grg1_bdd_satcount(mgr, f_exists));

    /* Universal quantification: forall v3 . (v3 ∨ ¬v3) = true */
    grg1_bdd_node_t* v3_or_not = grg1_bdd_or(mgr, v3, grg1_bdd_not(mgr, v3));
    grg1_bdd_node_t* f_forall = grg1_bdd_forall(mgr, v3_or_not, 3);
    printf("∀v3.(v3∨¬v3): satcount = %lld (expect 16)\n",
           (long long)grg1_bdd_satcount(mgr, f_forall));

    /* Restrict demo */
    printf("\n--- Restrict (Cofactor) Demo ---\n");
    grg1_bdd_node_t* f_restrict = grg1_bdd_restrict(mgr, f_or, 0, true);
    printf("(v0∨v1)|_{v0=true}: satcount = %lld (expect 8)\n",
           (long long)grg1_bdd_satcount(mgr, f_restrict));

    f_restrict = grg1_bdd_restrict(mgr, f_or, 0, false);
    printf("(v0∨v1)|_{v0=false}: satcount = %lld (expect 8)\n",
           (long long)grg1_bdd_satcount(mgr, f_restrict));

    /* SAT enumeration */
    printf("\n--- Any SAT Demo ---\n");
    bool assignment[4] = {false, false, false, false};
    bool sat_ok = grg1_bdd_any_sat(mgr, f_and, assignment);
    printf("SAT for v0∧v1: %s, assignment = {v0=%d, v1=%d, v2=%d, v3=%d}\n",
           sat_ok ? "found" : "not found",
           assignment[0], assignment[1], assignment[2], assignment[3]);

    sat_ok = grg1_bdd_any_sat(mgr, mgr->false_node, assignment);
    printf("SAT for FALSE: %s\n", sat_ok ? "found" : "not found");

    /* BDD to region conversion */
    printf("\n--- BDD to Region Conversion ---\n");
    grg1_region_t* region = grg1_region_alloc(16); /* 2^4 possible states */
    grg1_bdd_to_region(mgr, f_and, region);
    printf("Region from v0∧v1: %d states\n", grg1_region_count(region));

    /* Print */
    printf("\n--- BDD Print (v0 XOR v1) ---\n");
    grg1_bdd_print(mgr, f_xor);
    printf("\n");

    /* Cleanup */
    grg1_region_free(region);
    grg1_bdd_manager_free(mgr);

    printf("\nDone.\n");
    return 0;
}
