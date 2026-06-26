#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "obdd.h"
int main(void) {
    setbuf(stdout, NULL);
    obdd_manager_t *m = obdd_mgr_create(2);
    obdd_ref_t x0 = obdd_var(m, 0);
    obdd_ref_t nx0 = obdd_not(m, x0);
    obdd_ref_t r = obdd_and(m, x0, nx0);
    printf("x0=0x%llx nx0=0x%llx and=0x%llx\n",
           (unsigned long long)x0, (unsigned long long)nx0,
           (unsigned long long)r);
    printf("sat=%d taut=%d\n",
           obdd_is_satisfiable(m, r),
           obdd_is_tautology(m, r));
    printf("TRUE=0x%llx FALSE=0x%llx\n",
           (unsigned long long)OBDD_TRUE,
           (unsigned long long)OBDD_FALSE);
    printf("r==FALSE: %d\n", r == OBDD_FALSE);
    obdd_mgr_destroy(m);
    return 0;
}
