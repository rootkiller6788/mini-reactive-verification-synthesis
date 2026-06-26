#include <stdio.h>
#include "obdd.h"
int main(void) {
    printf("Starting...\n");
    fflush(stdout);
    obdd_manager_t *m = obdd_mgr_create(4);
    printf("mgr=%p\n", (void*)m);
    fflush(stdout);
    if (m) obdd_mgr_destroy(m);
    printf("Done.\n");
    return 0;
}
