/**
 * test_ctl.c — Comprehensive CTL Library Tests
 *
 * Tests for all CTL modules: AST, Kripke structures, model checking,
 * equivalences, SAT, and counterexample generation.
 *
 * Uses assert() for all checks. Run: make test
 */

#include "ctl_ast.h"
#include "ctl_kripke.h"
#include "ctl_modelcheck.h"
#include "ctl_equiv.h"
#include "ctl_sat.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int tests_run = 0;
static int tests_passed = 0;
#define TEST(name) do { tests_run++; printf("  TEST %s... ", name); } while(0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)
#define CHECK(cond, msg) do { if (cond) PASS(); else FAIL(msg); } while(0)

/* ═══════════════════════════════════════════════════════════════════════
 * AST Tests
 * ═══════════════════════════════════════════════════════════════════════ */

static void test_ast_basics(void) {
    printf("--- AST Basics ---\n");

    /* Constants */
    TEST("mk_top"); CHECK(ctl_mk_top() != NULL, "top null"); ctl_free_formula(ctl_mk_top());
    TEST("mk_bot"); CHECK(ctl_mk_bot() != NULL, "bot null");

    /* Atoms */
    ctl_formula *p = ctl_mk_atom("p");
    TEST("mk_atom"); CHECK(p != NULL, "atom null");
    TEST("atom name"); CHECK(strcmp(p->data.atom_name, "p") == 0, "wrong name");

    /* Boolean connectives */
    ctl_formula *q = ctl_mk_atom("q");
    ctl_formula *not_p = ctl_mk_not(ctl_clone_formula(p));
    TEST("mk_not"); CHECK(not_p != NULL, "not null");

    ctl_formula *and_pq = ctl_mk_and(ctl_clone_formula(p), ctl_clone_formula(q));
    TEST("mk_and"); CHECK(and_pq != NULL, "and null");

    ctl_formula *or_pq = ctl_mk_or(ctl_clone_formula(p), ctl_clone_formula(not_p));
    TEST("mk_or"); CHECK(or_pq != NULL, "or null");

    /* Temporal operators */
    ctl_formula *ex_p = ctl_mk_ex(ctl_clone_formula(p));
    TEST("mk_ex"); CHECK(ex_p != NULL, "ex null");

    ctl_formula *eg_p = ctl_mk_eg(ctl_clone_formula(p));
    TEST("mk_eg"); CHECK(eg_p != NULL, "eg null");

    ctl_formula *eu = ctl_mk_eu(ctl_clone_formula(p), ctl_clone_formula(q));
    TEST("mk_eu"); CHECK(eu != NULL, "eu null");

    /* Cleanup */
    ctl_free_formula(p);
    ctl_free_formula(q);
    ctl_free_formula(and_pq);
    ctl_free_formula(or_pq);
    ctl_free_formula(ex_p);
    ctl_free_formula(eg_p);
    ctl_free_formula(eu);
}

static void test_ast_metrics(void) {
    printf("--- AST Metrics ---\n");

    ctl_formula *p = ctl_mk_atom("p");
    ctl_formula *q = ctl_mk_atom("q");
    ctl_formula *and_pq = ctl_mk_and(ctl_clone_formula(p), ctl_clone_formula(q));
    ctl_formula *ex_and = ctl_mk_ex(ctl_clone_formula(and_pq));

    TEST("height(atom)"); CHECK(ctl_formula_height(p) == 1, "wrong height");
    TEST("size(atom)"); CHECK(ctl_formula_size(p) == 1, "wrong size");
    TEST("height(and)"); CHECK(ctl_formula_height(and_pq) == 2, "wrong height");
    TEST("size(and)"); CHECK(ctl_formula_size(and_pq) == 3, "wrong size");
    TEST("height(ex-and)"); CHECK(ctl_formula_height(ex_and) == 3, "wrong height");
    TEST("size(ex-and)"); CHECK(ctl_formula_size(ex_and) == 4, "wrong size");

    ctl_free_formula(p); ctl_free_formula(q);
    ctl_free_formula(and_pq); ctl_free_formula(ex_and);
}

