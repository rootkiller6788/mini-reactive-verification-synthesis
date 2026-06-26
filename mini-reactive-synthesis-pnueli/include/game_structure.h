/**
 * game_structure.h ¡ª Game Graphs and Game Solving Algorithms
 *
 * Defines the game-theoretic framework for reactive synthesis:
 * safety games, reachability games, B¨¹chi games, and parity games.
 * The controller synthesis problem reduces to computing winning
 * regions in a two-player game.
 *
 * In reactive synthesis, Player 0 is the environment (chooses inputs)
 * and Player 1 is the system (chooses outputs). The game is determined
 * (by Martin's theorem) and we compute the winning region for the system.
 *
 * Reference: Pnueli & Rosner (1989) FOCS
 *            Thomas (1995) "On the Synthesis of Strategies in Infinite Games"
 *            Zielonka (1998) "Infinite Games on Finitely Coloured Graphs"
 *
 * Knowledge Level: L2 Core Concepts, L3 Math Structures, L5 Algorithms
 */

#ifndef GAME_STRUCTURE_H
#define GAME_STRUCTURE_H

#include "reactive_types.h"

/* ====================================================================
 * Game Graph Types
 * ==================================================================== */

/** Player identifiers for two-player games */
typedef enum {
    PLAYER_ENV = 0,   /* Player 0: environment (chooses inputs) */
    PLAYER_SYS = 1    /* Player 1: system (chooses outputs) */
} player_t;

/** Winning condition type for infinite games */
typedef enum {
    WIN_SAFETY,       /* Stay within safe set forever */
    WIN_REACHABILITY, /* Eventually reach target set */
    WIN_BUCHI,        /* Visit accepting states infinitely often */
    WIN_COBUCHI,      /* Visit rejecting states only finitely often */
    WIN_PARITY,       /* Parity condition: max priority seen infinitely often is even */
    WIN_RABIN,        /* Rabin condition: pairs (E_i, F_i) */
    WIN_STREETT,      /* Streett condition: dual of Rabin */
    WIN_MULLER        /* Muller condition: set of states seen infinitely often in F */
} win_condition_t;

/** A game arena is a directed graph where vertices are partitioned
 *  between two players. This is the standard two-player game arena
 *  from the formal methods literature.
 *
 *  G = (V, V0, V1, E) where V0 are Player-0 vertices, V1 are Player-1. */
typedef struct {
    int32_t     num_vertices;   /* |V| ¡ª total number of vertices */
    int32_t     num_edges;      /* |E| ¡ª total number of edges */
    player_t   *owner;          /* owner[v] = which player controls vertex v */
    int32_t    *outdegree;      /* outdegree[v] = number of outgoing edges from v */
    int32_t    *edges;          /* flat adjacency: edges[id] = target vertex */
    int32_t    *edge_start;     /* edge_start[v] = first edge index for v */
    bool       *is_initial;     /* which vertices are initial */
} game_arena_t;

/* ====================================================================
 * Parity Game
 * ==================================================================== */

/** A parity game extends the game arena with a priority function
 *  c: V ¡ú {0, 1, ..., d} assigning each vertex a priority.
 *
 *  Player 1 wins a play if the maximum priority occurring infinitely
 *  often is even. Parity games are determined and solvable in NP ¡É co-NP.
 *
 *  Parity games are the canonical winning condition for reactive
 *  synthesis: every ¦Ø-regular specification can be reduced to a
 *  deterministic parity automaton, which yields a parity game. */
typedef struct {
    game_arena_t arena;        /* underlying game arena */
    int32_t     *priority;     /* priority[v] ¡Ê {0, ..., max_priority} */
    int32_t      max_priority; /* d = maximum priority value */
} parity_game_t;

/** Result of solving a parity game */
typedef struct {
    bool    *winning_region[2];  /* winning_region[p][v] = true if player p wins from v */
    int32_t *strategy;           /* strategy[v] = next vertex for winning player */
    int32_t  total_win_size[2];  /* number of winning vertices for each player */
} parity_game_solution_t;

/* ====================================================================
 * Safety Game
 * ==================================================================== */

