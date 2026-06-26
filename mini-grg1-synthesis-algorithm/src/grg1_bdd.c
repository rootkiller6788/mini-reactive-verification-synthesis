/**
 * grg1_bdd.c — Binary Decision Diagram operations for symbolic synthesis
 *
 * Implements a basic ROBDD (Reduced Ordered Binary Decision Diagram)
 * library for symbolic representation of state sets and transition
 * relations in GR(1) synthesis. BDDs enable handling of much larger
 * state spaces than explicit-state enumeration.
 *
 * Key operations:
 *   - ite (if-then-else): the universal BDD combinator
 *   - restrict (cofactor): variable substitution
 *   - exists/forall: quantification for CPre computation
 *   - apply: generic binary boolean operation
 *
 * The unique table ensures canonicity: each boolean function has a
 * unique BDD representation given a fixed variable ordering.
 *
 * References:
 *   Bryant, R.E. "Graph-Based Algorithms for Boolean Function
 *     Manipulation." IEEE Transactions on Computers, C-35(8), 1986.
 *   Burch, J.R. et al. "Symbolic Model Checking: 10²⁰ States and
 *     Beyond." Information and Computation, 98(2), 1992.
 *
 * Knowledge coverage:
 *   L3: Boolean function representation, canonical forms
 *   L5: BDD operations (ite, restrict, quantification)
 *   L8: Symbolic model checking, BDD-based state space reduction
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "grg1_types.h"

/* =========================================================================
 * BDD unique table hash function
 * ========================================================================*/

/**
 * Hash a BDD node triple (var, low, high) for insertion into the unique table.
 *
 * Uses a multiplicative hash combining the variable index and the
 * IDs of the low/high children. The hash table is sized to be a prime
 * for better dispersion.
 *
 * Complexity: O(1)
 * Reference: Knuth §6.4 — Multiplicative hashing
 */
static int grg1_bdd_hash(grg1_bdd_var_t var, grg1_bdd_node_t* low,
                          grg1_bdd_node_t* high, int table_size) {
    uint64_t h = (uint64_t)var;
    h = h * 2654435761ULL; /* Knuth's multiplicative hash constant */
    h += (uint64_t)(uintptr_t)low * 31;
    h += (uint64_t)(uintptr_t)high * 131;
    return (int)(h % (uint64_t)table_size);
}

/**
 * Look up or create a BDD node with the given parameters.
 *
 * If a node with (var, low, high) already exists in the unique table,
 * return it. Otherwise, create a new node. This ensures canonicity:
 * equal boolean functions have equal BDD representations.
 *
 * Complexity: O(1) average (hash table lookup)
 * Reference: Bryant (1986) §3 — Unique table and canonicity
 */
static grg1_bdd_node_t* grg1_bdd_unique(grg1_bdd_manager_t* mgr,
                                          grg1_bdd_var_t var,
                                          grg1_bdd_node_t* low,
                                          grg1_bdd_node_t* high) {
    /* Apply reduction rule: if low == high, skip the variable */
    if (low == high) return low;

    int h = grg1_bdd_hash(var, low, high, mgr->table_size);

    /* Search unique table for existing node */
    grg1_bdd_node_t* node = mgr->unique_table[h];
    while (node) {
        if (node->variable == var && node->low == low && node->high == high) {
            node->ref_count++;
            return node;
        }
        node = node->next;
    }

    /* Create new node */
    node = (grg1_bdd_node_t*)malloc(sizeof(grg1_bdd_node_t));
    if (!node) return mgr->false_node; /* Fallback on OOM */

    node->id = mgr->next_node_id++;
    node->type = GRG1_BDD_NONTERMINAL;
    node->variable = var;
    node->low = low;
    node->high = high;
    node->ref_count = 1;
    node->next = mgr->unique_table[h];
    mgr->unique_table[h] = node;

    /* Increment child reference counts */
    if (low) low->ref_count++;
    if (high) high->ref_count++;

    return node;
}

/* =========================================================================
 * BDD variable creation
 * ========================================================================*/

/**
 * Create a BDD variable (the leaf decision node for a boolean variable).
 *
 * BDD(v) = ite(v, TRUE, FALSE) — a single decision node.
 *
 * Complexity: O(1)
 */
