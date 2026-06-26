/**
 * grg1_types.h — Core data types for GR(1) reactive synthesis
 *
 * Defines the fundamental data structures used throughout the GR(1)
 * synthesis algorithm: states, valuations, game arenas, and transition
 * relations for two-player games between System and Environment.
 *
 * References:
 *   Piterman, Pnueli, Sa'ar. "Synthesis of Reactive(1) Designs." VMCAI 2006.
 *   Bloem, Jobstmann, Piterman, Pnueli, Sa'ar. "Synthesis of Reactive(1)
 *     Designs." Journal of Computer and System Sciences, 2012.
 *
 * Knowledge coverage:
 *   L1: State, valuation, transition, game arena definitions
 *   L2: Two-player game, system/environment distinction
 *   L3: Finite-state transition system as mathematical structure
 */

#ifndef GRG1_TYPES_H
#define GRG1_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------------------
 * L1: Variable domain — each variable has a finite domain of values
 * ---------------------------------------------------------------------------
 * In GR(1) synthesis, both system and environment variables take values
 * from finite domains. This is the foundational building block.
 * Reference: Sipser §1.1 — Finite Automata, states with finite alphabets.
 */

/** Maximum number of variables supported */
#define GRG1_MAX_VARIABLES 32

/** Maximum domain size per variable */
#define GRG1_MAX_DOMAIN_SIZE 16

/** Maximum number of states in explicit game graph */
#define GRG1_MAX_STATES_DEFAULT 65536

/** A single variable's domain definition */
typedef struct {
    int var_id;                        /**< Unique variable identifier */
    char name[32];                     /**< Human-readable variable name */
    int domain_size;                   /**< Number of values in domain (≥2) */
    bool is_system_controlled;         /**< true = System, false = Environment */
    int initial_value;                  /**< Default initial value for this var */
} grg1_variable_t;

/* ---------------------------------------------------------------------------
 * L1: Valuation — complete assignment of values to all variables
 * ---------------------------------------------------------------------------
 * A valuation is a snapshot of the system state at a point in time.
 * It assigns one value from each variable's domain to that variable.
 * The set of all valuations forms the state space.
 */

/** A complete valuation (state) in the game */
typedef struct {
    int values[GRG1_MAX_VARIABLES];   /**< values[i] = value of variable i */
    int num_variables;                /**< number of active variables */
} grg1_valuation_t;

/* ---------------------------------------------------------------------------
 * L1/L3: State in the game graph
 * ---------------------------------------------------------------------------
 * Each state in the game graph is a valuation annotated with metadata
 * for the synthesis algorithm: which player moves next, whether the
 * state is initial, and tracking for fixpoint computation.
 */

typedef enum {
    GRG1_PLAYER_SYSTEM = 0,           /**< System chooses next move */
    GRG1_PLAYER_ENVIRONMENT = 1       /**< Environment chooses next move */
} grg1_player_t;

/** A state in the two-player game arena */
typedef struct {
    int state_id;                      /**< Unique state identifier */
    grg1_valuation_t valuation;        /**< Variable valuations */
    grg1_player_t whose_turn;          /**< Which player moves from this state */
    bool is_initial;                   /**< Is this an initial state? */
    bool is_error;                     /**< Is this a violation state? */

    /* Synthesis algorithm bookkeeping */
    int rank;                          /**< Rank in justice ranking function */
    bool in_winning_region;            /**< Membership in winning region W */
    bool visited;                      /**< Visited flag for graph traversals */
} grg1_state_t;

/* ---------------------------------------------------------------------------
 * L1/L3: Transition relation — edges in the game graph
 * ---------------------------------------------------------------------------
 * Each transition represents a move by one player. The label can carry
 * additional information about the action taken (for strategy extraction).
 */

/** A labeled transition between two game states */
typedef struct {
    int from_state_id;                 /**< Source state */
    int to_state_id;                   /**< Target state */
    grg1_player_t by_player;           /**< Which player makes this move */
    int action_label;                  /**< Action identifier (for strategies) */
} grg1_transition_t;

/* ---------------------------------------------------------------------------
 * L3: Game Arena — the complete two-player game graph
 * ---------------------------------------------------------------------------
 * A game arena G = (S, S₀, Tₛ, Tₑ) where:
 *   S  = set of states (system + environment)
 *   S₀ = initial states
 *   Tₛ = system transitions ⊆ S × S
 *   Tₑ = environment transitions ⊆ S × S
 *
 * Reference: Grädel, Thomas, Wilke (Eds.). "Automata, Logics, and
 *   Infinite Games." LNCS 2500, Springer 2002, Chapter 2.
 */

