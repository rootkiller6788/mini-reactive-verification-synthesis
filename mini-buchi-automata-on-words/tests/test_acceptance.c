/*
 * test_acceptance.c -- Tests for omega-automata acceptance conditions
 *
 * L2 Core Concepts: Buchi, co-Buchi, Rabin, Streett, Parity, Muller
 * L3 Math Structures: acceptance condition constructors and checkers
 * L4 Fundamental Laws: duality, conversions between conditions
 */
#include "omega_automata.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  %-50s", name); } while(0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)
#define ASSERT_EQ(a,b,msg) do { if ((a)!=(b)) { FAIL(msg); return; } } while(0)
#define ASSERT_TRUE(cond,msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)
#define ASSERT_NOT_NULL(p,msg) do { if ((p)==NULL) { FAIL(msg); return; } } while(0)

/* L2: Acceptance condition names */
static void test_condition_names(void) {
    TEST("Acceptance condition type names");
    ASSERT_TRUE(strstr(acc_condition_name(ACC_BUCHI), "chi") != NULL,
                "Buchi name contains 'chi'");
    ASSERT_TRUE(strstr(acc_condition_name(ACC_RABIN), "Rabin") != NULL,
                "Rabin name");
    ASSERT_TRUE(strstr(acc_condition_name(ACC_STREETT), "Streett") != NULL,
                "Streett name");
    ASSERT_TRUE(strstr(acc_condition_name(ACC_PARITY_MIN_EVEN), "Parity") != NULL,
                "Parity name");
    ASSERT_TRUE(strstr(acc_condition_name(ACC_MULLER), "Muller") != NULL,
                "Muller name");
    PASS();
}

/* L2: Duality relationships */
static void test_duality(void) {
    TEST("Acceptance condition duality");
    ASSERT_EQ(acc_condition_is_dual(ACC_BUCHI, ACC_COBUCHI), 1, "Buchi-coBuchi dual");
    ASSERT_EQ(acc_condition_is_dual(ACC_COBUCHI, ACC_BUCHI), 1, "coBuchi-Buchi dual");
    ASSERT_EQ(acc_condition_is_dual(ACC_RABIN, ACC_STREETT), 1, "Rabin-Streett dual");
    ASSERT_EQ(acc_condition_is_dual(ACC_STREETT, ACC_RABIN), 1, "Streett-Rabin dual");
    ASSERT_EQ(acc_condition_is_dual(ACC_BUCHI, ACC_RABIN), 0, "Buchi-Rabin not dual");
    PASS();
}

/* L3: Rabin condition check */
static void test_rabin_check(void) {
    TEST("Rabin condition check: Inf cap E = empty AND Inf cap F != empty");
    RabinCondition* rc = rabin_create(1);
    ASSERT_NOT_NULL(rc, "Rabin creation");

    /* Pair: E={2}, F={1,3}; Inf={1,3} -> E disjunct Inf=yes, F cap Inf={1,3}!=empty -> accept */
    rc->pairs[0].E = (int*)malloc(1 * sizeof(int));
    rc->pairs[0].E[0] = 2;
    rc->pairs[0].E_size = 1;
    rc->pairs[0].F = (int*)malloc(2 * sizeof(int));
    rc->pairs[0].F[0] = 1;
    rc->pairs[0].F[1] = 3;
    rc->pairs[0].F_size = 2;

    int inf1[] = {1, 3};
    ASSERT_EQ(rabin_check(rc, inf1, 2), 1, "Inf={1,3} should satisfy");

    /* Inf={2} -> E cap Inf = {2} != empty -> reject */
    int inf2[] = {2};
    ASSERT_EQ(rabin_check(rc, inf2, 1), 0, "Inf={2} should NOT satisfy");

    /* Inf={0} -> E cap Inf = empty, F cap Inf = empty -> reject */
    int inf3[] = {0};
    ASSERT_EQ(rabin_check(rc, inf3, 1), 0, "Inf={0} F cap empty");

    PASS();
    rabin_free(rc);
}

