/**
 * grg1_types.c — Implementation of core GR(1) synthesis data types
 *
 * Provides memory management, equality, copying, and basic operations
 * for all core data structures: valuations, states, regions, strategies,
 * game graphs, fixpoint traces, and BDD managers.
 *
 * Knowledge coverage:
 *   L1: State and valuation operations
 *   L2: Region (bit-vector) operations for state sets
 *   L3: Lattice operations on state sets (union, intersection, complement)
 *   L5: Efficient bit-vector implementation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "grg1_types.h"

/* =========================================================================
 * Valuation operations
 * ========================================================================*/

grg1_valuation_t* grg1_valuation_alloc(int num_variables) {
    if (num_variables <= 0 || num_variables > GRG1_MAX_VARIABLES) {
        return NULL;
    }
    grg1_valuation_t* v = (grg1_valuation_t*)malloc(sizeof(grg1_valuation_t));
    if (!v) return NULL;
    memset(v->values, 0, sizeof(v->values));
    v->num_variables = num_variables;
    return v;
}

void grg1_valuation_free(grg1_valuation_t* val) {
    free(val);
}

/**
 * Copy a valuation (deep copy).
 *
 * Each variable's value is independently copied to ensure no shared state.
 * This is essential for game graph construction where states must be
 * independently modifiable.
 *
 * Complexity: O(num_variables)
 * Knowledge: L1 — state identity is determined by valuation equality
 */
grg1_valuation_t* grg1_valuation_copy(const grg1_valuation_t* src) {
    if (!src) return NULL;
    grg1_valuation_t* dst = grg1_valuation_alloc(src->num_variables);
    if (!dst) return NULL;
    memcpy(dst->values, src->values, src->num_variables * sizeof(int));
    return dst;
}

/**
 * Compare two valuations for equality.
 *
 * Two valuations are equal iff they have the same number of variables
 * and all variable values are equal. This defines state identity in the
 * game graph.
 *
 * Complexity: O(num_variables)
 * Knowledge: L1 — equivalence relation on the state space
 */
bool grg1_valuation_equal(const grg1_valuation_t* a, const grg1_valuation_t* b) {
    if (!a || !b) return false;
    if (a->num_variables != b->num_variables) return false;
    for (int i = 0; i < a->num_variables; i++) {
        if (a->values[i] != b->values[i]) return false;
    }
    return true;
}

/**
 * Compute a hash value for a valuation.
 *
 * Uses a simple multiplicative hash. The hash is used for fast state
 * lookup in hash tables (though the current explicit implementation
 * uses linear search, this enables future optimizations).
 *
 * Complexity: O(num_variables)
 * Knowledge: L3 — hashing as a collision-resistant function on state space
 */
uint32_t grg1_valuation_hash(const grg1_valuation_t* v) {
    if (!v) return 0;
    uint32_t hash = 5381;
    for (int i = 0; i < v->num_variables; i++) {
        hash = ((hash << 5) + hash) + (uint32_t)v->values[i]; /* hash * 33 + val */
    }
    return hash;
}

/**
 * Print a valuation in human-readable format.
 *
 * Outputs each variable name and its current value.
 * Used for debugging and strategy simulation output.
 *
 * Complexity: O(num_variables)
 */
void grg1_valuation_print(const grg1_valuation_t* v,
                           const grg1_variable_t* vars) {
    if (!v || !vars) {
        printf("(null valuation)\n");
        return;
    }
    printf("{");
    for (int i = 0; i < v->num_variables; i++) {
        printf("%s=%d", vars[i].name, v->values[i]);
        if (i < v->num_variables - 1) printf(", ");
    }
    printf("}");
}

/**
 * Set a variable's value in a valuation.
 *
 * Bounds-checked assignment. Returns false if the value exceeds
 * the variable's domain.
 *
 * Complexity: O(1)
 * Knowledge: L1 — variable assignment as atomic state change
 */