/** Adjacency list node for sparse graph representation */
typedef struct grg1_adj_node_t {
    int target_state_id;
    int action_label;
    struct grg1_adj_node_t* next;
} grg1_adj_node_t;

/** Adjacency list for a single state */
typedef struct {
    grg1_adj_node_t* head;             /**< First successor */
    int count;                         /**< Number of successors */
} grg1_adj_list_t;

/** The complete two-player game arena */
typedef struct {
    /* State space */
    int num_states;                    /**< Total number of states */
    int capacity;                      /**< Allocated capacity */
    grg1_state_t* states;              /**< Array of all states */

    /* Variable definitions */
    int num_variables;                 /**< Number of variables */
    grg1_variable_t* variables;        /**< Array of variable definitions */

    /* Adjacency lists for transitions */
    grg1_adj_list_t* successors;       /**< successors[i] = outgoing from state i */
    grg1_adj_list_t* predecessors;     /**< predecessors[i] = incoming to state i */

    /* For each state, which player's turn (redundant but fast lookup) */
    grg1_player_t* whose_turn;         /**< whose_turn[i] = player for state i */

    /* Computed properties */
    int* env_successor_count;          /**< Per-state count of env successors */
    int* sys_successor_count;          /**< Per-state count of sys successors */

    /* Reachability cache (lazily computed) */
    bool* reachable_from_initial;      /**< Which states are reachable */
    bool reachability_computed;        /**< Has reachability been computed */
} grg1_game_t;

/* ---------------------------------------------------------------------------
 * L2: Game property types — classification of states and subgames
 * ---------------------------------------------------------------------------
 */

/** Result of realizability checking */
typedef enum {
    GRG1_REALIZABLE = 0,               /**< System has a winning strategy */
    GRG1_UNREALIZABLE = 1,             /**< Environment can force a violation */
    GRG1_UNKNOWN = 2,                  /**< Computation incomplete or timeout */
    GRG1_TRIVIALLY_REALIZABLE = 3,     /**< All initial states are winning */
    GRG1_CONDITIONALLY_REALIZABLE = 4  /**< Some initial states winning */
} grg1_realizability_t;

/* ---------------------------------------------------------------------------
 * L5: Fixpoint lattice element — for iterative computation
 * ---------------------------------------------------------------------------
 * The fixpoint computation operates on subsets of states (regions).
 * Each region is represented as a bit-vector for efficiency.
 * The lattice ordering is subset inclusion ⊆.
 */

/** Bit-vector representation of a state set (region) */
typedef struct {
    uint64_t* bits;                    /**< Bit array, one bit per state */
    int num_states;                    /**< Total number of states */
    int words_needed;                  /**< Number of 64-bit words */
} grg1_region_t;

/** A sequence of regions for monitoring fixpoint convergence */
typedef struct {
    grg1_region_t* regions;            /**< Array of region snapshots */
    int capacity;                      /**< Allocated capacity */
    int count;                         /**< Number of snapshots taken */
} grg1_fixpoint_trace_t;

/* ---------------------------------------------------------------------------
 * L5: Strategy representation
 * ---------------------------------------------------------------------------
 * A strategy for the system player is a function σ: S → S that maps
 * each system state to a successor state (memoryless/deterministic).
 * For environment, it maps to a set of possible moves.
 */

/** A single strategy entry (deterministic) */
typedef struct {
    int from_state_id;
    int to_state_id;
    int action_label;
} grg1_strategy_entry_t;

/** Memoryless strategy for the system */
typedef struct {
    int* move;                         /**< move[state_id] = next_state_id, -1 if undefined */
    int* action;                       /**< action[state_id] = action_label */
    int num_states;                    /**< Total states */
    bool is_complete;                  /**< Does strategy cover all winning states */
} grg1_strategy_t;

/* ---------------------------------------------------------------------------
 * L8: BDD node — symbolic representation
 * ---------------------------------------------------------------------------
 * Binary Decision Diagrams for symbolic state-space representation.
 * Based on Bryant (1986). Enables handling of very large state spaces.
 */

/** BDD variable index */
typedef int grg1_bdd_var_t;