/* L3: Streett condition check */
static void test_streett_check(void) {
    TEST("Streett condition check: Inf cap G != empty -> Inf cap R != empty");
    StreettCondition* sc = streett_create(1);
    ASSERT_NOT_NULL(sc, "Streett creation");

    /* Pair: G={1}, R={2}; Inf={1,2} -> G inf -> need R inf -> yes; accept */
    sc->pairs[0].G = (int*)malloc(1 * sizeof(int));
    sc->pairs[0].G[0] = 1; sc->pairs[0].G_size = 1;
    sc->pairs[0].R = (int*)malloc(1 * sizeof(int));
    sc->pairs[0].R[0] = 2; sc->pairs[0].R_size = 1;

    int inf1[] = {1, 2};
    ASSERT_EQ(streett_check(sc, inf1, 2), 1, "Inf={1,2} satisfies");

    /* Inf={1} -> G inf true, R inf false -> reject */
    int inf2[] = {1};
    ASSERT_EQ(streett_check(sc, inf2, 1), 0, "Inf={1} R not inf -> reject");

    /* Inf={2} -> G inf false -> vacuous accept */
    int inf3[] = {2};
    ASSERT_EQ(streett_check(sc, inf3, 1), 1, "Inf={2} G not inf -> vacuous accept");

    PASS();
    streett_free(sc);
}

/* L3: Parity condition check (min-even) */
static void test_parity_check(void) {
    TEST("Parity condition check: min-even");
    ParityCondition* pc = parity_create(4, ACC_PARITY_MIN_EVEN);
    ASSERT_NOT_NULL(pc, "Parity creation");
    /* Colors: state0=1, state1=2, state2=0, state3=3 */
    pc->colors[0] = 1; /* odd */
    pc->colors[1] = 2; /* even */
    pc->colors[2] = 0; /* even */
    pc->colors[3] = 3; /* odd */
    pc->max_color = 3;

    /* Inf={0,1,2} -> min color = 0 (even) -> accept */
    int inf1[] = {0, 1, 2};
    ASSERT_EQ(parity_check(pc, inf1, 3), 1, "min color 0 is even -> accept");

    /* Inf={0,3} -> min color = 1 (odd) -> reject */
    int inf2[] = {0, 3};
    ASSERT_EQ(parity_check(pc, inf2, 2), 0, "min color 1 is odd -> reject");

    PASS();
    parity_free(pc);
}

/* L3: Muller condition check */
static void test_muller_check(void) {
    TEST("Muller condition check: Inf exact match");
    MullerCondition* mc = muller_create(2);
    ASSERT_NOT_NULL(mc, "Muller creation");

    /* Accepting set 0: {0, 1}; Accepting set 1: {2} */
    mc->accepting_sets[0] = (int*)malloc(2 * sizeof(int));
    mc->accepting_sets[0][0] = 0; mc->accepting_sets[0][1] = 1;
    mc->set_sizes[0] = 2;
    mc->accepting_sets[1] = (int*)malloc(1 * sizeof(int));
    mc->accepting_sets[1][0] = 2;
    mc->set_sizes[1] = 1;

    int inf1[] = {0, 1};
    ASSERT_EQ(muller_check(mc, inf1, 2), 1, "Inf={0,1} matches set 0");

    int inf2[] = {2};
    ASSERT_EQ(muller_check(mc, inf2, 1), 1, "Inf={2} matches set 1");

    int inf3[] = {0, 1, 2};
    ASSERT_EQ(muller_check(mc, inf3, 3), 0, "Inf={0,1,2} matches neither");

    PASS();
    muller_free(mc);
}

/* L3: Generalized Buchi condition check */
static void test_gbuchi_check(void) {
    TEST("Generalized Buchi condition: all F_sets intersect Inf");
    GeneralizedBuchiCondition* gbc = gbuchi_create(2);
    ASSERT_NOT_NULL(gbc, "GBC creation");

    gbc->F_sets[0] = (int*)malloc(1 * sizeof(int));
    gbc->F_sets[0][0] = 1; gbc->F_sizes[0] = 1;
    gbc->F_sets[1] = (int*)malloc(1 * sizeof(int));
    gbc->F_sets[1][0] = 2; gbc->F_sizes[1] = 1;

    int inf1[] = {1, 2};
    ASSERT_EQ(gbuchi_check(gbc, inf1, 2), 1, "all sets present");

    int inf2[] = {1};
    ASSERT_EQ(gbuchi_check(gbc, inf2, 1), 0, "missing F2");

    PASS();
    gbuchi_free(gbc);
}

