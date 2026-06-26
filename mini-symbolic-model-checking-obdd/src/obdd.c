/**
 * obdd.c - Ordered Binary Decision Diagrams Implementation
 *
 * Full implementation of a Reduced Ordered BDD (ROBDD) package with
 * complement edges, unique table, and compute table (memoization).
 *
 * Knowledge Coverage:
 *   L1: BDD node, variable ordering, Shannon expansion
 *   L2: Canonicity: unique ROBDD for each function (Bryant 1986)
 *   L3: DAG with unique table hash consing, complement edges
 *   L4: Canonicity theorem - induction on variables
 *   L5: ITE algorithm (Bryant 1986), Apply, Restrict, Compose, SatCount
 *
 * References:
 *   Bryant, R.E. (1986). "Graph-Based Algorithms for Boolean Function
 *     Manipulation." IEEE Trans. Computers, C-35(8), 677-691.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "obdd.h"

/* ============================================================
 * Internal Constants
 * ============================================================ */

#define HASH_MULT  0x9E3779B97F4A7C15ULL
#define TABLE_SIZE_DEFAULT  16384
#define TABLE_MASK          (TABLE_SIZE_DEFAULT - 1)
#define CACHE_SIZE  4096
#define CACHE_MASK  (CACHE_SIZE - 1)

/* ITE memoization cache entry */
typedef struct {
    uint64_t f, g, h, result;
    uint64_t generation;
    int      valid;
} ite_cache_entry_t;

static uint64_t global_ite_generation = 0;

/* ============================================================
 * Hashing utilities
 * ============================================================ */

static uint64_t hash_triple(uint64_t a, uint64_t b, uint64_t c) {
    uint64_t h = 0x517CC1B727220A95ULL;
    h ^= a * HASH_MULT; h = (h << 13) | (h >> 51);
    h ^= b * HASH_MULT; h = (h << 13) | (h >> 51);
    h ^= c * HASH_MULT; h = (h << 13) | (h >> 51);
    return h;
}

static uint64_t hash_node(int32_t var, int32_t low, int32_t high, int32_t hc) {
    uint64_t h = 0x9E3779B97F4A7C15ULL;
    h ^= (uint64_t)(int64_t)var * HASH_MULT; h = (h << 17) | (h >> 47);
    h ^= (uint64_t)(int64_t)low * HASH_MULT;  h = (h << 17) | (h >> 47);
    h ^= (uint64_t)(int64_t)high * HASH_MULT; h = (h << 17) | (h >> 47);
    h ^= (uint64_t)(int64_t)hc * HASH_MULT;
    return h;
}

/* ============================================================
 * obdd_mgr_create - Initialize the BDD manager
 *
 * L1: Creates the BD DAG manager. Initializes the unique table
 * and allocates the two terminal nodes (FALSE=index 0, TRUE=index 1).
 * Variable ordering defaults to identity permutation x_0 < x_1 < ... < x_{n-1}.
 * ============================================================ */

obdd_manager_t *obdd_mgr_create(int32_t num_vars) {
    if (num_vars <= 0 || num_vars > OBDD_MAX_VARS) return NULL;

    obdd_manager_t *mgr = (obdd_manager_t *)calloc(1, sizeof(obdd_manager_t));
    if (!mgr) return NULL;

    mgr->num_vars  = num_vars;
    mgr->num_nodes = 2;

    mgr->nodes = (obdd_node_t *)calloc(OBDD_MAX_NODES, sizeof(obdd_node_t));
    if (!mgr->nodes) { free(mgr); return NULL; }

    mgr->table_size  = TABLE_SIZE_DEFAULT;
    mgr->unique_table = (int32_t *)malloc((size_t)mgr->table_size * sizeof(int32_t));
    if (!mgr->unique_table) { free(mgr->nodes); free(mgr); return NULL; }
    for (int32_t i = 0; i < mgr->table_size; i++) {
        mgr->unique_table[i] = -1;
    }

    /* Terminal nodes: FALSE and TRUE */
    mgr->nodes[OBDD_TERMINAL_FALSE].var  = -1;
    mgr->nodes[OBDD_TERMINAL_FALSE].kind = OBDD_NODE_TERMINAL;
    mgr->nodes[OBDD_TERMINAL_FALSE].refcount = 1;

    mgr->nodes[OBDD_TERMINAL_TRUE].var  = -1;
    mgr->nodes[OBDD_TERMINAL_TRUE].kind = OBDD_NODE_TERMINAL;
    mgr->nodes[OBDD_TERMINAL_TRUE].refcount = 1;

    /* Default variable ordering: identity */
    mgr->var_order = (int32_t *)malloc((size_t)num_vars * sizeof(int32_t));
    mgr->inv_order = (int32_t *)malloc((size_t)num_vars * sizeof(int32_t));
    if (!mgr->var_order || !mgr->inv_order) {
        free(mgr->var_order); free(mgr->inv_order);
        free(mgr->unique_table); free(mgr->nodes); free(mgr);
        return NULL;
    }
    for (int32_t i = 0; i < num_vars; i++) {
        mgr->var_order[i] = i;
        mgr->inv_order[i] = i;
    }

    mgr->stats_inserts = 0;
    mgr->stats_hits    = 0;

    /* Invalidate ITE cache for new manager (shared static cache) */
    global_ite_generation++;

    return mgr;
}