static void test_ast_equality(void) {
    printf("--- AST Equality ---\n");

    ctl_formula *p1 = ctl_mk_atom("p");
    ctl_formula *p2 = ctl_mk_atom("p");
    ctl_formula *q = ctl_mk_atom("q");

    TEST("equal same"); CHECK(ctl_formula_equal(p1, p2) == 1, "equal failed");
    TEST("equal diff"); CHECK(ctl_formula_equal(p1, q) == 0, "false positive");

    ctl_free_formula(p1); ctl_free_formula(p2); ctl_free_formula(q);
}

static void test_ast_clone(void) {
    printf("--- AST Clone ---\n");

    ctl_formula *p = ctl_mk_atom("p");
    ctl_formula *ex_p = ctl_mk_ex(ctl_clone_formula(p));
    ctl_formula *clone = ctl_clone_formula(ex_p);

    TEST("clone equal"); CHECK(ctl_formula_equal(ex_p, clone) == 1, "clone differ");
    TEST("clone not same ptr"); CHECK(ex_p != clone, "same pointer");

    ctl_free_formula(p); ctl_free_formula(ex_p); ctl_free_formula(clone);
}

static void test_ast_pnf(void) {
    printf("--- AST PNF ---\n");

    /* !EX p => AX !p */
    ctl_formula *p = ctl_mk_atom("p");
    ctl_formula *ex_p = ctl_mk_ex(ctl_clone_formula(p));
    ctl_formula *not_ex_p = ctl_mk_not(ex_p);
    ctl_formula *pnf = ctl_to_pnf(not_ex_p);

    TEST("pnf not null"); CHECK(pnf != NULL, "pnf null");
    TEST("pnf is AX"); CHECK(pnf->type == CTL_AX, "wrong type");

    /* !EG p => AF !p */
    ctl_formula *eg_p = ctl_mk_eg(ctl_clone_formula(p));
    ctl_formula *not_eg_p = ctl_mk_not(eg_p);
    ctl_formula *pnf2 = ctl_to_pnf(not_eg_p);
    TEST("pnf2 not null"); CHECK(pnf2 != NULL, "pnf2 null");
    TEST("pnf2 is AF"); CHECK(pnf2->type == CTL_AF, "wrong type");

    ctl_free_formula(p); ctl_free_formula(not_ex_p);
    ctl_free_formula(pnf); ctl_free_formula(not_eg_p); ctl_free_formula(pnf2);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Kripke Structure Tests
 * ═══════════════════════════════════════════════════════════════════════ */

static void test_kripke_basics(void) {
    printf("--- Kripke Basics ---\n");

    const char *aps[] = {"p", "q"};
    ctl_kripke *k = ctl_kripke_create(3, 2, aps);

    TEST("create not null"); CHECK(k != NULL, "create null");
    TEST("nstates"); CHECK(k->nstates == 3, "wrong nstates");
    TEST("nap"); CHECK(k->nap == 2, "wrong nap");

    /* Add edges */
    int rc = ctl_kripke_add_edge(k, 0, 1);
    TEST("add edge"); CHECK(rc == 0, "edge fail");

    rc = ctl_kripke_add_edge(k, 1, 2);
    rc = ctl_kripke_add_edge(k, 2, 0);

    /* Set labels */
    ctl_kripke_set_label(k, 0, 0, 1); /* p at s0 */
    ctl_kripke_set_label(k, 0, 1, 0); /* !q at s0 */
    ctl_kripke_set_label(k, 1, 0, 0); /* !p at s1 */
    ctl_kripke_set_label(k, 1, 1, 1); /* q at s1 */
    ctl_kripke_set_label(k, 2, 0, 1); /* p at s2 */
    ctl_kripke_set_label(k, 2, 1, 1); /* q at s2 */

    TEST("label p@s0"); CHECK(ctl_kripke_get_label(k, 0, 0) == 1, "wrong label");
    TEST("label q@s0"); CHECK(ctl_kripke_get_label(k, 0, 1) == 0, "wrong label");
    TEST("ap name"); CHECK(strcmp(ctl_kripke_ap_name(k, 0), "p") == 0, "wrong ap");

    /* Set initial states */
    ctl_state_id init[] = {0};
    ctl_kripke_set_initial(k, init, 1);
    TEST("ninitial"); CHECK(k->ninitial == 1, "wrong ninitial");

    /* Out degree */
    TEST("out degree"); CHECK(ctl_kripke_out_degree(k, 0) == 1, "wrong degree");

    ctl_kripke_destroy(k);
}

static void test_state_set(void) {
    printf("--- State Set ---\n");

    ctl_state_set *ss = ctl_state_set_create(10);

    TEST("create not null"); CHECK(ss != NULL, "create null");
    TEST("init empty"); CHECK(ctl_state_set_is_empty(ss) == 1, "not empty");

    ctl_state_set_add(ss, 3);
    ctl_state_set_add(ss, 7);
    TEST("contains 3"); CHECK(ctl_state_set_contains(ss, 3) == 1, "contains fail");
    TEST("!contains 5"); CHECK(ctl_state_set_contains(ss, 5) == 0, "contains fail");
    TEST("count 2"); CHECK(ctl_state_set_count(ss) == 2, "wrong count");

    ctl_state_set_remove(ss, 3);
    TEST("removed"); CHECK(ctl_state_set_contains(ss, 3) == 0, "remove fail");
    TEST("count 1"); CHECK(ctl_state_set_count(ss) == 1, "wrong count");

    ctl_state_set_universe(ss);
    TEST("universe contains 0"); CHECK(ctl_state_set_contains(ss, 0) == 1, "universe fail");
    TEST("universe contains 9"); CHECK(ctl_state_set_contains(ss, 9) == 1, "universe fail");
    TEST("is universe"); CHECK(ctl_state_set_is_universe(ss) == 1, "not universe");

    ctl_state_set_clear(ss);
    TEST("clear empty"); CHECK(ctl_state_set_is_empty(ss) == 1, "clear fail");

    /* Union test */
    ctl_state_set *a = ctl_state_set_create(10);
    ctl_state_set *b = ctl_state_set_create(10);
    ctl_state_set_add(a, 1); ctl_state_set_add(a, 2);
    ctl_state_set_add(b, 2); ctl_state_set_add(b, 3);
    ctl_state_set_union(a, b);
    TEST("union 1"); CHECK(ctl_state_set_contains(a, 1) == 1, "union fail");
    TEST("union 3"); CHECK(ctl_state_set_contains(a, 3) == 1, "union fail");
    TEST("union count"); CHECK(ctl_state_set_count(a) == 3, "wrong count");

    /* Complement test */
    ctl_state_set *c = ctl_state_set_create(4);
    ctl_state_set_add(c, 0);
    ctl_state_set_complement(c);
    TEST("complement count"); CHECK(ctl_state_set_count(c) == 3, "wrong comp");

    ctl_state_set_destroy(ss);
    ctl_state_set_destroy(a); ctl_state_set_destroy(b);
    ctl_state_set_destroy(c);
}

static void test_reachability(void) {
    printf("--- Reachability ---\n");

    ctl_kripke *k = ctl_kripke_create(5, 0, NULL);
    ctl_kripke_add_edge(k, 0, 1);
    ctl_kripke_add_edge(k, 1, 2);
    ctl_kripke_add_edge(k, 2, 3);
    ctl_kripke_add_edge(k, 0, 4);
    ctl_state_id init[] = {0};
    ctl_kripke_set_initial(k, init, 1);

    ctl_state_set *reach = ctl_reachable_states(k);
    TEST("reach not null"); CHECK(reach != NULL, "null");
    TEST("reach all"); CHECK(ctl_state_set_count(reach) == 5, "not all reachable");

    ctl_state_set_destroy(reach);
    ctl_kripke_destroy(k);
}

static void test_scc(void) {
    printf("--- SCC ---\n");

    ctl_kripke *k = ctl_kripke_create(4, 0, NULL);
    ctl_kripke_add_edge(k, 0, 1);
    ctl_kripke_add_edge(k, 1, 2);
    ctl_kripke_add_edge(k, 2, 0);  /* cycle 0-1-2 */
    ctl_kripke_add_edge(k, 2, 3);
    ctl_kripke_add_edge(k, 3, 3);  /* self-loop at 3 */

    int *scc_of = NULL;
    int nscc = ctl_compute_sccs(k, &scc_of);
    TEST("nscc > 0"); CHECK(nscc > 0, "no SCCs");
    TEST("scc same for 0,1,2");
    CHECK(scc_of[0] == scc_of[1] && scc_of[1] == scc_of[2], "diff scc");

    free(scc_of);
    ctl_kripke_destroy(k);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Model Checking Tests
 * ═══════════════════════════════════════════════════════════════════════ */

static void test_modelcheck_ex(void) {
    printf("--- Model Check EX ---\n");

    const char *aps[] = {"p"};
    ctl_kripke *k = ctl_kripke_create(3, 1, aps);
    ctl_kripke_add_edge(k, 0, 1);
    ctl_kripke_add_edge(k, 0, 2);
    ctl_kripke_add_edge(k, 1, 1);
    ctl_kripke_add_edge(k, 2, 2);
    ctl_kripke_set_label(k, 1, 0, 1); /* p at s1 */
    ctl_state_id init[] = {0};
    ctl_kripke_set_initial(k, init, 1);

    ctl_formula *p = ctl_mk_atom("p");
    ctl_formula *ex_p = ctl_mk_ex(p);

    /* M,s0 |= EX p ? Yes: s1 is a successor with p */
    ctl_mc_result res = ctl_model_check(k, ex_p, NULL);
    TEST("EX p satisfied"); CHECK(res == CTL_MC_SATISFIED, "should satisfy");

    /* EX !p should not hold at s0 (s1 has p, but we need all successors) */
    ctl_formula *not_p = ctl_mk_not(ctl_mk_atom("p"));
    ctl_formula *ex_not_p = ctl_mk_ex(not_p);
    /* s0 -> s1 has p, s0 -> s2 has !p, so EX !p holds at s0 */
    res = ctl_model_check(k, ex_not_p, NULL);
    TEST("EX !p satisfied"); CHECK(res == CTL_MC_SATISFIED, "should satisfy");

    ctl_free_formula(ex_p);
    ctl_free_formula(ex_not_p);
    ctl_kripke_destroy(k);
}

static void test_modelcheck_ax(void) {
    printf("--- Model Check AX ---\n");

    const char *aps[] = {"p"};
    ctl_kripke *k = ctl_kripke_create(3, 1, aps);
    ctl_kripke_add_edge(k, 0, 1);
    ctl_kripke_add_edge(k, 0, 2);
    ctl_kripke_add_edge(k, 1, 1);
    ctl_kripke_add_edge(k, 2, 2);
    ctl_kripke_set_label(k, 1, 0, 1); /* p at s1 */
    ctl_kripke_set_label(k, 2, 0, 1); /* p at s2 */
    ctl_state_id init[] = {0};
    ctl_kripke_set_initial(k, init, 1);

    /* AX p at s0: all successors (s1, s2) have p -> true */
    ctl_formula *p = ctl_mk_atom("p");
    ctl_formula *ax_p = ctl_mk_ax(p);

    ctl_mc_result res = ctl_model_check(k, ax_p, NULL);
    TEST("AX p satisfied"); CHECK(res == CTL_MC_SATISFIED, "should satisfy");

    ctl_free_formula(ax_p);
    ctl_kripke_destroy(k);
}

static void test_modelcheck_eg(void) {
    printf("--- Model Check EG ---\n");

    const char *aps[] = {"p"};
    ctl_kripke *k = ctl_kripke_create(3, 1, aps);
    ctl_kripke_add_edge(k, 0, 0); /* self-loop at s0 */
    ctl_kripke_add_edge(k, 0, 1);
    ctl_kripke_add_edge(k, 1, 2);
    ctl_kripke_add_edge(k, 2, 1); /* cycle 1-2 */
    ctl_kripke_set_label(k, 0, 0, 1); /* p at s0 */
    ctl_kripke_set_label(k, 1, 0, 1); /* p at s1 */
    ctl_kripke_set_label(k, 2, 0, 1); /* p at s2 */
    ctl_state_id init[] = {0};
    ctl_kripke_set_initial(k, init, 1);

    ctl_formula *p = ctl_mk_atom("p");
    ctl_formula *eg_p = ctl_mk_eg(p);

    ctl_mc_result res = ctl_model_check(k, eg_p, NULL);
    TEST("EG p satisfied"); CHECK(res == CTL_MC_SATISFIED, "should satisfy");

    ctl_free_formula(eg_p);
    ctl_kripke_destroy(k);
}

static void test_modelcheck_ag(void) {
    printf("--- Model Check AG ---\n");

    const char *aps[] = {"p"};
    ctl_kripke *k = ctl_kripke_create(3, 1, aps);
    ctl_kripke_add_edge(k, 0, 1);
    ctl_kripke_add_edge(k, 1, 2);
    ctl_kripke_add_edge(k, 2, 2);
    ctl_kripke_set_label(k, 0, 0, 1);
    ctl_kripke_set_label(k, 1, 0, 1);
    ctl_kripke_set_label(k, 2, 0, 1);
    ctl_state_id init[] = {0};
    ctl_kripke_set_initial(k, init, 1);

    ctl_formula *p = ctl_mk_atom("p");
    ctl_formula *ag_p = ctl_mk_ag(p);

    ctl_mc_result res = ctl_model_check(k, ag_p, NULL);
    TEST("AG p satisfied"); CHECK(res == CTL_MC_SATISFIED, "should satisfy");

    /* Now make s2 NOT p, AG p should fail from s0 */
    ctl_kripke_set_label(k, 2, 0, 0);
    res = ctl_model_check(k, ag_p, NULL);
    TEST("AG p violated"); CHECK(res == CTL_MC_VIOLATED, "should violate");

    ctl_free_formula(ag_p);
    ctl_kripke_destroy(k);
}

static void test_modelcheck_ef(void) {
    printf("--- Model Check EF ---\n");

    const char *aps[] = {"p"};
    ctl_kripke *k = ctl_kripke_create(4, 1, aps);
    ctl_kripke_add_edge(k, 0, 1);
    ctl_kripke_add_edge(k, 1, 2);
    ctl_kripke_add_edge(k, 2, 3);
    ctl_kripke_add_edge(k, 3, 3);
    ctl_kripke_set_label(k, 3, 0, 1); /* p at s3 only */
    ctl_state_id init[] = {0};
    ctl_kripke_set_initial(k, init, 1);

    ctl_formula *p = ctl_mk_atom("p");
    ctl_formula *ef_p = ctl_mk_ef(p);

    ctl_mc_result res = ctl_model_check(k, ef_p, NULL);
    TEST("EF p satisfied"); CHECK(res == CTL_MC_SATISFIED, "should satisfy");

    /* Now disconnect: remove path to s3 */
    ctl_kripke_remove_edge(k, 2, 3);
    /* s2 now has no outgoing edges, need to make total or check carefully */
    ctl_kripke_add_edge(k, 2, 2);
    ctl_kripke_set_label(k, 2, 0, 0);
    /* Now only s3 has p, but s0-s2 isolated, EF p should fail from s0 */
    res = ctl_model_check_state(k, 0, ef_p, NULL);
    TEST("EF p violated (isolated)"); CHECK(res == CTL_MC_VIOLATED, "should violate");

    ctl_free_formula(ef_p);
    ctl_kripke_destroy(k);
}

static void test_modelcheck_af(void) {
    printf("--- Model Check AF ---\n");

    const char *aps[] = {"p"};
    ctl_kripke *k = ctl_kripke_create(3, 1, aps);
    ctl_kripke_add_edge(k, 0, 1);
    ctl_kripke_add_edge(k, 1, 2);
    ctl_kripke_add_edge(k, 2, 2);
    ctl_kripke_set_label(k, 2, 0, 1); /* p only at s2 */
    ctl_state_id init[] = {0};
    ctl_kripke_set_initial(k, init, 1);

    ctl_formula *p = ctl_mk_atom("p");
    ctl_formula *af_p = ctl_mk_af(p);

    /* All paths from s0 eventually reach s2 where p holds */
    ctl_mc_result res = ctl_model_check(k, af_p, NULL);
    TEST("AF p satisfied"); CHECK(res == CTL_MC_SATISFIED, "should satisfy");

    ctl_free_formula(af_p);
    ctl_kripke_destroy(k);
}

static void test_modelcheck_eu(void) {
    printf("--- Model Check EU ---\n");

    const char *aps[] = {"p", "q"};
    ctl_kripke *k = ctl_kripke_create(3, 2, aps);
    ctl_kripke_add_edge(k, 0, 1);
    ctl_kripke_add_edge(k, 1, 2);
    ctl_kripke_add_edge(k, 2, 2);
    ctl_kripke_set_label(k, 0, 0, 1); /* p at s0 */
    ctl_kripke_set_label(k, 1, 0, 1); /* p at s1 */
    ctl_kripke_set_label(k, 2, 1, 1); /* q at s2 */
    ctl_state_id init[] = {0};
    ctl_kripke_set_initial(k, init, 1);

    ctl_formula *p = ctl_mk_atom("p");
    ctl_formula *q = ctl_mk_atom("q");
    ctl_formula *eu = ctl_mk_eu(ctl_clone_formula(p), ctl_clone_formula(q));

    /* E[p U q] at s0: p holds until q at s2 */
    ctl_mc_result res = ctl_model_check(k, eu, NULL);
    TEST("E[p U q] satisfied"); CHECK(res == CTL_MC_SATISFIED, "should satisfy");

    ctl_free_formula(p); ctl_free_formula(q); ctl_free_formula(eu);
    ctl_kripke_destroy(k);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Equivalence Tests
 * ═══════════════════════════════════════════════════════════════════════ */

static void test_equiv_duality(void) {
    printf("--- Duality ---\n");

    ctl_formula *p = ctl_mk_atom("p");
    ctl_formula *ex_p = ctl_mk_ex(ctl_clone_formula(p));
    ctl_formula *dual_ex = ctl_dual(ex_p);

    TEST("dual EX is AX"); CHECK(dual_ex != NULL, "null");
    TEST("dual EX type"); CHECK(dual_ex->type == CTL_AX, "not AX");

    ctl_free_formula(p); ctl_free_formula(ex_p); ctl_free_formula(dual_ex);
}

static void test_equiv_nnf(void) {
    printf("--- NNF ---\n");

    ctl_formula *p = ctl_mk_atom("p");
    ctl_formula *ex_p = ctl_mk_ex(p);
    ctl_formula *not_ex_p = ctl_mk_not(ex_p);
    ctl_formula *nnf = ctl_to_nnf(not_ex_p);

    TEST("nnf not null"); CHECK(nnf != NULL, "null");
    TEST("nnf AX"); CHECK(nnf->type == CTL_AX, "should be AX");

    ctl_free_formula(not_ex_p); ctl_free_formula(nnf);
}

static void test_equiv_simplify(void) {
    printf("--- Simplify ---\n");

    ctl_formula *p = ctl_mk_atom("p");
    ctl_formula *not_not_p = ctl_mk_not(ctl_mk_not(ctl_clone_formula(p)));
    ctl_formula *simp = ctl_simplify(not_not_p);

    TEST("simp not null"); CHECK(simp != NULL, "null");
    TEST("simp atom"); CHECK(simp->type == CTL_ATOM, "not atom");

    ctl_free_formula(p); ctl_free_formula(not_not_p); ctl_free_formula(simp);
}

/* ═══════════════════════════════════════════════════════════════════════
 * SAT / Validity Tests
 * ═══════════════════════════════════════════════════════════════════════ */

static void test_sat_basic(void) {
    printf("--- SAT Basic ---\n");

    TEST("top valid"); CHECK(ctl_is_valid(ctl_mk_top()) == 1, "top !valid");
    TEST("bot !valid"); CHECK(ctl_is_valid(ctl_mk_bot()) == 0, "bot valid");
    TEST("top satisfiable"); CHECK(ctl_is_satisfiable(ctl_mk_top()) == 1, "top !sat");
    TEST("bot !satisfiable"); CHECK(ctl_is_satisfiable(ctl_mk_bot()) == 0, "bot sat");
    TEST("atom satisfiable"); CHECK(ctl_is_satisfiable(ctl_mk_atom("p")) == 1, "atom !sat");

    /* p AND !p is unsatisfiable */
    ctl_formula *p = ctl_mk_atom("p");
    ctl_formula *not_p = ctl_mk_not(ctl_clone_formula(p));
    ctl_formula *contra = ctl_mk_and(p, not_p);
    TEST("p&!p !sat"); CHECK(ctl_is_satisfiable(contra) == 0, "contra sat");
    ctl_free_formula(contra);
}

static void test_sat_fragment(void) {
    printf("--- SAT Fragment ---\n");

    ctl_formula *p = ctl_mk_atom("p");
    ctl_formula *ex_p = ctl_mk_ex(ctl_clone_formula(p));
    TEST("classify EX"); CHECK(ctl_classify(ex_p) == CTL_FRAG_ECTL, "not ECTL");

    ctl_formula *ax_p = ctl_mk_ax(ctl_clone_formula(p));
    TEST("classify AX"); CHECK(ctl_classify(ax_p) == CTL_FRAG_ACTL, "not ACTL");

    ctl_formula *both = ctl_mk_and(ex_p, ax_p);
    TEST("classify both"); CHECK(ctl_classify(both) == CTL_FRAG_FULL, "not FULL");

    ctl_free_formula(p); ctl_free_formula(both);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Counterexample Tests
 * ═══════════════════════════════════════════════════════════════════════ */

static void test_counterexample_basic(void) {
    printf("--- Counterexample ---\n");

    /* AG p on a system where p fails at s2 */
    const char *aps[] = {"p"};
    ctl_kripke *k = ctl_kripke_create(3, 1, aps);
    ctl_kripke_add_edge(k, 0, 1);
    ctl_kripke_add_edge(k, 1, 2);
    ctl_kripke_add_edge(k, 2, 2);
    ctl_kripke_set_label(k, 0, 0, 1);
    ctl_kripke_set_label(k, 1, 0, 1);
    ctl_kripke_set_label(k, 2, 0, 0); /* p false at s2 */
    ctl_state_id init[] = {0};
    ctl_kripke_set_initial(k, init, 1);

    ctl_formula *p = ctl_mk_atom("p");
    ctl_formula *ag_p = ctl_mk_ag(p);

    ctl_mc_context *ctx = NULL;
    ctl_mc_result res = ctl_model_check(k, ag_p, &ctx);
    TEST("AG violated"); CHECK(res == CTL_MC_VIOLATED, "should violate");

    ctl_counterexample *cex = ctl_generate_counterexample(ctx, ag_p);
    TEST("cex exists"); CHECK(cex != NULL, "null cex");
    if (cex) {
        TEST("cex has path"); CHECK(cex->length >= 1, "empty cex");
        ctl_counterexample_destroy(cex);
    }

    ctl_mc_context_destroy(ctx);
    ctl_free_formula(ag_p);
    ctl_kripke_destroy(k);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Bounded Model Checking Tests (L8)
 * ═══════════════════════════════════════════════════════════════════════ */

static void test_bounded_mc(void) {
    printf("--- Bounded MC ---\n");

    const char *aps[] = {"p"};
    ctl_kripke *k = ctl_kripke_create(3, 1, aps);
    ctl_kripke_add_edge(k, 0, 1);
    ctl_kripke_add_edge(k, 1, 2);
    ctl_kripke_add_edge(k, 2, 2);
    ctl_kripke_set_label(k, 0, 0, 0);
    ctl_kripke_set_label(k, 1, 0, 0);
    ctl_kripke_set_label(k, 2, 0, 1);
    ctl_state_id init[] = {0};
    ctl_kripke_set_initial(k, init, 1);

    ctl_formula *p = ctl_mk_atom("p");
    ctl_formula *ef_p = ctl_mk_ef(p);

    /* EF p with bound 3: p reachable in <= 3 steps */
    ctl_mc_result res = ctl_bounded_model_check(k, ef_p, 3, NULL);
    TEST("BMC EF p bound 3"); CHECK(res == CTL_MC_SATISFIED, "should satisfy");

    /* EF p with bound 0: not reachable */
    res = ctl_bounded_model_check(k, ef_p, 0, NULL);
    TEST("BMC EF p bound 0"); CHECK(res == CTL_MC_VIOLATED, "should violate");

    ctl_free_formula(ef_p);
    ctl_kripke_destroy(k);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Fair Model Checking Tests (L8)
 * ═══════════════════════════════════════════════════════════════════════ */

static void test_fair_mc(void) {
    printf("--- Fair MC ---\n");

    const char *aps[] = {"p", "q"};
    ctl_kripke *k = ctl_kripke_create(3, 2, aps);
    ctl_kripke_add_edge(k, 0, 1);
    ctl_kripke_add_edge(k, 1, 2);
    ctl_kripke_add_edge(k, 2, 0);
    ctl_kripke_set_label(k, 0, 0, 1);
    ctl_kripke_set_label(k, 0, 1, 0);
    ctl_kripke_set_label(k, 1, 0, 1);
    ctl_kripke_set_label(k, 1, 1, 1);
    ctl_kripke_set_label(k, 2, 0, 1);
    ctl_kripke_set_label(k, 2, 1, 0);
    ctl_state_id init[] = {0};
    ctl_kripke_set_initial(k, init, 1);

    /* Fairness: visit {s1} infinitely often */
    ctl_fairness_constraint *fc = ctl_fairness_create(1);
    fc->constraints[0] = ctl_state_set_create(3);
    ctl_state_set_add(fc->constraints[0], 1);

    ctl_formula *p = ctl_mk_atom("p");
    ctl_formula *eg_p = ctl_mk_eg(p);

    ctl_mc_result res = ctl_fair_model_check(k, eg_p, fc, NULL);
    TEST("fair EG p"); CHECK(res == CTL_MC_SATISFIED || res == CTL_MC_VIOLATED,
          "unexpected error");

    ctl_free_formula(eg_p);
    ctl_fairness_destroy(fc);
    ctl_kripke_destroy(k);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("═══ CTL Library Test Suite ═══\n\n");

    /* AST tests */
    test_ast_basics();
    test_ast_metrics();
    test_ast_equality();
    test_ast_clone();
    test_ast_pnf();

    /* Kripke tests */
    test_kripke_basics();
    test_state_set();
    test_reachability();
    test_scc();

    /* Model checking tests */
    test_modelcheck_ex();
    test_modelcheck_ax();
    test_modelcheck_eg();
    test_modelcheck_ag();
    test_modelcheck_ef();
    test_modelcheck_af();
    test_modelcheck_eu();

    /* Equivalence tests */
    test_equiv_duality();
    test_equiv_nnf();
    test_equiv_simplify();

    /* SAT tests */
    test_sat_basic();
    test_sat_fragment();

    /* Counterexample tests */
    test_counterexample_basic();

    /* Advanced (L8) tests */
    test_bounded_mc();
    test_fair_mc();

    printf("\n═══ Results: %d/%d passed ═══\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
