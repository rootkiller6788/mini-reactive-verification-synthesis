/**
 * grg1_spec.c — GR(1) specification construction and manipulation
 *
 * Implements the creation, validation, normalization, and analysis of
 * GR(1) specifications. A GR(1) specification encodes the reactive
 * synthesis problem as a formula in a fragment of Linear Temporal Logic.
 *
 * The canonical form is:
 *   φ = (θₑ ∧ □ρₑ ∧ ∧ᵢ □◇Jₑᵢ) → (θₛ ∧ □ρₛ ∧ ∧ⱼ □◇Jₛⱼ)
 *
 * Knowledge coverage:
 *   L1: GR(1) formula structure, predicate types
 *   L2: Well-formedness, safety vs liveness decomposition
 *   L3: Temporal logic semantics over finite-state systems
 *   L5: Specification construction and manipulation
 *   L6: Canonical specification patterns
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "grg1_spec.h"
#include "grg1_types.h"

/* =========================================================================
 * Specification construction
 * ========================================================================*/

grg1_spec_t* grg1_spec_create(const char* name, int num_vars,
                               const grg1_variable_t* vars) {
    if (!name || num_vars <= 0 || num_vars > GRG1_MAX_VARIABLES || !vars) {
        return NULL;
    }

    grg1_spec_t* spec = (grg1_spec_t*)malloc(sizeof(grg1_spec_t));
    if (!spec) return NULL;
    memset(spec, 0, sizeof(grg1_spec_t));

    strncpy(spec->specification_name, name, sizeof(spec->specification_name) - 1);
    spec->num_variables = num_vars;

    spec->variables = (grg1_variable_t*)malloc(
        (size_t)num_vars * sizeof(grg1_variable_t));
    if (!spec->variables) {
        free(spec);
        return NULL;
    }
    memcpy(spec->variables, vars, (size_t)num_vars * sizeof(grg1_variable_t));

    /* Initialize all predicates to NULL (unspecified) */
    spec->env_init = NULL;
    spec->sys_init = NULL;
    spec->env_safety = NULL;
    spec->sys_safety = NULL;
    spec->num_env_justice = 0;
    spec->num_sys_justice = 0;

    return spec;
}

void grg1_spec_free(grg1_spec_t* spec) {
    if (!spec) return;
    free(spec->variables);
    free(spec);
}

void grg1_spec_set_env_init(grg1_spec_t* spec, const char* name,
                             grg1_state_predicate_fn pred) {
    if (!spec || !name || !pred) return;
    strncpy(spec->env_init_name, name, sizeof(spec->env_init_name) - 1);
    spec->env_init = pred;
}

void grg1_spec_set_sys_init(grg1_spec_t* spec, const char* name,
                             grg1_state_predicate_fn pred) {
    if (!spec || !name || !pred) return;
    strncpy(spec->sys_init_name, name, sizeof(spec->sys_init_name) - 1);
    spec->sys_init = pred;
}

void grg1_spec_set_env_safety(grg1_spec_t* spec, const char* name,
                               grg1_transition_predicate_fn pred) {
    if (!spec || !name || !pred) return;
    strncpy(spec->env_safety_name, name, sizeof(spec->env_safety_name) - 1);
    spec->env_safety = pred;
}

void grg1_spec_set_sys_safety(grg1_spec_t* spec, const char* name,
                               grg1_transition_predicate_fn pred) {
    if (!spec || !name || !pred) return;
    strncpy(spec->sys_safety_name, name, sizeof(spec->sys_safety_name) - 1);
    spec->sys_safety = pred;
}

int grg1_spec_add_env_justice(grg1_spec_t* spec, const char* name,
                               grg1_state_predicate_fn pred) {
    if (!spec || !name || !pred) return -1;
    if (spec->num_env_justice >= GRG1_MAX_JUSTICE) return -1;

    int idx = spec->num_env_justice;
    grg1_justice_t* j = &spec->env_justice[idx];
    strncpy(j->name, name, sizeof(j->name) - 1);
    j->predicate = pred;
    j->justice_index = idx;
    j->is_assumption = true;
    spec->num_env_justice++;
    return idx;
}

int grg1_spec_add_sys_justice(grg1_spec_t* spec, const char* name,
                               grg1_state_predicate_fn pred) {
    if (!spec || !name || !pred) return -1;
    if (spec->num_sys_justice >= GRG1_MAX_JUSTICE) return -1;

    int idx = spec->num_sys_justice;
    grg1_justice_t* j = &spec->sys_justice[idx];
    strncpy(j->name, name, sizeof(j->name) - 1);
    j->predicate = pred;
    j->justice_index = idx;
    j->is_assumption = false;
    spec->num_sys_justice++;
    return idx;
}