bool grg1_valuation_set(grg1_valuation_t* v, int var_index, int value,
                         const grg1_variable_t* vars) {
    if (!v || var_index < 0 || var_index >= v->num_variables) return false;
    if (value < 0 || value >= vars[var_index].domain_size) return false;
    v->values[var_index] = value;
    return true;
}

/**
 * Get a variable's value from a valuation.
 *
 * Complexity: O(1)
 */
int grg1_valuation_get(const grg1_valuation_t* v, int var_index) {
    if (!v || var_index < 0 || var_index >= v->num_variables) return -1;
    return v->values[var_index];
}

/* =========================================================================
 * Game arena operations
 * ========================================================================*/

grg1_game_t* grg1_game_alloc(int capacity) {
    if (capacity <= 0) capacity = GRG1_MAX_STATES_DEFAULT;

    grg1_game_t* game = (grg1_game_t*)malloc(sizeof(grg1_game_t));
    if (!game) return NULL;
    memset(game, 0, sizeof(grg1_game_t));

    game->capacity = capacity;
    game->num_states = 0;

    game->states = (grg1_state_t*)malloc((size_t)capacity * sizeof(grg1_state_t));
    game->successors = (grg1_adj_list_t*)malloc((size_t)capacity * sizeof(grg1_adj_list_t));
    game->predecessors = (grg1_adj_list_t*)malloc((size_t)capacity * sizeof(grg1_adj_list_t));
    game->whose_turn = (grg1_player_t*)malloc((size_t)capacity * sizeof(grg1_player_t));
    game->env_successor_count = (int*)malloc((size_t)capacity * sizeof(int));
    game->sys_successor_count = (int*)malloc((size_t)capacity * sizeof(int));
    game->reachable_from_initial = (bool*)malloc((size_t)capacity * sizeof(bool));

    if (!game->states || !game->successors || !game->predecessors ||
        !game->whose_turn || !game->env_successor_count ||
        !game->sys_successor_count || !game->reachable_from_initial) {
        grg1_game_free(game);
        return NULL;
    }

    /* Initialize adjacency lists */
    for (int i = 0; i < capacity; i++) {
        game->successors[i].head = NULL;
        game->successors[i].count = 0;
        game->predecessors[i].head = NULL;
        game->predecessors[i].count = 0;
        game->env_successor_count[i] = 0;
        game->sys_successor_count[i] = 0;
        game->reachable_from_initial[i] = false;
    }

    game->num_variables = 0;
    game->variables = NULL;
    game->reachability_computed = false;

    return game;
}

void grg1_game_free(grg1_game_t* game) {
    if (!game) return;

    /* Free adjacency list nodes */
    for (int i = 0; i < game->capacity; i++) {
        grg1_adj_node_t* node = game->successors[i].head;
        while (node) {
            grg1_adj_node_t* next = node->next;
            free(node);
            node = next;
        }
        node = game->predecessors[i].head;
        while (node) {
            grg1_adj_node_t* next = node->next;
            free(node);
            node = next;
        }
    }

    free(game->states);
    free(game->successors);
    free(game->predecessors);
    free(game->whose_turn);
    free(game->env_successor_count);
    free(game->sys_successor_count);
    free(game->reachable_from_initial);
    free(game->variables);
    free(game);
}

/**
 * Add a state to the game arena.
 *
 * The state is added at the next available index. If the valuation already
 * exists (checked by linear scan), the existing state ID is returned instead.
 * This deduplication is essential for correct game semantics.
 *
 * Complexity: O(num_states × num_variables) for dedup scan
 */