/** BDD node types */
typedef enum {
    GRG1_BDD_TERMINAL_FALSE = 0,
    GRG1_BDD_TERMINAL_TRUE = 1,
    GRG1_BDD_NONTERMINAL = 2
} grg1_bdd_node_type_t;

/** A BDD node in a unique table */
typedef struct grg1_bdd_node_t {
    int id;                            /**< Unique node identifier */
    grg1_bdd_node_type_t type;         /**< Node type */
    grg1_bdd_var_t variable;           /**< Decision variable (for nonterminals) */
    struct grg1_bdd_node_t* low;       /**< Low child (variable = 0) */
    struct grg1_bdd_node_t* high;      /**< High child (variable = 1) */
    int ref_count;                     /**< Reference count for garbage collection */
    struct grg1_bdd_node_t* next;      /**< Hash table chain */
} grg1_bdd_node_t;

/** BDD manager for constructing and manipulating BDDs */
typedef struct {
    grg1_bdd_node_t** unique_table;    /**< Hash table for uniqueness */
    int table_size;                    /**< Size of hash table */
    int num_vars;                      /**< Number of BDD variables */
    grg1_bdd_node_t* true_node;        /**< Canonical TRUE terminal */
    grg1_bdd_node_t* false_node;       /**< Canonical FALSE terminal */
    int next_node_id;                  /**< Counter for node IDs */
} grg1_bdd_manager_t;

/* ---------------------------------------------------------------------------
 * Memory management — allocation and deallocation
 * -------------------------------------------------------------------------*/

/** Allocate and initialize an empty game arena */
grg1_game_t* grg1_game_alloc(int capacity);

/** Free all memory associated with a game arena */
void grg1_game_free(grg1_game_t* game);

/** Allocate and zero-initialize a valuation */
grg1_valuation_t* grg1_valuation_alloc(int num_variables);

/** Free a valuation */
void grg1_valuation_free(grg1_valuation_t* val);

/** Allocate a region (bit-vector for state set) */
grg1_region_t* grg1_region_alloc(int num_states);

/** Free a region */
void grg1_region_free(grg1_region_t* region);

/** Allocate a strategy structure */
grg1_strategy_t* grg1_strategy_alloc(int num_states);

/** Free a strategy */
void grg1_strategy_free(grg1_strategy_t* strategy);

/** Allocate a fixpoint trace */
grg1_fixpoint_trace_t* grg1_fixpoint_trace_alloc(int capacity);

/** Free a fixpoint trace */
void grg1_fixpoint_trace_free(grg1_fixpoint_trace_t* trace);

/** Allocate a BDD manager */
grg1_bdd_manager_t* grg1_bdd_manager_alloc(int num_vars, int table_size);

/** Free a BDD manager */
void grg1_bdd_manager_free(grg1_bdd_manager_t* mgr);

/* ---------------------------------------------------------------------------
 * Valuation operations
 * -------------------------------------------------------------------------*/

/** Deep copy a valuation */
grg1_valuation_t* grg1_valuation_copy(const grg1_valuation_t* src);

/** Check equality of two valuations */
bool grg1_valuation_equal(const grg1_valuation_t* a, const grg1_valuation_t* b);

/** Compute hash of a valuation */
uint32_t grg1_valuation_hash(const grg1_valuation_t* v);

/** Print a valuation */
void grg1_valuation_print(const grg1_valuation_t* v, const grg1_variable_t* vars);

/** Set a variable's value */
bool grg1_valuation_set(grg1_valuation_t* v, int var_index, int value,
                         const grg1_variable_t* vars);

/** Get a variable's value */
int grg1_valuation_get(const grg1_valuation_t* v, int var_index);

/* ---------------------------------------------------------------------------
 * Region (bit-vector for state set) operations
 * -------------------------------------------------------------------------*/

/** Check if state is in region */
bool grg1_region_contains(const grg1_region_t* region, int state_id);

/** Add state to region */
void grg1_region_add(grg1_region_t* region, int state_id);

/** Remove state from region */
void grg1_region_remove(grg1_region_t* region, int state_id);

/** Clear all bits (empty set) */
void grg1_region_clear(grg1_region_t* region);

/** Set all bits (full set) */
void grg1_region_fill(grg1_region_t* region);

/** Set union: R = A ∪ B */
void grg1_region_union(grg1_region_t* result,
                        const grg1_region_t* a, const grg1_region_t* b);

