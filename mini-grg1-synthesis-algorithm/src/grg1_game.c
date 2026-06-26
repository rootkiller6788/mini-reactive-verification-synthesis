/**
 * grg1_game.c — Game graph construction, traversal, and CPre operators
 *
 * Constructs a two-player game arena from a GR(1) specification and
 * provides the fundamental operators needed for game solving:
 * controllable predecessors (CPre), attractors, reachability, and
 * strongly connected component analysis.
 *
 * The game construction is the reduction from GR(1) synthesis to
 * solving an infinite-duration two-player game with Streett winning
 * conditions, which is further reduced to a chain of Büchi games
 * via the ranking construction.
 *
 * Knowledge coverage:
 *   L1: Game graph, transition system, successors/predecessors
 *   L2: Controllability, attractor sets
 *   L3: Graph theory (BFS, DFS, SCC, reachability)
 *   L4: Determinacy of finite-state games (Martin, 1975)
 *   L5: CPre operator, attractor computation, game reduction
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "grg1_game.h"
#include "grg1_spec.h"

/* =========================================================================
 * Game construction from GR(1) specification
 * ========================================================================*/

/**
 * Enumerate all possible valuations for the given variables.
 *
 * The state space is the Cartesian product of all variable domains:
 *   S = Dom(v₀) × Dom(v₁) × ... × Dom(v_{n-1})
 *
 * Each unique combination is a state. The number of states is Π|Dom(vᵢ)|.
 *
 * We generate states in lexicographic order. For each state, we
 * determine which player's turn it is based on who controls the next
 * variable (in a turn-based game, players alternate).
 *
 * Complexity: O(|S| × |V|)
 * Reference: Piterman et al. (2006) §4.1 — State space construction
 */
static int grg1_enumerate_valuations(grg1_game_t* game,
                                      const grg1_spec_t* spec) {
    int nv = spec->num_variables;
    int indices[GRG1_MAX_VARIABLES] = {0};
    int domains[GRG1_MAX_VARIABLES];

    for (int i = 0; i < nv; i++) {
        domains[i] = spec->variables[i].domain_size;
    }

    int count = 0;
    int total_states = 1;
    for (int i = 0; i < nv; i++) total_states *= domains[i];

    if (total_states > game->capacity) {
        return -1; /* State space exceeds capacity */
    }

    /* Generate all valuations via mixed-radix enumeration */
    bool done = false;
    while (!done && count < game->capacity) {
        /* Create valuation from current indices */
        grg1_valuation_t val;
        val.num_variables = nv;
        for (int i = 0; i < nv; i++) {
            val.values[i] = indices[i];
        }

        /* Determine whose turn: in proper GR(1) games, the turn is
         * determined by the system state. Here we assign turns based on
         * whether the first system-controlled variable has a specific
         * property. For simplicity, all states are environment-turn
         * states initially; the actual turn assignment depends on the
         * specific game structure.
         *
         * In the standard construction, the environment and system
         * take turns. We mark all states as environment-turn; the
         * system then moves via transitions to the next state.
         */
        grg1_player_t turn = GRG1_PLAYER_ENVIRONMENT;

        /* Check initial conditions */
        bool is_init = true;
        if (spec->env_init) {
            is_init = is_init && spec->env_init(&val, spec->variables, nv);
        }
        if (spec->sys_init) {
            is_init = is_init && spec->sys_init(&val, spec->variables, nv);
        }

        grg1_game_add_state(game, &val, turn, is_init);
        count++;

        /* Increment indices (mixed-radix counter) */
        int carry = 1;
        for (int i = nv - 1; i >= 0 && carry > 0; i--) {
            indices[i] += carry;
            if (indices[i] >= domains[i]) {
                indices[i] = 0;
                carry = 1;
            } else {
                carry = 0;
            }
        }
        if (carry > 0) done = true; /* Overflow — all combinations enumerated */
    }

    return count;
}