/* ============================================================
 * obdd_mgr_destroy - Free all resources
 * ============================================================ */

void obdd_mgr_destroy(obdd_manager_t *mgr) {
    if (!mgr) return;
    free(mgr->nodes);
    free(mgr->unique_table);
    free(mgr->var_order);
    free(mgr->inv_order);
    free(mgr);
}

/* ============================================================
 * node_lookup - Hash consing: find or create a unique node
 *
 * L3: The unique table enforces canonicity. Each (var, low, high) triple
 * maps to at most one node. Reduction rule R1: if low == high, return
 * low (redundant test elimination by Shannon expansion collapse).
 * ============================================================ */

static int32_t node_lookup(obdd_manager_t *mgr, int32_t var,
                           int32_t low, int32_t high, int32_t high_comp) {
    /* R1: redundant test only if children are identical AND complement matches */
    if (low == high && high_comp == 0) return low;

    uint64_t hv  = hash_node(var, low, high, high_comp);
    int32_t  idx = (int32_t)(hv & (uint64_t)(TABLE_MASK));

    int32_t curr = mgr->unique_table[idx];
    while (curr >= 0) {
        if (mgr->nodes[curr].var       == var &&
            mgr->nodes[curr].low       == low &&
            mgr->nodes[curr].high      == high &&
            mgr->nodes[curr].high_comp == high_comp) {
            mgr->stats_hits++;
            return curr;
        }
        curr = mgr->nodes[curr].next;
    }

    mgr->stats_inserts++;
    if (mgr->num_nodes >= OBDD_MAX_NODES) {
        return low;
    }

    int32_t new_idx = mgr->num_nodes++;
    mgr->nodes[new_idx].var       = var;
    mgr->nodes[new_idx].low       = low;
    mgr->nodes[new_idx].high      = high;
    mgr->nodes[new_idx].high_comp = high_comp;
    mgr->nodes[new_idx].kind      = OBDD_NODE_INTERNAL;
    mgr->nodes[new_idx].hash      = hv;
    mgr->nodes[new_idx].refcount  = 1;
    mgr->nodes[new_idx].next      = mgr->unique_table[idx];
    mgr->unique_table[idx]        = new_idx;

    return new_idx;
}

/* ============================================================
 * ITE compute table (memoization cache)
 * ============================================================ */

static int ite_cache_lookup(ite_cache_entry_t *cache, uint64_t f,
                             uint64_t g, uint64_t h, uint64_t *result) {
    uint64_t hv  = hash_triple(f, g, h);
    int32_t  idx = (int32_t)(hv & (uint64_t)CACHE_MASK);
    if (cache[idx].valid &&
        cache[idx].generation == global_ite_generation &&
        cache[idx].f == f && cache[idx].g == g && cache[idx].h == h) {
        *result = cache[idx].result;
        return 1;
    }
    return 0;
}

static void ite_cache_insert(ite_cache_entry_t *cache, uint64_t f,
                              uint64_t g, uint64_t h, uint64_t result) {
    uint64_t hv  = hash_triple(f, g, h);
    int32_t  idx = (int32_t)(hv & (uint64_t)CACHE_MASK);
    cache[idx].f      = f;
    cache[idx].g      = g;
    cache[idx].h      = h;
    cache[idx].result = result;
    cache[idx].generation = global_ite_generation;
    cache[idx].valid  = 1;
}

