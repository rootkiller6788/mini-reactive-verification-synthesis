/*
 * test_emptiness.c -- Tests for Buchi automaton emptiness checking
 *
 * L4 Fundamental Law: NBA emptiness is NLOGSPACE-complete
 * L5 Algorithms: SCC-based, Nested DFS, Generalized Buchi emptiness
 */
#include "buchi.h"
#include "buchi_emptiness.h"
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

static BuchiAutomaton* make_inf_b_automaton(void) {
    BuchiAutomaton* ba = buchi_create(3, 0, 2);
    BuchiSymbol alpha[] = {0, 1};
    buchi_set_alphabet(ba, alpha, 2);
    buchi_add_accepting(ba, 1);
    buchi_add_transition(ba, 0, 0, 0);
    buchi_add_transition(ba, 0, 1, 1);
    buchi_add_transition(ba, 1, 0, 2);
    buchi_add_transition(ba, 1, 1, 1);
    buchi_add_transition(ba, 2, 0, 2);
    buchi_add_transition(ba, 2, 1, 2);
    return ba;
}

static BuchiAutomaton* make_empty_automaton(void) {
    BuchiAutomaton* ba = buchi_create(2, 0, 2);
    BuchiSymbol alpha[] = {0, 1};
    buchi_set_alphabet(ba, alpha, 2);
    buchi_add_transition(ba, 0, 0, 1);
    buchi_add_transition(ba, 0, 1, 1);
    buchi_add_transition(ba, 1, 0, 1);
    buchi_add_transition(ba, 1, 1, 1);
    return ba;
}

static void test_scc_emptiness_nonempty(void) {
    TEST("SCC-based emptiness: non-empty automaton");
    BuchiAutomaton* ba = make_inf_b_automaton();
    ASSERT_NOT_NULL(ba, "automaton creation");
    int empty = buchi_is_empty(ba);
    ASSERT_EQ(empty, 0, "inf-b automaton should be non-empty");
    PASS();
    buchi_free(ba);
}

static void test_scc_emptiness_empty(void) {
    TEST("SCC-based emptiness: empty automaton");
    BuchiAutomaton* ba = make_empty_automaton();
    ASSERT_NOT_NULL(ba, "automaton creation");
    int empty = buchi_is_empty(ba);
    ASSERT_EQ(empty, 1, "no-accepting automaton should be empty");
    PASS();
    buchi_free(ba);
}

static void test_emptiness_null(void) {
    TEST("Emptiness: NULL automaton returns empty");
    ASSERT_EQ(buchi_is_empty(NULL), 1, "NULL should be empty");
    PASS();
}

static void test_find_accepting_lasso(void) {
    TEST("Find accepting lasso from non-empty automaton");
    BuchiAutomaton* ba = make_inf_b_automaton();
    ASSERT_NOT_NULL(ba, "automaton creation");
    OmegaWord* lasso = buchi_find_accepting_lasso(ba);
    ASSERT_NOT_NULL(lasso, "should find accepting lasso");
    ASSERT_TRUE(lasso->is_lasso, "result should be lasso");
    ASSERT_TRUE(lasso->period_len > 0, "cycle should be non-empty");
    int accepted = buchi_accepts_lasso(ba, lasso);
    ASSERT_EQ(accepted, 1, "extracted lasso should be accepted");
    PASS();
    omega_free(lasso);
    buchi_free(ba);
}

static void test_lasso_from_empty(void) {
    TEST("Lasso extraction from empty automaton returns NULL");
    BuchiAutomaton* ba = make_empty_automaton();
    OmegaWord* lasso = buchi_find_accepting_lasso(ba);
    ASSERT_TRUE(lasso == NULL, "empty automaton should return NULL lasso");
    PASS();
    buchi_free(ba);
}

static void test_nested_dfs_nonempty(void) {
    TEST("Nested DFS emptiness: non-empty automaton");
    BuchiAutomaton* ba = make_inf_b_automaton();
    NestedDFS* nd = nested_dfs_create(buchi_num_states(ba));
    ASSERT_NOT_NULL(nd, "NestedDFS creation");
    int empty = nested_dfs_check(ba, nd);
    ASSERT_EQ(empty, 0, "nested DFS should find cycle");
    ASSERT_EQ(nd->found_cycle, 1, "found_cycle flag");
    OmegaWord* lasso = nested_dfs_extract_lasso(nd, ba);
    ASSERT_NOT_NULL(lasso, "should extract lasso");
    ASSERT_TRUE(lasso->is_lasso, "should be lasso");
    int accepted = buchi_accepts_lasso(ba, lasso);
    ASSERT_EQ(accepted, 1, "extracted word should be accepted");
    PASS();
    omega_free(lasso);
    nested_dfs_free(nd);
    buchi_free(ba);
}