grg1_bdd_node_t* grg1_bdd_create_variable(grg1_bdd_manager_t* mgr,
                                            grg1_bdd_var_t var) {
    if (!mgr || var < 0 || var >= mgr->num_vars) return NULL;
    return grg1_bdd_unique(mgr, var, mgr->false_node, mgr->true_node);
}

/* =========================================================================
 * BDD binary apply operation
 * ========================================================================*/

/**
 * Compute table entry for memoizing apply results.
 *
 * The compute table caches pairs of (op, u, v) → result to avoid
 * recomputing BDD operations. This is essential for performance:
 * without caching, BDD operations can be exponential.
 *
 * Reference: Bryant (1986) §5 — Compute table for efficiency
 */
typedef struct {
    int op;                     /**< Operation code */
    grg1_bdd_node_t* u;
    grg1_bdd_node_t* v;
    grg1_bdd_node_t* result;
} grg1_bdd_cache_entry_t;

#define GRG1_BDD_CACHE_SIZE 10007

static grg1_bdd_cache_entry_t bdd_cache[GRG1_BDD_CACHE_SIZE];
static bool bdd_cache_initialized = false;

static void grg1_bdd_cache_init(void) {
    if (bdd_cache_initialized) return;
    memset(bdd_cache, 0, sizeof(bdd_cache));
    bdd_cache_initialized = true;
}

static int grg1_bdd_cache_hash(int op, grg1_bdd_node_t* u, grg1_bdd_node_t* v) {
    uint64_t h = (uint64_t)op;
    h = h * 2654435761ULL;
    h += (uint64_t)(uintptr_t)u * 31;
    h += (uint64_t)(uintptr_t)v * 131;
    return (int)(h % GRG1_BDD_CACHE_SIZE);
}

/**
 * Generic binary boolean operation on BDDs.
 *
 * op codes:
 *   0 = AND (u ∧ v)
 *   1 = OR  (u ∨ v)
 *   2 = XOR (u ⊕ v)
 *   3 = IMP (u → v)
 *
 * The algorithm recursively decomposes both BDDs by their top variables,
 * applying the boolean operator to the cofactors, and reconstructing
 * the result via the unique table.
 *
 * Complexity: O(|u| × |v|) worst case, O(|u| + |v|) with caching
 * Reference: Bryant (1986) §4 — Apply algorithm
 */
static grg1_bdd_node_t* grg1_bdd_apply_op(grg1_bdd_manager_t* mgr,
                                            int op,
                                            grg1_bdd_node_t* u,
                                            grg1_bdd_node_t* v) {
    if (!u || !v) return mgr->false_node;

    /* Terminal cases */
    if (u->type != GRG1_BDD_NONTERMINAL && v->type != GRG1_BDD_NONTERMINAL) {
        bool a = (u->type == GRG1_BDD_TERMINAL_TRUE);
        bool b = (v->type == GRG1_BDD_TERMINAL_TRUE);
        bool r;
        switch (op) {
            case 0: r = a && b; break; /* AND */
            case 1: r = a || b; break; /* OR */
            case 2: r = a ^ b;  break; /* XOR */
            case 3: r = (!a) || b; break; /* IMP (¬a ∨ b) */
            default: r = false;
        }
        return r ? mgr->true_node : mgr->false_node;
    }

    /* Check cache */
    grg1_bdd_cache_init();
    int ch = grg1_bdd_cache_hash(op, u, v);
    if (bdd_cache[ch].op == op &&
        bdd_cache[ch].u == u &&
        bdd_cache[ch].v == v) {
        return bdd_cache[ch].result;
    }

    /* Determine top variable */
    grg1_bdd_var_t top_var;
    grg1_bdd_node_t *u_low, *u_high, *v_low, *v_high;

    if (u->type == GRG1_BDD_NONTERMINAL &&
        (v->type == GRG1_BDD_TERMINAL_FALSE || v->type == GRG1_BDD_TERMINAL_TRUE ||
         u->variable <= v->variable)) {
        top_var = u->variable;
        u_low = u->low;
        u_high = u->high;
        v_low = v;
        v_high = v;
    } else if (v->type == GRG1_BDD_NONTERMINAL) {
        top_var = v->variable;
        u_low = u;
        u_high = u;
        v_low = v->low;
        v_high = v->high;
    } else {
        /* Both terminals — already handled above */
        return mgr->false_node;
    }

    /* Recurse on cofactors */
    grg1_bdd_node_t* low = grg1_bdd_apply_op(mgr, op, u_low, v_low);
    grg1_bdd_node_t* high = grg1_bdd_apply_op(mgr, op, u_high, v_high);

    /* Build result node */
    grg1_bdd_node_t* result = grg1_bdd_unique(mgr, top_var, low, high);

    /* Update cache */
    bdd_cache[ch].op = op;
    bdd_cache[ch].u = u;
    bdd_cache[ch].v = v;
    bdd_cache[ch].result = result;

    return result;
}

