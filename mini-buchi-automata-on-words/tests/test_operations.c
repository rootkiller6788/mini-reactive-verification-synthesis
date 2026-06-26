/*
 * test_operations.c -- Tests for Buchi automaton operations
 *
 * L4 Fundamental Laws: closure properties of omega-regular languages
 *   - Union, Intersection, Projection
 * L5 Algorithms: product constructions, trimming, complementation
 */
#include "buchi.h"
#include "buchi_emptiness.h"
#include "omega_operations.h"
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

/* NBA accepting words with infinitely many 0s */
static BuchiAutomaton* make_A1(void) {
    BuchiAutomaton* ba = buchi_create(2, 0, 2);
    BuchiSymbol alpha[] = {0, 1};
    buchi_set_alphabet(ba, alpha, 2);
    buchi_set_name(ba, "A1");
    buchi_add_accepting(ba, 0);
    buchi_add_transition(ba, 0, 0, 0);
    buchi_add_transition(ba, 0, 1, 1);
    buchi_add_transition(ba, 1, 0, 0);
    buchi_add_transition(ba, 1, 1, 1);
    return ba;
}

/* NBA accepting words with infinitely many 1s */
static BuchiAutomaton* make_A2(void) {
    BuchiAutomaton* ba = buchi_create(2, 0, 2);
    BuchiSymbol alpha[] = {0, 1};
    buchi_set_alphabet(ba, alpha, 2);
    buchi_set_name(ba, "A2");
    buchi_add_accepting(ba, 1);
    buchi_add_transition(ba, 0, 0, 0);
    buchi_add_transition(ba, 0, 1, 1);
    buchi_add_transition(ba, 1, 0, 0);
    buchi_add_transition(ba, 1, 1, 1);
    return ba;
}

/* L4: Union closure -- L(A1) U L(A2) */
static void test_union(void) {
    TEST("Union: L(A1) U L(A2)");
    BuchiAutomaton *A1 = make_A1(), *A2 = make_A2();
    BuchiAutomaton* U = buchi_union(A1, A2);
    ASSERT_NOT_NULL(U, "union creation");

    /* Word (01)^omega should be in union (has both 0 and 1 inf often) */
    BuchiSymbol cyc[] = {0, 1};
    OmegaWord* w = omega_make_periodic(cyc, 2);
    ASSERT_EQ(buchi_accepts_lasso(U, w), 1, "(01)^omega in L(A1) U L(A2)");

    /* Word 1^omega has inf many 1s, but not 0s. In A2 so in union */
    BuchiSymbol ones[] = {1};
    OmegaWord* w1 = omega_make_periodic(ones, 1);
    ASSERT_EQ(buchi_accepts_lasso(U, w1), 1, "1^omega in union");

    PASS();
    omega_free(w); omega_free(w1);
    buchi_free(U);
    buchi_free(A1); buchi_free(A2);
}

/* L4: Intersection closure -- L(A1) cap L(A2) */
static void test_intersection(void) {
    TEST("Intersection: L(A1) cap L(A2)");
    BuchiAutomaton *A1 = make_A1(), *A2 = make_A2();
    BuchiAutomaton* I = buchi_intersect(A1, A2);
    ASSERT_NOT_NULL(I, "intersection creation");

    /* (01)^omega has both 0 and 1 inf often, should be in intersection */
    BuchiSymbol cyc[] = {0, 1};
    OmegaWord* w = omega_make_periodic(cyc, 2);
    ASSERT_EQ(buchi_accepts_lasso(I, w), 1, "(01)^omega in intersection");

    /* 0^omega has inf many 0s but not 1s, should not be in intersection */
    BuchiSymbol zeros[] = {0};
    OmegaWord* wz = omega_make_periodic(zeros, 1);
    ASSERT_EQ(buchi_accepts_lasso(I, wz), 0, "0^omega not in intersection");

    PASS();
    omega_free(w); omega_free(wz);
    buchi_free(I);
    buchi_free(A1); buchi_free(A2);
}

/* L4: Projection -- hide symbols */
static BuchiSymbol proj_id(BuchiSymbol sym) { return sym; }
static void test_projection(void) {
    TEST("Projection: symbol renaming");
    BuchiAutomaton* A1 = make_A1();
    BuchiSymbol new_alpha[] = {0, 1};
    BuchiAutomaton* P = buchi_project(A1, proj_id, new_alpha, 2);
    ASSERT_NOT_NULL(P, "projection creation");

    /* Identity projection preserves language */
    BuchiSymbol cyc[] = {0};
    OmegaWord* w = omega_make_periodic(cyc, 1);
    ASSERT_EQ(buchi_accepts_lasso(P, w), 1, "identity projection preserves 0^omega");

    PASS();
    omega_free(w);
    buchi_free(P);
    buchi_free(A1);
}

/* L5: Trimming -- remove useless states */
static void test_trim(void) {
    TEST("Trimming: remove unreachable and dead states");
    /* Build automaton where state 2 is unreachable and state 3
     * cannot reach any accepting SCC. Both should be removed. */
    BuchiAutomaton* ba = buchi_create(4, 0, 2);
    BuchiSymbol alpha[] = {0, 1};
    buchi_set_alphabet(ba, alpha, 2);
    buchi_add_accepting(ba, 1);
    buchi_add_transition(ba, 0, 0, 1);
    buchi_add_transition(ba, 1, 1, 1); /* self-loop, accepting */
    /* State 2 is unreachable, state 3 dead-end */
    buchi_add_transition(ba, 3, 1, 3);

    BuchiAutomaton* T = buchi_trim(ba);
    ASSERT_NOT_NULL(T, "trim result");

    /* Trimmed automaton should have fewer states */
    ASSERT_TRUE(buchi_num_states(T) < 4, "trimmed should reduce states");

    /* Should still be non-empty (accepting loop exists) */
    ASSERT_EQ(buchi_is_empty(T), 0, "trimmed should still be non-empty");

    PASS();
    buchi_free(T);
    buchi_free(ba);
}

