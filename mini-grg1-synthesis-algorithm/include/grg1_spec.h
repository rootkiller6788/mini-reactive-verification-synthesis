/**
 * grg1_spec.h — GR(1) specification representation and operations
 *
 * A GR(1) formula has the canonical structure:
 *   (θₑ ∧ □ρₑ ∧ ∧_{i∈I} □◇Jₑᵢ) → (θₛ ∧ □ρₛ ∧ ∧_{j∈J} □◇Jₛⱼ)
 *
 * Where:
 *   θₑ, θₛ : Initial conditions (state predicates)
 *   ρₑ, ρₛ : Safety (transition) conditions
 *   Jₑᵢ    : Environment justice (liveness) assumptions
 *   Jₛⱼ    : System justice (liveness) guarantees
 *
 * The synthesis problem asks: does there exist a system strategy such that
 * for all environment behaviors satisfying the assumptions, the guarantees
 * hold?
 *
 * References:
 *   Piterman, Pnueli, Sa'ar. "Synthesis of Reactive(1) Designs." VMCAI 2006.
 *   Bloem et al. "Synthesis of Reactive(1) Designs." JCSS 2012.
 *   Pnueli, Rosner. "On the Synthesis of a Reactive Module." POPL 1989.
 *
 * Knowledge coverage:
 *   L1: GR(1) formula definition, justice, safety, initial conditions
 *   L2: Realizability, environment assumptions, system guarantees
 *   L3: Predicate logic over finite domains, temporal logic LTL fragment
 *   L6: GR(1) as a canonical reactive synthesis specification format
 */

#ifndef GRG1_SPEC_H
#define GRG1_SPEC_H

#include "grg1_types.h"

/* ---------------------------------------------------------------------------
 * L1: Predicate — a boolean function over valuations
 * ---------------------------------------------------------------------------
 * In GR(1) synthesis, all conditions (initial, safety, justice) are
 * state predicates — boolean functions over the valuation space.
 * We represent them as function pointers for explicit-state evaluation
 * and as BDDs for symbolic computation.
 */

/** Forward declaration for symbolic representation */
typedef struct grg1_predicate_t grg1_predicate_t;

/**
 * A state predicate: maps a valuation to {true, false}.
 * In the explicit setting, this is a C function pointer.
 * In the symbolic setting, this is a BDD.
 */
typedef bool (*grg1_state_predicate_fn)(const grg1_valuation_t* v,
                                         const grg1_variable_t* vars,
                                         int num_vars);

/**
 * A transition predicate: maps a pair of valuations (pre, post) to
 * {true, false}. Used for safety conditions that constrain allowed
 * transitions.
 */
typedef bool (*grg1_transition_predicate_fn)(const grg1_valuation_t* pre,
                                              const grg1_valuation_t* post,
                                              const grg1_variable_t* vars,
                                              int num_vars);

/* ---------------------------------------------------------------------------
 * L1: GR(1) specification structure
 * ---------------------------------------------------------------------------
 * Encodes the full φ = (Aₑ → Gₛ) specification with all components.
 *
 * The frame □ operator is modeled as: in every step, the transition
 * must satisfy the safety condition.
 * The eventually ◇ operator (justice) is modeled as: infinitely often,
 * the state must satisfy the justice condition.
 */

/** Maximum number of justice assumptions/guarantees */
#define GRG1_MAX_JUSTICE 16

/** A justice (liveness) condition: □◇p — infinitely often p holds */
typedef struct {
    char name[64];                       /**< Human-readable name */
    grg1_state_predicate_fn predicate;   /**< State predicate for this justice */
    int justice_index;                   /**< Index in the justice array */
    bool is_assumption;                  /**< true=env assumption, false=sys guarantee */
} grg1_justice_t;

/** Complete GR(1) specification */
typedef struct {
    /* Environment side (assumptions / antecedent) */

    /** θₑ: Environment initial condition */
    char env_init_name[64];
    grg1_state_predicate_fn env_init;

    /** ρₑ: Environment safety (transition) condition */
    char env_safety_name[64];
    grg1_transition_predicate_fn env_safety;

    /** Jₑᵢ: Environment justice (liveness) assumptions */
    int num_env_justice;
    grg1_justice_t env_justice[GRG1_MAX_JUSTICE];

    /* System side (guarantees / consequent) */

    /** θₛ: System initial condition */
    char sys_init_name[64];
    grg1_state_predicate_fn sys_init;

    /** ρₛ: System safety (transition) condition */
    char sys_safety_name[64];
    grg1_transition_predicate_fn sys_safety;

    /** Jₛⱼ: System justice (liveness) guarantees */
    int num_sys_justice;
    grg1_justice_t sys_justice[GRG1_MAX_JUSTICE];

    /* Metadata */
    char specification_name[128];        /**< Human-readable identifier */
    int num_variables;                   /**< Number of variables */
    grg1_variable_t* variables;          /**< Variable definitions */
} grg1_spec_t;