/**
 * Add all legal transitions between states based on safety conditions.
 *
 * For each pair of states (s, s'), we add a transition if:
 *   - The environment safety ρₑ(s, s') allows the environment to move there
 *   - The system safety ρₛ(s, s') allows the system to move there
 *
 * Transitions are added between all pairs where the corresponding safety
 * condition holds. This defines the complete transition relation T.
 *
 * Complexity: O(|S|² × cost(safety_predicate))
 *
 * Note: For large state spaces, this quadratic enumeration is
 * infeasible. In practice, transitions are computed lazily or
 * symbolically (see grg1_bdd.c for the BDD-based variant).
 *
 * Reference: Piterman et al. (2006) §4.2 — Transition relation
 */
static int grg1_add_all_transitions(grg1_game_t* game,
                                     const grg1_spec_t* spec) {
    int n = game->num_states;
    int trans_count = 0;

    for (int s = 0; s < n; s++) {
        for (int t = 0; t < n; t++) {
            /* Determine which player is moving based on state s's turn */
            grg1_player_t player = game->whose_turn[s];

            if (player == GRG1_PLAYER_ENVIRONMENT) {
                /* Environment move: must satisfy env safety */
                if (spec->env_safety) {
                    if (spec->env_safety(&game->states[s].valuation,
                                          &game->states[t].valuation,
                                          spec->variables,
                                          spec->num_variables)) {
                        grg1_game_add_transition(game, s, t,
                                                  GRG1_PLAYER_ENVIRONMENT, 0);
                        trans_count++;
                    }
                } else {
                    /* No env safety constraint = all moves allowed */
                    grg1_game_add_transition(game, s, t,
                                              GRG1_PLAYER_ENVIRONMENT, 0);
                    trans_count++;
                }
            } else {
                /* System move: must satisfy sys safety */
                if (spec->sys_safety) {
                    if (spec->sys_safety(&game->states[s].valuation,
                                          &game->states[t].valuation,
                                          spec->variables,
                                          spec->num_variables)) {
                        grg1_game_add_transition(game, s, t,
                                                  GRG1_PLAYER_SYSTEM, 1);
                        trans_count++;
                    }
                } else {
                    grg1_game_add_transition(game, s, t,
                                              GRG1_PLAYER_SYSTEM, 1);
                    trans_count++;
                }
            }
        }
    }
    return trans_count;
}

grg1_game_t* grg1_game_from_spec(const grg1_spec_t* spec) {
    if (!spec) return NULL;

    int64_t est = grg1_spec_estimate_state_space(spec);
    if (est < 0 || est > GRG1_MAX_STATES_DEFAULT) {
        /* State space too large for explicit construction */
        return NULL;
    }

    int capacity = (int)est;
    if (capacity <= 0) capacity = GRG1_MAX_STATES_DEFAULT;

    grg1_game_t* game = grg1_game_alloc(capacity);
    if (!game) return NULL;

    /* Copy variable definitions */
    game->num_variables = spec->num_variables;
    game->variables = (grg1_variable_t*)malloc(
        (size_t)spec->num_variables * sizeof(grg1_variable_t));
    if (!game->variables) {
        grg1_game_free(game);
        return NULL;
    }
    memcpy(game->variables, spec->variables,
           (size_t)spec->num_variables * sizeof(grg1_variable_t));

    /* Enumerate all states */
    int num_states = grg1_enumerate_valuations(game, spec);
    if (num_states < 0) {
        grg1_game_free(game);
        return NULL;
    }

    /* Add all transitions */
    grg1_add_all_transitions(game, spec);

    return game;
}