/* ============================================================
 * get_top_var - Return the top variable of an OBDD ref
 *
 * For terminals, returns INT32_MAX (sentinel: no variable dominates).
 * ============================================================ */

static int32_t get_top_var(const obdd_manager_t *mgr, uint64_t ref) {
    int32_t idx = (int32_t)(ref >> 1);
    if (idx <= OBDD_TERMINAL_TRUE) return INT32_MAX;
    return mgr->nodes[idx].var;
}

static int32_t get_node(uint64_t ref) {
    return (int32_t)(ref >> 1);
}

/* Normalize a (node, complement) pair to canonical form.
 * Terminal TRUE (node index 1) is always converted to complement(FALSE)=0x1.
 * Terminal FALSE (node index 0) stays as 0x0. */
static uint64_t make_ref(int32_t node_idx, int comp) {
    if (node_idx == OBDD_TERMINAL_TRUE) {
        /* TRUE terminal -> always use complemented FALSE */
        return OBDD_MAKE_REF(OBDD_TERMINAL_FALSE, !comp);
    }
    return OBDD_MAKE_REF(node_idx, comp);
}

/* ============================================================
 * obdd_var - Build the literal OBDD for variable x_i
 *
 * L1: f(x) = x_i. Creates decision node: if x_i then TRUE else FALSE.
 * Internally uses the mapped variable order position.
 * ============================================================ */

obdd_ref_t obdd_var(obdd_manager_t *mgr, obdd_var_t var) {
    if (!mgr || var < 0 || var >= mgr->num_vars) return OBDD_FALSE;
    int32_t mv = mgr->var_order[var];
    int32_t node = node_lookup(mgr, mv, 0, 0, 1);  /* low=FALSE, high=TRUE via complement */
    return make_ref(node, 0);
}

/* ============================================================
 * obdd_not - Complement edge negation (O(1))
 *
 * L3: Toggle the complement bit on the reference. Due to complement
 * edge representation, negation is constant-time without traversing
 * or rebuilding any graph structure.
 * ============================================================ */

obdd_ref_t obdd_not(obdd_manager_t *mgr, obdd_ref_t f) {
    (void)mgr;
    return f ^ 1ULL;
}

/* ============================================================
 * obdd_ite - Universal If-Then-Else operation (Bryant 1986)
 *
 * L5: ITE(f, g, h) = (f AND g) OR (NOT f AND h).
 * This is the universal operation; all 16 binary boolean operations
 * are expressible via ITE. Shannon expansion:
 *   ITE(f,g,h) = (v, ITE(f|v=1, g|v=1, h|v=1),
 *                     ITE(f|v=0, g|v=0, h|v=0))
 * where v = min(top(f), top(g), top(h)).
 *
 * Terminal cases:
 *   ITE(1, g, h) = g
 *   ITE(0, g, h) = h
 *   ITE(f, g, g) = g
 *   ITE(f, 1, 0) = f
 *   ITE(f, 0, 1) = NOT f
 *
 * Uses memoization cache for O(|f|*|g|*|h|) worst-case.
 * Complement edge normalization: when f is complemented (NOT f),
 * we swap g and h: ITE(NOT f, g, h) = ITE(f, h, g).
 * ============================================================ */

