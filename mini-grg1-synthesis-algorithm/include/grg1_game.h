/**
 * grg1_game.h — Game graph construction and manipulation
 *
 * Constructs a two-player game arena from a GR(1) specification.
 * The game graph is the explicit-state representation where:
 *   - States encode valuations of all variables
 *   - Transitions encode legally allowed moves by each player
 *   - The game alternates between System and Environment moves
 *
 * The construction follows the reduction from GR(1) synthesis to
 * solving a Streett game, then to solving a chain of Büchi games
 * via the ranking construction.
 *
 * References:
 *   Thomas (1995). "On the Synthesis of Strategies in Infinite Games." STACS.
 *   Piterman, Pnueli, Sa'ar (2006). VMCAI.
 *   Grädel, Thomas, Wilke (2002). "Automata, Logics, and Infinite Games." LNCS 2500.
 *
 * Knowledge coverage:
 *   L1: Game graph, successors, predecessors, controllability
 *   L2: Two-player determinacy, Büchi/Streett winning conditions
 *   L3: Graph traversal, strongly connected components
 *   L4: Determinacy theorem for finite-state games
 *   L5: Game construction from GR(1) specification
 */

#ifndef GRG1_GAME_H
#define GRG1_GAME_H

#include "grg1_types.h"
#include "grg1_spec.h"

/* ---------------------------------------------------------------------------
 * L1/L5: Game construction from specification
 * ---------------------------------------------------------------------------
 */

/**
 * Construct a game arena from a GR(1) specification.
 *
 * This is the core reduction: given a GR(1) formula φ = (Aₑ → Gₛ),
 * build a two-player game G = (S, S₀, Tₛ, Tₑ) such that:
 *    φ is realizable ⇔ System wins G from all states in S₀.
 *
 * The construction enumerates all valuations as explicit states.
 * For each state, it computes legal successor states for each player
 * based on the safety conditions ρₑ and ρₛ.
 *
 * @param spec  A fully-initialized, validated GR(1) specification
 * @return  Game arena, or NULL if state space exceeds capacity
 *
 * Complexity: O(|S| × |V| × (|Dom(v)|)_{avg}) in worst case
 * Reference: Piterman et al. (2006) §4 — Game Construction
 * Theorem: The constructed game faithfully represents the GR(1)
 *          specification (soundness and completeness, Thm 4.1)
 */
grg1_game_t* grg1_game_from_spec(const grg1_spec_t* spec);

/**
 * Construct a game arena from a specification with a given state capacity.
 * Use when the full state space is too large but a subset is needed.
 *
 * @param spec  The GR(1) specification
 * @param max_states  Maximum number of states to construct
 * @return  Game arena with at most max_states states
 *
 * Complexity: O(max_states × |V| × avg_successors)
 */
grg1_game_t* grg1_game_from_spec_bounded(const grg1_spec_t* spec,
                                          int max_states);

/* ---------------------------------------------------------------------------
 * L1: State and transition operations
 * ---------------------------------------------------------------------------
 */

/**
 * Add a state to the game arena.
 *
 * @param game  The game arena
 * @param valuation  Complete valuation for the new state
 * @param whose_turn  Which player moves from this state
 * @param is_initial  Whether this is an initial state
 * @return  State ID of the new or existing state
 *
 * Complexity: O(1) amortized
 */
int grg1_game_add_state(grg1_game_t* game, const grg1_valuation_t* valuation,
                         grg1_player_t whose_turn, bool is_initial);

/**
 * Add a transition between two states in the game.
 * The transition is added to the source's successor list.
 *
 * @param game  The game arena
 * @param from_state  Source state ID
 * @param to_state  Target state ID
 * @param by_player  Which player makes this move
 * @param action  Action label (for strategy extraction)
 *
 * Complexity: O(1)
 */
void grg1_game_add_transition(grg1_game_t* game, int from_state,
                               int to_state, grg1_player_t by_player,
                               int action);