int grg1_game_add_state(grg1_game_t* game, const grg1_valuation_t* valuation,
                         grg1_player_t whose_turn, bool is_initial) {
    if (!game || !valuation) return -1;

    /* Check for duplicate valuation */
    for (int i = 0; i < game->num_states; i++) {
        if (grg1_valuation_equal(&game->states[i].valuation, valuation)) {
            return i;
        }
    }

    if (game->num_states >= game->capacity) {
        return -1;
    }

    int id = game->num_states;
    game->states[id].state_id = id;
    game->states[id].valuation = *valuation;
    game->states[id].whose_turn = whose_turn;
    game->states[id].is_initial = is_initial;
    game->states[id].is_error = false;
    game->states[id].rank = 0;
    game->states[id].in_winning_region = false;
    game->states[id].visited = false;
    game->whose_turn[id] = whose_turn;
    game->num_states++;

    return id;
}

/**
 * Add a transition between two states.
 *
 * Appends to both the source's successor list and the target's predecessor
 * list. Both are maintained for efficient forward and backward traversals.
 * Successor/predecessor counts are updated immediately.
 *
 * The action label records which action was taken, enabling strategy extraction
 * to produce human-readable strategies.
 *
 * Complexity: O(1) amortized (linked list append)
 */
void grg1_game_add_transition(grg1_game_t* game, int from_state,
                               int to_state, grg1_player_t by_player,
                               int action) {
    if (!game) return;
    if (from_state < 0 || from_state >= game->num_states) return;
    if (to_state < 0 || to_state >= game->num_states) return;

    /* Add to successors */
    grg1_adj_node_t* succ_node = (grg1_adj_node_t*)malloc(sizeof(grg1_adj_node_t));
    if (!succ_node) return;
    succ_node->target_state_id = to_state;
    succ_node->action_label = action;
    succ_node->next = game->successors[from_state].head;
    game->successors[from_state].head = succ_node;
    game->successors[from_state].count++;

    /* Update per-player successor counts */
    if (by_player == GRG1_PLAYER_ENVIRONMENT) {
        game->env_successor_count[from_state]++;
    } else {
        game->sys_successor_count[from_state]++;
    }

    /* Add to predecessors */
    grg1_adj_node_t* pred_node = (grg1_adj_node_t*)malloc(sizeof(grg1_adj_node_t));
    if (!pred_node) return;
    pred_node->target_state_id = from_state;
    pred_node->action_label = action;
    pred_node->next = game->predecessors[to_state].head;
    game->predecessors[to_state].head = pred_node;
    game->predecessors[to_state].count++;
}

/* =========================================================================
 * Region (bit-vector) operations
 * ========================================================================*/

grg1_region_t* grg1_region_alloc(int num_states) {
    if (num_states <= 0) return NULL;
    grg1_region_t* r = (grg1_region_t*)malloc(sizeof(grg1_region_t));
    if (!r) return NULL;

    r->num_states = num_states;
    r->words_needed = (num_states + 63) / 64;
    r->bits = (uint64_t*)calloc((size_t)r->words_needed, sizeof(uint64_t));
    if (!r->bits) {
        free(r);
        return NULL;
    }
    return r;
}

void grg1_region_free(grg1_region_t* region) {
    if (!region) return;
    free(region->bits);
    free(region);
}

/**
 * Check if a state is in the region.
 *
 * Uses bitwise AND to test the specific bit. Bit index = state_id within
 * the appropriate word.
 *
 * Complexity: O(1)
 * Knowledge: L3 — characteristic function of a set
 */
bool grg1_region_contains(const grg1_region_t* region, int state_id) {
    if (!region || state_id < 0 || state_id >= region->num_states) return false;
    int word = state_id / 64;
    int bit = state_id % 64;
    return (region->bits[word] & ((uint64_t)1 << bit)) != 0;
}

/**
 * Add a state to the region (set union with singleton).
 *
 * Complexity: O(1)
 */
void grg1_region_add(grg1_region_t* region, int state_id) {
    if (!region || state_id < 0 || state_id >= region->num_states) return;
    int word = state_id / 64;
    int bit = state_id % 64;
    region->bits[word] |= ((uint64_t)1 << bit);
}

/**
 * Remove a state from the region (set difference with singleton).
 *
 * Complexity: O(1)
 */
