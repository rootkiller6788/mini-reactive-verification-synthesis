/**
 * reactive_types.h ¡ª Core Type Definitions for Reactive Synthesis
 *
 * Defines the fundamental data structures used throughout the reactive
 * synthesis pipeline: states, traces, reactive modules, signals, and
 * the open-system computation model pioneered by Pnueli.
 *
 * Reference: Pnueli (1977) "The Temporal Logic of Programs"
 *            Pnueli & Rosner (1989) "On the Synthesis of a Reactive Module"
 *
 * Knowledge Level: L1 Definitions, L3 Mathematical Structures
 */

#ifndef REACTIVE_TYPES_H
#define REACTIVE_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ====================================================================
 * Basic domain types
 * ==================================================================== */

/** A propositional variable name (max 63 chars + null) */
typedef char varname_t[64];

/** A literal is a variable or its negation, stored as (var_id, polarity).
 *  polarity=true means positive, polarity=false means negated. */
typedef struct {
    int32_t var_id;
    bool    polarity;
} literal_t;

/** Valuation: bit-vector of truth assignments for up to 128 variables.
 *  bit i is the truth value of variable with id i. */
typedef struct {
    uint64_t bits[2];
} valuation_t;

/* ====================================================================
 * Linear Temporal Logic (LTL) ¡ª Formula Nodes
 * ==================================================================== */

/** LTL formula node types, following standard LTL syntax.
 *  Reference: Pnueli (1977), Lichtenstein & Pnueli (1985) */
typedef enum {
    LTL_TRUE,
    LTL_FALSE,
    LTL_VAR,
    LTL_NOT,
    LTL_AND,
    LTL_OR,
    LTL_IMPLIES,
    LTL_EQUIV,
    LTL_X,
    LTL_F,
    LTL_G,
    LTL_U,
    LTL_R,
    LTL_W,
    LTL_M
} ltl_node_type_t;

/** Forward declaration for LTL formula DAG node */
typedef struct ltl_formula ltl_formula_t;

/** LTL formula node; formulas form a DAG with sharing.
 *  Each node carries a type, children pointers, and a unique id
 *  for canonicalization/hashing. */
struct ltl_formula {
    ltl_node_type_t type;
    int32_t         id;
    int32_t         var_id;
    ltl_formula_t  *left;
    ltl_formula_t  *right;
    int32_t         refcount;
    bool            is_canonical;
};

/* ====================================================================
 * States and Transitions
 * ==================================================================== */

/** A state in a reactive system is a labeled node with a valuation
 *  of output signals. Input signals are determined by the environment. */
typedef struct {
    int32_t     state_id;
    valuation_t output_labels;
    bool        is_initial;
    bool        is_accepting;
    int32_t     priority;
} state_t;

/** A transition between states, labeled with input valuation.
 *  For a deterministic Mealy machine, the transition fires when
 *  the input matches 'input_guard'. */
typedef struct {
    int32_t     from_state;
    int32_t     to_state;
    valuation_t input_guard;
    valuation_t input_mask;
} transition_t;

/* ====================================================================
 * Finite State Machine (Mealy/Moore)
 * ==================================================================== */

/** A reactive module as a finite-state machine with input alphabet
 *  and output alphabet. This is the synthesis target.
 *
 *  Mealy semantics: output depends on current state AND current input.
 *  Moore semantics: output depends only on current state. */
typedef struct {
    int32_t         num_states;
    int32_t         num_inputs;
    int32_t         num_outputs;
    state_t        *states;
    transition_t   *transitions;
    int32_t         num_trans;
    int32_t         init_state;
    int32_t        *outdegree;
    int32_t        *trans_start;
    char          **input_names;
    char          **output_names;
} reactive_module_t;

/* ====================================================================
 * Traces and Executions
 * ==================================================================== */

/** A single step in a trace: (input_vector, output_vector, state_id). */
typedef struct {
    valuation_t inputs;
    valuation_t outputs;
    int32_t     state_id;
} trace_step_t;

/** An infinite trace (computation path) as a finite prefix plus
 *  a lasso (cycle). All omega-regular properties can be checked on
 *  ultimately periodic traces. */
typedef struct {
    trace_step_t *prefix;
    int32_t       prefix_len;
    trace_step_t *cycle;
    int32_t       cycle_len;
} infinite_trace_t;

