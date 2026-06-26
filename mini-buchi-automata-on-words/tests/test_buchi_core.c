/*
 * test_buchi_core.c -- Tests for core Buchi automaton construction
 *
 * L1 Definitions + L3 Math Structures + L6 Canonical Problems
 * Tests verify NBA construction, transition relation, omega-words,
 * SCC decomposition, and acceptance checking.
 */
#include "buchi.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  %-50s", name); } while(0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)
#define ASSERT_EQ(a,b,msg) do { if ((a)!=(b)) { FAIL(msg); return; } } while(0)
#define ASSERT_TRUE(cond,msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)
#define ASSERT_NULL(p,msg) do { if ((p)!=NULL) { FAIL(msg); return; } } while(0)
#define ASSERT_NOT_NULL(p,msg) do { if ((p)==NULL) { FAIL(msg); return; } } while(0)

/* L1: NBA 5-tuple (Q,Sigma,delta,q0,F) construction */
static void test_nba_construction(void) {
    TEST("NBA 5-tuple (Q,Sigma,delta,q0,F) construction");
    BuchiAutomaton* ba = buchi_create(4, 0, 2);
    ASSERT_NOT_NULL(ba, "NBA creation failed");
    ASSERT_EQ(buchi_num_states(ba), 4, "state count");
    ASSERT_EQ(buchi_num_trans(ba), 0, "initial transitions");
    BuchiSymbol alpha[] = {0, 1};
    buchi_set_alphabet(ba, alpha, 2);
    buchi_add_accepting(ba, 3);
    ASSERT_TRUE(buchi_is_accepting(ba, 3), "state 3 accepting");
    ASSERT_TRUE(!buchi_is_accepting(ba, 0), "state 0 not accepting");
    buchi_set_name(ba, "test-NBA");
    PASS();
    buchi_free(ba);
}

/* L3: Transition relation delta: Q x Sigma -> 2^Q */
static void test_transition_relation(void) {
    TEST("Transition relation delta: QxSigma->2^Q");
    BuchiAutomaton* ba = buchi_create(3, 0, 2);
    BuchiSymbol alpha[] = {0, 1};
    buchi_set_alphabet(ba, alpha, 2);
    ASSERT_EQ(buchi_add_transition(ba, 0, 0, 1), 1, "add d(0,0,1)");
    ASSERT_EQ(buchi_add_transition(ba, 0, 1, 2), 1, "add d(0,1,2)");
    ASSERT_EQ(buchi_add_transition(ba, 1, 0, 1), 1, "add d(1,0,1)");
    ASSERT_EQ(buchi_add_transition(ba, 1, 1, 0), 1, "add d(1,1,0)");
    ASSERT_EQ(buchi_add_transition(ba, 2, 0, 2), 1, "add d(2,0,2)");
    ASSERT_EQ(buchi_add_transition(ba, 2, 1, 1), 1, "add d(2,1,1)");
    ASSERT_EQ(buchi_add_transition(ba, 0, 0, 1), 0, "duplicate rejected");
    ASSERT_EQ(buchi_num_trans(ba), 6, "total transitions");
    ASSERT_TRUE(buchi_has_transition(ba, 0, 0, 1), "d(0,0,1) exists");
    ASSERT_TRUE(!buchi_has_transition(ba, 0, 0, 2), "d(0,0,2) absent");
    const BuchiTransitionSet* post = buchi_post(ba, 0, 0);
    ASSERT_NOT_NULL(post, "post(0,0)");
    PASS();
    buchi_free(ba);
}

/* L1: Omega-word lasso (prefix.cycle^omega) */
static void test_omega_word_lasso(void) {
    TEST("Omega-word lasso (prefix.cycle^omega)");
    BuchiSymbol prefix[] = {0, 1};
    BuchiSymbol cycle[] = {2, 3};
    OmegaWord* w = omega_make_lasso(prefix, 2, cycle, 2);
    ASSERT_NOT_NULL(w, "lasso creation");
    ASSERT_TRUE(w->is_lasso, "is lasso");
    ASSERT_EQ(*w->period_start, 2, "period start");
    ASSERT_EQ(omega_get(w, 0), 0, "w[0]");
    ASSERT_EQ(omega_get(w, 1), 1, "w[1]");
    ASSERT_EQ(omega_get(w, 2), 2, "w[2]");
    ASSERT_EQ(omega_get(w, 3), 3, "w[3]");
    ASSERT_EQ(omega_get(w, 4), 2, "w[4] repeat cycle");
    PASS();
    omega_free(w);
}

/* L1: Omega-word periodic (period^omega) */
static void test_omega_word_periodic(void) {
    TEST("Omega-word periodic (period^omega)");
    BuchiSymbol period[] = {0, 1, 0};
    OmegaWord* w = omega_make_periodic(period, 3);
    ASSERT_NOT_NULL(w, "periodic creation");
    ASSERT_EQ(omega_get(w, 0), 0, "w[0]");
    ASSERT_EQ(omega_get(w, 1), 1, "w[1]");
    ASSERT_EQ(omega_get(w, 3), 0, "w[3]=period repeat");
    ASSERT_EQ(omega_get(w, 5), 0, "w[5]");
    PASS();
    omega_free(w);
}