/* ---------------------------------------------------------------------------
 * L2: Specification validation and well-formedness
 * ---------------------------------------------------------------------------
 * A GR(1) specification must satisfy well-formedness conditions:
 *   - No dead variables (all variables appear in some predicate)
 *   - No contradictory justice conditions
 *   - Safety predicates are defined over correct variable sets
 */

/** Validation result codes */
typedef enum {
    GRG1_SPEC_VALID = 0,
    GRG1_SPEC_ERR_NO_VARIABLES = 1,
    GRG1_SPEC_ERR_NO_INIT = 2,
    GRG1_SPEC_ERR_EMPTY_JUSTICE = 3,
    GRG1_SPEC_ERR_CONTRADICTORY_JUSTICE = 4,
    GRG1_SPEC_ERR_INVALID_SAFETY = 5,
    GRG1_SPEC_ERR_VARIABLE_MISMATCH = 6,
    GRG1_SPEC_ERR_MISSING_SAFETY = 7
} grg1_spec_validation_t;

/* ---------------------------------------------------------------------------
 * L3: Duality and normal forms
 * ---------------------------------------------------------------------------
 * The GR(1) formula can be transformed into equivalent forms:
 *   - Negation normal form (pushing negations inward)
 *   - Disjunctive normal form for the antecedent
 *   - Conjunctive normal form for the consequent
 */

/** Normal form types for GR(1) specification */
typedef enum {
    GRG1_NF_IMPLICATION = 0,      /**< Standard A → G implication form */
    GRG1_NF_DISJUNCTIVE = 1,      /**< ¬A ∨ G disjunctive form */
    GRG1_NF_CONJUNCTIVE = 2,      /**< CNF: conjunction of component properties */
    GRG1_NF_GAME_FORM = 3         /**< Converted to game solving problem */
} grg1_normal_form_t;

/* ---------------------------------------------------------------------------
 * L5: Specification construction and manipulation API
 * ---------------------------------------------------------------------------
 */

/**
 * Create an empty GR(1) specification.
 *
 * @param name  Human-readable name for the specification
 * @param num_vars  Number of variables in the system
 * @param vars  Array of variable definitions (copied internally)
 * @return  Newly allocated specification, caller must grg1_spec_free()
 *
 * Complexity: O(num_vars)
 * Theorem: GR(1) is syntactically safe (Piterman et al., 2006 §3.1)
 */
grg1_spec_t* grg1_spec_create(const char* name, int num_vars,
                               const grg1_variable_t* vars);

/**
 * Free a GR(1) specification and all associated memory.
 *
 * Complexity: O(num_vars + num_justice)
 */
void grg1_spec_free(grg1_spec_t* spec);

/**
 * Set the environment initial condition predicate.
 *
 * @param spec  The specification to modify
 * @param name  Descriptive name
 * @param pred  Predicate function (must not be NULL)
 *
 * The initial condition θₑ restricts the set of valid initial states.
 * In realizability checking, the environment may start in any state
 * satisfying θₑ ∧ θₛ.
 *
 * Complexity: O(1)
 * Reference: Pnueli (1977) — initial condition in temporal logic
 */
void grg1_spec_set_env_init(grg1_spec_t* spec, const char* name,
                             grg1_state_predicate_fn pred);

/**
 * Set the system initial condition predicate.
 *
 * Complexity: O(1)
 * Reference: Piterman et al. (2006) §2.1 — initial condition
 */
void grg1_spec_set_sys_init(grg1_spec_t* spec, const char* name,
                             grg1_state_predicate_fn pred);

/**
 * Set the environment safety (transition) condition.
 *
 * The safety condition ρₑ(A, A') constrains valid environment transitions.
 * In game-theoretic terms, it defines which environment moves are legal.
 * Violations of ρₑ mean the environment has broken its assumptions.
 *
 * Complexity: O(1)
 * Reference: Alpern & Schneider (1985) — safety-liveness decomposition
 */
void grg1_spec_set_env_safety(grg1_spec_t* spec, const char* name,
                               grg1_transition_predicate_fn pred);

/**
 * Set the system safety (transition) condition.
 *
 * Complexity: O(1)
 */
void grg1_spec_set_sys_safety(grg1_spec_t* spec, const char* name,
                               grg1_transition_predicate_fn pred);

