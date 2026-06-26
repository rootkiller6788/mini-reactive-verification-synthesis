#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "obdd.h"
int main(void) {
    setbuf(stdout, NULL);
    obdd_manager_t *m = obdd_mgr_create(4);
    obdd_ref_t x0 = obdd_var(m, 0), x1 = obdd_var(m, 1);
    obdd_ref_t f = obdd_and(m, x0, x1);
    obdd_ref_t c = obdd_compose(m, f, 0, x1);
    printf("f=0x%llx x1=0x%llx c=0x%llx c==x1:%d\n",
           (unsigned long long)f, (unsigned long long)x1,
           (unsigned long long)c, c == x1);
    /* Also test forall */
    obdd_ref_t g = obdd_or(m, x0, x1);
    obdd_ref_t fa = obdd_forall(m, g, 0);
    printf("g=0x%llx fa=0x%llx x1=0x%llx fa==x1:%d\n",
           (unsigned long long)g, (unsigned long long)fa,
           (unsigned long long)x1, fa == x1);
    /* Test evaluate */
    printf("eval f(0x7)=%d f(0x6)=%d f(0x0)=%d\n",
           obdd_evaluate(m, f, 3, 0x7),
           obdd_evaluate(m, f, 3, 0x6),
           obdd_evaluate(m, f, 3, 0x0));
    obdd_mgr_destroy(m);
    return 0;
}