/**
 * Find a state by its valuation (linear search, for small games).
 *
 * @param game  The game arena
 * @param valuation  The valuation to search for
 * @return  State ID, or -1 if not found
 *
 * Complexity: O(|S|)
 */
int grg1_game_find_state(const grg1_game_t* game,
                          const grg1_valuation_t* valuation);

/**
 * Get the number of successor states for a given state.
 *
 * Complexity: O(1) — uses cached count
 */
int grg1_game_successor_count(const grg1_game_t* game, int state_id);

/**
 * Get the number of predecessor states for a given state.
 *
 * Complexity: O(1) — uses cached count
 */
int grg1_game_predecessor_count(const grg1_game_t* game, int state_id);

/**
 * Check if a transition exists between two states.
 *
 * @return  true if from_state → to_state is a valid transition
 *
 * Complexity: O(outdegree(from_state))
 */
bool grg1_game_has_transition(const grg1_game_t* game,
                               int from_state, int to_state);

/* ---------------------------------------------------------------------------
 * L3: Graph analysis — reachability and structure
 * ---------------------------------------------------------------------------
 */

/**
 * Compute the set of states reachable from initial states via BFS.
 *
 * After this call, game->reachable_from_initial[] is populated and
 * game->reachability_computed is set to true.
 *
 * Complexity: O(|S| + |T|)
 * Reference: Cormen et al. §22.2 — Breadth-First Search
 */
void grg1_game_compute_reachable(grg1_game_t* game);

/**
 * Compute strongly connected components (SCCs) of the game graph.
 * Uses Tarjan's algorithm.
 *
 * @param game  The game arena
 * @param scc_id  Output array of size game->num_states, scc_id[i] = component ID of state i
 * @param num_sccs  Output: number of SCCs found
 *
 * Complexity: O(|S| + |T|)
 * Reference: Tarjan (1972) — Depth-first search and linear graph algorithms
 */
void grg1_game_compute_scc(const grg1_game_t* game, int* scc_id,
                            int* num_sccs);

/**
 * Check if the game is alternating (system and environment turns alternate).
 * In a properly constructed GR(1) game, turns should alternate.
 *
 * Complexity: O(|S|)
 * Reference: Grädel et al. (2002) §2.3 — Alternating games
 */
bool grg1_game_is_alternating(const grg1_game_t* game);

/**
 * Check if the game is deadlock-free for the system player.
 * A deadlock is a system state with no outgoing transitions.
 *
 * @return  true if every system state has at least one successor
 *
 * Complexity: O(|S|)
 */
bool grg1_game_is_deadlock_free(const grg1_game_t* game);

/* ---------------------------------------------------------------------------
 * L5: Controllable predecessor operators (CPre)
 * ---------------------------------------------------------------------------
 * These are the fundamental operators for game solving.
 *
 * CPreₛ(Z): set of states from which the System can force a visit to Z
 *           in one step, regardless of the Environment's choice.
 *
 * CPreₑ(Z): set of states from which the Environment can force a visit
 *           to Z in one step.
 */

/**
 * Compute the system controllable predecessor of a region.
 *
 * CPreₛ(Z) = { s ∈ Sₛ | ∃s'. (s,s') ∈ Tₛ ∧ s' ∈ Z }
 *          ∪ { s ∈ Sₑ | ∀s'. (s,s') ∈ Tₑ → s' ∈ Z }
 *
 * In system states: exists a system move to Z.
 * In environment states: ALL environment moves go to Z.
 *
 * @param game  The game arena
 * @param Z  The target region (set of states)
 * @param result  Output: CPreₛ(Z), allocated by caller
 *
 * Complexity: O(|S| + |T|)
 * Reference: Thomas (1995) — controllable predecessor in infinite games
 *            Bloem et al. (2012) §3.2 — CPre operator for GR(1)
 */
void grg1_game_cpre_sys(const grg1_game_t* game, const grg1_region_t* Z,
                         grg1_region_t* result);