void grg1_region_remove(grg1_region_t* region, int state_id) {
    if (!region || state_id < 0 || state_id >= region->num_states) return;
    int word = state_id / 64;
    int bit = state_id % 64;
    region->bits[word] &= ~((uint64_t)1 << bit);
}

/**
 * Clear all bits in the region (set to empty set).
 *
 * Complexity: O(words_needed)
 */
void grg1_region_clear(grg1_region_t* region) {
    if (!region) return;
    memset(region->bits, 0, (size_t)region->words_needed * sizeof(uint64_t));
}

/**
 * Set all bits in the region (set to the full state space).
 *
 * Used as the initial approximation for greatest fixpoint computation,
 * which starts from the full set S.
 *
 * Complexity: O(words_needed)
 * Knowledge: L2 — the top element ⊤ of the lattice P(S)
 */
void grg1_region_fill(grg1_region_t* region) {
    if (!region) return;
    /* Set all bits to 1 */
    for (int i = 0; i < region->words_needed; i++) {
        region->bits[i] = ~((uint64_t)0);
    }
    /* Clear excess bits in the last word beyond num_states */
    int rem = region->num_states % 64;
    if (rem != 0 && region->words_needed > 0) {
        uint64_t mask = ((uint64_t)1 << rem) - 1;
        region->bits[region->words_needed - 1] = mask;
    }
}

/**
 * Set union: R = A ∪ B.
 *
 * Each word is OR'd independently — this exploits 64-bit word-level
 * parallelism for efficiency.
 *
 * Complexity: O(words_needed)
 * Knowledge: L3 — join operation in the Boolean lattice
 */
void grg1_region_union(grg1_region_t* result,
                        const grg1_region_t* a,
                        const grg1_region_t* b) {
    if (!result || !a || !b) return;
    int n = result->words_needed;
    for (int i = 0; i < n; i++) {
        result->bits[i] = a->bits[i] | b->bits[i];
    }
}

/**
 * Set intersection: R = A ∩ B.
 *
 * Complexity: O(words_needed)
 * Knowledge: L3 — meet operation in the Boolean lattice
 */
void grg1_region_intersect(grg1_region_t* result,
                            const grg1_region_t* a,
                            const grg1_region_t* b) {
    if (!result || !a || !b) return;
    int n = result->words_needed;
    for (int i = 0; i < n; i++) {
        result->bits[i] = a->bits[i] & b->bits[i];
    }
}

/**
 * Set difference: R = A \ B.
 *
 * Complexity: O(words_needed)
 * Knowledge: L3 — relative complement in the Boolean lattice
 */
void grg1_region_difference(grg1_region_t* result,
                             const grg1_region_t* a,
                             const grg1_region_t* b) {
    if (!result || !a || !b) return;
    int n = result->words_needed;
    for (int i = 0; i < n; i++) {
        result->bits[i] = a->bits[i] & ~b->bits[i];
    }
}

/**
 * Set complement: R = S \ A (relative to the full state space S).
 *
 * Complexity: O(words_needed)
 * Knowledge: L3 — complement in the Boolean lattice; De Morgan's laws
 */
void grg1_region_complement(grg1_region_t* result, const grg1_region_t* a) {
    if (!result || !a) return;

    /* Copy the full set first */
    grg1_region_fill(result);

    /* Then subtract a */
    int n = result->words_needed;
    for (int i = 0; i < n; i++) {
        result->bits[i] &= ~a->bits[i];
    }
}

/**
 * Check if two regions are equal.
 *
 * Complexity: O(words_needed)
 * Knowledge: L1 — extensional equality of sets
 */
bool grg1_region_equal(const grg1_region_t* a, const grg1_region_t* b) {
    if (!a || !b) return false;
    if (a->num_states != b->num_states) return false;
    int n = a->words_needed;
    for (int i = 0; i < n; i++) {
        if (a->bits[i] != b->bits[i]) return false;
    }
    return true;
}