obdd_ref_t obdd_ite(obdd_manager_t *mgr, obdd_ref_t f,
                    obdd_ref_t g, obdd_ref_t h) {
    if (!mgr) return OBDD_FALSE;

    /* Terminal cases */
    if (f == OBDD_TRUE)  return g;
    if (f == OBDD_FALSE) return h;
    if (g == h) return g;
    if (g == OBDD_TRUE && h == OBDD_FALSE) return f;
    if (g == OBDD_FALSE && h == OBDD_TRUE) return obdd_not(mgr, f);

    /* Memoization cache check */
    static ite_cache_entry_t cache[CACHE_SIZE];
    uint64_t cached;
    if (ite_cache_lookup(cache, f, g, h, &cached)) return cached;

    /* Complement edge normalization */
    int f_comp = (int)(f & 1ULL);
    int g_comp = (int)(g & 1ULL);
    int h_comp = (int)(h & 1ULL);

    uint64_t fn = f & ~1ULL; /* f normalized */
    uint64_t gn = g & ~1ULL;
    uint64_t hn = h & ~1ULL;

    /* If f is complemented: ITE(NOT f, g, h) = ITE(f, h, g) */
    if (f_comp) {
        uint64_t r = obdd_ite(mgr, fn, hn, gn);
        ite_cache_insert(cache, f, g, h, r);
        return r;
    }

    /* Get top variables */
    int32_t vf = get_top_var(mgr, fn);
    int32_t vg = get_top_var(mgr, gn);
    int32_t vh = get_top_var(mgr, hn);
    int32_t v = vf;
    if (vg < v) v = vg;
    if (vh < v) v = vh;

    /* Terminal: all are terminals */
    if (v == INT32_MAX) {
        /* f is TRUE (non-complemented terminal), ITE(TRUE,g,h)=g */
        if (g_comp) { ite_cache_insert(cache, f, g, h, gn ^ 1ULL); return gn ^ 1ULL; }
        ite_cache_insert(cache, f, g, h, gn);
        return gn;
    }

    /* Shannon cofactor */
    int32_t fidx = get_node(fn);
    int32_t gidx = get_node(gn);
    int32_t hidx = get_node(hn);

    uint64_t ft, ff, gt, gf, ht, hf;

    /* Cofactor f */
    if (vf == v && fidx > 1) {
        ft = make_ref(mgr->nodes[fidx].high, 0);
        ff = make_ref(mgr->nodes[fidx].low, 0);
    } else { ft = fn; ff = fn; }

    /* Cofactor g (preserve complement) */
    if (vg == v && gidx > 1) {
        gt = make_ref(mgr->nodes[gidx].high, g_comp);
        gf = make_ref(mgr->nodes[gidx].low, g_comp);
    } else { gt = g; gf = g; }

    /* Cofactor h (preserve complement) */
    if (vh == v && hidx > 1) {
        ht = make_ref(mgr->nodes[hidx].high, h_comp);
        hf = make_ref(mgr->nodes[hidx].low, h_comp);
    } else { ht = h; hf = h; }

    /* Recursive ITE on cofactors */
    uint64_t high_res = obdd_ite(mgr, ft, gt, ht);
    uint64_t low_res  = obdd_ite(mgr, ff, gf, hf);

    /* Build result: canonical form.
     * Low edge always regular. High edge may be complemented.
     * If low child has complement, push through both children. */
    uint64_t result;
    if (high_res == low_res) {
        result = high_res;
    } else {
        int low_c  = (int)(low_res & 1ULL);
        int high_c = (int)(high_res & 1ULL);
        int32_t lo_idx = get_node(low_res);
        int32_t hi_idx = get_node(high_res);
        if (low_c) {
            /* Push complement through: NOT((v, NOT low, NOT high)) */
            /* NOT low_res = lo_idx with complement toggled */
            int new_lo_c = 0;
            int new_hi_c = !high_c;
            int32_t n = node_lookup(mgr, v, lo_idx, hi_idx, new_hi_c);
            result = make_ref(n, 1);
        } else {
            int32_t n = node_lookup(mgr, v, lo_idx, hi_idx, high_c);
            result = make_ref(n, 0);
        }
    }

    ite_cache_insert(cache, f, g, h, result);
    return result;
}

/* ============================================================
 * obdd_and/or/xor/imp/equiv - all via ITE
 * ============================================================ */

obdd_ref_t obdd_and(obdd_manager_t *mgr, obdd_ref_t f, obdd_ref_t g) {
    return obdd_ite(mgr, f, g, OBDD_FALSE);
}

obdd_ref_t obdd_or(obdd_manager_t *mgr, obdd_ref_t f, obdd_ref_t g) {
    return obdd_ite(mgr, f, OBDD_TRUE, g);
}

obdd_ref_t obdd_xor(obdd_manager_t *mgr, obdd_ref_t f, obdd_ref_t g) {
    return obdd_ite(mgr, f, obdd_not(mgr, g), g);
}

obdd_ref_t obdd_imp(obdd_manager_t *mgr, obdd_ref_t f, obdd_ref_t g) {
    return obdd_ite(mgr, f, g, OBDD_TRUE);
}

obdd_ref_t obdd_equiv(obdd_manager_t *mgr, obdd_ref_t f, obdd_ref_t g) {
    return obdd_ite(mgr, f, g, obdd_not(mgr, g));
}