grg1_bdd_node_t* grg1_bdd_and(grg1_bdd_manager_t* mgr,
                                grg1_bdd_node_t* u,
                                grg1_bdd_node_t* v) {
    return grg1_bdd_apply_op(mgr, 0, u, v);
}

grg1_bdd_node_t* grg1_bdd_or(grg1_bdd_manager_t* mgr,
                               grg1_bdd_node_t* u,
                               grg1_bdd_node_t* v) {
    return grg1_bdd_apply_op(mgr, 1, u, v);
}

grg1_bdd_node_t* grg1_bdd_xor(grg1_bdd_manager_t* mgr,
                                grg1_bdd_node_t* u,
                                grg1_bdd_node_t* v) {
    return grg1_bdd_apply_op(mgr, 2, u, v);
}

grg1_bdd_node_t* grg1_bdd_imp(grg1_bdd_manager_t* mgr,
                                grg1_bdd_node_t* u,
                                grg1_bdd_node_t* v) {
    return grg1_bdd_apply_op(mgr, 3, u, v);
}

/**
 * BDD negation (NOT).
 *
 * Efficient: just swap the children. ¬ite(v, l, h) = ite(v, ¬l, ¬h).
 * For terminals: ¬TRUE = FALSE, ¬FALSE = TRUE.
 *
 * Complexity: O(1) — just switch terminal references
 * Reference: Bryant (1986) — complement edges for O(1) negation
 */
grg1_bdd_node_t* grg1_bdd_not(grg1_bdd_manager_t* mgr,
                                grg1_bdd_node_t* u) {
    if (!u) return mgr->false_node;
    if (u == mgr->true_node) return mgr->false_node;
    if (u == mgr->false_node) return mgr->true_node;

    /* For non-terminal: recursively negate by ite(var, ¬low, ¬high) */
    grg1_bdd_node_t* not_low = grg1_bdd_not(mgr, u->low);
    grg1_bdd_node_t* not_high = grg1_bdd_not(mgr, u->high);
    return grg1_bdd_unique(mgr, u->variable, not_low, not_high);
}

/* =========================================================================
 * BDD quantification (exists / forall)
 * ========================================================================*/

/**
 * Existential quantification: ∃v. f(v, ...) = f|_{v=0} ∨ f|_{v=1}
 *
 * The Shannon expansion gives:
 *   f = (¬v ∧ f|_{v=0}) ∨ (v ∧ f|_{v=1})
 *   ∃v. f = f|_{v=0} ∨ f|_{v=1}
 *
 * This operation eliminates a variable by taking the OR of its
 * positive and negative cofactors.
 *
 * Complexity: O(|f|²) worst case, but typically O(|f|) with sharing
 * Reference: Burch et al. (1992) — Relational product computation
 */
grg1_bdd_node_t* grg1_bdd_exists(grg1_bdd_manager_t* mgr,
                                   grg1_bdd_node_t* f,
                                   grg1_bdd_var_t var) {
    if (!f || f->type != GRG1_BDD_NONTERMINAL) return f;

    /* If current variable is the one we're quantifying, OR the cofactors */
    if (f->variable == var) {
        return grg1_bdd_or(mgr, f->low, f->high);
    }

    /* If variable appears lower in the BDD */
    if (f->variable < var) {
        grg1_bdd_node_t* low = grg1_bdd_exists(mgr, f->low, var);
        grg1_bdd_node_t* high = grg1_bdd_exists(mgr, f->high, var);
        return grg1_bdd_unique(mgr, f->variable, low, high);
    }

    /* If var < f->variable, the variable doesn't appear in f, so ∃v.f = f */
    return f;
}

/**
 * Universal quantification: ∀v. f = f|_{v=0} ∧ f|_{v=1}
 *
 * Can be expressed as: ∀v. f = ¬∃v. ¬f
 *
 * Complexity: O(|f|²) worst case
 * Reference: Burch et al. (1992) — Universal quantification for CTL
 */