/** A safety game: Player 1 (system) wants to keep the play within a
 *  designated safe set forever. The environment tries to force a
 *  violation. The winning region is the maximal set of states from
 *  which the system can guarantee safety.
 *
 *  Reference: GR(1) synthesis uses iterated safety game solutions. */
typedef struct {
    game_arena_t arena;      /* underlying game arena */
    bool        *safe_set;   /* safe[v] = true if vertex v is safe */
} safety_game_t;

/* ====================================================================
 * Reachability Game
 * ==================================================================== */

/** A reachability game: Player 1 (system) wants to eventually reach a
 *  target set. The environment tries to avoid it forever.
 *
 *  Reachability games are dual to safety games. */
typedef struct {
    game_arena_t arena;        /* underlying game arena */
    bool        *target_set;   /* target[v] = true if vertex v is in target */
} reachability_game_t;

/* ====================================================================
 * Core API: Game Arena
 * ==================================================================== */

/**
 * Create a new game arena with given number of vertices.
 * @param num_vertices  number of vertices in the arena
 * @return allocated game_arena_t, all vertices owned by PLAYER_ENV by default
 *
 * Time: O(|V|)  Space: O(|V|) */
game_arena_t *game_arena_create(int32_t num_vertices);

/**
 * Free a game arena and all associated memory.
 * Time: O(|V|) */
void game_arena_destroy(game_arena_t *arena);

/**
 * Add a directed edge from vertex `src` to vertex `dst`.
 * Self-loops are allowed.
 * Time: O(1) amortized */
void game_arena_add_edge(game_arena_t *arena, int32_t src, int32_t dst);

/**
 * Set the owner of a vertex.
 * Time: O(1) */
void game_arena_set_owner(game_arena_t *arena, int32_t vertex, player_t owner);

/**
 * Mark a vertex as initial.
 * Time: O(1) */
void game_arena_set_initial(game_arena_t *arena, int32_t vertex, bool is_init);

/**
 * Get the successors of a vertex as a (pointer, count) pair.
 * Time: O(1) */
void game_arena_get_successors(const game_arena_t *arena,
                                int32_t vertex,
                                int32_t **successors,
                                int32_t *count);

/**
 * Print the game arena in DOT format (for Graphviz).
 * Time: O(|V| + |E|) */
void game_arena_print_dot(const game_arena_t *arena);

/* ====================================================================
 * Core API: Parity Game Solving
 * ==================================================================== */

/**
 * Create a parity game.
 * @param num_vertices   number of vertices
 * @param max_priority   maximum priority d (priorities in [0, d])
 *
 * Time: O(|V|)  Space: O(|V|) */
parity_game_t *parity_game_create(int32_t num_vertices, int32_t max_priority);

/**
 * Free a parity game.
 * Time: O(|V|) */
void parity_game_destroy(parity_game_t *game);

/**
 * Set the priority of a vertex.
 * Time: O(1) */
void parity_game_set_priority(parity_game_t *game,
                               int32_t vertex, int32_t priority);

/**
 * Solve a parity game using Zielonka's recursive algorithm.
 *
 * This computes the winning regions and positional winning strategies
 * for both players. The algorithm runs in time O(d * |V|^{d/2}) in
 * the worst case, where d = max_priority.
 *
 * For common parity games arising from LTL synthesis, d is small
 * (typically 0-4), making the algorithm practical.
 *
 * Reference: Zielonka (1998) "Infinite Games on Finitely Coloured Graphs
 *            with Applications to Automata on Infinite Trees"
 *
 * @return allocated parity_game_solution_t, caller owns memory
 * Time: O(d * |V|^{d/2})  Space: O(d * |V|) */
parity_game_solution_t *parity_game_solve_zielonka(const parity_game_t *game);

/**
 * Solve a parity game using Jurdzi¨½ski's small progress measures.
 * Often faster than Zielonka for games with many priorities.
 *
 * Reference: Jurdzi¨½ski (2000) "Small Progress Measures for Solving
 *            Parity Games", STACS 2000
 *
 * @return allocated parity_game_solution_t
 * Time: O(d * |E| * (|V| / d)^{d/2}) */
parity_game_solution_t *parity_game_solve_jurdzinski(const parity_game_t *game);

/**
 * Free a parity game solution.
 * Time: O(|V|) */