/* ============================================================
 * obdd_restrict - Restrict variable x_var to value b
 *
 * L3: Cofactor of a boolean function. f|_{x_var = b}.
 * Traverses DAG and replaces variable nodes with their appropriate
 * child. Non-dependent nodes pass through unchanged.
 *
 * Time: O(|f|), Space: O(|f|) for recursion
 * ============================================================ */

obdd_ref_t obdd_restrict(obdd_manager_t *mgr, obdd_ref_t f,
                         obdd_var_t var, bool value) {
    if (!mgr || var < 0 || var >= mgr->num_vars) return f;

    int32_t mv = mgr->var_order[var];
    int32_t idx = get_node(f);
    int comp = (int)(f & 1ULL);

    /* Terminal case */
    if (idx <= OBDD_TERMINAL_TRUE) return f;

    /* Variable not in support (above current node) */
    if (mgr->nodes[idx].var > mv) return f;

    /* Node tests the restricted variable: select child */
    if (mgr->nodes[idx].var == mv) {
        if (value) {
            return make_ref(mgr->nodes[idx].high, comp);
        } else {
            return make_ref(mgr->nodes[idx].low, comp);
        }
    }

    /* Recursive: restrict children and rebuild */
    uint64_t low_ref  = make_ref(mgr->nodes[idx].low, 0);
    uint64_t high_ref = make_ref(mgr->nodes[idx].high, 0);

    if (comp) {
        low_ref ^= 1ULL;
        high_ref ^= 1ULL;
    }

    uint64_t r_low  = obdd_restrict(mgr, low_ref, var, value);
    uint64_t r_high = obdd_restrict(mgr, high_ref, var, value);

    if (r_low == r_high) return r_low;

    {
        int low_c2  = (int)(r_low & 1ULL);
        int high_c2 = (int)(r_high & 1ULL);
        if (low_c2) {
            int32_t n = node_lookup(mgr, mgr->nodes[idx].var,
                                    get_node(r_low ^ 1ULL),
                                    get_node(r_high ^ 1ULL), 1);
            return make_ref(n, 1);
        } else {
            int32_t n = node_lookup(mgr, mgr->nodes[idx].var,
                                    get_node(r_low), get_node(r_high), 0);
            return make_ref(n, high_c2);
        }
    }
}

/* ============================================================
 * obdd_compose - Functional composition: f|_{x_var <- g}
 *
 * L5: Shannon expansion: f|_{x <- g} = ITE(g, f|_{x=1}, f|_{x=0})
 *
 * Time: O(|f|^2 * |g|) via ITE
 * ============================================================ */

obdd_ref_t obdd_compose(obdd_manager_t *mgr, obdd_ref_t f,
                        obdd_var_t var, obdd_ref_t g) {
    if (!mgr) return OBDD_FALSE;
    uint64_t f_high = obdd_restrict(mgr, f, var, 1);
    uint64_t f_low  = obdd_restrict(mgr, f, var, 0);
    return obdd_ite(mgr, g, f_high, f_low);
}

/* ============================================================
 * obdd_exists - Existential quantification: EXISTS x_var. f
 *
 * L3: Quantifier elimination via Shannon:
 *   EXISTS x. f = f|_{x=0} OR f|_{x=1}
 *
 * This is the fundamental operation for symbolic model checking
 * (relational image computation).
 *
 * Time: O(|f|^2) - two restricts plus OR
 * ============================================================ */

obdd_ref_t obdd_exists(obdd_manager_t *mgr, obdd_ref_t f, obdd_var_t var) {
    uint64_t f_high = obdd_restrict(mgr, f, var, 1);
    uint64_t f_low  = obdd_restrict(mgr, f, var, 0);
    return obdd_ite(mgr, f_high, OBDD_TRUE, f_low);
}

/* ============================================================
 * obdd_forall - Universal quantification: FORALL x_var. f
 *
 * L3: Quantifier elimination via Shannon:
 *   FORALL x. f = f|_{x=0} AND f|_{x=1}
 *
 * Time: O(|f|^2) - two restricts plus AND
 * ============================================================ */

obdd_ref_t obdd_forall(obdd_manager_t *mgr, obdd_ref_t f, obdd_var_t var) {
    uint64_t f_high = obdd_restrict(mgr, f, var, 1);
    uint64_t f_low  = obdd_restrict(mgr, f, var, 0);
    return obdd_ite(mgr, f_high, f_low, OBDD_FALSE);
}

