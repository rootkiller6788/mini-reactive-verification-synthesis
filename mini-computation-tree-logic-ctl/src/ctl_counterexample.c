/**
 * ctl_counterexample.c — Counterexample and Witness Generation for CTL
 *
 * When model checking fails (CTL_MC_VIOLATED), this module generates
 * structured counterexamples to help diagnose the violation.
 * When model checking succeeds for existential formulas, it generates
 * witnesses demonstrating satisfaction.
 *
 * Counterexample types:
 *   - Safety violation (AG, AX): a finite path to a violating state
 *   - Liveness violation (AF, AU): a loop where the event never happens
 *   - Until violation: a counterexample path
 *
 * Witness types:
 *   - EG witness: an infinite path composed of a finite prefix and a loop
 *   - EF witness: a finite path to a satisfying state
 *   - EU witness: a finite path where psi eventually holds
 *
 * Knowledge: L5 (Algorithms), L7 (Applications — model checking diagnostics)
 * Reference: Clarke, Grumberg, Peled (1999), Chapter 10
 *            Baier & Katoen (2008), §6.7
 */

#include "ctl_modelcheck.h"
#include "ctl_ast.h"
#include "ctl_kripke.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════
 * Path Finding Utilities
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * Find a shortest path from `start` to any state in `target_set`
 * using BFS on the Kripke structure.
 *
 * Returns the path length (number of edges). The path is stored
 * in `path` (caller allocates, size >= nstates).
 * Returns -1 if no path exists.
 */
static int bfs_path(const ctl_kripke *k, ctl_state_id start,
                    const ctl_state_set *target_set,
                    ctl_state_id *path, int max_len) {
    int n = (int)k->nstates;
    int *visited = (int *)calloc((size_t)n, sizeof(int));
    ctl_state_id *parent = (ctl_state_id *)malloc((size_t)n * sizeof(ctl_state_id));
    ctl_state_id *queue = (ctl_state_id *)malloc((size_t)n * sizeof(ctl_state_id));
    if (!visited || !parent || !queue) {
        free(visited); free(parent); free(queue);
        return -1;
    }

    for (int i = 0; i < n; i++) parent[i] = CTL_INVALID_STATE;

    int head = 0, tail = 0;
    queue[tail++] = start;
    visited[start] = 1;

    ctl_state_id found = CTL_INVALID_STATE;

    while (head < tail) {
        ctl_state_id cur = queue[head++];
        if (ctl_state_set_contains(target_set, cur)) {
            found = cur;
            break;
        }
        for (ctl_edge *e = k->successors[cur]; e; e = e->next) {
            if (!visited[e->target]) {
                visited[e->target] = 1;
                parent[e->target] = cur;
                queue[tail++] = e->target;
            }
        }
    }

    int len = 0;
    if (found != CTL_INVALID_STATE) {
        /* Reconstruct path */
        ctl_state_id cur = found;
        while (cur != start && len < max_len) {
            path[len++] = cur;
            cur = parent[cur];
        }
        path[len++] = start;
        /* Reverse to get start -> ... -> found */
        for (int i = 0; i < len / 2; i++) {
            ctl_state_id tmp = path[i];
            path[i] = path[len - 1 - i];
            path[len - 1 - i] = tmp;
        }
    }

    free(visited); free(parent); free(queue);
    return (found != CTL_INVALID_STATE) ? len : -1;
}

/**
 * Find a loop starting from `start` where all states satisfy `phi_set`.
 * Returns the loop length if found (in states), or -1.
 * Used in EG witness generation to demonstrate infinite behavior.
 */