/** Set intersection: R = A ∩ B */
void grg1_region_intersect(grg1_region_t* result,
                            const grg1_region_t* a, const grg1_region_t* b);

/** Set difference: R = A \ B */
void grg1_region_difference(grg1_region_t* result,
                             const grg1_region_t* a, const grg1_region_t* b);

/** Set complement: R = S \ A */
void grg1_region_complement(grg1_region_t* result, const grg1_region_t* a);

/** Check if two regions are equal */
bool grg1_region_equal(const grg1_region_t* a, const grg1_region_t* b);

/** Check if A ⊆ B */
bool grg1_region_subset(const grg1_region_t* a, const grg1_region_t* b);

/** Check if region is empty */
bool grg1_region_is_empty(const grg1_region_t* region);

/** Count states in region */
int grg1_region_count(const grg1_region_t* region);

/** Copy region contents */
void grg1_region_copy(grg1_region_t* dst, const grg1_region_t* src);

/* ---------------------------------------------------------------------------
 * Strategy operations
 * -------------------------------------------------------------------------*/

/** Set strategy move for a state */
void grg1_strategy_set_move(grg1_strategy_t* s, int from_state,
                             int to_state, int action);

/** Get strategy move for a state */
int grg1_strategy_get_move(const grg1_strategy_t* s, int from_state);

/* ---------------------------------------------------------------------------
 * Fixpoint trace operations
 * -------------------------------------------------------------------------*/

/** Record a snapshot in the fixpoint trace */
bool grg1_fixpoint_trace_record(grg1_fixpoint_trace_t* trace,
                                 const grg1_region_t* region);

/* ---------------------------------------------------------------------------
 * BDD operations
 * -------------------------------------------------------------------------*/

/** Create a BDD variable node */
grg1_bdd_node_t* grg1_bdd_create_variable(grg1_bdd_manager_t* mgr,
                                            grg1_bdd_var_t var);

/** BDD conjunction (AND) */
grg1_bdd_node_t* grg1_bdd_and(grg1_bdd_manager_t* mgr,
                                grg1_bdd_node_t* u, grg1_bdd_node_t* v);

/** BDD disjunction (OR) */
grg1_bdd_node_t* grg1_bdd_or(grg1_bdd_manager_t* mgr,
                               grg1_bdd_node_t* u, grg1_bdd_node_t* v);

/** BDD exclusive OR */
grg1_bdd_node_t* grg1_bdd_xor(grg1_bdd_manager_t* mgr,
                                grg1_bdd_node_t* u, grg1_bdd_node_t* v);

/** BDD implication (→) */
grg1_bdd_node_t* grg1_bdd_imp(grg1_bdd_manager_t* mgr,
                                grg1_bdd_node_t* u, grg1_bdd_node_t* v);

/** BDD negation (NOT) */
grg1_bdd_node_t* grg1_bdd_not(grg1_bdd_manager_t* mgr,
                                grg1_bdd_node_t* u);

/** Existential quantification */
grg1_bdd_node_t* grg1_bdd_exists(grg1_bdd_manager_t* mgr,
                                   grg1_bdd_node_t* f, grg1_bdd_var_t var);

/** Universal quantification */
grg1_bdd_node_t* grg1_bdd_forall(grg1_bdd_manager_t* mgr,
                                   grg1_bdd_node_t* f, grg1_bdd_var_t var);

/** BDD cofactor/restrict */
grg1_bdd_node_t* grg1_bdd_restrict(grg1_bdd_manager_t* mgr,
                                     grg1_bdd_node_t* f,
                                     grg1_bdd_var_t var, bool value);

/** Count satisfying assignments */
int64_t grg1_bdd_satcount(grg1_bdd_manager_t* mgr, grg1_bdd_node_t* f);

/** Find one satisfying assignment */
bool grg1_bdd_any_sat(grg1_bdd_manager_t* mgr, grg1_bdd_node_t* f,
                       bool* assignment);

/** Convert BDD to explicit region */
void grg1_bdd_to_region(grg1_bdd_manager_t* mgr, grg1_bdd_node_t* f,
                         grg1_region_t* region);

/** Count BDD nodes */
int grg1_bdd_node_count(grg1_bdd_node_t* f);

/** Print BDD */
void grg1_bdd_print(grg1_bdd_manager_t* mgr, grg1_bdd_node_t* f);

#endif /* GRG1_TYPES_H */
