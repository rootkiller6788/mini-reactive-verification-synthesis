/*
 * test_ltl.c — Comprehensive test suite for mini-linear-temporal-logic-ltl
 *
 * Tests all core APIs: AST construction, semantic evaluation,
 * equivalence transformations, specification patterns, model checking.
 */

#include "ltl_ast.h"
#include "ltl_semantics.h"
#include "ltl_equiv.h"
#include "ltl_patterns.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  TEST %s ... ", #name); } while(0)
#define PASS() do { tests_passed++; printf("PASSED\n"); } while(0)

#define EXPECT(cond, msg) do { \
    if (!(cond)) { printf("FAILED: %s\n", msg); return; } \
} while(0)

/* ── AST Construction Tests ───────────────────────────────────── */

static void test_ast_constructors(void) {
    TEST(ast_constructors);

    /* Test all constructors */
    LtlNode* t = ltl_mk_true();
    LtlNode* f = ltl_mk_false();
    LtlNode* p0 = ltl_mk_atom(0);
    LtlNode* p1 = ltl_mk_atom(1);
    LtlNode* not_p0 = ltl_mk_not(ltl_clone(p0));
    LtlNode* and_f = ltl_mk_and(ltl_clone(p0), ltl_clone(p1));
    LtlNode* or_f = ltl_mk_or(ltl_clone(p0), ltl_clone(p1));
    LtlNode* imp_f = ltl_mk_implies(ltl_clone(p0), ltl_clone(p1));
    LtlNode* eq_f = ltl_mk_equiv(ltl_clone(p0), ltl_clone(p1));
    LtlNode* next_f = ltl_mk_next(ltl_clone(p0));
    LtlNode* finally_f = ltl_mk_finally(ltl_clone(p0));
    LtlNode* globally_f = ltl_mk_globally(ltl_clone(p0));
    LtlNode* until_f = ltl_mk_until(ltl_clone(p0), ltl_clone(p1));
    LtlNode* release_f = ltl_mk_release(ltl_clone(p0), ltl_clone(p1));
    LtlNode* weak_f = ltl_mk_weak_until(ltl_clone(p0), ltl_clone(p1));

    EXPECT(t != NULL, "true constructed");
    EXPECT(t->type == LTL_TRUE, "true type correct");
    EXPECT(f->type == LTL_FALSE, "false type correct");
    EXPECT(p0->type == LTL_ATOM, "atom type correct");
    EXPECT(p0->atom_idx == 0, "atom index correct");
    EXPECT(not_p0->type == LTL_NOT, "not type correct");
    EXPECT(and_f->type == LTL_AND, "and type correct");
    EXPECT(or_f->type == LTL_OR, "or type correct");
    EXPECT(imp_f->type == LTL_IMPLIES, "implies type correct");
    EXPECT(eq_f->type == LTL_EQUIV, "equiv type correct");
    EXPECT(next_f->type == LTL_NEXT, "next type correct");
    EXPECT(finally_f->type == LTL_FINALLY, "finally type correct");
    EXPECT(globally_f->type == LTL_GLOBALLY, "globally type correct");
    EXPECT(until_f->type == LTL_UNTIL, "until type correct");
    EXPECT(release_f->type == LTL_RELEASE, "release type correct");
    EXPECT(weak_f->type == LTL_WEAK_UNTIL, "weak until type correct");

    EXPECT(ltl_size(t) == 1, "true size=1");
    EXPECT(ltl_size(and_f) == 3, "and(p0,p1) size=3");
    EXPECT(ltl_depth(until_f) == 2, "until(p0,p1) depth=2");
    EXPECT(ltl_temporal_depth(globally_f) == 1, "G(p0) temporal depth=1");

    /* Cleanup */
    ltl_free(t);
    ltl_free(f);
    ltl_free(p0);
    ltl_free(p1);
    ltl_free(not_p0);
    ltl_free(and_f);
    ltl_free(or_f);
    ltl_free(imp_f);
    ltl_free(eq_f);
    ltl_free(next_f);
    ltl_free(finally_f);
    ltl_free(globally_f);
    ltl_free(until_f);
    ltl_free(release_f);
    ltl_free(weak_f);

    PASS();
}

/* ── Clone and Equality Tests ─────────────────────────────────── */