grg1_bdd_node_t* grg1_bdd_forall(grg1_bdd_manager_t* mgr,
                                   grg1_bdd_node_t* f,
                                   grg1_bdd_var_t var) {
    if (!f) return mgr->false_node;
    /* ∀v. f = ¬∃v. ¬f */
    grg1_bdd_node_t* not_f = grg1_bdd_not(mgr, f);
    grg1_bdd_node_t* ex_not_f = grg1_bdd_exists(mgr, not_f, var);
    return grg1_bdd_not(mgr, ex_not_f);
}

/* =========================================================================
 * BDD restrict (cofactor)
 * ========================================================================*/

/**
 * Restrict (cofactor): compute f|_{var=value}.
 *
 * This substitutes a specific value for a variable, simplifying the BDD.
 * For existential quantification over large variable sets, restrict
 * followed by OR is more efficient than repeated exists.
 *
 * Complexity: O(|f|)
 * Reference: Bryant (1986) §4 — Restrict operation
 */
grg1_bdd_node_t* grg1_bdd_restrict(grg1_bdd_manager_t* mgr,
                                     grg1_bdd_node_t* f,
                                     grg1_bdd_var_t var,
                                     bool value) {
    if (!f || f->type != GRG1_BDD_NONTERMINAL) return f;

    if (f->variable == var) {
        return value ? f->high : f->low;
    }

    if (f->variable < var) {
        grg1_bdd_node_t* low = grg1_bdd_restrict(mgr, f->low, var, value);
        grg1_bdd_node_t* high = grg1_bdd_restrict(mgr, f->high, var, value);
        return grg1_bdd_unique(mgr, f->variable, low, high);
    }

    return f;
}

/* =========================================================================
 * BDD satisfiability and counting
 * ========================================================================*/

/**
 * Count the number of satisfying assignments for a BDD.
 *
 * Each path from root to TRUE terminal corresponds to a set of
 * satisfying assignments. The count is the sum over all paths.
 *
 * Uses dynamic programming: satcount(v) = 2^{(next_var - var - 1)} ×
 *   (satcount(low) + satcount(high)), accounting for skipped variables.
 *
 * Complexity: O(|f|) with memoization
 * Reference: Bryant (1986) §5 — Satisfiability counting
 */
static int64_t grg1_bdd_satcount_rec(grg1_bdd_node_t* node,
                                       int num_vars,
                                       int* var_levels,
                                       int64_t* memo) {
    if (!node) return 0;
    if (node->type == GRG1_BDD_TERMINAL_FALSE) return 0;
    if (node->type == GRG1_BDD_TERMINAL_TRUE) {
        /* All remaining variables are free */
        return (int64_t)1 << (num_vars - (var_levels ? var_levels[0] : 0));
    }

    int id = node->id;
    if (memo && memo[id] >= 0) return memo[id];

    int64_t count = 0;
    int current_level = var_levels ? var_levels[node->variable] : node->variable;

    /* Recursively count low and high branches */
    if (node->low) {
        /* Skip variables between current and low child */
        int next_level = (node->low->type == GRG1_BDD_NONTERMINAL)
                          ? (var_levels ? var_levels[node->low->variable] : node->low->variable)
                          : num_vars;
        int skipped = next_level - current_level - 1;
        int64_t factor = (int64_t)1 << (skipped > 0 ? skipped : 0);
        count += factor * grg1_bdd_satcount_rec(node->low, num_vars, var_levels, memo);
    }
    if (node->high) {
        int next_level = (node->high->type == GRG1_BDD_NONTERMINAL)
                          ? (var_levels ? var_levels[node->high->variable] : node->high->variable)
                          : num_vars;
        int skipped = next_level - current_level - 1;
        int64_t factor = (int64_t)1 << (skipped > 0 ? skipped : 0);
        count += factor * grg1_bdd_satcount_rec(node->high, num_vars, var_levels, memo);
    }

    if (memo) memo[id] = count;
    return count;
}

int64_t grg1_bdd_satcount(grg1_bdd_manager_t* mgr, grg1_bdd_node_t* f) {
    if (!mgr || !f) return 0;

    /* Allocate memoization table */
    int total_nodes = mgr->next_node_id;
    int64_t* memo = (int64_t*)malloc((size_t)total_nodes * sizeof(int64_t));
    if (!memo) return -1;
    for (int i = 0; i < total_nodes; i++) memo[i] = -1;

    /* Build variable level map (variable index → level in order) */
    int* var_levels = (int*)malloc((size_t)mgr->num_vars * sizeof(int));
    if (var_levels) {
        for (int i = 0; i < mgr->num_vars; i++) var_levels[i] = i;
    }

    int64_t count = grg1_bdd_satcount_rec(f, mgr->num_vars, var_levels, memo);

    free(memo);
    free(var_levels);
    return count;
}