/**
 * Check if region A is a subset of region B.
 *
 * A ⊆ B iff A \ B = ∅, i.e., for every word, A's bits are all in B's bits.
 *
 * Complexity: O(words_needed)
 * Knowledge: L2 — subset ordering in the lattice; monotonicity testing
 */
bool grg1_region_subset(const grg1_region_t* a, const grg1_region_t* b) {
    if (!a || !b) return false;
    int n = a->words_needed;
    for (int i = 0; i < n; i++) {
        if ((a->bits[i] & ~b->bits[i]) != 0) return false;
    }
    return true;
}

/**
 * Check if the region is empty.
 *
 * Complexity: O(words_needed)
 */
bool grg1_region_is_empty(const grg1_region_t* region) {
    if (!region) return true;
    for (int i = 0; i < region->words_needed; i++) {
        if (region->bits[i] != 0) return false;
    }
    return true;
}

/**
 * Count the number of states in the region.
 *
 * Uses a popcount (population count) on each 64-bit word.
 * This is more efficient than counting bit-by-bit.
 *
 * Complexity: O(words_needed)
 * Knowledge: L5 — efficient set cardinality via hardware popcount where available
 */
int grg1_region_count(const grg1_region_t* region) {
    if (!region) return 0;
    int count = 0;
    for (int i = 0; i < region->words_needed; i++) {
        uint64_t w = region->bits[i];
        /* Brian Kernighan's popcount for portability */
        while (w) {
            w &= w - 1;
            count++;
        }
    }
    return count;
}

/**
 * Copy one region to another.
 *
 * Complexity: O(words_needed)
 */
void grg1_region_copy(grg1_region_t* dst, const grg1_region_t* src) {
    if (!dst || !src) return;
    int n = dst->words_needed < src->words_needed ? dst->words_needed : src->words_needed;
    memcpy(dst->bits, src->bits, (size_t)n * sizeof(uint64_t));
}

/* =========================================================================
 * Strategy operations
 * ========================================================================*/

grg1_strategy_t* grg1_strategy_alloc(int num_states) {
    if (num_states <= 0) return NULL;
    grg1_strategy_t* s = (grg1_strategy_t*)malloc(sizeof(grg1_strategy_t));
    if (!s) return NULL;

    s->num_states = num_states;
    s->move = (int*)malloc((size_t)num_states * sizeof(int));
    s->action = (int*)malloc((size_t)num_states * sizeof(int));
    if (!s->move || !s->action) {
        grg1_strategy_free(s);
        return NULL;
    }

    /* Initialize: no move defined */
    for (int i = 0; i < num_states; i++) {
        s->move[i] = -1;
        s->action[i] = -1;
    }
    s->is_complete = false;
    return s;
}

void grg1_strategy_free(grg1_strategy_t* strategy) {
    if (!strategy) return;
    free(strategy->move);
    free(strategy->action);
    free(strategy);
}

/**
 * Set the strategy's move for a given state.
 *
 * Complexity: O(1)
 */
void grg1_strategy_set_move(grg1_strategy_t* s, int from_state,
                             int to_state, int action) {
    if (!s || from_state < 0 || from_state >= s->num_states) return;
    s->move[from_state] = to_state;
    s->action[from_state] = action;
}

/**
 * Get the strategy's move for a given state.
 *
 * Complexity: O(1)
 */
int grg1_strategy_get_move(const grg1_strategy_t* s, int from_state) {
    if (!s || from_state < 0 || from_state >= s->num_states) return -1;
    return s->move[from_state];
}

/* =========================================================================
 * Fixpoint trace operations
 * ========================================================================*/

grg1_fixpoint_trace_t* grg1_fixpoint_trace_alloc(int capacity) {
    if (capacity <= 0) capacity = 100;
    grg1_fixpoint_trace_t* t = (grg1_fixpoint_trace_t*)malloc(
        sizeof(grg1_fixpoint_trace_t));
    if (!t) return NULL;
    t->capacity = capacity;
    t->count = 0;
    t->regions = (grg1_region_t*)malloc((size_t)capacity * sizeof(grg1_region_t));
    if (!t->regions) {
        free(t);
        return NULL;
    }
    /* Don't initialize regions — they'll be populated on record */
    return t;
}