static void test_ast_clone_equals(void) {
    TEST(ast_clone_equals);

    LtlNode* orig = ltl_mk_and(
        ltl_mk_atom(0),
        ltl_mk_globally(ltl_mk_implies(ltl_mk_atom(1), ltl_mk_next(ltl_mk_atom(2))))
    );

    LtlNode* clone = ltl_clone(orig);
    EXPECT(clone != NULL, "clone created");
    EXPECT(clone != orig, "clone is different pointer");
    EXPECT(ltl_equals(orig, clone), "clone equals original");
    EXPECT(ltl_size(orig) == ltl_size(clone), "same size after clone");

    LtlNode* diff = ltl_mk_or(ltl_mk_atom(0), ltl_mk_atom(1));
    EXPECT(!ltl_equals(orig, diff), "different formulas not equal");

    ltl_free(orig);
    ltl_free(clone);
    ltl_free(diff);

    PASS();
}

/* ── Formula I/O Tests ────────────────────────────────────────── */

static void test_ast_print(void) {
    TEST(ast_print);

    const char* names[] = {"request", "acknowledge", "done"};
    ltl_set_atom_names(names, 3);

    LtlNode* f = ltl_mk_globally(
        ltl_mk_implies(ltl_mk_atom(0), ltl_mk_finally(ltl_mk_atom(1)))
    );

    printf("\n    Formula: ");
    ltl_print(f);
    printf("    Prefix:  ");
    ltl_print_prefix(f);

    /* Test s-expression round-trip */
    char* sexpr = ltl_to_sexpr(f);
    EXPECT(sexpr != NULL, "s-expr generated");
    printf("    S-expr:  %s\n", sexpr);

    LtlNode* parsed = ltl_parse(sexpr);
    EXPECT(parsed != NULL, "s-expr parsed");
    EXPECT(ltl_equals(f, parsed), "round-trip equality");

    free(sexpr);
    ltl_free(f);
    ltl_free(parsed);

    PASS();
}

/* ── Trace Semantics Tests ────────────────────────────────────── */

static void test_trace_basics(void) {
    TEST(trace_basics);

    /* Create a lasso trace: prefix [p0], cycle [p0, p1] */
    LtlPropSet prefix[1] = { LTL_PROP_ADD(LTL_PROP_EMPTY, 0) };
    LtlPropSet cycle[2] = {
        LTL_PROP_ADD(LTL_PROP_EMPTY, 0),
        LTL_PROP_ADD(LTL_PROP_EMPTY, 1)
    };

    LtlTrace* trace = ltl_trace_create_lasso(prefix, 1, cycle, 2);
    EXPECT(trace != NULL, "trace created");
    EXPECT(trace->prefix_len == 1, "prefix length");
    EXPECT(trace->cycle_len == 2, "cycle length");
    EXPECT(LTL_PROP_CONTAINS(ltl_trace_get(trace, 0), 0), "step 0 has p0");
    EXPECT(LTL_PROP_CONTAINS(ltl_trace_get(trace, 1), 0), "step 1 has p0");
    EXPECT(LTL_PROP_CONTAINS(ltl_trace_get(trace, 2), 1), "step 2 has p1");
    EXPECT(LTL_PROP_CONTAINS(ltl_trace_get(trace, 3), 0), "step 3 has p0");

    ltl_trace_free(trace);

    /* Single infinite state */
    LtlPropSet state = LTL_PROP_ADD(LTL_PROP_ADD(LTL_PROP_EMPTY, 0), 1);
    LtlTrace* t2 = ltl_trace_create_single(state);
    EXPECT(t2 != NULL, "single state trace");
    EXPECT(LTL_PROP_CONTAINS(ltl_trace_get(t2, 0), 0), "always p0");
    EXPECT(LTL_PROP_CONTAINS(ltl_trace_get(t2, 100), 1), "always p1");
    ltl_trace_free(t2);

    PASS();
}

/* ── LTL Satisfaction Tests ───────────────────────────────────── */

