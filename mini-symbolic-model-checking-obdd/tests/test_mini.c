#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "obdd.h"
#include "model.h"
#include "ctlsmc.h"
#include "fixpoint.h"
#include "params.h"
int main(void) {
    setbuf(stdout, NULL);
    printf("t1...\n");
    obdd_manager_t *m = obdd_mgr_create(8);
    if (!m) { printf("FAIL t1\n"); return 1; }
    obdd_mgr_destroy(m);
    printf("t2...\n");
    m = obdd_mgr_create(4);
    obdd_ref_t x0 = obdd_var(m, 0);
    if (!obdd_is_satisfiable(m, x0)) { printf("FAIL t2\n"); return 1; }
    obdd_mgr_destroy(m);
    printf("t3...\n");
    m = obdd_mgr_create(4);
    obdd_ref_t a = obdd_var(m, 0), b = obdd_var(m, 1);
    if (obdd_sat_count(m, obdd_and(m, a, b), 4) != 4) { printf("FAIL t3\n"); return 1; }
    obdd_mgr_destroy(m);
    printf("t4...\n");
    smc_config_t cfg = smc_config_default();
    if (cfg.max_vars != 64) { printf("FAIL t4\n"); return 1; }
    printf("All passed.\n");
    return 0;
}
