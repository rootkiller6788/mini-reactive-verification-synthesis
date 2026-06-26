#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "obdd.h"
#include "model.h"
#include "ctlsmc.h"
#include "fixpoint.h"
#include "params.h"
static void t1(void){obdd_manager_t*m=obdd_mgr_create(8);obdd_mgr_destroy(m);}
static void t2(void){obdd_manager_t*m=obdd_mgr_create(4);obdd_ref_t x0=obdd_var(m,0);obdd_mgr_destroy(m);}
static void t3(void){obdd_manager_t*m=obdd_mgr_create(4);obdd_var(m,0);obdd_var(m,1);obdd_mgr_destroy(m);}
static void t4(void){obdd_manager_t*m=obdd_mgr_create(4);obdd_ref_t x0=obdd_var(m,0),x1=obdd_var(m,1);obdd_and(m,x0,x1);obdd_mgr_destroy(m);}
static void t5(void){obdd_manager_t*m=obdd_mgr_create(4);obdd_ref_t x0=obdd_var(m,0),x1=obdd_var(m,1);obdd_exists(m,obdd_and(m,x0,x1),0);obdd_mgr_destroy(m);}
static void t6(void){obdd_manager_t*m=obdd_mgr_create(3);obdd_sat_count(m,OBDD_TRUE,3);obdd_mgr_destroy(m);}
static void t7(void){obdd_manager_t*m=obdd_mgr_create(3);obdd_evaluate(m,OBDD_TRUE,3,0);obdd_mgr_destroy(m);}
static void t8(void){kripke_t*K=kripke_create(3,2);kripke_destroy(K);}
static void t9(void){ctl_formula_t*p=ctl_atom(0);ctl_free(p);}
static void t10(void){
    kripke_t*K=kripke_create(1,2);
    kripke_add_initial_state(K,0);kripke_add_transition(K,0,1);kripke_add_transition(K,1,0);
    kripke_set_label_mask(K,0,"p",0x1,2);kripke_set_label_mask(K,1,"q",0x2,2);
    ctl_formula_t*p=ctl_atom(0),*q=ctl_atom(1);
    ctl_formula_t*ag=ctl_ag(ctl_or(p,q));
    ctl_check_initial(K,ag);
    ctl_free(ag);
    kripke_destroy(K);
}
int main(void) {
    setbuf(stdout, NULL);
    printf("1 "); fflush(stdout); t1();
    printf("2 "); fflush(stdout); t2();
    printf("3 "); fflush(stdout); t3();
    printf("4 "); fflush(stdout); t4();
    printf("5 "); fflush(stdout); t5();
    printf("6 "); fflush(stdout); t6();
    printf("7 "); fflush(stdout); t7();
    printf("8 "); fflush(stdout); t8();
    printf("9 "); fflush(stdout); t9();
    printf("10 "); fflush(stdout); t10();
    printf("\nAll passed.\n");
    return 0;
}
