#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "obdd.h"
int main(void) {
    setbuf(stdout, NULL);
    obdd_manager_t *m = obdd_mgr_create(2);
    obdd_ref_t x0 = obdd_var(m, 0);
    obdd_ref_t nx0 = obdd_not(m, x0);
    
    /* Test ITE(x0, nx0, FALSE) = x0 AND nx0 = FALSE */
    obdd_ref_t r = obdd_ite(m, x0, nx0, OBDD_FALSE);
    printf("ITE(x0, nx0, FALSE) = 0x%llx (expect 0)\n", (unsigned long long)r);
    
    /* Test ITE(TRUE, x0, FALSE) = x0 */
    r = obdd_ite(m, OBDD_TRUE, x0, OBDD_FALSE);
    printf("ITE(TRUE, x0, FALSE) = 0x%llx (expect 0x%llx)\n", (unsigned long long)r, (unsigned long long)x0);
    
    /* Test ITE(FALSE, x0, FALSE) = FALSE */
    r = obdd_ite(m, OBDD_FALSE, x0, OBDD_FALSE);
    printf("ITE(FALSE, x0, FALSE) = 0x%llx (expect 0)\n", (unsigned long long)r);
    
    /* Test ITE(x0, TRUE, FALSE) = x0 */
    r = obdd_ite(m, x0, OBDD_TRUE, OBDD_FALSE);
    printf("ITE(x0, TRUE, FALSE) = 0x%llx (expect 0x%llx)\n", (unsigned long long)r, (unsigned long long)x0);
    
    obdd_mgr_destroy(m);
    return 0;
}