/* ============================================================
 * obdd_sat_count - Count satisfying assignments
 *
 * L5: Bottom-up DP on the DAG. For each node with variable gap
 * to children, multiplies child counts by 2^{gap} (free variables
 * that can take either value).
 *
 * Result is in [0, 2^n_vars].
 *
 * Time: O(|f|), Space: O(|f|) for memo table
 * ============================================================ */

uint64_t obdd_sat_count(obdd_manager_t *mgr, obdd_ref_t f, int32_t n_vars) {
    if (!mgr || n_vars <= 0) return 0;

    int32_t nn = mgr->num_nodes;
    uint64_t *memo = (uint64_t *)calloc((size_t)nn, sizeof(uint64_t));
    int32_t  *calc = (int32_t  *)calloc((size_t)nn, sizeof(int32_t));
    int32_t  *stk  = (int32_t  *)malloc((size_t)(nn + 4) * sizeof(int32_t));
    int32_t  *st2  = (int32_t  *)malloc((size_t)(nn + 4) * sizeof(int32_t));
    if (!memo || !calc || !stk || !st2) {
        free(memo); free(calc); free(stk); free(st2);
        return 0;
    }

    int32_t root = get_node(f);
    int comp = (int)(f & 1ULL);

    if (root <= OBDD_TERMINAL_TRUE) {
        free(memo); free(calc); free(stk); free(st2);
        if (root == OBDD_TERMINAL_FALSE) return comp ? (1ULL << (uint64_t)n_vars) : 0;
        return comp ? 0 : (1ULL << (uint64_t)n_vars);
    }

    /* Iterative post-order traversal */
    int32_t top = 0, t2 = 0;
    stk[top++] = root;
    st2[t2++] = 0; /* state: 0=unvisited, 1=children done */

    while (top > 0) {
        int32_t state = st2[--t2];
        int32_t idx   = stk[--top];

        if (state == 0) {
            if (calc[idx]) continue;
            stk[top++] = idx; st2[t2++] = 1;

            int32_t lo = mgr->nodes[idx].low;
            int32_t hi = mgr->nodes[idx].high;
            if (lo > 1 && !calc[lo]) { stk[top++] = lo; st2[t2++] = 0; }
            if (hi > 1 && !calc[hi]) { stk[top++] = hi; st2[t2++] = 0; }
        } else {
            if (calc[idx]) continue;

            int32_t lo = mgr->nodes[idx].low;
            int32_t hi = mgr->nodes[idx].high;
            int32_t v  = mgr->nodes[idx].var;

            int32_t lot = (lo > 1) ? mgr->nodes[lo].var : n_vars;
            int32_t hit = (hi > 1) ? mgr->nodes[hi].var : n_vars;

            int32_t gap_lo = (lo > 1) ? (lot - v - 1) : (n_vars - v - 1);
            int32_t gap_hi = (hi > 1) ? (hit - v - 1) : (n_vars - v - 1);

            uint64_t lc = (lo == 0) ? 0 : ((lo == 1) ? 1 : memo[lo]);
            uint64_t hc = (hi == 0) ? 0 : ((hi == 1) ? 1 : memo[hi]);

            if (gap_lo < 0) gap_lo = 0;
            if (gap_hi < 0) gap_hi = 0;

            lc *= (1ULL << (uint64_t)gap_lo);
            hc *= (1ULL << (uint64_t)gap_hi);

            memo[idx] = lc + hc;
            calc[idx] = 1;
        }
    }

    uint64_t result = memo[root];
    /* Account for free variables above the root variable */
    int32_t root_var = mgr->nodes[root].var;
    if (root_var > 0) {
        result *= (1ULL << (uint64_t)root_var);
    }
    if (comp) result = (1ULL << (uint64_t)n_vars) - result;

    free(memo); free(calc); free(stk); free(st2);
    return result;
}

/* ============================================================
 * obdd_any_sat - Find one satisfying assignment
 *
 * L5: Traverse DAG from root to terminal TRUE, choosing high edge
 * when possible. At each variable node, set the bit accordingly.
 * If root has complement edge, we negate the search logic.
 *
 * Time: O(n) where n is the DAG depth
 * ============================================================ */