grg1_game_t* grg1_game_from_spec_bounded(const grg1_spec_t* spec,
                                          int max_states) {
    if (!spec || max_states <= 0) return NULL;

    grg1_game_t* game = grg1_game_alloc(max_states);
    if (!game) return NULL;

    game->num_variables = spec->num_variables;
    game->variables = (grg1_variable_t*)malloc(
        (size_t)spec->num_variables * sizeof(grg1_variable_t));
    if (!game->variables) {
        grg1_game_free(game);
        return NULL;
    }
    memcpy(game->variables, spec->variables,
           (size_t)spec->num_variables * sizeof(grg1_variable_t));

    /* For bounded construction, enumerate up to max_states states */
    int nv = spec->num_variables;
    int indices[GRG1_MAX_VARIABLES] = {0};
    int domains[GRG1_MAX_VARIABLES];
    for (int i = 0; i < nv; i++) {
        domains[i] = spec->variables[i].domain_size;
    }

    for (int count = 0; count < max_states; count++) {
        grg1_valuation_t val;
        val.num_variables = nv;
        for (int i = 0; i < nv; i++) {
            val.values[i] = indices[i];
        }

        bool is_init = true;
        if (spec->env_init) {
            is_init = is_init && spec->env_init(&val, spec->variables, nv);
        }
        if (spec->sys_init) {
            is_init = is_init && spec->sys_init(&val, spec->variables, nv);
        }

        grg1_game_add_state(game, &val, GRG1_PLAYER_ENVIRONMENT, is_init);

        /* Increment indices */
        int carry = 1;
        for (int i = nv - 1; i >= 0 && carry > 0; i--) {
            indices[i] += carry;
            if (indices[i] >= domains[i]) {
                indices[i] = 0;
                carry = 1;
            } else {
                carry = 0;
            }
        }
        if (carry > 0) break; /* All states enumerated */
    }

    /* Add transitions for the bounded set */
    grg1_add_all_transitions(game, spec);

    return game;
}

/* =========================================================================
 * State lookup
 * ========================================================================*/

int grg1_game_find_state(const grg1_game_t* game,
                          const grg1_valuation_t* valuation) {
    if (!game || !valuation) return -1;
    for (int i = 0; i < game->num_states; i++) {
        if (grg1_valuation_equal(&game->states[i].valuation, valuation)) {
            return i;
        }
    }
    return -1;
}

int grg1_game_successor_count(const grg1_game_t* game, int state_id) {
    if (!game || state_id < 0 || state_id >= game->num_states) return -1;
    return game->successors[state_id].count;
}

int grg1_game_predecessor_count(const grg1_game_t* game, int state_id) {
    if (!game || state_id < 0 || state_id >= game->num_states) return -1;
    return game->predecessors[state_id].count;
}

bool grg1_game_has_transition(const grg1_game_t* game,
                               int from_state, int to_state) {
    if (!game || from_state < 0 || from_state >= game->num_states) return false;

    grg1_adj_node_t* node = game->successors[from_state].head;
    while (node) {
        if (node->target_state_id == to_state) return true;
        node = node->next;
    }
    return false;
}

/* =========================================================================
 * Reachability analysis (BFS)
 * ========================================================================*/

void grg1_game_compute_reachable(grg1_game_t* game) {
    if (!game || game->reachability_computed) return;

    /* Reset reachable flags */
    for (int i = 0; i < game->num_states; i++) {
        game->reachable_from_initial[i] = false;
    }

    /* BFS queue (simple array-based) */
    int* queue = (int*)malloc((size_t)game->num_states * sizeof(int));
    if (!queue) return;
    int qhead = 0, qtail = 0;

    /* Seed queue with all initial states */
    for (int i = 0; i < game->num_states; i++) {
        if (game->states[i].is_initial) {
            queue[qtail++] = i;
            game->reachable_from_initial[i] = true;
        }
    }

    /* BFS traversal */
    while (qhead < qtail) {
        int current = queue[qhead++];
        grg1_adj_node_t* succ = game->successors[current].head;
        while (succ) {
            int next = succ->target_state_id;
            if (!game->reachable_from_initial[next]) {
                game->reachable_from_initial[next] = true;
                queue[qtail++] = next;
            }
            succ = succ->next;
        }
    }

    free(queue);
    game->reachability_computed = true;
}