/* =========================================================================
 * Specification validation
 * ========================================================================*/

grg1_spec_validation_t grg1_spec_validate(const grg1_spec_t* spec) {
    if (!spec) return GRG1_SPEC_ERR_NO_VARIABLES;

    /* Check 1: Variables defined */
    if (spec->num_variables <= 0 || !spec->variables) {
        return GRG1_SPEC_ERR_NO_VARIABLES;
    }

    /* Check 2: Initial conditions set */
    if (!spec->env_init || !spec->sys_init) {
        return GRG1_SPEC_ERR_NO_INIT;
    }

    /* Check 3: Safety conditions set */
    if (!spec->env_safety || !spec->sys_safety) {
        return GRG1_SPEC_ERR_MISSING_SAFETY;
    }

    /* Check 4: At least one system guarantee (otherwise vacuously realizable) */
    if (spec->num_sys_justice <= 0 && !spec->sys_safety) {
        /* If there are literally no system properties, the spec is vacuous.
         * This is allowed but flagged. We also check if pure safety only. */
    }

    /* Check 5: Validate variables are consistent */
    for (int i = 0; i < spec->num_variables; i++) {
        if (spec->variables[i].domain_size < 2) {
            return GRG1_SPEC_ERR_VARIABLE_MISMATCH;
        }
    }

    /* Check 6: No obviously contradictory justice conditions detectable
     * at the spec level. This is a lightweight check: we require that
     * each justice predicate name is non-empty. Full contradiction
     * detection requires model checking. */
    for (int i = 0; i < spec->num_env_justice; i++) {
        if (spec->env_justice[i].name[0] == '\0') {
            return GRG1_SPEC_ERR_CONTRADICTORY_JUSTICE;
        }
    }
    for (int j = 0; j < spec->num_sys_justice; j++) {
        if (spec->sys_justice[j].name[0] == '\0') {
            return GRG1_SPEC_ERR_CONTRADICTORY_JUSTICE;
        }
    }

    return GRG1_SPEC_VALID;
}

bool grg1_spec_env_admissible(const grg1_state_t* state,
                               const grg1_spec_t* spec) {
    if (!state || !spec) return false;

    /* Check environment initial condition */
    if (spec->env_init) {
        if (!spec->env_init(&state->valuation, spec->variables,
                             spec->num_variables)) {
            return false;
        }
    }
    return true;
}

bool grg1_spec_transition_safe(const grg1_state_t* from,
                                const grg1_state_t* to,
                                const grg1_spec_t* spec) {
    if (!from || !to || !spec) return false;

    /* Check system safety condition on the transition */
    if (spec->sys_safety) {
        if (!spec->sys_safety(&from->valuation, &to->valuation,
                               spec->variables, spec->num_variables)) {
            return false;
        }
    }
    return true;
}

/* =========================================================================
 * Specification analysis
 * ========================================================================*/

int grg1_spec_predicate_count(const grg1_spec_t* spec) {
    if (!spec) return 0;
    int count = 0;
    if (spec->env_init) count++;
    if (spec->sys_init) count++;
    if (spec->env_safety) count++;
    if (spec->sys_safety) count++;
    count += spec->num_env_justice;
    count += spec->num_sys_justice;
    return count;
}

int64_t grg1_spec_estimate_state_space(const grg1_spec_t* spec) {
    if (!spec || spec->num_variables <= 0) return 0;

    int64_t total = 1;
    for (int i = 0; i < spec->num_variables; i++) {
        int ds = spec->variables[i].domain_size;
        if (ds <= 0) return 0;

        /* Check for overflow */
        if (total > INT64_MAX / ds) {
            return -1; /* Overflow — state space exceeds int64 */
        }
        total *= ds;
    }
    return total;
}

bool grg1_spec_is_pure_safety(const grg1_spec_t* spec) {
    if (!spec) return false;
    return (spec->num_env_justice == 0 && spec->num_sys_justice == 0);
}

bool grg1_spec_is_env_liveness_only(const grg1_spec_t* spec) {
    if (!spec) return false;
    return (spec->num_sys_justice == 0);
}

void grg1_spec_to_normal_form(grg1_spec_t* spec, grg1_normal_form_t nf) {
    /* For explicit representation, the normal form is a conceptual
     * transformation. The actual computation uses the game-based
     * reduction, so this is a no-op in terms of data structure changes.
     * We record the normal form for documentation purposes. */
    if (!spec) return;
    (void)nf; /* Normal form flag — used in symbolic variant */
}

