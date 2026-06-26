/**
 * synthesis.h ¡ª Reactive Synthesis Engine API
 *
 * This is the main entry point for reactive synthesis. Given an LTL
 * specification in the form (assumption ¡ú guarantee), the engine:
 *
 *   1. Translates LTL formulas to deterministic automata
 *   2. Constructs a synthesis game (product of specification automaton
 *      with the input/output choice structure)
 *   3. Solves the parity game to determine realizability
 *   4. If realizable, extracts a winning strategy as a Mealy machine
 *
 * Reference: Pnueli & Rosner (1989) "On the Synthesis of a Reactive Module"
 *            Pnueli & Rosner (1989) "On the Synthesis of an Asynchronous
 *            Reactive Module", ICALP 1989
 *
 * Knowledge Level: L4 Fundamental Laws, L5 Algorithms, L6 Canonical Problems
 */

#ifndef SYNTHESIS_H
#define SYNTHESIS_H

#include "reactive_types.h"
#include "game_structure.h"

/* ====================================================================
 * Synthesis Result Types
 * ==================================================================== */

/** Status of a synthesis attempt */
typedef enum {
    SYNTH_REALIZABLE,          /* specification is realizable, strategy found */
    SYNTH_UNREALIZABLE,        /* specification is not realizable */
    SYNTH_TIMEOUT,             /* synthesis timed out */
    SYNTH_ERROR,               /* an error occurred during synthesis */
    SYNTH_UNKNOWN              /* could not determine realizability */
} synthesis_status_t;

/** Complete result of a synthesis attempt */
typedef struct {
    synthesis_status_t status;         /* overall result */
    reactive_module_t *module;         /* synthesized module (NULL if unrealizable) */
    ltl_formula_t     *counter_strategy; /* environment counter-strategy for unrealizable */
    char               message[256];   /* human-readable status message */
    double             time_seconds;   /* wall-clock time for synthesis */
    int32_t            num_states;     /* number of states in result (0 if none) */
} synthesis_result_t;

/* ====================================================================
 * Synthesis Configuration
 * ==================================================================== */

/** Algorithm selection for the synthesis pipeline */
typedef enum {
    ALGO_SAFRA_DETERMINIZE,    /* Safra's determinization (classic, 2EXPTIME) */
    ALGO_PITLERMAN_DETERMINIZE, /* Piterman's improvement of Safra */
    ALGO_BOUNDED_SYNTHESIS,    /* Bounded synthesis (Finkbeiner & Schewe) */
    ALGO_GR1,                  /* GR(1) synthesis (polynomial time) */
    ALGO_AUTO                  /* auto-select best algorithm */
} synthesis_algorithm_t;

/** Configuration options for the synthesis engine */
typedef struct {
    synthesis_algorithm_t algorithm; /* which synthesis algorithm to use */
    int32_t  bound_k;               /* bound for bounded synthesis (0=auto) */
    double   timeout_seconds;       /* timeout (0=no timeout) */
    bool     verbose;               /* whether to print progress */
    bool     minimize_result;       /* whether to minimize the resulting FSM */
    int32_t  max_states;            /* maximum states to consider (0=unlimited) */
} synthesis_config_t;

/* ====================================================================
 * Main Synthesis API
 * ==================================================================== */

/**
 * Check realizability of an LTL specification.
 *
 * Given an LTL specification ¦Õ = (assumption ¡ú guarantee) over
 * input variables I and output variables O, determine whether
 * there exists a finite-state reactive module M such that for
 * all input sequences satisfying the assumption, M's outputs
 * satisfy the guarantee.
 *
 * @param spec          the synthesis specification (assumption + guarantee)
 * @param config        synthesis configuration (NULL = use defaults)
 * @return synthesis_result_t with status and (if realizable) the module.
 *         Caller owns the result and must call synthesis_result_destroy().
 *
 * Complexity: 2EXPTIME-complete in general (Pnueli & Rosner 1989)
 *             EXPTIME-complete if assumption = true
 *             Polynomial for GR(1) fragment
 *
 * Reference: Pnueli & Rosner (1989) "On the Synthesis of a Reactive Module",
 *            POPL 1989 (conference version), FOCS 1989 */