static void test_ltl_satisfaction(void) {
    TEST(ltl_satisfaction);

    /* Trace where p0 is always true, p1 is always false */
    LtlPropSet state = LTL_PROP_ADD(LTL_PROP_EMPTY, 0);
    LtlTrace* trace = ltl_trace_create_single(state);

    /* Test atomic propositions */
    LtlNode* p0 = ltl_mk_atom(0);
    LtlNode* p1 = ltl_mk_atom(1);

    EXPECT(ltl_satisfies(trace, p0) == 1, "trace satisfies p0");
    EXPECT(ltl_satisfies(trace, p1) == 0, "trace does not satisfy p1");

    /* Test boolean connectives */
    LtlNode* not_p1 = ltl_mk_not(ltl_clone(p1));
    EXPECT(ltl_satisfies(trace, not_p1) == 1, "trace satisfies ¬p1");

    LtlNode* and_f = ltl_mk_and(ltl_clone(p0), ltl_clone(not_p1));
    EXPECT(ltl_satisfies(trace, and_f) == 1, "trace satisfies p0 ∧ ¬p1");

    ltl_free(and_f);

    LtlNode* or_f = ltl_mk_or(ltl_clone(p0), ltl_mk_atom(1));
    EXPECT(ltl_satisfies(trace, or_f) == 1, "trace satisfies p0 ∨ p1");
    ltl_free(or_f);

    /* Test temporal operators */
    LtlNode* next_p0 = ltl_mk_next(ltl_clone(p0));
    EXPECT(ltl_satisfies(trace, next_p0) == 1, "trace satisfies X p0");

    LtlNode* globally_p0 = ltl_mk_globally(ltl_clone(p0));
    EXPECT(ltl_satisfies(trace, globally_p0) == 1, "trace satisfies G p0");

    LtlNode* finally_p1 = ltl_mk_finally(ltl_clone(p1));
    EXPECT(ltl_satisfies(trace, finally_p1) == 0, "trace does not satisfy F p1");

    /* Trace where p0 alternates: always p0, with p1 every 3rd step */
    LtlPropSet pre[3] = { 0, 0, 0 };
    LtlPropSet cyc[3] = {
        LTL_PROP_ADD(LTL_PROP_EMPTY, 0),
        LTL_PROP_ADD(LTL_PROP_ADD(LTL_PROP_EMPTY, 0), 1),
        LTL_PROP_ADD(LTL_PROP_EMPTY, 0)
    };
    LtlTrace* t2 = ltl_trace_create_lasso(pre, 0, cyc, 3);

    /* G F p1: p1 holds infinitely often */
    LtlNode* gf_p1 = ltl_mk_globally(ltl_mk_finally(ltl_clone(p1)));
    EXPECT(ltl_satisfies(t2, gf_p1) == 1, "G F p1 holds (infinitely often)");

    /* G p0: p0 always holds */
    LtlNode* g_p0 = ltl_mk_globally(ltl_clone(p0));
    EXPECT(ltl_satisfies(t2, g_p0) == 1, "G p0 holds (always)");

    ltl_free(p0);
    ltl_free(p1);
    ltl_free(next_p0);
    ltl_free(globally_p0);
    ltl_free(finally_p1);
    ltl_free(gf_p1);
    ltl_free(g_p0);
    ltl_trace_free(trace);
    ltl_trace_free(t2);

    PASS();
}

/* ── Until Operator Tests ─────────────────────────────────────── */

static void test_ltl_until(void) {
    TEST(ltl_until);

    /* Trace: p0 at steps 0,1,2; p1 at step 3; both then forever.
     * Use a lasso: prefix (p0, p0, p0) then cycle (p0∧p1) repeated infinitely. */
    LtlPropSet pre[3] = {
        LTL_PROP_ADD(LTL_PROP_EMPTY, 0),
        LTL_PROP_ADD(LTL_PROP_EMPTY, 0),
        LTL_PROP_ADD(LTL_PROP_EMPTY, 0)
    };
    LtlPropSet cycle[1] = {
        LTL_PROP_ADD(LTL_PROP_ADD(LTL_PROP_EMPTY, 0), 1)
    };
    LtlTrace* trace = ltl_trace_create_lasso(pre, 3, cycle, 1);

    /* p0 U p1: should be true (p1 at step 3, p0 at 0,1,2) */
    LtlNode* until_f = ltl_mk_until(ltl_mk_atom(0), ltl_mk_atom(1));
    EXPECT(ltl_satisfies(trace, until_f) == 1, "p0 U p1 holds");

    /* p1 U p0: p0 holds at step 0, so p1 U p0 is satisfied vacuously at j=0.
     * For a meaningful test, use p2 U p1 where p2 never holds: should be false
     * since p2 never holds before p1 at step 3. */
    LtlNode* until2 = ltl_mk_until(ltl_mk_atom(2), ltl_mk_atom(1));
    EXPECT(ltl_satisfies(trace, until2) == 0, "p2 U p1 does not hold (p2 never true)");

    ltl_free(until_f);
    ltl_free(until2);
    ltl_trace_free(trace);

    PASS();
}