/* L4: Rabin-to-Streett conversion */
static void test_rabin_to_streett_conversion(void) {
    TEST("Rabin-to-Streett conversion: dualize pairs");
    RabinCondition* rc = rabin_create(1);
    rc->pairs[0].E = (int*)malloc(1 * sizeof(int));
    rc->pairs[0].E[0] = 1; rc->pairs[0].E_size = 1;
    rc->pairs[0].F = (int*)malloc(1 * sizeof(int));
    rc->pairs[0].F[0] = 2; rc->pairs[0].F_size = 1;

    StreettCondition* sc = rabin_to_streett(rc);
    ASSERT_NOT_NULL(sc, "conversion result");
    ASSERT_EQ(sc->n_pairs, 1, "same number of pairs");
    ASSERT_EQ(sc->pairs[0].G[0], 1, "G = old E");
    ASSERT_EQ(sc->pairs[0].R[0], 2, "R = old F");

    PASS();
    rabin_free(rc);
    streett_free(sc);
}

/* L4: Buchi-to-Rabin conversion (1-pair Rabin) */
static void test_buchi_to_rabin(void) {
    TEST("Buchi-to-Rabin: Buchi = Rabin({},F)");
    int F[] = {0, 2};
    RabinCondition* rc = buchi_to_rabin(F, 2, 5);
    ASSERT_NOT_NULL(rc, "conversion");
    ASSERT_EQ(rc->n_pairs, 1, "single pair");
    ASSERT_EQ(rc->pairs[0].E_size, 0, "E = empty");
    ASSERT_EQ(rc->pairs[0].F_size, 2, "F size");
    ASSERT_EQ(rc->pairs[0].F[0], 0, "F[0]");
    ASSERT_EQ(rc->pairs[0].F[1], 2, "F[1]");

    /* Verify correctness: Inf={0} -> should satisfy Rabin (F non-empty) */
    int inf[] = {0};
    ASSERT_EQ(rabin_check(rc, inf, 1), 1, "Inf={0} satisfies Buchi-Rabin");

    PASS();
    rabin_free(rc);
}

/* L4: Parity-to-Rabin conversion */
static void test_parity_to_rabin(void) {
    TEST("Parity-to-Rabin: worst-case O(k) pairs");
    ParityCondition* pc = parity_create(3, ACC_PARITY_MAX_EVEN);
    pc->colors[0] = 0; pc->colors[1] = 1; pc->colors[2] = 2;
    pc->max_color = 2;

    RabinCondition* rc = parity_to_rabin(pc, 1);
    ASSERT_NOT_NULL(rc, "conversion result");
    /* max_color=2 -> n_pairs = ceil(3/2) = 2 */
    ASSERT_EQ(rc->n_pairs, 2, "ceil((max+1)/2) pairs");

    PASS();
    rabin_free(rc);
    parity_free(pc);
}

/* L4: Rabin complement (swap E and F) */
static void test_rabin_complement(void) {
    TEST("Rabin complement: swap E and F in each pair");
    RabinCondition* rc = rabin_create(1);
    rc->pairs[0].E = (int*)malloc(1 * sizeof(int));
    rc->pairs[0].E[0] = 1; rc->pairs[0].E_size = 1;
    rc->pairs[0].F = (int*)malloc(1 * sizeof(int));
    rc->pairs[0].F[0] = 2; rc->pairs[0].F_size = 1;

    RabinCondition* comp = rabin_complement(rc);
    ASSERT_NOT_NULL(comp, "complement");
    ASSERT_EQ(comp->pairs[0].E[0], 2, "new E = old F");
    ASSERT_EQ(comp->pairs[0].F[0], 1, "new F = old E");

    PASS();
    rabin_free(rc);
    rabin_free(comp);
}

int main(void) {
    printf("=== test_acceptance ===\n\n");
    test_condition_names();
    test_duality();
    test_rabin_check();
    test_streett_check();
    test_parity_check();
    test_muller_check();
    test_gbuchi_check();
    test_rabin_to_streett_conversion();
    test_buchi_to_rabin();
    test_parity_to_rabin();
    test_rabin_complement();
    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