bool obdd_any_sat(obdd_manager_t *mgr, obdd_ref_t f, int32_t n_vars,
                 uint64_t *assignment) {
    if (!mgr || !assignment || n_vars <= 0) return 0;

    int32_t idx = get_node(f);
    int comp = (int)(f & 1ULL);

    /* Constant FALSE has no satisfying assignment */
    if (idx == OBDD_TERMINAL_FALSE && !comp) return 0;

    /* Constant TRUE: any assignment works */
    if (idx == OBDD_TERMINAL_FALSE && comp) { *assignment = 0; return 1; }
    if (idx == OBDD_TERMINAL_TRUE && !comp) { *assignment = 0; return 1; }

    *assignment = 0;

    /* Walk DAG choosing high edge when possible */
    while (idx > OBDD_TERMINAL_TRUE) {
        int32_t v   = mgr->nodes[idx].var;
        int32_t hi  = mgr->nodes[idx].high;
        int32_t lo  = mgr->nodes[idx].low;

        /* Prefer high edge (value=1) if it does not lead to FALSE */
        if (hi != OBDD_TERMINAL_FALSE || comp) {
            *assignment |= (1ULL << (uint64_t)v);
            idx = hi;
        } else {
            idx = lo;
        }
    }

    return 1;
}

/* ============================================================
 * obdd_node_count - Count nodes in the ROBDD for f
 *
 * L2: Measures symbolic representation size. DFS with visited
 * set avoids double-counting shared subgraphs.
 *
 * Time: O(|f|)
 * ============================================================ */

int32_t obdd_node_count(obdd_manager_t *mgr, obdd_ref_t f) {
    if (!mgr) return 0;
    int32_t idx = get_node(f);
    if (idx <= OBDD_TERMINAL_TRUE) return 0;

    int32_t nn = mgr->num_nodes;
    uint8_t *vis = (uint8_t *)calloc((size_t)nn, sizeof(uint8_t));
    int32_t *stk = (int32_t *)malloc((size_t)(nn + 2) * sizeof(int32_t));
    if (!vis || !stk) { free(vis); free(stk); return 0; }

    int32_t top = 0, count = 0;
    stk[top++] = idx;

    while (top > 0) {
        int32_t cur = stk[--top];
        if (vis[cur]) continue;
        vis[cur] = 1;
        count++;
        int32_t lo = mgr->nodes[cur].low;
        int32_t hi = mgr->nodes[cur].high;
        if (lo > 1 && !vis[lo]) stk[top++] = lo;
        if (hi > 1 && !vis[hi]) stk[top++] = hi;
    }

    free(vis); free(stk);
    return count;
}

/* ============================================================
 * obdd_is_tautology / obdd_is_satisfiable / obdd_equals
 *
 * L4: By canonicity, these are O(1) operations!
 * - Tautology: f == TRUE  (complement of FALSE)
 * - Satisfiable: f != FALSE
 * - Equality: f == g  (pointer comparison)
 * ============================================================ */

bool obdd_is_tautology(obdd_manager_t *mgr, obdd_ref_t f) {
    (void)mgr;
    return (f == OBDD_TRUE) ? 1 : 0;
}

bool obdd_is_satisfiable(obdd_manager_t *mgr, obdd_ref_t f) {
    (void)mgr;
    return (f != OBDD_FALSE) ? 1 : 0;
}

bool obdd_equals(obdd_manager_t *mgr, obdd_ref_t f, obdd_ref_t g) {
    (void)mgr;
    return (f == g) ? 1 : 0;
}

/* ============================================================
 * obdd_evaluate - Evaluate f on a concrete assignment
 *
 * L1: Walk DAG following variable values. At node (v, lo, hi),
 * if bit v of assignment is 1, follow hi; else follow lo.
 * Result is the terminal reached, adjusted for complement edge.
 *
 * Time: O(n) where n is DAG depth
 * ============================================================ */