/* ── NNF Conversion Tests ─────────────────────────────────────── */

static void test_nnf(void) {
    TEST(nnf);

    /* ¬(p0 ∧ F p1) → NNF: ¬p0 ∨ G ¬p1 */
    LtlNode* f = ltl_mk_not(
        ltl_mk_and(ltl_mk_atom(0), ltl_mk_finally(ltl_mk_atom(1)))
    );

    LtlNode* nnf = ltl_to_nnf(f);
    EXPECT(nnf != NULL, "NNF created");

    printf("\n    Original: ");
    ltl_print(f);
    printf("    NNF:      ");
    ltl_print(nnf);

    /* NNF should have no NOT nodes over complex subformulas */
    EXPECT(ltl_count_operators(nnf, LTL_NOT) == 0 ||
           (nnf->type == LTL_OR && nnf->left->type == LTL_NOT),
           "NNF has negations only at literals");

    /* Test ¬G p → NNF: F ¬p */
    LtlNode* f2 = ltl_mk_not(ltl_mk_globally(ltl_mk_atom(0)));
    LtlNode* nnf2 = ltl_to_nnf(f2);
    EXPECT(nnf2->type == LTL_FINALLY, "¬G p → F ¬p");
    printf("    NNF(¬G p0): ");
    ltl_print(nnf2);

    /* Test ¬(φ U ψ) → NNF: (¬φ) R (¬ψ) */
    LtlNode* f3 = ltl_mk_not(
        ltl_mk_until(ltl_mk_atom(0), ltl_mk_atom(1))
    );
    LtlNode* nnf3 = ltl_to_nnf(f3);
    EXPECT(nnf3->type == LTL_RELEASE, "¬(φ U ψ) → (¬φ) R (¬ψ)");
    printf("    NNF(¬(p0 U p1)): ");
    ltl_print(nnf3);

    /* Verify duality: ¬(φ U ψ) ≡ (¬φ) R (¬ψ) */
    EXPECT(ltl_check_until_release_duality(ltl_mk_atom(0), ltl_mk_atom(1)) == 1,
           "until-release duality holds");

    ltl_free(f);
    ltl_free(nnf);
    ltl_free(f2);
    ltl_free(nnf2);
    ltl_free(f3);
    ltl_free(nnf3);

    PASS();
}

/* ── Simplification Tests ─────────────────────────────────────── */

static void test_simplify(void) {
    TEST(simplify);

    /* true ∧ p0 → p0 */
    LtlNode* f1 = ltl_mk_and(ltl_mk_true(), ltl_mk_atom(0));
    LtlNode* s1 = ltl_simplify(f1);
    EXPECT(s1->type == LTL_ATOM && s1->atom_idx == 0,
           "true ∧ p → p");
    ltl_free(f1); ltl_free(s1);

    /* false ∨ p0 → p0 */
    LtlNode* f2 = ltl_mk_or(ltl_mk_false(), ltl_mk_atom(0));
    LtlNode* s2 = ltl_simplify(f2);
    EXPECT(s2->type == LTL_ATOM && s2->atom_idx == 0,
           "false ∨ p → p");
    ltl_free(f2); ltl_free(s2);

    /* F F p0 → F p0 */
    LtlNode* f3 = ltl_mk_finally(ltl_mk_finally(ltl_mk_atom(0)));
    LtlNode* s3 = ltl_simplify(f3);
    EXPECT(s3->type == LTL_FINALLY && s3->left->type == LTL_ATOM,
           "F F p → F p");
    ltl_free(f3); ltl_free(s3);

    /* G G p0 → G p0 */
    LtlNode* f4 = ltl_mk_globally(ltl_mk_globally(ltl_mk_atom(0)));
    LtlNode* s4 = ltl_simplify(f4);
    EXPECT(s4->type == LTL_GLOBALLY && s4->left->type == LTL_ATOM,
           "G G p → G p");
    ltl_free(f4); ltl_free(s4);

    /* true U φ → F φ */
    LtlNode* f5 = ltl_mk_until(ltl_mk_true(), ltl_mk_atom(0));
    LtlNode* s5 = ltl_simplify(f5);
    EXPECT(s5->type == LTL_FINALLY, "true U φ → F φ");
    ltl_free(f5); ltl_free(s5);

    PASS();
}