/* =========================================================================
 * Strongly Connected Components (Tarjan's algorithm)
 * ========================================================================*/

/**
 * Tarjan's SCC algorithm — recursive DFS helper.
 *
 * Maintains discovery time (index) and low-link value for each node.
 * Uses an explicit stack for the SCC under construction.
 *
 * Complexity: O(|S| + |T|) linear in graph size
 * Reference: Tarjan, R. "Depth-first search and linear graph algorithms."
 *            SIAM Journal on Computing, 1(2):146-160, 1972.
 */
static void grg1_tarjan_dfs(const grg1_game_t* game, int v,
                             int* index, int* lowlink,
                             int* stack, int* stack_top,
                             bool* on_stack, int* scc_id,
                             int* current_index, int* current_scc) {
    index[v] = *current_index;
    lowlink[v] = *current_index;
    (*current_index)++;
    stack[(*stack_top)++] = v;
    on_stack[v] = true;

    /* Explore all successors */
    grg1_adj_node_t* succ = game->successors[v].head;
    while (succ) {
        int w = succ->target_state_id;
        if (index[w] == -1) {
            /* Not yet visited */
            grg1_tarjan_dfs(game, w, index, lowlink, stack, stack_top,
                            on_stack, scc_id, current_index, current_scc);
            lowlink[v] = (lowlink[v] < lowlink[w]) ? lowlink[v] : lowlink[w];
        } else if (on_stack[w]) {
            /* Back edge — w is in current SCC */
            lowlink[v] = (lowlink[v] < index[w]) ? lowlink[v] : index[w];
        }
        succ = succ->next;
    }

    /* If v is the root of an SCC */
    if (lowlink[v] == index[v]) {
        int w;
        do {
            w = stack[--(*stack_top)];
            on_stack[w] = false;
            scc_id[w] = *current_scc;
        } while (w != v);
        (*current_scc)++;
    }
}

void grg1_game_compute_scc(const grg1_game_t* game, int* scc_id,
                            int* num_sccs) {
    if (!game || !scc_id || !num_sccs) return;

    int n = game->num_states;
    int* index = (int*)malloc((size_t)n * sizeof(int));
    int* lowlink = (int*)malloc((size_t)n * sizeof(int));
    int* stack = (int*)malloc((size_t)n * sizeof(int));
    bool* on_stack = (bool*)malloc((size_t)n * sizeof(bool));

    if (!index || !lowlink || !stack || !on_stack) {
        free(index); free(lowlink); free(stack); free(on_stack);
        return;
    }

    for (int i = 0; i < n; i++) {
        index[i] = -1;
        lowlink[i] = -1;
        on_stack[i] = false;
        scc_id[i] = -1;
    }

    int current_index = 0;
    int current_scc = 0;
    int stack_top = 0;

    for (int i = 0; i < n; i++) {
        if (index[i] == -1) {
            grg1_tarjan_dfs(game, i, index, lowlink, stack, &stack_top,
                            on_stack, scc_id, &current_index, &current_scc);
        }
    }

    *num_sccs = current_scc;

    free(index);
    free(lowlink);
    free(stack);
    free(on_stack);
}

/* =========================================================================
 * Game property checks
 * ========================================================================*/

bool grg1_game_is_alternating(const grg1_game_t* game) {
    if (!game || game->num_states <= 1) return true;

    /* In an alternating game, transitions go from env states to sys states
     * and vice versa. Check that no transition connects same-type states. */
    for (int i = 0; i < game->num_states; i++) {
        grg1_adj_node_t* succ = game->successors[i].head;
        while (succ) {
            int j = succ->target_state_id;
            if (game->whose_turn[i] == game->whose_turn[j]) {
                return false;
            }
            succ = succ->next;
        }
    }
    return true;
}