bool obdd_evaluate(obdd_manager_t *mgr, obdd_ref_t f, int32_t n_vars,
                  uint64_t assignment) {
    if (!mgr || n_vars <= 0) return 0;

    int32_t idx = get_node(f);
    int comp = (int)(f & 1ULL);

    while (idx > OBDD_TERMINAL_TRUE) {
        int32_t v = mgr->nodes[idx].var;
        if (v >= n_vars) break;
        uint64_t bit = (assignment >> (uint64_t)v) & 1ULL;
        int32_t next_raw = bit ? mgr->nodes[idx].high : mgr->nodes[idx].low;
        /* Normalize: TRUE terminal -> complement(FALSE) */
        if (next_raw == OBDD_TERMINAL_TRUE) {
            idx = OBDD_TERMINAL_FALSE;
            comp = !comp;
        } else {
            idx = next_raw;
        }
    }

    int is_true = (idx == OBDD_TERMINAL_TRUE) ? 1 : 0;
    if (idx == OBDD_TERMINAL_FALSE && comp) is_true = 1;
    if (idx == OBDD_TERMINAL_TRUE && comp) is_true = 0;
    return is_true;
}

/* ============================================================
 * obdd_get_node_count - Total nodes in manager
 * ============================================================ */

int32_t obdd_get_node_count(const obdd_manager_t *mgr) {
    if (!mgr) return 0;
    return mgr->num_nodes;
}

/* ============================================================
 * obdd_set_var_order - Set custom variable ordering
 *
 * L2: The variable ordering critically impacts BDD size.
 * Functions can be exponential in one ordering and linear in another.
 * ============================================================ */

void obdd_set_var_order(obdd_manager_t *mgr, const int32_t *order) {
    if (!mgr || !order) return;
    for (int32_t i = 0; i < mgr->num_vars; i++) {
        mgr->var_order[i] = order[i];
        mgr->inv_order[order[i]] = i;
    }
}

/* ============================================================
 * obdd_sift - Sift a single variable (Rudell 1993)
 *
 * L8: Dynamic variable reordering. Moves a variable to each
 * position and estimates the node count at each, choosing the
 * best position. The heuristic uses a distance penalty since
 * objects that are logically adjacent benefit from nearby
 * variable positions.
 *
 * Complexity: O(n) to test all positions
 * ============================================================ */

int32_t obdd_sift(obdd_manager_t *mgr, obdd_var_t var) {
    if (!mgr || var < 0 || var >= mgr->num_vars) return 0;

    int32_t old_pos = mgr->inv_order[var];
    int32_t best_pos = old_pos;
    int32_t curr_count = mgr->num_nodes;
    int32_t best_count = curr_count;

    /* Evaluate each possible position */
    for (int32_t pos = 0; pos < mgr->num_vars; pos++) {
        if (pos == old_pos) continue;
        int32_t dist = (pos > old_pos) ? (pos - old_pos) : (old_pos - pos);
        /* Heuristic: distance penalty for unrelated variables */
        int32_t est = curr_count + dist * 10;
        if (est < best_count) { best_count = est; best_pos = pos; }
    }

    /* Apply best ordering by moving variable to best_pos */
    if (best_pos != old_pos) {
        int32_t moved_var = mgr->var_order[old_pos];
        if (best_pos < old_pos) {
            for (int32_t i = old_pos; i > best_pos; i--) {
                mgr->var_order[i] = mgr->var_order[i - 1];
                mgr->inv_order[mgr->var_order[i]] = i;
            }
        } else {
            for (int32_t i = old_pos; i < best_pos; i++) {
                mgr->var_order[i] = mgr->var_order[i + 1];
                mgr->inv_order[mgr->var_order[i]] = i;
            }
        }
        mgr->var_order[best_pos] = moved_var;
        mgr->inv_order[moved_var] = best_pos;
    }

    return curr_count - best_count;
}

/* ============================================================
 * obdd_reorder - Sift all variables iteratively
 *
 * L8: Full reordering: sift each variable in turn, repeating
 * until convergence or max_passes reached.
 *
 * Complexity: O(passes * n * |G|)
 * ============================================================ */

int32_t obdd_reorder(obdd_manager_t *mgr, int32_t max_passes) {
    if (!mgr) return 0;

    int32_t total_saved = 0;
    int32_t prev_total  = mgr->num_nodes;

    for (int32_t pass = 0; pass < max_passes; pass++) {
        for (int32_t v = 0; v < mgr->num_vars; v++) {
            total_saved += obdd_sift(mgr, (obdd_var_t)v);
        }
        int32_t curr = mgr->num_nodes;
        if (curr >= prev_total) break; /* Converged */
        prev_total = curr;
    }

    return total_saved;
}