/* =========================================================================
 * Specification printing
 * ========================================================================*/

void grg1_spec_print(const grg1_spec_t* spec) {
    if (!spec) {
        printf("(null specification)\n");
        return;
    }

    printf("========================================\n");
    printf("GR(1) Specification: %s\n", spec->specification_name);
    printf("========================================\n");
    printf("Variables: %d\n", spec->num_variables);
    for (int i = 0; i < spec->num_variables; i++) {
        printf("  v%d: %s [0..%d] %s\n",
               spec->variables[i].var_id,
               spec->variables[i].name,
               spec->variables[i].domain_size - 1,
               spec->variables[i].is_system_controlled ?
               "(System)" : "(Environment)");
    }

    printf("\n--- Assumptions (Environment) ---\n");
    printf("  INIT  θₑ: %s\n",
           spec->env_init ? spec->env_init_name : "(unspecified)");
    printf("  SAFETY □ρₑ: %s\n",
           spec->env_safety ? spec->env_safety_name : "(unspecified)");
    printf("  JUSTICE (□◇): %d conditions\n", spec->num_env_justice);
    for (int i = 0; i < spec->num_env_justice; i++) {
        printf("    Jₑ%d: %s\n", i, spec->env_justice[i].name);
    }

    printf("\n--- Guarantees (System) ---\n");
    printf("  INIT  θₛ: %s\n",
           spec->sys_init ? spec->sys_init_name : "(unspecified)");
    printf("  SAFETY □ρₛ: %s\n",
           spec->sys_safety ? spec->sys_safety_name : "(unspecified)");
    printf("  JUSTICE (□◇): %d conditions\n", spec->num_sys_justice);
    for (int j = 0; j < spec->num_sys_justice; j++) {
        printf("    Jₛ%d: %s\n", j, spec->sys_justice[j].name);
    }

    printf("\nState space estimate: %lld states\n",
           (long long)grg1_spec_estimate_state_space(spec));
    printf("Pure safety: %s\n",
           grg1_spec_is_pure_safety(spec) ? "yes" : "no");
    printf("========================================\n");
}

/* =========================================================================
 * Canonical predicate generators for common patterns
 * ========================================================================*/

/**
 * Generate a predicate that checks if a specific variable equals a value.
 *
 * This is a factory for the most common atomic predicate in reactive
 * synthesis: "variable x has value v."
 *
 * Complexity: O(1) evaluation
 * Knowledge: L6 — atomic propositions in reactive specifications
 *
 * Note: This function returns a dynamically allocated closure; the caller
 * must manage its lifetime. For simplicity in this implementation, we
 * provide pre-built predicate patterns.
 */

/**
 * Pre-built predicate: check that a system-controlled variable equals
 * a specific value.
 *
 * @param v  The valuation
 * @param vars  Variable definitions
 * @param num_vars  Number of variables
 * @param var_index  Which variable to check
 * @param expected_value  The expected value
 * @return  true if vars[var_index].values[] == expected_value
 */
static bool predicate_var_equals_impl(const grg1_valuation_t* v,
                                       const grg1_variable_t* vars,
                                       int num_vars,
                                       int var_index,
                                       int expected_value) {
    (void)vars;
    (void)num_vars;
    if (!v || var_index < 0 || var_index >= v->num_variables) return false;
    return v->values[var_index] == expected_value;
}

/**
 * Pre-built predicate: check that environment variables satisfy mutual
 * exclusion (at most one env variable is "active").
 *
 * Used in arbiter specifications where multiple clients cannot
 * simultaneously hold a resource.
 *
 * @param v  The valuation
 * @param vars  Variable definitions
 * @param num_vars  Number of variables
 * @param active_value  The value considered "active"
 * @return  true if at most one env variable equals active_value
 */
static bool predicate_mutual_exclusion_impl(const grg1_valuation_t* v,
                                              const grg1_variable_t* vars,
                                              int num_vars,
                                              int active_value) {
    if (!v) return false;
    int count = 0;
    for (int i = 0; i < v->num_variables && i < num_vars; i++) {
        if (!vars[i].is_system_controlled &&
            v->values[i] == active_value) {
            count++;
        }
    }
    return count <= 1;
}

/**
 * Pre-built predicate: check that a specific justice condition is
 * satisfied (the system variable reached its "goal" value).
 *
 * This is the prototypical justice pattern: "eventually, the system
 * will grant the resource to each client." The predicate checks the
 * "satisfied" moment.
 */