void grg1_fixpoint_trace_free(grg1_fixpoint_trace_t* trace) {
    if (!trace) return;
    for (int i = 0; i < trace->count; i++) {
        free(trace->regions[i].bits);
    }
    free(trace->regions);
    free(trace);
}

/**
 * Record a snapshot of a region in the trace.
 *
 * Each call records the current state of the fixpoint iteration.
 * This enables convergence analysis and debugging.
 *
 * Complexity: O(words_needed)
 */
bool grg1_fixpoint_trace_record(grg1_fixpoint_trace_t* trace,
                                 const grg1_region_t* region) {
    if (!trace || !region) return false;
    if (trace->count >= trace->capacity) return false;

    int idx = trace->count;
    trace->regions[idx].num_states = region->num_states;
    trace->regions[idx].words_needed = region->words_needed;
    trace->regions[idx].bits = (uint64_t*)malloc(
        (size_t)region->words_needed * sizeof(uint64_t));
    if (!trace->regions[idx].bits) return false;
    memcpy(trace->regions[idx].bits, region->bits,
           (size_t)region->words_needed * sizeof(uint64_t));
    trace->count++;
    return true;
}

/* =========================================================================
 * BDD manager operations
 * ========================================================================*/

grg1_bdd_manager_t* grg1_bdd_manager_alloc(int num_vars, int table_size) {
    if (num_vars <= 0) num_vars = 16;
    if (table_size <= 0) table_size = 4099; /* prime for good hashing */

    grg1_bdd_manager_t* mgr = (grg1_bdd_manager_t*)malloc(
        sizeof(grg1_bdd_manager_t));
    if (!mgr) return NULL;

    mgr->num_vars = num_vars;
    mgr->table_size = table_size;
    mgr->next_node_id = 2; /* 0 and 1 reserved for terminals */

    /* Allocate unique table */
    mgr->unique_table = (grg1_bdd_node_t**)calloc(
        (size_t)table_size, sizeof(grg1_bdd_node_t*));
    if (!mgr->unique_table) {
        free(mgr);
        return NULL;
    }

    /* Create terminal nodes */
    mgr->false_node = (grg1_bdd_node_t*)malloc(sizeof(grg1_bdd_node_t));
    mgr->true_node = (grg1_bdd_node_t*)malloc(sizeof(grg1_bdd_node_t));
    if (!mgr->false_node || !mgr->true_node) {
        free(mgr->false_node);
        free(mgr->true_node);
        free(mgr->unique_table);
        free(mgr);
        return NULL;
    }

    mgr->false_node->id = 0;
    mgr->false_node->type = GRG1_BDD_TERMINAL_FALSE;
    mgr->false_node->variable = -1;
    mgr->false_node->low = NULL;
    mgr->false_node->high = NULL;
    mgr->false_node->ref_count = 1;
    mgr->false_node->next = NULL;

    mgr->true_node->id = 1;
    mgr->true_node->type = GRG1_BDD_TERMINAL_TRUE;
    mgr->true_node->variable = -1;
    mgr->true_node->low = NULL;
    mgr->true_node->high = NULL;
    mgr->true_node->ref_count = 1;
    mgr->true_node->next = NULL;

    return mgr;
}

void grg1_bdd_manager_free(grg1_bdd_manager_t* mgr) {
    if (!mgr) return;

    /* Free all nodes in the unique table */
    for (int i = 0; i < mgr->table_size; i++) {
        grg1_bdd_node_t* node = mgr->unique_table[i];
        while (node) {
            grg1_bdd_node_t* next = node->next;
            free(node);
            node = next;
        }
    }

    free(mgr->false_node);
    free(mgr->true_node);
    free(mgr->unique_table);
    free(mgr);
}