/**
 * Add an environment justice (liveness) assumption.
 *
 * The justice condition □◇p means "infinitely often p holds."
 * In the algorithm, this is handled by ranking functions that ensure
 * progress toward satisfaction.
 *
 * @param spec  The specification to modify
 * @param name  Descriptive name of this justice
 * @param pred  State predicate (true when justice is satisfied)
 * @return  Index of the added justice, or -1 if max reached
 *
 * Complexity: O(1)
 * Reference: Francez (1986) — justice/fairness in concurrency
 *            Piterman et al. (2006) §3.3 — justice handling
 */
int grg1_spec_add_env_justice(grg1_spec_t* spec, const char* name,
                               grg1_state_predicate_fn pred);

/**
 * Add a system justice (liveness) guarantee.
 *
 * @return  Index of the added justice, or -1 if max reached
 *
 * Complexity: O(1)
 */
int grg1_spec_add_sys_justice(grg1_spec_t* spec, const char* name,
                               grg1_state_predicate_fn pred);

/**
 * Validate a GR(1) specification for well-formedness.
 *
 * Checks:
 *   1. At least one variable defined
 *   2. Both initial conditions set (non-NULL)
 *   3. Both safety conditions set (non-NULL)
 *   4. At least one system justice guarantee
 *   5. No contradictory predicates detectable at spec level
 *
 * @return  Validation code (GRG1_SPEC_VALID on success)
 *
 * Complexity: O(num_justice * num_vars)
 */
grg1_spec_validation_t grg1_spec_validate(const grg1_spec_t* spec);

/**
 * Check if a state satisfies all environment assumptions up to
 * the initial condition and safety (used for game construction).
 *
 * @param state  The state to check
 * @param spec  The specification
 * @return  true if the state is admissible under environment assumptions
 *
 * Complexity: O(num_env_justice)
 */
bool grg1_spec_env_admissible(const grg1_state_t* state,
                               const grg1_spec_t* spec);

/**
 * Check if a transition satisfies the system safety condition.
 *
 * @param from  Source state
 * @param to  Target state
 * @param spec  The specification
 * @return  true if the transition is safe for the system
 *
 * Complexity: O(1) if predicate is constant-time
 */
bool grg1_spec_transition_safe(const grg1_state_t* from,
                                const grg1_state_t* to,
                                const grg1_spec_t* spec);

/**
 * Convert specification to disjunctive normal form (¬A ∨ G).
 * Returns the same spec pointer (in-place conversion marker).
 *
 * This normalization is used as a preprocessing step before
 * constructing the game graph, as it separates assumptions from
 * guarantees more clearly for game-solving algorithms.
 *
 * Complexity: O(1) — only sets a flag
 * Reference: Piterman et al. (2006) §4 — game construction
 */
void grg1_spec_to_normal_form(grg1_spec_t* spec, grg1_normal_form_t nf);

/**
 * Print a human-readable representation of the specification.
 *
 * Complexity: O(num_justice * formula_size)
 */
void grg1_spec_print(const grg1_spec_t* spec);

/**
 * Count the total number of predicates (initial + safety + justice)
 * in the specification. Useful for complexity estimation.
 *
 * @return  Total predicate count
 *
 * Complexity: O(1)
 */
int grg1_spec_predicate_count(const grg1_spec_t* spec);

/**
 * Estimate the number of states in the explicit game graph for this
 * specification, based on the product of variable domain sizes.
 *
 * @return  State count estimate (may overflow for large domains)
 *
 * Complexity: O(num_vars)
 * Theorem: The state space for n variables with domains Dᵢ has size Π|Dᵢ|
 */
int64_t grg1_spec_estimate_state_space(const grg1_spec_t* spec);

/**
 * Check if the specification is purely a safety specification
 * (i.e., has no justice/liveness conditions). Safety-only
 * specifications can be solved more efficiently using simple
 * reachability games.
 *
 * @return  true if num_env_justice == 0 && num_sys_justice == 0
 *
 * Complexity: O(1)
 * Reference: Sistla (1994) — safety games are in PTIME
 */
bool grg1_spec_is_pure_safety(const grg1_spec_t* spec);

/**
 * Check if the specification uses only environment justice
 * (the system only has safety guarantees). This is a simpler
 * form of GR(1) often encountered in practice.
 *
 * @return  true if sys has no justice, env may have justice
 *
 * Complexity: O(1)
 */
bool grg1_spec_is_env_liveness_only(const grg1_spec_t* spec);

/**
 * Create a specification for a traffic light controller.
 */
grg1_spec_t* grg1_spec_create_traffic_light_example(void);

/**
 * Create a simple mutual exclusion arbiter specification.
 */
grg1_spec_t* grg1_spec_create_mutex_example(void);

#endif /* GRG1_SPEC_H */