static void test_nested_dfs_empty(void) {
    TEST("Nested DFS emptiness: empty automaton");
    BuchiAutomaton* ba = make_empty_automaton();
    NestedDFS* nd = nested_dfs_create(buchi_num_states(ba));
    ASSERT_NOT_NULL(nd, "NestedDFS creation");
    int empty = nested_dfs_check(ba, nd);
    ASSERT_EQ(empty, 1, "nested DFS should return empty");
    ASSERT_EQ(nd->found_cycle, 0, "found_cycle should be 0");
    OmegaWord* lasso = nested_dfs_extract_lasso(nd, ba);
    ASSERT_TRUE(lasso == NULL, "empty -> no lasso");
    PASS();
    omega_free(lasso);
    nested_dfs_free(nd);
    buchi_free(ba);
}

static void test_gba_emptiness_two_sets(void) {
    TEST("Generalized Buchi emptiness: 2 acceptance sets");
    BuchiAutomaton* ba = buchi_create(3, 0, 2);
    BuchiSymbol alpha[] = {0, 1};
    buchi_set_alphabet(ba, alpha, 2);
    buchi_add_transition(ba, 0, 0, 1);
    buchi_add_transition(ba, 1, 1, 2);
    buchi_add_transition(ba, 2, 0, 1);
    BuchiState F1[] = {1};
    BuchiState F2[] = {2};
    const BuchiState* F_sets[2];
    F_sets[0] = F1;
    F_sets[1] = F2;
    int F_sizes[] = {1, 1};
    int empty = gba_is_empty(ba, F_sets, F_sizes, 2);
    ASSERT_EQ(empty, 0, "GBA with intersecting SCC should be non-empty");
    PASS();
    buchi_free(ba);
}

static void test_degeneralization(void) {
    TEST("Degeneralization: GBA (2 sets) -> NBA (1 set)");
    BuchiAutomaton* base = buchi_create(2, 0, 2);
    BuchiSymbol alpha[] = {0, 1};
    buchi_set_alphabet(base, alpha, 2);
    buchi_add_transition(base, 0, 0, 1);
    buchi_add_transition(base, 1, 1, 0);
    GBA* gba = gba_create(base, 2);
    ASSERT_NOT_NULL(gba, "GBA creation");
    BuchiState F0[] = {1};
    BuchiState F1[] = {0};
    gba_add_accepting_set(gba, 0, F0, 1);
    gba_add_accepting_set(gba, 1, F1, 1);
    BuchiAutomaton* nba = gba_degeneralize(gba);
    ASSERT_NOT_NULL(nba, "degeneralization result");
    ASSERT_EQ(buchi_num_states(nba), 4, "degeneralized state count");
    int empty = buchi_is_empty(nba);
    ASSERT_EQ(empty, 0, "degeneralized NBA should be non-empty");
    PASS();
    buchi_free(nba);
    gba_free(gba);
    buchi_free(base);
}

static void test_emptiness_report(void) {
    TEST("Emptiness report statistics");
    BuchiAutomaton* ba = make_inf_b_automaton();
    EmptinessReport r = buchi_emptiness_detailed(ba);
    ASSERT_EQ(r.n_states, 3, "report n_states");
    ASSERT_EQ(r.n_trans, 6, "report n_trans");
    ASSERT_EQ(r.found_empty, 0, "should be non-empty");
    PASS();
    buchi_free(ba);
}

int main(void) {
    printf("=== test_emptiness ===\n\n");
    test_scc_emptiness_nonempty();
    test_scc_emptiness_empty();
    test_emptiness_null();
    test_find_accepting_lasso();
    test_lasso_from_empty();
    test_nested_dfs_nonempty();
    test_nested_dfs_empty();
    test_gba_emptiness_two_sets();
    test_degeneralization();
    test_emptiness_report();
    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
