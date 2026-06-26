#include "ltl.h"
#include "buchi.h"
#include "ltl_to_buchi.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static int passed = 0, failed = 0;
#define T(n) printf("  TEST: %s ... ", n)
#define OK() do { printf("PASS
"); passed++; } while(0)
#define FAIL(m) do { printf("FAIL: %s
", m); failed++; } while(0)
#define C(c,m) do { if(c) OK(); else FAIL(m); } while(0)

int main(void) {
    printf("
=== LTL-to-Buchi Tests ===

");
    printf("L1: Definitions
");
    T("constructors");
    ltl_formula_t *t=ltl_true(),*f=ltl_false(),*p=ltl_atom(0);
    C(t&&f&&p&&t->op==LTL_CONST_TRUE&&f->op==LTL_CONST_FALSE&&p->op==LTL_ATOM,"ok");
    ltl_free(t);ltl_free(f);ltl_free(p);
    T("binary ops");
    ltl_formula_t *a=ltl_atom(0),*b=ltl_atom(1);
    ltl_formula_t *n=ltl_not(a),*cj=ltl_and(a,b),*dj=ltl_or(a,b);
    C(n->op==LTL_NOT&&cj->op==LTL_AND&&dj->op==LTL_OR,"ok");
    ltl_free(n);ltl_free(cj);ltl_free(dj);
    T("temporal ops");
    ltl_formula_t *x=ltl_next(ltl_atom(0)),*ff=ltl_finally(ltl_atom(0)),*g=ltl_globally(ltl_atom(0));
    ltl_formula_t *u=ltl_until(ltl_atom(0),ltl_atom(1));
    C(x->op==LTL_NEXT&&ff->op==LTL_FINALLY&&g->op==LTL_GLOBALLY&&u->op==LTL_UNTIL,"ok");
    ltl_free(x);ltl_free(ff);ltl_free(g);ltl_free(u);

    printf("L2: Core Concepts
");
    T("size/depth");
    ltl_formula_t *s0=ltl_atom(0),*s1=ltl_next(ltl_next(ltl_atom(0)));
    C(ltl_size(s0)==1&<(l_depth(s0)==1,"ok");
    C(ltl_size(s1)==3&<(l_depth(s1)==3,"ok");
    ltl_free(s0);ltl_free(s1);
    T("subformula count");
    ltl_formula_t *p0=ltl_atom(0),*p1=ltl_atom(1);
    ltl_formula_t *cj2=ltl_and(p0,p1);
    C(ltl_subformula_count(cj2)==3,"ok");
    ltl_free(cj2);
    T("atoms used");
    ltl_formula_t *q0=ltl_atom(0),*q2=ltl_atom(2);
    C(ltl_atoms_used(ltl_and(q0,q2))==((1u<<0)|(1u<<2)),"ok");
    ltl_free(q0);ltl_free(q2);

    printf("L3: NNF
");
    T("not-not");
    ltl_formula_t *nn1=ltl_to_nnf(ltl_not(ltl_not(ltl_atom(0))));
    C(ltl_is_nnf(nn1),"ok");ltl_free(nn1);
    T("de morgan");
    ltl_formula_t *dm=ltl_to_nnf(ltl_not(ltl_and(ltl_atom(0),ltl_atom(1))));
    C(dm->op==LTL_OR,"ok");ltl_free(dm);
    T("temporal duality");
    ltl_formula_t *nt=ltl_to_nnf(ltl_not(ltl_finally(ltl_atom(0))));
    C(nt->op==LTL_GLOBALLY,"ok");ltl_free(nt);

    printf("L4: Simplification
");
    T("true and p");
    ltl_formula_t *tp=ltl_simplify(ltl_and(ltl_true(),ltl_atom(0)));
    C(tp->op==LTL_ATOM,"ok");ltl_free(tp);
    T("p U true");
    ltl_formula_t *ut=ltl_simplify(ltl_until(ltl_atom(0),ltl_true()));
    C(ut->op==LTL_CONST_TRUE,"ok");ltl_free(ut);
    T("expansion");
    ltl_formula_t *exp=ltl_unfold_until(ltl_atom(0),ltl_atom(1));
    C(exp->op==LTL_OR,"ok");ltl_free(exp);

    printf("L5: Algorithms
");
    T("buchi construction");
    buchi_t *ba=buchi_new(4,2);
    buchi_add_state(ba,true,false);buchi_add_state(ba,false,true);
    C(ba->num_states==2&&ba->states[0].initial&&ba->states[1].accepting,"ok");
    buchi_free(ba);
    T("buchi transitions");
    buchi_t *ba2=buchi_new(4,2);
    buchi_add_state(ba2,true,false);buchi_add_state(ba2,false,true);
    buchi_label_t lbl=buchi_label_make(1,0);
    buchi_add_transition(ba2,0,1,&lbl);
    C(buchi_trans_count(ba2)==1,"ok");buchi_free(ba2);
    T("label ops");
    buchi_label_t l1=buchi_label_make(1,0),l2=buchi_label_make(0,2);
    C(buchi_label_compatible(&l1,&l2),"ok");
    C(buchi_label_satisfies(&l1,1)&&!buchi_label_satisfies(&l1,0),"ok");
    T("FL closure");
    ltl_formula_t *pu=ltl_until(ltl_atom(0),ltl_atom(1));
    ltl_closure_t *cl=ltl_fischer_ladner_closure(pu);
    C(cl&&cl->count>=4,"ok");ltl_closure_free(cl);ltl_free(pu);
    T("buchi non-empty");
    buchi_t *ba3=buchi_new(4,1);
    buchi_add_state(ba3,true,true);buchi_add_true_transition(ba3,0,0);
    C(!buchi_is_empty(ba3),"ok");buchi_free(ba3);
    T("ndfs cycle");
    buchi_t *ba4=buchi_new(4,1);
    buchi_add_state(ba4,true,true);buchi_add_true_transition(ba4,0,0);
    uint32_t *cyc=NULL;size_t clen=0;
    C(buchi_ndfs_empty(ba4,&cyc,&clen)&&cyc&&clen>0,"ok");
    free(cyc);buchi_free(ba4);

    printf("L6: Translation
");
    T("G(p->Fq) tableau");
    ltl_formula_t *phi=ltl_globally(ltl_implies(ltl_atom(0),ltl_finally(ltl_atom(1))));
    buchi_t *ba5=ltl_to_buchi(phi,TRANSLATE_TABLEAU,NULL);
    C(ba5&&buchi_state_count(ba5)>0,"ok");ltl_free(phi);buchi_free(ba5);
    T("GFp GPVW");
    ltl_formula_t *gfp=ltl_globally(ltl_finally(ltl_atom(0)));
    buchi_t *ba6=ltl_to_buchi(gfp,TRANSLATE_GPVW,NULL);
    C(ba6&&!buchi_is_empty(ba6),"ok");ltl_free(gfp);buchi_free(ba6);
    T("satisfiable");
    C(ltl_satisfiable(ltl_finally(ltl_atom(0))),"ok");
    T("valid");
    C(ltl_valid(ltl_globally(ltl_true())),"ok");
    T("safety");
    ltl_formula_t *safe=ltl_globally(ltl_not(ltl_atom(0)));
    C(ltl_is_safety(safe)&&ltl_classify(safe)==LTL_CLASS_SAFETY,"ok");ltl_free(safe);
    T("liveness");
    ltl_formula_t *live=ltl_finally(ltl_atom(0));
    C(ltl_is_liveness(live),"ok");ltl_free(live);
    T("recurrence");
    ltl_formula_t *rec=ltl_globally(ltl_finally(ltl_atom(0)));
    C(ltl_is_recurrence(rec),"ok");ltl_free(rec);

    printf("L7: Applications
");
    T("patterns");
    C(ltl_pattern_to_buchi("G(p -> F q)",NULL)!=NULL,"ok");
    C(ltl_pattern_to_buchi("G F p",NULL)!=NULL,"ok");
    C(ltl_pattern_to_buchi("p U q",NULL)!=NULL,"ok");
    T("model check");
    buchi_t *sys=buchi_new(3,2);
    buchi_add_state(sys,true,false);buchi_add_state(sys,false,true);buchi_add_state(sys,false,false);
    buchi_add_true_transition(sys,0,0);buchi_add_true_transition(sys,0,1);
    buchi_add_true_transition(sys,1,1);buchi_add_true_transition(sys,0,2);buchi_add_true_transition(sys,2,2);
    buchi_trace_t *ce=NULL;
    C(ltl_model_check(sys,ltl_globally(ltl_true()),&ce),"ok");buchi_free(sys);

    printf("
=== Results: %d passed, %d failed ===
",passed,failed);
    return failed>0?1:0;
}