bool grg1_game_is_deadlock_free(const grg1_game_t* game) {
    if (!game) return false;

    for (int i = 0; i < game->num_states; i++) {
        if (game->whose_turn[i] == GRG1_PLAYER_SYSTEM) {
            if (game->successors[i].count == 0) {
                return false; /* System deadlock */
            }
        }
    }
    return true;
}

/* =========================================================================
 * CPre operators
 * ========================================================================*/

void grg1_game_cpre_sys(const grg1_game_t* game, const grg1_region_t* Z,
                         grg1_region_t* result) {
    if (!game || !Z || !result) return;

    grg1_region_clear(result);
    int n = game->num_states;

    for (int s = 0; s < n; s++) {
        if (game->whose_turn[s] == GRG1_PLAYER_SYSTEM) {
            /* System state: ∃s'. (s,s') ∈ Tₛ ∧ s' ∈ Z */
            grg1_adj_node_t* succ = game->successors[s].head;
            while (succ) {
                if (grg1_region_contains(Z, succ->target_state_id)) {
                    grg1_region_add(result, s);
                    break; /* Found one — condition satisfied */
                }
                succ = succ->next;
            }
        } else {
            /* Environment state: ∀s'. (s,s') ∈ Tₑ → s' ∈ Z */
            bool all_in_Z = true;
            grg1_adj_node_t* succ = game->successors[s].head;
            if (!succ) {
                /* No environment moves — vacuously true */
                all_in_Z = true;
            } else {
                while (succ) {
                    if (!grg1_region_contains(Z, succ->target_state_id)) {
                        all_in_Z = false;
                        break;
                    }
                    succ = succ->next;
                }
            }
            if (all_in_Z) {
                grg1_region_add(result, s);
            }
        }
    }
}

void grg1_game_cpre_env(const grg1_game_t* game, const grg1_region_t* Z,
                         grg1_region_t* result) {
    if (!game || !Z || !result) return;

    grg1_region_clear(result);
    int n = game->num_states;

    for (int s = 0; s < n; s++) {
        if (game->whose_turn[s] == GRG1_PLAYER_ENVIRONMENT) {
            /* Environment state: ∃s'. (s,s') ∈ Tₑ ∧ s' ∈ Z */
            grg1_adj_node_t* succ = game->successors[s].head;
            while (succ) {
                if (grg1_region_contains(Z, succ->target_state_id)) {
                    grg1_region_add(result, s);
                    break;
                }
                succ = succ->next;
            }
        } else {
            /* System state: ∀s'. (s,s') ∈ Tₛ → s' ∈ Z */
            bool all_in_Z = true;
            grg1_adj_node_t* succ = game->successors[s].head;
            if (!succ) {
                all_in_Z = false; /* System deadlocked → not controllable */
            } else {
                while (succ) {
                    if (!grg1_region_contains(Z, succ->target_state_id)) {
                        all_in_Z = false;
                        break;
                    }
                    succ = succ->next;
                }
            }
            if (all_in_Z) {
                grg1_region_add(result, s);
            }
        }
    }
}

/* =========================================================================
 * Attractor computation
 * ========================================================================*/

void grg1_game_attractor_sys(const grg1_game_t* game, const grg1_region_t* Z,
                              grg1_region_t* result, int* iterations) {
    if (!game || !Z || !result) return;

    int n = game->num_states;
    grg1_region_t* current = grg1_region_alloc(n);
    grg1_region_t* next = grg1_region_alloc(n);
    if (!current || !next) {
        grg1_region_free(current);
        grg1_region_free(next);
        return;
    }

    /* Start from Z */
    grg1_region_copy(current, Z);
    int iter = 0;
    int max_iter = n + 1; /* Safety bound */

    while (iter < max_iter) {
        /* Compute CPre_sys(current) */
        grg1_region_t* cpre = grg1_region_alloc(n);
        if (!cpre) break;
        grg1_game_cpre_sys(game, current, cpre);

        /* next = current ∪ CPre_sys(current) */
        grg1_region_union(next, current, cpre);
        grg1_region_free(cpre);

        iter++;

        /* Check fixpoint */
        if (grg1_region_equal(current, next)) {
            break;
        }
        grg1_region_copy(current, next);
    }

    grg1_region_copy(result, current);
    if (iterations) *iterations = iter;

    grg1_region_free(current);
    grg1_region_free(next);
}