/* L2: Buchi acceptance -- Inf(rho) cap F != empty */
static void test_buchi_acceptance_inf_often(void) {
    TEST("Buchi acceptance: Inf(rho) cap F != empty");
    BuchiAutomaton* ba = buchi_create(2, 0, 2);
    BuchiSymbol alpha[] = {0, 1};
    buchi_set_alphabet(ba, alpha, 2);
    buchi_add_accepting(ba, 1);
    buchi_add_transition(ba, 0, 0, 0);
    buchi_add_transition(ba, 0, 1, 1);
    buchi_add_transition(ba, 1, 0, 0);
    buchi_add_transition(ba, 1, 1, 1);
    BuchiSymbol pref[] = {0};
    BuchiSymbol cyc[] = {1};
    OmegaWord* w = omega_make_lasso(pref, 1, cyc, 1);
    ASSERT_EQ(buchi_accepts_lasso(ba, w), 1, "should accept (inf many 1s)");
    omega_free(w);
    buchi_free(ba);
    PASS();
}

/* L3: Tarjan SCC decomposition on transition graph */
static void test_scc_decomposition(void) {
    TEST("SCC decomposition via Tarjan algorithm");
    BuchiAutomaton* ba = buchi_create(5, 0, 2);
    BuchiSymbol alpha[] = {0, 1};
    buchi_set_alphabet(ba, alpha, 2);
    buchi_add_accepting(ba, 4);
    buchi_add_transition(ba, 0, 0, 1);
    buchi_add_transition(ba, 1, 0, 0);
    buchi_add_transition(ba, 0, 1, 2);
    buchi_add_transition(ba, 2, 0, 3);
    buchi_add_transition(ba, 3, 1, 4);
    buchi_add_transition(ba, 4, 1, 3);
    BuchiSCCDecomp* scc = buchi_scc_decompose(ba);
    ASSERT_NOT_NULL(scc, "SCC decomp");
    ASSERT_TRUE(scc->n_sccs >= 3, "at least 3 SCCs");
    ASSERT_EQ(buchi_has_accepting_scc(scc), 1, "has accepting SCC");
    PASS();
    buchi_scc_free(scc);
    buchi_free(ba);
}

/* L1: Null-pointer safety for all core APIs */
static void test_null_safety(void) {
    TEST("NULL-pointer safety for core APIs");
    ASSERT_NULL(buchi_create(0, 0, 0), "zero states");
    ASSERT_NULL(buchi_create(-1, 0, 1), "neg states");
    ASSERT_NULL(omega_make_prefix(NULL, 3), "NULL prefix");
    ASSERT_NULL(omega_make_lasso(NULL, 1, NULL, 1), "NULL lasso");
    ASSERT_NULL(omega_make_periodic(NULL, 1), "NULL period");
    ASSERT_TRUE(!buchi_is_accepting(NULL, 0), "NULL is_acc");
    ASSERT_EQ(buchi_num_states(NULL), 0, "NULL nstates");
    ASSERT_EQ(buchi_num_trans(NULL), 0, "NULL ntrans");
    ASSERT_TRUE(!buchi_has_transition(NULL, 0, 0, 0), "NULL has_trans");
    ASSERT_NULL(buchi_post(NULL, 0, 0), "NULL post");
    buchi_set_name(NULL, "x");
    buchi_set_accepting(NULL, NULL, 0);
    buchi_add_accepting(NULL, 0);
    buchi_set_alphabet(NULL, NULL, 0);
    ASSERT_EQ(buchi_add_transition(NULL, 0, 0, 0), -1, "NULL add_trans");
    buchi_free(NULL);
    omega_free(NULL);
    buchi_print(NULL);
    buchi_print_dot(NULL, NULL);
    buchi_print_run(NULL, 0);
    PASS();
}

/* L3: DOT graph output format */
static void test_dot_output(void) {
    TEST("DOT output format verification");
    BuchiAutomaton* ba = buchi_create(2, 0, 2);
    BuchiSymbol alpha[] = {0, 1};
    buchi_set_alphabet(ba, alpha, 2);
    buchi_add_accepting(ba, 1);
    buchi_add_transition(ba, 0, 0, 1);
    buchi_add_transition(ba, 1, 0, 1);
    buchi_print_dot(ba, "build/test_output.dot");
    buchi_free(ba);
    FILE* f = fopen("build/test_output.dot", "r");
    ASSERT_NOT_NULL(f, "DOT file");
    fseek(f, 0, SEEK_END);
    ASSERT_TRUE(ftell(f) > 50, "DOT file too small");
    fclose(f);
    PASS();
}

/* L2: Omega-word equality up to bound */
static void test_omega_equality(void) {
    TEST("Omega-word equality up to bound");
    BuchiSymbol p1[] = {0, 1, 2};
    OmegaWord* w1 = omega_make_prefix(p1, 3);
    OmegaWord* w2 = omega_make_prefix(p1, 3);
    ASSERT_TRUE(omega_equals_up_to(w1, w2, 3), "same prefix match");
    ASSERT_TRUE(!omega_equals_up_to(w1, NULL, 3), "NULL compare");
    omega_free(w1); omega_free(w2);
    PASS();
}

int main(void) {
    printf("=== test_buchi_core ===\n\n");
    test_nba_construction();
    test_transition_relation();
    test_omega_word_lasso();
    test_omega_word_periodic();
    test_buchi_acceptance_inf_often();
    test_scc_decomposition();
    test_null_safety();
    test_dot_output();
    test_omega_equality();
    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