/**
 * Compute the environment controllable predecessor of a region.
 *
 * CPreₑ(Z) = { s ∈ Sₑ | ∃s'. (s,s') ∈ Tₑ ∧ s' ∈ Z }
 *          ∪ { s ∈ Sₛ | ∀s'. (s,s') ∈ Tₛ → s' ∈ Z }
 *
 * @param game  The game arena
 * @param Z  The target region
 * @param result  Output: CPreₑ(Z), allocated by caller
 *
 * Complexity: O(|S| + |T|)
 */
void grg1_game_cpre_env(const grg1_game_t* game, const grg1_region_t* Z,
                         grg1_region_t* result);

/**
 * Compute the system attractor to a target region.
 *
 * Attrₛ(Z) = μX. (Z ∪ CPreₛ(X))
 *
 * This is the least fixpoint: starting from Z, repeatedly add states
 * from which the system can force a visit to the current set.
 * Reaches the fixpoint when no new states are added.
 *
 * @param game  The game arena
 * @param Z  The target region
 * @param result  Output: Attrₛ(Z), allocated by caller
 * @param iterations  Output: number of iterations until convergence
 *
 * Complexity: O(|S| × |T|) worst case, O(|S| + |T|) typical
 * Reference: Zielonka (1998) — attractor in infinite games
 */
void grg1_game_attractor_sys(const grg1_game_t* game, const grg1_region_t* Z,
                              grg1_region_t* result, int* iterations);

/**
 * Compute the environment attractor to a target region.
 *
 * Attrₑ(Z) = μX. (Z ∪ CPreₑ(X))
 *
 * Complexity: O(|S| × |T|) worst case
 */
void grg1_game_attractor_env(const grg1_game_t* game, const grg1_region_t* Z,
                              grg1_region_t* result, int* iterations);

/* ---------------------------------------------------------------------------
 * L3: Game simplification and decomposition
 * ---------------------------------------------------------------------------
 */

/**
 * Prune states that are not reachable from any initial state.
 * This reduces the game graph before solving.
 *
 * @param game  Game arena modified in-place
 *
 * Complexity: O(|S| + |T|)
 */
void grg1_game_prune_unreachable(grg1_game_t* game);

/**
 * Remove all states in a given region from the game.
 * Transitions to/from removed states are also removed.
 *
 * @param game  Game arena modified in-place
 * @param to_remove  Region of states to remove
 *
 * Complexity: O(|S| + |T|)
 */
void grg1_game_remove_region(grg1_game_t* game, const grg1_region_t* to_remove);

/**
 * Compute the subgame induced by a set of states.
 *
 * @param game  Original game
 * @param region  Set of states to keep
 * @return  New game containing only states in region and transitions between them
 *
 * Complexity: O(|S| + |T|)
 */
grg1_game_t* grg1_game_induced_subgame(const grg1_game_t* game,
                                        const grg1_region_t* region);

/**
 * Count the number of distinct actions (transition labels) in the game.
 * Useful for analyzing strategy complexity.
 *
 * Complexity: O(|T|)
 */
int grg1_game_count_actions(const grg1_game_t* game);

/* ---------------------------------------------------------------------------
 * L5: Game display and debugging
 * ---------------------------------------------------------------------------
 */

/**
 * Print a summary of the game arena to stdout.
 *
 * Complexity: O(1)
 */
void grg1_game_print_summary(const grg1_game_t* game);

/**
 * Export the game arena in DOT format for visualization with Graphviz.
 *
 * @param game  The game arena
 * @param filename  Output file path
 *
 * Complexity: O(|S| + |T|)
 */
void grg1_game_export_dot(const grg1_game_t* game, const char* filename);

/**
 * Export game statistics as a CSV row.
 *
 * @param game  The game arena
 * @param filename  Output file path (appends if exists)
 *
 * Complexity: O(|S|)
 */
void grg1_game_export_stats(const grg1_game_t* game, const char* filename);

#endif /* GRG1_GAME_H */