void grg1_game_attractor_env(const grg1_game_t* game, const grg1_region_t* Z,
                              grg1_region_t* result, int* iterations) {
    if (!game || !Z || !result) return;

    int n = game->num_states;
    grg1_region_t* current = grg1_region_alloc(n);
    grg1_region_t* next = grg1_region_alloc(n);
    if (!current || !next) {
        grg1_region_free(current);
        grg1_region_free(next);
        return;
    }

    grg1_region_copy(current, Z);
    int iter = 0;
    int max_iter = n + 1;

    while (iter < max_iter) {
        grg1_region_t* cpre = grg1_region_alloc(n);
        if (!cpre) break;
        grg1_game_cpre_env(game, current, cpre);

        grg1_region_union(next, current, cpre);
        grg1_region_free(cpre);
        iter++;

        if (grg1_region_equal(current, next)) break;
        grg1_region_copy(current, next);
    }

    grg1_region_copy(result, current);
    if (iterations) *iterations = iter;

    grg1_region_free(current);
    grg1_region_free(next);
}

/* =========================================================================
 * Game simplification
 * ========================================================================*/

void grg1_game_prune_unreachable(grg1_game_t* game) {
    if (!game) return;

    if (!game->reachability_computed) {
        grg1_game_compute_reachable(game);
    }

    grg1_region_t* unreachable = grg1_region_alloc(game->num_states);
    if (!unreachable) return;

    grg1_region_fill(unreachable);
    for (int i = 0; i < game->num_states; i++) {
        if (game->reachable_from_initial[i]) {
            grg1_region_remove(unreachable, i);
        }
    }

    grg1_game_remove_region(game, unreachable);
    grg1_region_free(unreachable);
}

void grg1_game_remove_region(grg1_game_t* game, const grg1_region_t* to_remove) {
    if (!game || !to_remove) return;

    /* Mark removed states */
    for (int i = 0; i < game->num_states; i++) {
        if (grg1_region_contains(to_remove, i)) {
            game->states[i].is_error = true; /* Mark as removed */
        }
    }
}

grg1_game_t* grg1_game_induced_subgame(const grg1_game_t* game,
                                        const grg1_region_t* region) {
    if (!game || !region) return NULL;

    int count = grg1_region_count(region);
    if (count <= 0) return NULL;

    grg1_game_t* sub = grg1_game_alloc(count);
    if (!sub) return NULL;

    /* Map old IDs to new IDs */
    int* id_map = (int*)malloc((size_t)game->num_states * sizeof(int));
    if (!id_map) { grg1_game_free(sub); return NULL; }
    for (int i = 0; i < game->num_states; i++) {
        id_map[i] = -1;
    }

    /* Copy variables */
    sub->num_variables = game->num_variables;
    sub->variables = (grg1_variable_t*)malloc(
        (size_t)game->num_variables * sizeof(grg1_variable_t));
    if (!sub->variables) {
        free(id_map);
        grg1_game_free(sub);
        return NULL;
    }
    memcpy(sub->variables, game->variables,
           (size_t)game->num_variables * sizeof(grg1_variable_t));

    /* Add states in region */
    for (int i = 0; i < game->num_states; i++) {
        if (grg1_region_contains(region, i)) {
            int new_id = grg1_game_add_state(sub, &game->states[i].valuation,
                                              game->whose_turn[i],
                                              game->states[i].is_initial);
            id_map[i] = new_id;
        }
    }

    /* Add transitions between states both in region */
    for (int i = 0; i < game->num_states; i++) {
        if (id_map[i] < 0) continue;
        grg1_adj_node_t* succ = game->successors[i].head;
        while (succ) {
            int j = succ->target_state_id;
            if (id_map[j] >= 0) {
                grg1_game_add_transition(sub, id_map[i], id_map[j],
                                          game->whose_turn[i],
                                          succ->action_label);
            }
            succ = succ->next;
        }
    }

    free(id_map);
    return sub;
}