synthesis_result_t *synthesis_realizability_check(
    const synthesis_spec_t *spec,
    const synthesis_config_t *config);

/**
 * Synthesize a reactive module from an LTL specification.
 * Convenience wrapper around synthesis_realizability_check();
 * returns the module directly (NULL if unrealizable).
 *
 * @return synthesized module, or NULL. Caller owns the module. */
reactive_module_t *synthesis_synthesize(
    const synthesis_spec_t *spec,
    const synthesis_config_t *config);

/**
 * Free a synthesis result and all associated memory.
 * Time: O(|Q| + |¦Ä|) for the contained module */
void synthesis_result_destroy(synthesis_result_t *result);

/* ====================================================================
 * Default Configuration
 * ==================================================================== */

/** Get a default synthesis configuration (all-safe defaults).
 *  Time: O(1) */
synthesis_config_t synthesis_config_default(void);

/* ====================================================================
 * GR(1) Synthesis (Polynomial-Time Fragment)
 * ==================================================================== */

/**
 * Check realizability of a GR(1) specification.
 *
 * GR(1) = Generalized Reactivity(1) formulas have the form:
 *
 *   (¡Ä_i ¡õ¡ó J_i^e ¡ú ¡Ä_j ¡õ¡ó J_j^s)  ¡Ä
 *   (¡õ ¡ó B^e ¡ú ¡õ ¡ó B^s)  ¡Ä
 *   (¡Ä_i ¡õ B_i^e  ¡ú ¡Ä_j ¡õ B_j^s)  ¡Ä
 *   (¡Ä_i ¡õ B_i^e  ¡ú ¡Ä_j ¡õ G_j^s)
 *
 * where:
 *   - J_i^e: environment justice (infinitely often) assumptions
 *   - B_i^e: environment safety assumptions
 *   - J_i^s: system justice guarantees
 *   - B_i^s: system safety guarantees
 *
 * This fragment is polynomial-time solvable via fixed-point
 * computation over symbolic representations.
 *
 * Reference: Piterman, Pnueli, Sa'ar (2006) "Synthesis of Reactive(1)
 *            Designs", VMCAI 2006.  Also Bloem et al. (2007) CAV.
 *
 * @param num_inputs   number of input propositions
 * @param num_outputs  number of output propositions
 * @param env_safety   array of LTL formulas (safety assumptions on env)
 * @param num_env_safety count of environment safety formulas
 * @param env_justice  array of LTL formulas (justice assumptions on env)
 * @param num_env_justice count of environment justice formulas
 * @param sys_safety   array of LTL formulas (safety guarantees)
 * @param num_sys_safety count of system safety formulas
 * @param sys_justice  array of LTL formulas (justice guarantees)
 * @param num_sys_justice count of system justice formulas
 * @return synthesis_result_t with status
 *
 * Time: O((m*n)^3) where m = |I ¡È O|, n = |S| (state space)
 *        where S is built from safety formulas */
synthesis_result_t *synthesis_gr1(
    int32_t num_inputs,
    int32_t num_outputs,
    ltl_formula_t **env_safety,
    int32_t num_env_safety,
    ltl_formula_t **env_justice,
    int32_t num_env_justice,
    ltl_formula_t **sys_safety,
    int32_t num_sys_safety,
    ltl_formula_t **sys_justice,
    int32_t num_sys_justice,
    const synthesis_config_t *config);

/* ====================================================================
 * Utility: Specification Validation
 * ==================================================================== */

/**
 * Validate a synthesis specification for basic correctness:
 * - All variables in formulas are within num_inputs + num_outputs range
 * - Assumption refers only to input variables
 * - Formulas are well-formed (no NULL pointers in tree)
 *
 * @return true if specification is valid, false otherwise.
 *         On false, sets *error_msg (if non-NULL) to an explanation.
 *
 * Time: O(|¦Õ|) */
bool synthesis_validate_spec(const synthesis_spec_t *spec,
                              const char **error_msg);

/**
 * Count the total number of distinct subformulas in a specification.
 * Useful for estimating the state space of the generated automaton.
 *
 * Time: O(|¦Õ_e| + |¦Õ_s|) */
int32_t synthesis_spec_subformula_count(const synthesis_spec_t *spec);

#endif /* SYNTHESIS_H */