/**
 * Find one satisfying assignment (any single SAT solution).
 *
 * Walks from root to TRUE terminal, making arbitrary choices at each
 * decision node. Returns the assignment as a bit-vector.
 *
 * Complexity: O(|f|)
 * Reference: Bryant (1986) — SAT enumeration
 */
bool grg1_bdd_any_sat(grg1_bdd_manager_t* mgr, grg1_bdd_node_t* f,
                       bool* assignment) {
    if (!mgr || !f || !assignment) return false;

    if (f == mgr->false_node) return false;
    if (f == mgr->true_node) return true;

    /* Walk down the BDD, choosing branches that lead to TRUE */
    grg1_bdd_node_t* current = f;
    while (current && current->type == GRG1_BDD_NONTERMINAL) {
        /* Prefer the branch that can reach TRUE */
        bool can_go_high = (current->high != mgr->false_node);
        if (can_go_high) {
            assignment[current->variable] = true;
            current = current->high;
        } else {
            assignment[current->variable] = false;
            current = current->low;
        }
    }

    return (current == mgr->true_node);
}

/* =========================================================================
 * BDD to explicit game conversion
 * ========================================================================*/

/**
 * Convert a BDD representing a set of states to an explicit state region.
 *
 * For each variable, the BDD variable corresponds to one bit of the
 * state encoding. This function evaluates the BDD for each possible
 * state and populates the region bit-vector.
 *
 * Complexity: O(|S| × |f|) — exponential in number of variables
 *            (this is the fundamental limitation that BDDs try to overcome)
 */
void grg1_bdd_to_region(grg1_bdd_manager_t* mgr, grg1_bdd_node_t* f,
                         grg1_region_t* region) {
    if (!mgr || !f || !region) return;

    int n = region->num_states;
    grg1_region_clear(region);

    for (int state = 0; state < n; state++) {
        /* Build assignment for this state */
        bool assignment[64] = {false}; /* Assume ≤64 BDD variables */
        for (int v = 0; v < mgr->num_vars && v < 64; v++) {
            assignment[v] = (state >> v) & 1;
        }

        /* Evaluate BDD on this assignment */
        grg1_bdd_node_t* current = f;
        while (current && current->type == GRG1_BDD_NONTERMINAL) {
            if (current->variable < 64 && assignment[current->variable]) {
                current = current->high;
            } else {
                current = current->low;
            }
        }

        if (current == mgr->true_node) {
            grg1_region_add(region, state);
        }
    }
}

/* =========================================================================
 * BDD node count (for statistics)
 * ========================================================================*/

/**
 * Count the number of BDD nodes (excluding terminals).
 *
 * Useful for measuring BDD size and detecting blow-up.
 *
 * Complexity: O(|f|) with visited set
 */
int grg1_bdd_node_count(grg1_bdd_node_t* f) {
    if (!f || f->type != GRG1_BDD_NONTERMINAL) return 0;

    /* Use a simple recursion with node ID tracking.
     * In production, a proper visited set would be used. */
    int count = 1; /* This node */
    if (f->low && f->low->type == GRG1_BDD_NONTERMINAL &&
        f->low->id > f->id) {
        count += grg1_bdd_node_count(f->low);
    }
    if (f->high && f->high->type == GRG1_BDD_NONTERMINAL &&
        f->high->id > f->id) {
        count += grg1_bdd_node_count(f->high);
    }
    return count;
}

/**
 * Print a BDD in a human-readable format (for debugging small BDDs).
 */
void grg1_bdd_print(grg1_bdd_manager_t* mgr, grg1_bdd_node_t* f) {
    if (!f) {
        printf("NULL\n");
        return;
    }
    if (f == mgr->true_node) {
        printf("TRUE\n");
        return;
    }
    if (f == mgr->false_node) {
        printf("FALSE\n");
        return;
    }
    printf("BDD(v%d, ", f->variable);
    grg1_bdd_print(mgr, f->low);
    printf(", ");
    grg1_bdd_print(mgr, f->high);
    printf(")");
}