/* ====================================================================
 * Environment Assumptions / System Guarantees
 * ==================================================================== */

/** The specification for reactive synthesis: (E, S) where
 *  E = environment assumptions (LTL formula over inputs)
 *  S = system guarantees (LTL formula over inputs and outputs)
 *
 *  The synthesis problem: find M such that M satisfies E ¡ú S. */
typedef struct {
    ltl_formula_t  *assumption;
    ltl_formula_t  *guarantee;
    int32_t         num_inputs;
    int32_t         num_outputs;
    char           *input_names[64];
    char           *output_names[64];
} synthesis_spec_t;

/* ====================================================================
 * Core API: Reactive Modules
 * ==================================================================== */

/** Allocate and initialize a reactive module.
 *  Time: O(|Q|)   Space: O(|Q| * (|I| + |O|)) */
reactive_module_t *reactive_module_create(int32_t num_states,
                                           int32_t num_inputs,
                                           int32_t num_outputs);

/** Free all memory associated with a reactive module.
 *  Time: O(|Q| + |delta|) */
void reactive_module_destroy(reactive_module_t *mod);

/** Add a transition from state 'from' to state 'to' under input guard.
 *  Time: O(1) amortized */
int32_t reactive_module_add_transition(reactive_module_t *mod,
                                        int32_t from_state,
                                        int32_t to_state,
                                        valuation_t input_guard,
                                        valuation_t input_mask);

/** Set the output label (valuation) for a given state.
 *  Time: O(1) */
void reactive_module_set_output(reactive_module_t *mod,
                                 int32_t state_id,
                                 valuation_t outputs);

/** Compute the next state given current state and input valuation.
 *  Returns -1 if no matching transition exists.
 *  Time: O(outdegree(state)) */
int32_t reactive_module_step(const reactive_module_t *mod,
                              int32_t current_state,
                              valuation_t inputs);

/** Compute the output valuation at a given state.
 *  Time: O(1) */
valuation_t reactive_module_get_output(const reactive_module_t *mod,
                                        int32_t state_id);

/** Print a human-readable representation of the module to stdout.
 *  Time: O(|Q| + |delta|) */
void reactive_module_print(const reactive_module_t *mod);

/** Generate a random trace of the module of given length.
 *  Caller must free the returned trace_step_t array.
 *  Time: O(length * log(|Q|)) */
trace_step_t *reactive_module_simulate(const reactive_module_t *mod,
                                        int32_t length,
                                        int32_t *actual_len);

/* ====================================================================
 * Core API: Valuations
 * ==================================================================== */

/** Create valuation with specific variable set to true */
valuation_t valuation_singleton(int32_t var_id);

/** Set a variable's truth value in a valuation */
valuation_t valuation_set(valuation_t v, int32_t var_id, bool value);

/** Get a variable's truth value from a valuation */
bool valuation_get(valuation_t v, int32_t var_id);

/** Bitwise AND of two valuations */
valuation_t valuation_and(valuation_t a, valuation_t b);

/** Bitwise OR of two valuations */
valuation_t valuation_or(valuation_t a, valuation_t b);

/** Bitwise NOT of a valuation (within valid variable range) */
valuation_t valuation_not(valuation_t v, int32_t num_vars);

/** Check if valuation 'a' is consistent with guard under mask.
 *  Returns true iff for all i where mask[i]=1, a[i]=guard[i]. */
bool valuation_matches(valuation_t a, valuation_t guard, valuation_t mask);

#define VALUATION_EMPTY ((valuation_t){{0, 0}})
#define VALUATION_ALL   ((valuation_t){{~0ULL, ~0ULL}})

/* ====================================================================
 * Core API: Synthesis Specification
 * ==================================================================== */

/** Create a synthesis spec from assumption and guarantee formulas. */
synthesis_spec_t *synthesis_spec_create(ltl_formula_t *assumption,
                                         ltl_formula_t *guarantee,
                                         int32_t num_inputs,
                                         int32_t num_outputs);

/** Free a synthesis specification (does NOT free LTL formulas). */
void synthesis_spec_destroy(synthesis_spec_t *spec);

#endif /* REACTIVE_TYPES_H */
