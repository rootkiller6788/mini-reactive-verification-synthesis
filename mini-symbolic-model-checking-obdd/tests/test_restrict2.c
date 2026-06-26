#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "obdd.h"
int main(void) {
    setbuf(stdout, NULL);
    obdd_manager_t *m = obdd_mgr_create(2);
    obdd_ref_t x0 = obdd_var(m, 0);
    obdd_ref_t x1 = obdd_var(m, 1);
    /* f = x0 AND x1 */
    obdd_ref_t f = obdd_and(m, x0, x1);
    printf("f = 0x%llx\n", (unsigned long long)f);
    /* restrict x0=1: should get x1 */
    obdd_ref_t r = obdd_restrict(m, f, 0, true);
    printf("r = 0x%llx (expect 0x%llx = x1)\n", (unsigned long long)r, (unsigned long long)x1);
    printf("r==x1: %d\n", r == x1);
    /* Check sat_counts */
    printf("sat_count(f,2)=%llu (expect 1)\n", (unsigned long long)obdd_sat_count(m, f, 2));
    printf("sat_count(r,2)=%llu (expect 2)\n", (unsigned long long)obdd_sat_count(m, r, 2));
    obdd_mgr_destroy(m);
    return 0;
}