/* L5: Language containment: word-sampling check */
static void test_language_containment(void) {
    TEST("Language containment: sample-word check");
    /* Build two automata with different languages.
     * A_io1: accepts inf-1 (words with infinitely many 1s)
     * A_ioa: accepts inf-0 (words with infinitely many 0s)
     *
     * L(inf-1) is NOT contained in L(inf-0):
     *   witness: 1^omega is in inf-1 but not in inf-0
     * Use direct word acceptance to verify this. */
    BuchiAutomaton* A_io1 = make_A2(); /* inf-1 */
    BuchiAutomaton* A_ioa = make_A1(); /* inf-0 */

    /* 1^omega: in inf-1, not in inf-0 */
    BuchiSymbol ones[] = {1};
    OmegaWord* w1 = omega_make_periodic(ones, 1);
    ASSERT_EQ(buchi_accepts_lasso(A_io1, w1), 1, "1^omega in inf-1");

    /* 1^omega should NOT be in inf-0 */
    ASSERT_EQ(buchi_accepts_lasso(A_ioa, w1), 0, "1^omega not in inf-0");

    /* (01)^omega: in both inf-1 and inf-0 */
    BuchiSymbol ab[] = {0, 1};
    OmegaWord* w_ab = omega_make_periodic(ab, 2);
    ASSERT_EQ(buchi_accepts_lasso(A_io1, w_ab), 1, "(01)^omega in inf-1");
    ASSERT_EQ(buchi_accepts_lasso(A_ioa, w_ab), 1, "(01)^omega in inf-0");

    PASS();
    omega_free(w1); omega_free(w_ab);
    buchi_free(A_io1); buchi_free(A_ioa);
}

/* L5: Language equivalence: identical automata via trim */
static void test_language_equivalence(void) {
    TEST("Language equivalence: trim preserves language");
    /* Trimming should produce a language-equivalent automaton.
     * We check that an accepting word for the original
     * is also accepted by the trimmed version. */
    BuchiAutomaton* A1 = make_A1();
    BuchiAutomaton* T = buchi_trim(A1);

    /* Both should accept the same test word */
    BuchiSymbol ab[] = {0, 1};
    OmegaWord* w = omega_make_periodic(ab, 2);
    int acc_A1 = buchi_accepts_lasso(A1, w);
    int acc_T = buchi_accepts_lasso(T, w);
    ASSERT_EQ(acc_A1, acc_T, "trim preserves language on test word");

    /* Both should be non-empty */
    ASSERT_EQ(buchi_is_empty(A1), 0, "A1 non-empty");
    ASSERT_EQ(buchi_is_empty(T), 0, "T non-empty");

    PASS();
    omega_free(w);
    buchi_free(T);
    buchi_free(A1);
}

/* L5: BuchiStats collection */
static void test_stats(void) {
    TEST("BuchiStats: automaton statistics");
    BuchiAutomaton* ba = make_A1();
    BuchiStats bs = buchi_stats(ba);
    ASSERT_EQ(bs.n_states, 2, "stats n_states");
    ASSERT_EQ(bs.n_trans, 4, "stats n_trans");
    ASSERT_EQ(bs.n_accepting, 1, "stats n_accepting");
    PASS();
    buchi_free(ba);
}

/* L4: Product with Kripke structure (basic model checking) */
static void test_kripke_product(void) {
    TEST("Product with Kripke structure");
    /* Simple KS: 2 states, each labeled with a proposition.
     * KS state 0: label=0, succ={1}; KS state 1: label=1, succ={0}
     * NBA accepts infinitely many 1s.
     * Product should be non-empty (the KS always alternates 0,1) */
    BuchiAutomaton* ba = make_A2();
    KripkeStructure ks;
    ks.n_states = 2;
    ks.n_props = 2;
    ks.labels = (int*)malloc(2 * sizeof(int));
    ks.labels[0] = 0;
    ks.labels[1] = 1;
    ks.successors = (int**)malloc(2 * sizeof(int*));
    ks.successors[0] = (int*)malloc(3 * sizeof(int));
    ks.successors[0][0] = 1; ks.successors[0][1] = -1;
    ks.successors[1] = (int*)malloc(3 * sizeof(int));
    ks.successors[1][0] = 0; ks.successors[1][1] = -1;

    BuchiAutomaton* P = buchi_product_ks(ba, &ks);
    ASSERT_NOT_NULL(P, "product creation");

    /* The product should be non-empty (infinitely many 1-labeled states visited) */
    ASSERT_EQ(buchi_is_empty(P), 0, "product should be non-empty");

    PASS();
    buchi_free(P);
    buchi_free(ba);
    free(ks.labels);
    free(ks.successors[0]); free(ks.successors[1]);
    free(ks.successors);
}

int main(void) {
    printf("=== test_operations ===\n\n");
    test_union();
    test_intersection();
    test_projection();
    test_trim();
    test_language_containment();
    test_language_equivalence();
    test_stats();
    test_kripke_product();
    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