/* ── Pattern Tests ────────────────────────────────────────────── */

static void test_patterns(void) {
    TEST(patterns);

    /* Response pattern: G(p → F q) */
    LtlNode* resp = ltl_pattern_response_global(0, 1);
    EXPECT(resp != NULL, "response pattern created");
    EXPECT(resp->type == LTL_GLOBALLY, "response: outermost G");
    printf("\n    Response: ");
    ltl_print(resp);

    /* Mutex pattern: G ¬(p ∧ q) */
    LtlNode* mutex = ltl_pattern_mutex(0, 1);
    EXPECT(mutex != NULL, "mutex pattern created");
    printf("    Mutex:    ");
    ltl_print(mutex);

    /* Infinitely often: G F p */
    LtlNode* gf = ltl_pattern_infinitely_often(0);
    EXPECT(gf->type == LTL_GLOBALLY, "GF p: outermost G");
    printf("    GF p:     ");
    ltl_print(gf);

    /* Sequence F(p ∧ X F(q ∧ X F r)) */
    int seq[3] = {0, 1, 2};
    LtlNode* sequence = ltl_pattern_sequence(seq, 3);
    EXPECT(sequence != NULL, "sequence pattern");
    printf("    Seq(p,q,r): ");
    ltl_print(sequence);

    ltl_free(resp);
    ltl_free(mutex);
    ltl_free(gf);
    ltl_free(sequence);

    PASS();
}

/* ── Kripke Structure Tests ───────────────────────────────────── */

static void test_kripke(void) {
    TEST(kripke);

    /* Simple 3-state Kripke structure:
     *   s0 (init, {p0}) → s1 ({p1}) → s2 ({p0,p1}) → s0
     */
    int edges[] = {0, 1, 1, 2, 2, 0};
    LtlPropSet labels[3] = {
        LTL_PROP_ADD(LTL_PROP_EMPTY, 0),
        LTL_PROP_ADD(LTL_PROP_EMPTY, 1),
        LTL_PROP_ADD(LTL_PROP_ADD(LTL_PROP_EMPTY, 0), 1)
    };
    int init[1] = {0};

    LtlKripke* K = ltl_kripke_from_edges(3, edges, 3, init, 1, labels);
    EXPECT(K != NULL, "Kripke structure created");
    EXPECT(K->n_states == 3, "3 states");
    EXPECT(K->n_initial == 1, "1 initial state");
    EXPECT(K->initial[0] == 0, "initial state is s0");

    /* Check labels */
    EXPECT(LTL_PROP_CONTAINS(K->labels[0], 0), "s0 has p0");
    EXPECT(LTL_PROP_CONTAINS(K->labels[2], 0), "s2 has p0");
    EXPECT(LTL_PROP_CONTAINS(K->labels[2], 1), "s2 has p1");

    printf("\n");
    ltl_kripke_print(K);
    ltl_kripke_free(K);

    PASS();
}

/* ── Model Checking Tests ─────────────────────────────────────── */

static void test_model_checking(void) {
    TEST(model_checking);

    /* Kripke: 2 states, s0→s1, s1→s1
     * s0: {p0}, s1: {p1} */
    int edges[] = {0, 1, 1, 1};
    LtlPropSet labels[2] = {
        LTL_PROP_ADD(LTL_PROP_EMPTY, 0),
        LTL_PROP_ADD(LTL_PROP_EMPTY, 1)
    };
    int init[1] = {0};

    LtlKripke* K = ltl_kripke_from_edges(2, edges, 2, init, 1, labels);
    EXPECT(K != NULL, "Kripke for MC created");

    /* Property: G F p0 — p0 holds infinitely often
     * In this Kripke: p0 only at s0 (initial), then never again.
     * So G F p0 should be FALSE. */
    LtlNode* gf_p0 = ltl_mk_globally(ltl_mk_finally(ltl_mk_atom(0)));

    LtlPath* cex = NULL;
    int result = ltl_model_check(K, gf_p0, &cex);

    /* G F p0 does NOT hold in this Kripke (p0 only occurs once) */
    EXPECT(result == 0, "G F p0 violated (p0 only at start)");
    if (cex) {
        printf("\n    Counterexample: ");
        ltl_path_print(cex);
        ltl_path_free(cex);
    }

    ltl_free(gf_p0);

    /* Property: F G p1 — eventually always p1
     * In this Kripke: after step 1, always in s1 with p1.
     * So F G p1 should be TRUE. */
    LtlNode* fg_p1 = ltl_mk_finally(ltl_mk_globally(ltl_mk_atom(1)));
    LtlPath* cex2 = NULL;
    int result2 = ltl_model_check(K, fg_p1, &cex2);
    /* F G p1 holds: from s1 onward, p1 always true */
    EXPECT(result2 == 1, "F G p1 holds (eventually always p1)");
    ltl_free(fg_p1);
    if (cex2) ltl_path_free(cex2);

    ltl_kripke_free(K);

    PASS();
}

