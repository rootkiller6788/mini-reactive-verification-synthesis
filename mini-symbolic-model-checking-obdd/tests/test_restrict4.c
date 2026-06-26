#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "obdd.h"
int main(void) {
    setbuf(stdout, NULL);
    obdd_manager_t *m = obdd_mgr_create(4);
    obdd_ref_t x0 = obdd_var(m, 0), x1 = obdd_var(m, 1);
    obdd_ref_t f = obdd_and(m, x0, x1);
    obdd_ref_t r = obdd_restrict(m, f, 0, true);
    printf("r==x1: %d\n", r == x1);
    printf("f=0x%llx x1=0x%llx r=0x%llx\n",
           (unsigned long long)f, (unsigned long long)x1, (unsigned long long)r);
    printf("sat_count(r,4)=%llu sat_count(x1,4)=%llu\n",
           (unsigned long long)obdd_sat_count(m, r, 4),
           (unsigned long long)obdd_sat_count(m, x1, 4));
    obdd_mgr_destroy(m);
    return 0;
}