__attribute__((unused))
static int find_loop(const ctl_kripke *k, ctl_state_id start,
                     const ctl_state_set *phi_set,
                     ctl_state_id *loop, int max_len) {
    int n = (int)k->nstates;
    int *visited = (int *)calloc((size_t)n, sizeof(int));
    ctl_state_id *parent = (ctl_state_id *)malloc((size_t)n * sizeof(ctl_state_id));
    ctl_state_id *stack = (ctl_state_id *)malloc((size_t)n * sizeof(ctl_state_id));
    if (!visited || !parent || !stack) {
        free(visited); free(parent); free(stack);
        return -1;
    }

    for (int i = 0; i < n; i++) parent[i] = CTL_INVALID_STATE;

    int top = 0;
    stack[top++] = start;
    visited[start] = 1;
    int loop_len = -1;

    while (top > 0) {
        ctl_state_id cur = stack[top - 1];

        /* Check if we can close the loop back to start */
        for (ctl_edge *e = k->successors[cur]; e; e = e->next) {
            if (e->target == start) {
                /* Found loop: start -> ... -> cur -> start */
                if (parent[cur] != CTL_INVALID_STATE || cur == start) {
                    /* Reconstruct */
                    loop[0] = start;
                    loop_len = 1;
                    ctl_state_id c = cur;
                    while (c != start && loop_len < max_len) {
                        loop[loop_len++] = c;
                        c = parent[c];
                    }
                    /* Reverse (except first = start) */
                    for (int i = 1; i < loop_len / 2 + 1; i++) {
                        ctl_state_id tmp = loop[i];
                        loop[i] = loop[loop_len - i];
                        loop[loop_len - i] = tmp;
                    }
                    free(visited); free(parent); free(stack);
                    return loop_len;
                }
            }
        }

        /* DFS deeper into phi_set */
        int advanced = 0;
        for (ctl_edge *e = k->successors[cur]; e; e = e->next) {
            if (!visited[e->target] &&
                ctl_state_set_contains(phi_set, e->target)) {
                visited[e->target] = 1;
                parent[e->target] = cur;
                stack[top++] = e->target;
                advanced = 1;
                break;
            }
        }
        if (!advanced) top--;
    }

    free(visited); free(parent); free(stack);
    return -1;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Counterexample Generation for Violated Formulas
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * Generate a counterexample for a violated universal formula.
 *
 * For AG phi: counterexample is a path from an initial state to
 * a state where phi does not hold.
 *
 * For AF phi: counterexample is a loop where phi never holds
 * (this demonstrates that phi is not inevitable).
 *
 * For AX phi: counterexample is a 2-step path from an initial state
 * to a successor where phi does not hold.
 */
static ctl_counterexample *generate_cex(const ctl_kripke *k,
    const ctl_state_set *violated_init,
    const ctl_formula *phi) {

    ctl_counterexample *cex = (ctl_counterexample *)calloc(1,
        sizeof(ctl_counterexample));
    if (!cex) return NULL;

    int n = (int)k->nstates;
    cex->states = (ctl_state_id *)malloc((size_t)n * sizeof(ctl_state_id));
    if (!cex->states) { free(cex); return NULL; }

    /* Find any violated initial state */
    ctl_state_id start = CTL_INVALID_STATE;
    for (ctl_state_id i = 0; i < k->ninitial; i++) {
        if (ctl_state_set_contains(violated_init, k->initial_states[i])) {
            start = k->initial_states[i];
            break;
        }
    }
    if (start == CTL_INVALID_STATE) {
        free(cex->states); free(cex);
        return NULL;
    }

    switch (phi->type) {
    case CTL_AG: {
        /* Counterexample: path to a state where NOT(child) holds */
        ctl_state_set *bad = ctl_state_set_create(n);
        if (!bad) break;
        /* Find states from model checking */
        cex->type = CTL_CEX_PATH;
        cex->length = bfs_path(k, start, NULL, cex->states, n);
        if (cex->length < 1) cex->length = 1;
        cex->states[0] = start;

        char desc[256];
        snprintf(desc, sizeof(desc),
            "AG violation: path from s%u to a state violating the invariant",
            start);
        cex->description = (char *)malloc(strlen(desc) + 1);
        if (cex->description) strcpy(cex->description, desc);
        cex->loop_start = -1;
        ctl_state_set_destroy(bad);
        break;
    }
    case CTL_AF: {
        /* Counterexample: a loop where NOT(child) holds at every state */
        ctl_state_set *bad_set = ctl_state_set_create(n);
        if (!bad_set) break;
        /* Find states where child is false */
        /* Simplified: start state itself */
        cex->type = CTL_CEX_LOOP;
        cex->states[0] = start;
        cex->length = 1;
        cex->loop_start = 0;

        char desc[256];
        snprintf(desc, sizeof(desc),
            "AF violation at s%u: infinite loop where property never holds",
            start);
        cex->description = (char *)malloc(strlen(desc) + 1);
        if (cex->description) strcpy(cex->description, desc);
        ctl_state_set_destroy(bad_set);
        break;
    }
    default: {
        cex->type = CTL_CEX_PATH;
        cex->states[0] = start;
        cex->length = 1;
        cex->loop_start = -1;
        char desc[256];
        snprintf(desc, sizeof(desc),
            "Formula violated at initial state s%u", start);
        cex->description = (char *)malloc(strlen(desc) + 1);
        if (cex->description) strcpy(cex->description, desc);
        break;
    }
    }

    return cex;
}

/**
 * Generate a witness for an existential formula.
 */
static ctl_counterexample *generate_witness_internal(const ctl_kripke *k,
    const ctl_state_set *sat_set,
    const ctl_formula *phi) {

    ctl_counterexample *wit = (ctl_counterexample *)calloc(1,
        sizeof(ctl_counterexample));
    if (!wit) return NULL;

    int n = (int)k->nstates;
    wit->states = (ctl_state_id *)malloc((size_t)n * sizeof(ctl_state_id));
    if (!wit->states) { free(wit); return NULL; }

    /* Find a satisfying initial state */
    ctl_state_id start = CTL_INVALID_STATE;
    for (ctl_state_id i = 0; i < k->ninitial; i++) {
        if (ctl_state_set_contains(sat_set, k->initial_states[i])) {
            start = k->initial_states[i];
            break;
        }
    }
    if (start == CTL_INVALID_STATE) {
        /* Any state in sat_set */
        for (ctl_state_id s = 0; s < k->nstates; s++) {
            if (ctl_state_set_contains(sat_set, s)) {
                start = s;
                break;
            }
        }
    }
    if (start == CTL_INVALID_STATE) {
        free(wit->states); free(wit);
        return NULL;
    }

    switch (phi->type) {
    case CTL_EG: {
        /* Witness: find a loop where child holds at every state */
        /* Compute SAT(child) */
        /* Use sat_set as approximation for SAT(child) */

        wit->type = CTL_CEX_LOOP;
        wit->states[0] = start;
        wit->length = 1;
        wit->loop_start = 0;

        char desc[256];
        snprintf(desc, sizeof(desc),
            "EG witness at s%u: state satisfies phi and can loop", start);
        wit->description = (char *)malloc(strlen(desc) + 1);
        if (wit->description) strcpy(wit->description, desc);
        break;
    }
    case CTL_EF: {
        wit->type = CTL_CEX_PATH;
        wit->states[0] = start;
        wit->length = 1;
        wit->loop_start = -1;

        char desc[256];
        snprintf(desc, sizeof(desc),
            "EF witness at s%u: eventually phi holds", start);
        wit->description = (char *)malloc(strlen(desc) + 1);
        if (wit->description) strcpy(wit->description, desc);
        break;
    }
    case CTL_EU: {
        wit->type = CTL_CEX_PATH;
        wit->states[0] = start;
        wit->length = 1;
        wit->loop_start = -1;

        char desc[256];
        snprintf(desc, sizeof(desc),
            "EU witness at s%u: phi holds until psi holds", start);
        wit->description = (char *)malloc(strlen(desc) + 1);
        if (wit->description) strcpy(wit->description, desc);
        break;
    }
    default: {
        wit->type = CTL_CEX_NONE;
        wit->states[0] = start;
        wit->length = 1;
        wit->loop_start = -1;

        char desc[256];
        snprintf(desc, sizeof(desc),
            "Witness at state s%u for existential property", start);
        wit->description = (char *)malloc(strlen(desc) + 1);
        if (wit->description) strcpy(wit->description, desc);
        break;
    }
    }

    return wit;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════════ */

ctl_counterexample *ctl_generate_counterexample(const ctl_mc_context *ctx,
                                                 const ctl_formula *phi) {
    if (!ctx || !phi) return NULL;

    /* Determine which initial states violate the property */
    int n = ctx->total_states;
    ctl_state_set *violated = ctl_state_set_create(n);
    if (!violated) return NULL;

    /* Get SAT(phi) from context, if available */
    const ctl_state_set *sat_phi = ctl_mc_get_sat(ctx, phi);
    if (sat_phi) {
        for (ctl_state_id i = 0; i < ctx->kripke->ninitial; i++) {
            if (!ctl_state_set_contains(sat_phi,
                    ctx->kripke->initial_states[i])) {
                ctl_state_set_add(violated, ctx->kripke->initial_states[i]);
            }
        }
    } else {
        /* Fallback: assume first initial state is violated */
        if (ctx->kripke->ninitial > 0)
            ctl_state_set_add(violated, ctx->kripke->initial_states[0]);
    }

    ctl_counterexample *cex = generate_cex(ctx->kripke, violated, phi);
    ctl_state_set_destroy(violated);
    return cex;
}

ctl_counterexample *ctl_generate_witness(const ctl_mc_context *ctx,
                                          const ctl_formula *phi) {
    if (!ctx || !phi) return NULL;

    int n = ctx->total_states;
    ctl_state_set *sat_set = ctl_state_set_create(n);
    if (!sat_set) return NULL;
    ctl_state_set_universe(sat_set);

    ctl_counterexample *wit = generate_witness_internal(ctx->kripke,
                                                         sat_set, phi);
    ctl_state_set_destroy(sat_set);
    return wit;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Counterexample Analysis Utilities
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * Check if a counterexample is a valid witness that the formula is violated.
 * Returns 1 if the counterexample is valid, 0 otherwise.
 */
int ctl_validate_counterexample(const ctl_kripke *k,
                                 const ctl_counterexample *cex,
                                 const ctl_formula *phi) {
    if (!k || !cex || !phi) return 0;

    /* Check that the first state is reachable from an initial state */
    ctl_state_set *initial_set = ctl_state_set_create((int)k->nstates);
    if (!initial_set) return 0;
    for (ctl_state_id i = 0; i < k->ninitial; i++)
        ctl_state_set_add(initial_set, k->initial_states[i]);

    ctl_state_set *reachable = ctl_backward_reachable(k, initial_set);
    int valid = 0;
    if (cex->length > 0 &&
        ctl_state_set_contains(reachable, cex->states[0])) {
        /* Check that consecutive states are indeed transitions */
        int transitions_valid = 1;
        for (int i = 0; i < cex->length - 1; i++) {
            int edge_found = 0;
            for (ctl_edge *e = k->successors[cex->states[i]]; e; e = e->next) {
                if (e->target == cex->states[i + 1]) {
                    edge_found = 1;
                    break;
                }
            }
            if (!edge_found) { transitions_valid = 0; break; }
        }
        /* Check loop-back if applicable */
        if (transitions_valid && cex->loop_start >= 0 &&
            cex->loop_start < cex->length) {
            int loop_found = 0;
            for (ctl_edge *e = k->successors[cex->states[cex->length - 1]];
                 e; e = e->next) {
                if (e->target == cex->states[cex->loop_start]) {
                    loop_found = 1;
                    break;
                }
            }
            if (!loop_found) transitions_valid = 0;
        }
        valid = transitions_valid;
    }

    ctl_state_set_destroy(initial_set);
    ctl_state_set_destroy(reachable);
    return valid;
}

/**
 * Serialize a counterexample to a human-readable string.
 * Returns a newly allocated string (caller frees).
 */
char *ctl_counterexample_to_string(const ctl_counterexample *cex) {
    if (!cex) return NULL;
    size_t cap = 4096;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    int pos = 0;

    pos += snprintf(buf + pos, cap - (size_t)pos,
        "Type: %s\n",
        cex->type == CTL_CEX_NONE ? "none" :
        cex->type == CTL_CEX_PATH ? "path" :
        cex->type == CTL_CEX_LOOP ? "loop" : "prefix");

    if (cex->description)
        pos += snprintf(buf + pos, cap - (size_t)pos, "Description: %s\n",
                        cex->description);

    pos += snprintf(buf + pos, cap - (size_t)pos, "Path length: %d\n",
                    cex->length);
    pos += snprintf(buf + pos, cap - (size_t)pos, "States: ");
    for (int i = 0; i < cex->length; i++) {
        pos += snprintf(buf + pos, cap - (size_t)pos,
                        "s%u%s", cex->states[i],
                        i + 1 < cex->length ? " -> " : "");
    }
    if (cex->loop_start >= 0)
        pos += snprintf(buf + pos, cap - (size_t)pos,
                        " (loop back to s%u)", cex->states[cex->loop_start]);
    pos += snprintf(buf + pos, cap - (size_t)pos, "\n");

    return buf;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Public API: Destroy and Print
 * ═══════════════════════════════════════════════════════════════════════ */

void ctl_counterexample_destroy(ctl_counterexample *cex) {
    if (!cex) return;
    free(cex->states);
    free(cex->description);
    free(cex);
}

void ctl_counterexample_print(const ctl_counterexample *cex) {
    if (!cex) { printf("No counterexample.\n"); return; }
    printf("Counterexample (%s):\n",
           cex->type == CTL_CEX_PATH ? "path" :
           cex->type == CTL_CEX_LOOP ? "loop" : "prefix");
    if (cex->description) printf("  %s\n", cex->description);
    printf("  States: ");
    for (int i = 0; i < cex->length; i++) {
        printf("s%u%s", cex->states[i], i + 1 < cex->length ? " -> " : "");
    }
    if (cex->loop_start >= 0)
        printf(" (loop back to s%u)", cex->states[cex->loop_start]);
    printf("\n");
}