/* ── Expansion Law Tests ──────────────────────────────────────── */

static void test_expansion(void) {
    TEST(expansion);

    /* G φ → φ ∧ X(G φ) */
    LtlNode* g_p0 = ltl_mk_globally(ltl_mk_atom(0));
    LtlNode* expanded = ltl_expand(g_p0);
    EXPECT(expanded->type == LTL_AND, "G φ expansion is conjunction");
    printf("\n    G p0:           ");
    ltl_print(g_p0);
    printf("    expand(G p0):   ");
    ltl_print(expanded);

    ltl_free(g_p0);
    ltl_free(expanded);

    /* φ U ψ → ψ ∨ (φ ∧ X(φ U ψ)) */
    LtlNode* until_f = ltl_mk_until(ltl_mk_atom(0), ltl_mk_atom(1));
    LtlNode* exp_u = ltl_expand(until_f);
    EXPECT(exp_u->type == LTL_OR, "U expansion is disjunction");
    printf("    expand(p0 U p1): ");
    ltl_print(exp_u);

    ltl_free(until_f);
    ltl_free(exp_u);

    PASS();
}

/* ── Safety/Liveness Classification Tests ─────────────────────── */

static void test_fragments(void) {
    TEST(fragments);

    /* Safety: G p */
    LtlNode* g_p = ltl_mk_globally(ltl_mk_atom(0));
    EXPECT(ltl_is_safety_fragment(g_p) == 1, "G p is safety");
    EXPECT(ltl_is_liveness_fragment(g_p) == 0, "G p is not liveness");
    ltl_free(g_p);

    /* Liveness: F p */
    LtlNode* f_p = ltl_mk_finally(ltl_mk_atom(0));
    EXPECT(ltl_is_safety_fragment(f_p) == 0, "F p is not safety");
    EXPECT(ltl_is_liveness_fragment(f_p) == 1, "F p is liveness");
    ltl_free(f_p);

    /* Both: G F p (not safety in simple check) */
    LtlNode* gf_p = ltl_mk_globally(ltl_mk_finally(ltl_mk_atom(0)));
    EXPECT(ltl_is_safety_fragment(gf_p) == 0, "G F p is not safety");
    ltl_free(gf_p);

    PASS();
}

/* ── Bounded Semantics Tests ──────────────────────────────────── */

static void test_bounded_semantics(void) {
    TEST(bounded_semantics);

    /* Trace: p0 at step 0, p1 at step 2 */
    LtlPropSet pre[3] = {
        LTL_PROP_ADD(LTL_PROP_EMPTY, 0),
        LTL_PROP_EMPTY,
        LTL_PROP_ADD(LTL_PROP_EMPTY, 1)
    };
    LtlPropSet cycle[1] = { LTL_PROP_EMPTY };
    LtlTrace* trace = ltl_trace_create_lasso(pre, 3, cycle, 1);

    /* Bounded F p1 within bound 5: should be true (p1 at step 2 ≤ 5) */
    LtlNode* f_p1 = ltl_mk_finally(ltl_mk_atom(1));
    int r1 = ltl_evaluate_bounded(trace, 0, f_p1, 5);
    EXPECT(r1 == 1, "bounded F p1 within 5: true");

    /* Bounded F p1 within bound 1: should be false (p1 at step 2 > 1) */
    int r2 = ltl_evaluate_bounded(trace, 0, f_p1, 1);
    EXPECT(r2 == 0, "bounded F p1 within 1: false");

    ltl_free(f_p1);
    ltl_trace_free(trace);

    PASS();
}

/* ── S-Expression Parser Tests ────────────────────────────────── */

