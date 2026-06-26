/**
 * obdd.h — Ordered Binary Decision Diagrams (OBDD)
 *
 * Implements a canonical ROBDD (Reduced Ordered Binary Decision Diagram)
 * package for boolean function representation and manipulation.
 *
 * Knowledge Coverage:
 *   L1: OBDD node, ROBDD, variable ordering, Shannon expansion
 *   L2: Canonical representation, cofactoring, variable ordering dependency
 *   L3: DAG structure, unique table, complement edges, hash consing
 *   L4: Canonicity theorem (Bryant 1986): for any fixed variable ordering,
 *       every boolean function has a unique ROBDD representation
 *   L5: ITE algorithm, Apply algorithm, Restrict, Compose, Satisfy-count
 *
 * References:
 *   Bryant, R.E. (1986). "Graph-Based Algorithms for Boolean Function
 *   Manipulation." IEEE Trans. Computers, C-35(8), 677-691.
 *   Bryant, R.E. (1992). "Symbolic Boolean manipulation with ordered
 *   binary-decision diagrams." ACM Computing Surveys, 24(3), 293-318.
 *
 * Complexity:
 *   Reduce: O(n log n) with unique table
 *   Apply: O(|f|·|g|) worst case
 *   ITE: O(|f|·|g|·|h|) worst case
 *   SatCount: O(|f|) on reduced DAG
 */

#ifndef OBDD_H
#define OBDD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * L3: Core Data Structures — Boolean DAG + Unique Table
 * ============================================================ */

#define OBDD_MAX_VARS  64
#define OBDD_MAX_NODES 65536
#define OBDD_TERMINAL_FALSE  0
#define OBDD_TERMINAL_TRUE   1

typedef int32_t obdd_var_t;

typedef enum {
    OBDD_NODE_EMPTY    = 0,
    OBDD_NODE_TERMINAL = 1,
    OBDD_NODE_INTERNAL = 2,
    OBDD_NODE_DEAD     = 3
} obdd_node_kind_t;

typedef struct {
    obdd_var_t      var;
    int32_t         low;
    int32_t         high;
    int32_t         high_comp;
    obdd_node_kind_t kind;
    int32_t         next;
    uint64_t        hash;
    int32_t         refcount;
} obdd_node_t;

typedef uint64_t obdd_ref_t;

#define OBDD_REF_TO_INDEX(ref) ((int32_t)((ref) >> 1))
#define OBDD_REF_IS_COMPLEMENTED(ref) ((bool)((ref) & 1ULL))
#define OBDD_MAKE_REF(index, comp) (((uint64_t)(index) << 1) | ((comp) ? 1ULL : 0ULL))
#define OBDD_FALSE  OBDD_MAKE_REF(OBDD_TERMINAL_FALSE, false)
#define OBDD_TRUE   OBDD_MAKE_REF(OBDD_TERMINAL_FALSE, true)

typedef struct {
    obdd_node_t   *nodes;
    int32_t       *unique_table;
    int32_t        num_nodes;
    int32_t        num_vars;
    int32_t        table_size;
    uint64_t       stats_inserts;
    uint64_t       stats_hits;
    int32_t       *var_order;
    int32_t       *inv_order;
} obdd_manager_t;

/* ============================================================
 * L1: Core OBDD Operations
 * ============================================================ */

obdd_manager_t *obdd_mgr_create(int32_t num_vars);
void obdd_mgr_destroy(obdd_manager_t *mgr);

obdd_ref_t obdd_var(obdd_manager_t *mgr, obdd_var_t var);
obdd_ref_t obdd_not(obdd_manager_t *mgr, obdd_ref_t f);

obdd_ref_t obdd_and(obdd_manager_t *mgr, obdd_ref_t f, obdd_ref_t g);
obdd_ref_t obdd_or(obdd_manager_t *mgr, obdd_ref_t f, obdd_ref_t g);
obdd_ref_t obdd_xor(obdd_manager_t *mgr, obdd_ref_t f, obdd_ref_t g);
obdd_ref_t obdd_imp(obdd_manager_t *mgr, obdd_ref_t f, obdd_ref_t g);
obdd_ref_t obdd_equiv(obdd_manager_t *mgr, obdd_ref_t f, obdd_ref_t g);

obdd_ref_t obdd_ite(obdd_manager_t *mgr, obdd_ref_t f, obdd_ref_t g, obdd_ref_t h);

/* ============================================================
 * L3: Restriction and Composition
 * ============================================================ */

obdd_ref_t obdd_restrict(obdd_manager_t *mgr, obdd_ref_t f,
                         obdd_var_t var, bool value);
obdd_ref_t obdd_compose(obdd_manager_t *mgr, obdd_ref_t f,
                        obdd_var_t var, obdd_ref_t g);
obdd_ref_t obdd_exists(obdd_manager_t *mgr, obdd_ref_t f, obdd_var_t var);
obdd_ref_t obdd_forall(obdd_manager_t *mgr, obdd_ref_t f, obdd_var_t var);

/* ============================================================
 * L5: Analysis and Utility Operations
 * ============================================================ */

uint64_t obdd_sat_count(obdd_manager_t *mgr, obdd_ref_t f, int32_t n_vars);
bool obdd_any_sat(obdd_manager_t *mgr, obdd_ref_t f, int32_t n_vars,
                  uint64_t *assignment);
int32_t obdd_node_count(obdd_manager_t *mgr, obdd_ref_t f);
bool obdd_is_tautology(obdd_manager_t *mgr, obdd_ref_t f);
bool obdd_is_satisfiable(obdd_manager_t *mgr, obdd_ref_t f);
bool obdd_equals(obdd_manager_t *mgr, obdd_ref_t f, obdd_ref_t g);
bool obdd_evaluate(obdd_manager_t *mgr, obdd_ref_t f, int32_t n_vars,
                   uint64_t assignment);

/* ============================================================
 * L8: Variable Ordering — Dynamic Reordering
 * ============================================================ */

int32_t obdd_sift(obdd_manager_t *mgr, obdd_var_t var);
int32_t obdd_reorder(obdd_manager_t *mgr, int32_t max_passes);
int32_t obdd_get_node_count(const obdd_manager_t *mgr);
void obdd_set_var_order(obdd_manager_t *mgr, const int32_t *order);

#ifdef __cplusplus
}
#endif

#endif /* OBDD_H */