void parity_game_solution_destroy(parity_game_solution_t *sol);

/**
 * Extract a Mealy machine (reactive module) from the winning region
 * of the system player in a parity game.
 *
 * The game must arise from a synthesis problem: system vertices
 * correspond to choosing outputs, environment vertices to choosing inputs.
 *
 * @param game    the solved parity game
 * @param sol     the solution (winning regions + strategies)
 * @param num_inputs   number of input signals for the reactive module
 * @param num_outputs  number of output signals for the reactive module
 * @return allocated reactive_module_t representing the winning strategy,
 *         or NULL if the system does not win from any initial state
 *
 * Time: O(|V| + |E|) */
reactive_module_t *parity_game_extract_strategy(const parity_game_t *game,
                                                  const parity_game_solution_t *sol,
                                                  int32_t num_inputs,
                                                  int32_t num_outputs);

/* ====================================================================
 * Core API: Safety Game Solving
 * ==================================================================== */

/**
 * Create a safety game.
 * Time: O(|V|)  Space: O(|V|) */
safety_game_t *safety_game_create(int32_t num_vertices);

/**
 * Free a safety game.
 * Time: O(|V|) */
void safety_game_destroy(safety_game_t *game);

/**
 * Mark a vertex as safe/unsafe.
 * Time: O(1) */
void safety_game_set_safe(safety_game_t *game, int32_t vertex, bool is_safe);

/**
 * Solve a safety game by computing the maximal winning region for the
 * system (Player 1) using the classic greatest-fixed-point algorithm.
 *
 * Algorithm: start with W = safe_set, iteratively remove vertices where
 * the environment can force an exit from W, until reaching a fixed point.
 *
 *   W(0) = S (all safe states)
 *   W(i+1) = W(i) ¡É {v | owner(v)=sys ¡ú ? u ¡Ê W(i): (v,u) ¡Ê E
 *                        ¡Ä owner(v)=env ¡ú ? u: (v,u) ¡Ê E ¡ú u ¡Ê W(i)}
 *
 * @return bool array (size = num_vertices), W[v] = true if system wins
 *         Caller must free the returned array.
 *
 * Time: O(|V| * |E|) = O(|V|^2) worst case
 * Reference: Pnueli, Piterman, Sa'ar (2006) for GR(1) */
bool *safety_game_solve(const safety_game_t *game);

/**
 * Extract a positional winning strategy for the system from the
 * winning region of a solved safety game.
 *
 * @return array strategy[v] = chosen successor, or -1 if not winning.
 *         Caller must free.
 *
 * Time: O(|V| + |E|) */
int32_t *safety_game_extract_strategy(const safety_game_t *game,
                                       const bool *winning_region);

/* ====================================================================
 * Core API: Reachability Game Solving
 * ==================================================================== */

/**
 * Create a reachability game.
 * Time: O(|V|)  Space: O(|V|) */
reachability_game_t *reachability_game_create(int32_t num_vertices);

/**
 * Free a reachability game.
 * Time: O(|V|) */
void reachability_game_destroy(reachability_game_t *game);

/**
 * Mark a vertex as target/non-target.
 * Time: O(1) */
void reachability_game_set_target(reachability_game_t *game,
                                    int32_t vertex, bool is_target);

/**
 * Solve a reachability game by computing the winning region for the
 * system (Player 1) using the least-fixed-point algorithm (attractor
 * computation).
 *
 * Algorithm: compute the attractor of the target set for Player 1,
 * iterating until fixed point.
 *
 *   Attr(0)(T) = T
 *   Attr(i+1)(T) = Attr(i)(T) ¡È {v | owner(v)=sys ¡Ä ?u¡ÊAttr(i)(T):(v,u)¡ÊE}
 *                             ¡È {v | owner(v)=env ¡Ä ?u:(v,u)¡ÊE ¡ú u¡ÊAttr(i)(T)}
 *
 * @return bool array (size = num_vertices), R[v] = true if system wins.
 *         Caller must free.
 *
 * Time: O(|V| + |E|)
 * Reference: Thomas (1995) */
bool *reachability_game_solve(const reachability_game_t *game);

#endif /* GAME_STRUCTURE_H */