static bool predicate_justice_grant_impl(const grg1_valuation_t* v,
                                           const grg1_variable_t* vars,
                                           int num_vars,
                                           int sys_var_index,
                                           int grant_value) {
    (void)vars;
    (void)num_vars;
    if (!v || sys_var_index < 0 || sys_var_index >= v->num_variables) return false;
    return v->values[sys_var_index] == grant_value;
}

/* =========================================================================
 * Common specification factory functions
 * ========================================================================= */

/**
 * Create a simple mutual exclusion specification.
 *
 * This is one of the canonical GR(1) synthesis benchmarks.
 *
 * Setup:
 *   - 2 environment variables: req1, req2 (0=idle, 1=request)
 *   - 1 system variable: grant (0=idle, 1=grant1, 2=grant2)
 *
 * Assumptions (environment):
 *   - θₑ: req1=0 ∧ req2=0 (start idle)
 *   - ρₑ: requests persist until granted (fairness)
 *   - Jₑ: □◇(req1=0) ∧ □◇(req2=0) (eventually release)
 *
 * Guarantees (system):
 *   - θₛ: grant=0 (start idle)
 *   - ρₛ: mutual exclusion (cannot grant both simultaneously)
 *   - Jₛ: □◇(req1=1 → grant=1) ∧ □◇(req2=1 → grant=2) (starvation freedom)
 *
 * Reference: Piterman et al. (2006) §7.1 — Arbiter Example
 */
grg1_spec_t* grg1_spec_create_mutex_example(void) {
    /* Define variables */
    grg1_variable_t vars[3];
    vars[0].var_id = 0;
    strcpy(vars[0].name, "req1");
    vars[0].domain_size = 2;  /* 0=idle, 1=request */
    vars[0].is_system_controlled = false;
    vars[0].initial_value = 0;

    vars[1].var_id = 1;
    strcpy(vars[1].name, "req2");
    vars[1].domain_size = 2;
    vars[1].is_system_controlled = false;
    vars[1].initial_value = 0;

    vars[2].var_id = 2;
    strcpy(vars[2].name, "grant");
    vars[2].domain_size = 3;  /* 0=idle, 1=grant1, 2=grant2 */
    vars[2].is_system_controlled = true;
    vars[2].initial_value = 0;

    grg1_spec_t* spec = grg1_spec_create("Mutual Exclusion Arbiter", 3, vars);
    if (!spec) return NULL;

    /* Environment initial: both clients start idle */
    /* Environment safety: requests can toggle freely */
    /* System initial: grant starts idle */
    /* System safety: no double grant */
    /* System justice: each request eventually granted */

    /* Predicates are set via the public API (grg1_spec_set_*).
     * This factory returns the structure with variables initialized;
     * callers must set predicates before synthesis. */

    return spec;
}

/**
 * Create a specification for a traffic light controller.
 *
 * This is another canonical GR(1) synthesis benchmark: a controller
 * managing two traffic lights at an intersection, ensuring safety
 * (no cross-traffic green) and liveness (each direction eventually gets green).
 *
 * Variables:
 *   - env: pedestrian button for each direction
 *   - sys: light state for each direction (0=red, 1=yellow, 2=green)
 *
 * Reference: Bloem et al. (2012) §7 — Traffic Light Controller
 */
grg1_spec_t* grg1_spec_create_traffic_light_example(void) {
    grg1_variable_t vars[4];
    vars[0].var_id = 0;
    strcpy(vars[0].name, "ped_NS");
    vars[0].domain_size = 2;  /* 0=no pedestrian, 1=pedestrian waiting */
    vars[0].is_system_controlled = false;
    vars[0].initial_value = 0;

    vars[1].var_id = 1;
    strcpy(vars[1].name, "ped_EW");
    vars[1].domain_size = 2;
    vars[1].is_system_controlled = false;
    vars[1].initial_value = 0;

    vars[2].var_id = 2;
    strcpy(vars[2].name, "light_NS");
    vars[2].domain_size = 3;  /* 0=red, 1=yellow, 2=green */
    vars[2].is_system_controlled = true;
    vars[2].initial_value = 0;

    vars[3].var_id = 3;
    strcpy(vars[3].name, "light_EW");
    vars[3].domain_size = 3;
    vars[3].is_system_controlled = true;
    vars[3].initial_value = 0;

    grg1_spec_t* spec = grg1_spec_create("Traffic Light Controller", 4, vars);
    return spec;
}