int grg1_game_count_actions(const grg1_game_t* game) {
    if (!game) return 0;

    /* Count distinct action labels */
    bool seen[256] = {false}; /* Assumes ≤256 action labels */
    int count = 0;

    for (int i = 0; i < game->num_states; i++) {
        grg1_adj_node_t* succ = game->successors[i].head;
        while (succ) {
            int a = succ->action_label;
            if (a >= 0 && a < 256 && !seen[a]) {
                seen[a] = true;
                count++;
            }
            succ = succ->next;
        }
    }
    return count;
}

/* =========================================================================
 * Game display
 * ========================================================================*/

void grg1_game_print_summary(const grg1_game_t* game) {
    if (!game) {
        printf("(null game)\n");
        return;
    }

    int total_trans = 0;
    int sys_trans = 0;
    int env_trans = 0;
    int sys_states = 0;
    int env_states = 0;
    int init_states = 0;

    for (int i = 0; i < game->num_states; i++) {
        if (game->whose_turn[i] == GRG1_PLAYER_SYSTEM) sys_states++;
        else env_states++;
        if (game->states[i].is_initial) init_states++;

        grg1_adj_node_t* succ = game->successors[i].head;
        while (succ) {
            total_trans++;
            if (game->whose_turn[i] == GRG1_PLAYER_SYSTEM) sys_trans++;
            else env_trans++;
            succ = succ->next;
        }
    }

    printf("Game Arena Summary:\n");
    printf("  States: %d total (%d sys, %d env, %d initial)\n",
           game->num_states, sys_states, env_states, init_states);
    printf("  Transitions: %d total (%d sys, %d env)\n",
           total_trans, sys_trans, env_trans);
    printf("  Variables: %d\n", game->num_variables);
    printf("  Avg out-degree: %.2f\n",
           game->num_states > 0 ? (double)total_trans / game->num_states : 0.0);
}

void grg1_game_export_dot(const grg1_game_t* game, const char* filename) {
    if (!game || !filename) return;

    FILE* f = fopen(filename, "w");
    if (!f) return;

    fprintf(f, "digraph grg1_game {\n");
    fprintf(f, "  rankdir=LR;\n");
    fprintf(f, "  node [shape=circle];\n");

    /* Nodes */
    for (int i = 0; i < game->num_states; i++) {
        const char* color = game->whose_turn[i] == GRG1_PLAYER_SYSTEM ?
                            "lightblue" : "lightcoral";
        const char* shape = game->whose_turn[i] == GRG1_PLAYER_SYSTEM ?
                            "box" : "diamond";
        fprintf(f, "  s%d [label=\"%d\", shape=%s, style=filled, fillcolor=%s%s];\n",
                i, i, shape, color,
                game->states[i].is_initial ? ", peripheries=2" : "");
    }

    /* Edges */
    for (int i = 0; i < game->num_states; i++) {
        grg1_adj_node_t* succ = game->successors[i].head;
        while (succ) {
            fprintf(f, "  s%d -> s%d [label=\"a%d\"];\n",
                    i, succ->target_state_id, succ->action_label);
            succ = succ->next;
        }
    }

    fprintf(f, "}\n");
    fclose(f);
}

void grg1_game_export_stats(const grg1_game_t* game, const char* filename) {
    if (!game || !filename) return;

    FILE* f = fopen(filename, "a");
    if (!f) return;

    int total_trans = 0;
    for (int i = 0; i < game->num_states; i++) {
        total_trans += game->successors[i].count;
    }

    fprintf(f, "%d,%d,%d,%d\n",
            game->num_variables,
            game->num_states,
            total_trans,
            grg1_game_count_actions(game));
    fclose(f);
}