static void test_parser(void) {
    TEST(parser);

    LtlNode* f = ltl_parse("(G (IMPLIES (ATOM 0) (F (ATOM 1))))");
    EXPECT(f != NULL, "parsed G(p0 → F p1)");
    if (f) {
        EXPECT(f->type == LTL_GLOBALLY, "outermost G");
        printf("\n    Parsed: ");
        ltl_print(f);

        char* sexpr = ltl_to_sexpr(f);
        printf("    S-expr: %s\n", sexpr);
        free(sexpr);

        ltl_free(f);
    }

    PASS();
}

/* ── Equivalence Check Tests ──────────────────────────────────── */

static void test_equiv_check(void) {
    TEST(equiv_check);

    /* p0 → p1  ≡  ¬p0 ∨ p1 */
    LtlNode* f1 = ltl_mk_implies(ltl_mk_atom(0), ltl_mk_atom(1));
    LtlNode* f2 = ltl_mk_or(ltl_mk_not(ltl_mk_atom(0)), ltl_mk_atom(1));

    /* Syntactic equivalence after simplification */
    int syn_eq = ltl_syntactic_equiv(f1, f2);
    printf("\n    Syntactic equiv: %s\n", syn_eq ? "yes" : "no");

    /* Bounded semantic equivalence */
    int sem_eq = ltl_check_equivalence_bounded(f1, f2, 2, 2, 2);
    printf("    Bounded sem equiv: %s\n", sem_eq ? "yes" : "no");
    EXPECT(sem_eq == 1, "p0→p1 ≡ ¬p0∨p1 (bounded check)");

    ltl_free(f1);
    ltl_free(f2);

    PASS();
}

/* ── Fischer-Ladner Closure Tests ─────────────────────────────── */

static void test_fl_closure(void) {
    TEST(fl_closure);

    LtlNode* phi = ltl_mk_until(ltl_mk_atom(0), ltl_mk_atom(1));
    LtlSubformulaSet closure;
    ltl_fischer_ladner_closure(phi, &closure);

    EXPECT(closure.count >= 3, "FL closure has multiple formulas");
    printf("\n    FL closure of p0 U p1: %d formulas\n", closure.count);

    ltl_subformula_set_free(&closure);
    ltl_free(phi);

    PASS();
}

/* ── Unwind Tests ─────────────────────────────────────────────── */

static void test_unwind(void) {
    TEST(unwind);

    /* F p unwound 2 times: p ∨ X(p ∨ X(F p)) */
    LtlNode* f_p = ltl_mk_finally(ltl_mk_atom(0));
    LtlNode* unwound = ltl_unwind(f_p, 2);
    EXPECT(unwound != NULL, "unwound formula created");
    printf("\n    Unwind(F p0, 2): ");
    ltl_print(unwound);

    ltl_free(f_p);
    ltl_free(unwound);

    PASS();
}

/* ── Deadlock Pattern Test ────────────────────────────────────── */

static void test_deadline(void) {
    TEST(deadline);

    LtlNode* dl = ltl_pattern_deadline(0, 3);
    EXPECT(dl != NULL, "deadline pattern created");
    printf("\n    Deadline(p0, 3): ");
    ltl_print(dl);

    EXPECT(ltl_size(dl) >= 4, "deadline formula has reasonable size");
    ltl_free(dl);

    PASS();
}

/* ── Path/Trace Conversion Test ───────────────────────────────── */

static void test_path_trace(void) {
    TEST(path_trace);

    int prefix_states[] = {0, 1};
    int cycle_states[] = {2, 3, 1};
    LtlPath* path = ltl_path_create(prefix_states, 2, cycle_states, 3);
    EXPECT(path != NULL, "path created");

    printf("\n    ");
    ltl_path_print(path);

    ltl_path_free(path);

    PASS();
}

/* ── Main ─────────────────────────────────────────────────────── */

int main(void) {
    printf("=== LTL Test Suite ===\n\n");

    test_ast_constructors();
    test_ast_clone_equals();
    test_ast_print();
    test_trace_basics();
    test_ltl_satisfaction();
    test_ltl_until();
    test_nnf();
    test_simplify();
    test_patterns();
    test_kripke();
    test_model_checking();
    test_expansion();
    test_fragments();
    test_bounded_semantics();
    test_parser();
    test_equiv_check();
    test_fl_closure();
    test_unwind();
    test_deadline();
    test_path_trace();

    printf("\n=== Results: %d/%d tests passed ===\n", tests_passed, tests_run);

    if (tests_passed == tests_run) {
        printf("ALL TESTS PASSED ✅\n");
        return 0;
    } else {
        printf("SOME TESTS FAILED ❌\n");
        return 1;
    }
}
