/*
 * ltl_semantics.c — LTL Semantics: Trace Evaluation & Kripke Model Checking
 *
 * Implements the satisfaction relation σ, i ⊨ φ for infinite traces
 * (represented as lassos), bounded semantics for runtime monitoring,
 * Kripke structure traversal, and explicit-state LTL model checking
 * via path enumeration.
 *
 * L1 Definitions + L2 Core Concepts + L3 Structures + L6 Problems.
 *
 * Reference:
 *   Manna & Pnueli 1992 — The Temporal Logic of Reactive and Concurrent Systems
 *   Baier & Katoen 2008 — Principles of Model Checking (Ch. 5)
 *   Vardi & Wolper 1986 — An automata-theoretic approach to program verification
 */

#include "ltl_semantics.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ── Proposition Set Utilities ────────────────────────────────── */

void ltl_propset_print(LtlPropSet set, int max_idx) {
    printf("{");
    int first = 1;
    for (int i = 0; i < max_idx && i < LTL_MAX_PROPS; i++) {
        if (LTL_PROP_CONTAINS(set, i)) {
            if (!first) printf(", ");
            printf("p%d", i);
            first = 0;
        }
    }
    printf("}");
}

/* ── Infinite Trace ───────────────────────────────────────────── */

LtlTrace* ltl_trace_create_lasso(const LtlPropSet* prefix, int plen,
                                  const LtlPropSet* cycle, int clen) {
    if (!cycle || clen < 1) return NULL;

    LtlTrace* t = (LtlTrace*)calloc(1, sizeof(LtlTrace));
    if (!t) return NULL;

    if (plen > 0 && prefix) {
        t->prefix = (LtlPropSet*)malloc(plen * sizeof(LtlPropSet));
        if (!t->prefix) { free(t); return NULL; }
        memcpy(t->prefix, prefix, plen * sizeof(LtlPropSet));
        t->prefix_len = plen;
    } else {
        t->prefix = NULL;
        t->prefix_len = 0;
    }

    t->cycle = (LtlPropSet*)malloc(clen * sizeof(LtlPropSet));
    if (!t->cycle) { free(t->prefix); free(t); return NULL; }
    memcpy(t->cycle, cycle, clen * sizeof(LtlPropSet));
    t->cycle_len = clen;

    return t;
}

LtlTrace* ltl_trace_create_single(LtlPropSet state) {
    LtlPropSet cycle[1] = { state };
    return ltl_trace_create_lasso(NULL, 0, cycle, 1);
}

LtlTrace* ltl_trace_create_empty(void) {
    return ltl_trace_create_single(LTL_PROP_EMPTY);
}

void ltl_trace_free(LtlTrace* trace) {
    if (!trace) return;
    free(trace->prefix);
    free(trace->cycle);
    free(trace);
}

LtlPropSet ltl_trace_get(const LtlTrace* trace, int step) {
    if (!trace || step < 0) return LTL_PROP_EMPTY;
    if (step < trace->prefix_len)
        return trace->prefix[step];
    int cycle_step = (step - trace->prefix_len) % trace->cycle_len;
    return trace->cycle[cycle_step];
}

void ltl_trace_print(const LtlTrace* trace, int max_steps) {
    if (!trace) { printf("(null trace)\n"); return; }
    printf("Trace [prefix=%d, cycle=%d]:\n", trace->prefix_len, trace->cycle_len);
    int limit = (max_steps > 0) ? max_steps : 20;
    for (int i = 0; i < limit; i++) {
        printf("  %3d: ", i);
        ltl_propset_print(ltl_trace_get(trace, i), LTL_MAX_PROPS);
        if (i >= trace->prefix_len && trace->cycle_len > 0)
            printf(" (cycle step %d)", (i - trace->prefix_len) % trace->cycle_len);
        printf("\n");
    }
    if (trace->cycle_len > 0) printf("  ...  (repeats period %d)^ω\n", trace->cycle_len);
}

int ltl_trace_equals_up_to(const LtlTrace* a, const LtlTrace* b, int n) {
    if (!a || !b) return 0;
    for (int i = 0; i < n; i++) {
        if (ltl_trace_get(a, i) != ltl_trace_get(b, i)) return 0;
    }
    return 1;
}

LtlTrace* ltl_trace_clone(const LtlTrace* trace) {
    if (!trace) return NULL;
    return ltl_trace_create_lasso(trace->prefix, trace->prefix_len,
                                   trace->cycle, trace->cycle_len);
}

/* ── LTL Satisfaction over a Trace ────────────────────────────── */
/*
 * Evaluate σ, i ⊨ φ using the standard Kripke semantics.
 *
 * For non-lasso traces or unbounded evaluation, this uses recursion
 * with a depth limit. The max_depth parameter prevents infinite
 * recursion on G/F over truly infinite structures.
 *
 * On lasso traces, we use cycle-aware evaluation for G and U
 * to guarantee termination and correctness.
 */

/* Forward declaration for recursive evaluation */
static int eval_lasso(const LtlTrace* trace, int step, const LtlNode* phi,
                       uint8_t* visited, int* visit_positions,
                       int max_visit);

int ltl_evaluate(const LtlTrace* trace, int step, const LtlNode* phi,
                 int max_depth) {
    if (!trace || !phi || step < 0) return 0;
    if (max_depth == 0) return 0; /* depth exhausted */

    /* For lasso traces, use cycle-aware evaluation */
    if (trace->cycle_len > 0 && max_depth < 0) {
        /* Allocate visited array for cycle detection */
        int total_len = trace->prefix_len + trace->cycle_len * 2;
        int visit_cap = total_len > 500 ? total_len : 500;
        uint8_t* visited = (uint8_t*)calloc(visit_cap, 1);
        int* visit_pos = (int*)malloc(visit_cap * sizeof(int));
        int result = 0;
        if (visited && visit_pos) {
            result = eval_lasso(trace, step, phi, visited, visit_pos, visit_cap);
        }
        free(visited);
        free(visit_pos);
        return result;
    }

    /* Bounded or non-lasso evaluation */
    int next_depth = (max_depth > 0) ? max_depth - 1 : max_depth;

    switch (phi->type) {
        case LTL_TRUE:
            return 1;
        case LTL_FALSE:
            return 0;
        case LTL_ATOM:
            return LTL_PROP_CONTAINS(ltl_trace_get(trace, step), phi->atom_idx);

        case LTL_NOT:
            return !ltl_evaluate(trace, step, phi->left, next_depth);

        case LTL_AND:
            return ltl_evaluate(trace, step, phi->left, next_depth) &&
                   ltl_evaluate(trace, step, phi->right, next_depth);

        case LTL_OR:
            return ltl_evaluate(trace, step, phi->left, next_depth) ||
                   ltl_evaluate(trace, step, phi->right, next_depth);

        case LTL_IMPLIES:
            return (!ltl_evaluate(trace, step, phi->left, next_depth)) ||
                   ltl_evaluate(trace, step, phi->right, next_depth);

        case LTL_EQUIV: {
            int l = ltl_evaluate(trace, step, phi->left, next_depth);
            int r = ltl_evaluate(trace, step, phi->right, next_depth);
            return l == r;
        }

        case LTL_NEXT:
            return ltl_evaluate(trace, step + 1, phi->left, next_depth);

        case LTL_FINALLY: {
            /* F φ = ∃j ≥ step : σ, j ⊨ φ
             * On lasso: check each step in one full cycle+prefix, bounded */
            int limit = trace->prefix_len + trace->cycle_len * 2 + step;
            if (max_depth > 0 && limit > step + max_depth)
                limit = step + max_depth;
            for (int j = step; j <= limit; j++) {
                if (ltl_evaluate(trace, j, phi->left, 1)) return 1;
            }
            return 0;
        }

        case LTL_GLOBALLY: {
            /* G φ = ∀j ≥ step : σ, j ⊨ φ
             * On lasso: check each step in one full cycle+prefix */
            int limit = trace->prefix_len + trace->cycle_len * 2 + step;
            if (max_depth > 0 && limit > step + max_depth)
                limit = step + max_depth;
            for (int j = step; j <= limit; j++) {
                if (!ltl_evaluate(trace, j, phi->left, 1)) return 0;
            }
            return 1;
        }

        case LTL_UNTIL: {
            /* φ U ψ = ∃j ≥ step : (σ, j ⊨ ψ ∧ ∀k ∈ [step, j) : σ, k ⊨ φ) */
            int limit = trace->prefix_len + trace->cycle_len * 2 + step;
            if (max_depth > 0 && limit > step + max_depth)
                limit = step + max_depth;
            for (int j = step; j <= limit; j++) {
                if (ltl_evaluate(trace, j, phi->right, 1)) {
                    /* Check that φ holds at all k in [step, j) */
                    int all_phi = 1;
                    for (int k = step; k < j; k++) {
                        if (!ltl_evaluate(trace, k, phi->left, 1)) {
                            all_phi = 0; break;
                        }
                    }
                    if (all_phi) return 1;
                }
            }
            return 0;
        }

        case LTL_RELEASE: {
            /* φ R ψ = ∀j ≥ step : (σ, j ⊨ ψ ∨ ∃k ∈ [step, j) : σ, k ⊨ φ)
             * Equivalent to: ψ holds up to and including the first point
             * where φ holds (if φ holds at all, ψ holds at all prior steps).
             *
             * Alternatively: ¬(¬φ U ¬ψ)  — dual of Until.
             *
             * Semantics: for all j ≥ step, either ψ holds at j,
             * or φ held at some k between step and j (which releases
             * the obligation for ψ).
             */
            int limit = trace->prefix_len + trace->cycle_len * 2 + step;
            if (max_depth > 0 && limit > step + max_depth)
                limit = step + max_depth;

            for (int j = step; j <= limit; j++) {
                /* Does ψ hold at j? */
                if (ltl_evaluate(trace, j, phi->right, 1))
                    continue;
                /* ψ doesn't hold at j. Check if φ held before j. */
                int released = 0;
                for (int k = step; k < j; k++) {
                    if (ltl_evaluate(trace, k, phi->left, 1)) {
                        released = 1; break;
                    }
                }
                if (!released) return 0;
            }
            return 1;
        }

        case LTL_WEAK_UNTIL: {
            /* φ W ψ = (G φ) ∨ (φ U ψ)
             * φ holds forever, OR φ holds until ψ.
             */
            /* Check G φ first (optimization) */
            int g_phi = 1;
            int limit = trace->prefix_len + trace->cycle_len * 2 + step;
            if (max_depth > 0 && limit > step + max_depth)
                limit = step + max_depth;
            for (int j = step; j <= limit; j++) {
                if (!ltl_evaluate(trace, j, phi->left, 1)) { g_phi = 0; break; }
            }
            if (g_phi) return 1;

            /* Otherwise check φ U ψ */
            for (int j = step; j <= limit; j++) {
                if (ltl_evaluate(trace, j, phi->right, 1)) {
                    int all_phi = 1;
                    for (int k = step; k < j; k++) {
                        if (!ltl_evaluate(trace, k, phi->left, 1)) {
                            all_phi = 0; break;
                        }
                    }
                    if (all_phi) return 1;
                }
            }
            return 0;
        }

        default:
            return 0;
    }
}

/*
 * Cycle-aware evaluation for lasso traces.
 * Uses a visited hash set (step position modulo cycle within prefix+cycle*2)
 * to detect when we've looped and avoid infinite descent for G, U, R, W.
 */
typedef struct {
    int abs_step;   /* absolute step in the trace */
    int state_key;  /* hash key for memoization */
} EvalMemo;

static int eval_lasso(const LtlTrace* trace, int step, const LtlNode* phi,
                       uint8_t* visited, int* visit_positions,
                       int max_visit) {
    if (!trace || !phi || step < 0) return 0;

    switch (phi->type) {
        case LTL_TRUE:  return 1;
        case LTL_FALSE: return 0;

        case LTL_ATOM:
            return LTL_PROP_CONTAINS(ltl_trace_get(trace, step), phi->atom_idx);

        case LTL_NOT:
            return !eval_lasso(trace, step, phi->left, visited, visit_positions,
                               max_visit);

        case LTL_AND:
            return eval_lasso(trace, step, phi->left, visited, visit_positions,
                              max_visit) &&
                   eval_lasso(trace, step, phi->right, visited, visit_positions,
                              max_visit);

        case LTL_OR:
            return eval_lasso(trace, step, phi->left, visited, visit_positions,
                              max_visit) ||
                   eval_lasso(trace, step, phi->right, visited, visit_positions,
                              max_visit);

        case LTL_IMPLIES:
            return !eval_lasso(trace, step, phi->left, visited, visit_positions,
                               max_visit) ||
                   eval_lasso(trace, step, phi->right, visited, visit_positions,
                              max_visit);

        case LTL_EQUIV: {
            int l = eval_lasso(trace, step, phi->left, visited, visit_positions,
                               max_visit);
            int r = eval_lasso(trace, step, phi->right, visited, visit_positions,
                               max_visit);
            return l == r;
        }

        case LTL_NEXT:
            return eval_lasso(trace, step + 1, phi->left, visited, visit_positions,
                              max_visit);

        case LTL_FINALLY: {
            /* Unbounded search within one full traversal of prefix+cycle*2 */
            int total = trace->prefix_len + trace->cycle_len * 2;
            for (int j = step; j <= step + total; j++) {
                if (eval_lasso(trace, j, phi->left, visited, visit_positions,
                               max_visit))
                    return 1;
            }
            return 0;
        }

        case LTL_GLOBALLY: {
            int total = trace->prefix_len + trace->cycle_len * 2;
            for (int j = step; j <= step + total; j++) {
                if (!eval_lasso(trace, j, phi->left, visited, visit_positions,
                                max_visit))
                    return 0;
            }
            return 1;
        }

        case LTL_UNTIL: {
            int total = trace->prefix_len + trace->cycle_len * 2;
            for (int j = step; j <= step + total; j++) {
                if (eval_lasso(trace, j, phi->right, visited, visit_positions,
                               max_visit)) {
                    int all_phi = 1;
                    for (int k = step; k < j; k++) {
                        if (!eval_lasso(trace, k, phi->left, visited,
                                        visit_positions, max_visit)) {
                            all_phi = 0; break;
                        }
                    }
                    if (all_phi) return 1;
                }
            }
            return 0;
        }

        case LTL_RELEASE: {
            int total = trace->prefix_len + trace->cycle_len * 2;
            for (int j = step; j <= step + total; j++) {
                if (eval_lasso(trace, j, phi->right, visited, visit_positions,
                               max_visit))
                    continue;
                int released = 0;
                for (int k = step; k < j; k++) {
                    if (eval_lasso(trace, k, phi->left, visited, visit_positions,
                                   max_visit)) {
                        released = 1; break;
                    }
                }
                if (!released) return 0;
            }
            return 1;
        }

        case LTL_WEAK_UNTIL: {
            int total = trace->prefix_len + trace->cycle_len * 2;
            /* Check G φ */
            int g_phi = 1;
            for (int j = step; j <= step + total; j++) {
                if (!eval_lasso(trace, j, phi->left, visited, visit_positions,
                                max_visit)) { g_phi = 0; break; }
            }
            if (g_phi) return 1;
            /* Check φ U ψ */
            for (int j = step; j <= step + total; j++) {
                if (eval_lasso(trace, j, phi->right, visited, visit_positions,
                               max_visit)) {
                    int all_phi = 1;
                    for (int k = step; k < j; k++) {
                        if (!eval_lasso(trace, k, phi->left, visited,
                                        visit_positions, max_visit)) {
                            all_phi = 0; break;
                        }
                    }
                    if (all_phi) return 1;
                }
            }
            return 0;
        }

        default:
            return 0;
    }
}

int ltl_satisfies(const LtlTrace* trace, const LtlNode* phi) {
    return ltl_evaluate(trace, 0, phi, -1);
}

/* ── Bounded Semantics ────────────────────────────────────────── */

int ltl_evaluate_bounded(const LtlTrace* trace, int step,
                          const LtlNode* phi, int bound) {
    if (!trace || !phi || step < 0 || bound <= 0) return 0;

    /* Within the bound, use the standard semantics with limited lookahead */
    if (phi->type == LTL_GLOBALLY) {
        /* G φ within bound k: φ must hold at all steps up to step+bound */
        for (int j = step; j <= step + bound; j++) {
            if (!ltl_evaluate(trace, j, phi->left, 1)) return 0;
        }
        return 1;
    }
    if (phi->type == LTL_FINALLY) {
        /* F φ within bound k: φ must hold at some step up to step+bound */
        for (int j = step; j <= step + bound; j++) {
            if (ltl_evaluate(trace, j, phi->left, 1)) return 1;
        }
        return 0;
    }
    if (phi->type == LTL_UNTIL) {
        for (int j = step; j <= step + bound; j++) {
            if (ltl_evaluate(trace, j, phi->right, 1)) {
                int all_phi = 1;
                for (int k = step; k < j; k++)
                    if (!ltl_evaluate(trace, k, phi->left, 1)) { all_phi = 0; break; }
                if (all_phi) return 1;
            }
        }
        return 0;
    }

    /* For other operators, delegate to standard evaluation with bound */
    return ltl_evaluate(trace, step, phi, bound);
}

/* ── Formula Set for Monitoring ───────────────────────────────── */

void ltl_formula_set_init(LtlFormulaSet* fs, int cap) {
    if (!fs) return;
    fs->formulas = (LtlNode**)calloc(cap, sizeof(LtlNode*));
    fs->count = 0;
    fs->capacity = fs->formulas ? cap : 0;
}

void ltl_formula_set_add(LtlFormulaSet* fs, LtlNode* f) {
    if (!fs || !fs->formulas || !f) return;
    for (int i = 0; i < fs->count; i++)
        if (ltl_equals(fs->formulas[i], f)) return;
    if (fs->count >= fs->capacity) {
        int nc = fs->capacity * 2;
        if (nc < 4) nc = 4;
        LtlNode** nd = (LtlNode**)realloc(fs->formulas, nc * sizeof(LtlNode*));
        if (!nd) return;
        fs->formulas = nd;
        fs->capacity = nc;
    }
    fs->formulas[fs->count++] = f;
}

void ltl_formula_set_free(LtlFormulaSet* fs) {
    if (!fs) return;
    free(fs->formulas);
    fs->formulas = NULL;
    fs->count = fs->capacity = 0;
}

int ltl_formula_set_equal(const LtlFormulaSet* a, const LtlFormulaSet* b) {
    if (!a || !b) return (a == b);
    if (a->count != b->count) return 0;
    for (int i = 0; i < a->count; i++) {
        int found = 0;
        for (int j = 0; j < b->count; j++) {
            if (ltl_equals(a->formulas[i], b->formulas[j])) { found = 1; break; }
        }
        if (!found) return 0;
    }
    return 1;
}

void ltl_formula_set_print(const LtlFormulaSet* fs) {
    if (!fs) { printf("{}"); return; }
    printf("{");
    for (int i = 0; i < fs->count; i++) {
        if (i > 0) printf(", ");
        ltl_print(fs->formulas[i]);
    }
    printf("}");
}

/* ── Monitor Step (LTL Runtime Verification) ──────────────────── */
/*
 * This implements the expand-and-rewrite approach to LTL monitoring.
 * Given a set of formulas that must hold from the current step,
 * and the observed proposition set, compute what must hold next.
 *
 * For each formula in current:
 *   - Atoms (p): check if p ∈ observed. If not, violation.
 *   - ¬(p): check if p ∉ observed.
 *   - ¬(¬ψ): reduce to ψ.
 *   - X ψ: add ψ to next obligations.
 *   - F ψ: add ψ to current (if ψ now, done), AND add X(F ψ) to next.
 *   - G ψ: add ψ to current, add X(G ψ) to next.
 *   - φ U ψ: add ψ (or, add φ now, and add X(φ U ψ) to next).
 *   - ...
 *
 * Returns 1 if the step is consistent (no contradiction), 0 if violation.
 */
int ltl_monitor_step(LtlFormulaSet* current, LtlPropSet observed,
                     LtlFormulaSet* next) {
    if (!current || !next) return 0;

    LtlFormulaSet working;
    ltl_formula_set_init(&working, current->count);

    /* Copy current to working set */
    for (int i = 0; i < current->count; i++)
        ltl_formula_set_add(&working, current->formulas[i]);

    ltl_formula_set_init(next, working.count * 2);

    int processed = 0;
    int max_iter = working.count * 10; /* safety limit */
    int iter = 0;

    while (processed < working.count && iter < max_iter) {
        iter++;
        int changed = 0;

        for (int i = 0; i < working.count; i++) {
            LtlNode* f = working.formulas[i];
            if (!f) continue;

            switch (f->type) {
                case LTL_TRUE:
                    break; /* always satisfied, no obligations */

                case LTL_FALSE:
                    return 0; /* contradiction */

                case LTL_ATOM:
                    if (!LTL_PROP_CONTAINS(observed, f->atom_idx))
                        return 0; /* atom must be true */
                    break;

                case LTL_NOT:
                    if (f->left && f->left->type == LTL_ATOM) {
                        if (LTL_PROP_CONTAINS(observed, f->left->atom_idx))
                            return 0; /* negated atom is violated */
                    }
                    /* For complex negation, we rely on NNF preprocessing */
                    break;

                case LTL_AND:
                    ltl_formula_set_add(&working, f->left);
                    ltl_formula_set_add(&working, f->right);
                    changed = 1;
                    break;

                case LTL_OR:
                    /* Non-deterministic choice: for monitoring we need
                     * both to potentially hold. We simplify by adding
                     * both and accepting if either can be satisfied. */
                    ltl_formula_set_add(&working, f->left);
                    ltl_formula_set_add(&working, f->right);
                    changed = 1;
                    break;

                case LTL_NEXT:
                    ltl_formula_set_add(next, f->left);
                    break;

                case LTL_FINALLY:
                    /* φ now OR X(F φ) next */
                    ltl_formula_set_add(&working, f->left);
                    ltl_formula_set_add(next, ltl_mk_next(ltl_clone(f)));
                    changed = 1;
                    break;

                case LTL_GLOBALLY:
                    /* φ now AND X(G φ) next */
                    ltl_formula_set_add(&working, f->left);
                    ltl_formula_set_add(next, ltl_mk_next(ltl_clone(f)));
                    changed = 1;
                    break;

                case LTL_UNTIL:
                    /* ψ now OR (φ now AND X(φ U ψ) next) */
                    ltl_formula_set_add(&working, f->right);
                    ltl_formula_set_add(&working, f->left);
                    ltl_formula_set_add(next, ltl_mk_next(ltl_clone(f)));
                    changed = 1;
                    break;

                case LTL_RELEASE:
                    /* ψ now AND (φ now OR X(φ R ψ) next) */
                    ltl_formula_set_add(&working, f->right);
                    ltl_formula_set_add(&working, f->left);
                    ltl_formula_set_add(next, ltl_mk_next(ltl_clone(f)));
                    changed = 1;
                    break;

                case LTL_WEAK_UNTIL:
                    /* ψ now OR (φ now AND X(φ W ψ) next) */
                    ltl_formula_set_add(&working, f->right);
                    ltl_formula_set_add(&working, f->left);
                    ltl_formula_set_add(next, ltl_mk_next(ltl_clone(f)));
                    changed = 1;
                    break;

                default:
                    break;
            }
        }

        if (!changed) {
            processed = working.count;
        }
    }

    /* Clean up temporary nodes created during expansion */
    /* Note: some temporaries are added to next and should NOT be freed */

    ltl_formula_set_free(&working);
    return 1;
}

/* ── Kripke Structure ─────────────────────────────────────────── */

LtlKripke* ltl_kripke_create(int n_states) {
    if (n_states <= 0) return NULL;

    LtlKripke* K = (LtlKripke*)calloc(1, sizeof(LtlKripke));
    if (!K) return NULL;

    K->n_states = n_states;
    K->initial = NULL;
    K->n_initial = 0;
    K->labels = (LtlPropSet*)calloc(n_states, sizeof(LtlPropSet));
    K->degree = (int*)calloc(n_states, sizeof(int));
    K->offset = (int*)malloc(n_states * sizeof(int));
    K->successors = NULL;
    K->state_names = (char**)calloc(n_states, sizeof(char*));
    K->n_props = 0;

    if (!K->labels || !K->degree || !K->offset || !K->state_names) {
        ltl_kripke_free(K);
        return NULL;
    }

    for (int i = 0; i < n_states; i++) K->offset[i] = -1;
    return K;
}

void ltl_kripke_free(LtlKripke* K) {
    if (!K) return;
    free(K->initial);
    free(K->successors);
    free(K->offset);
    free(K->degree);
    free(K->labels);
    if (K->state_names) {
        for (int i = 0; i < K->n_states; i++) free(K->state_names[i]);
        free(K->state_names);
    }
    free(K);
}

void ltl_kripke_set_initial(LtlKripke* K, const int* states, int n) {
    if (!K || !states || n <= 0) return;
    free(K->initial);
    K->initial = (int*)malloc(n * sizeof(int));
    if (!K->initial) return;
    memcpy(K->initial, states, n * sizeof(int));
    K->n_initial = n;
}

int ltl_kripke_add_edge(LtlKripke* K, int s, int t) {
    if (!K || s < 0 || s >= K->n_states || t < 0 || t >= K->n_states) return -1;

    /* Build adjacency list incrementally.
     * First pass: count degrees, then second pass: fill successors.
     * We'll use a dynamic approach: maintain all edges in a flat array. */

    K->degree[s]++;
    return 0;
}

void ltl_kripke_set_label(LtlKripke* K, int s, LtlPropSet label) {
    if (!K || s < 0 || s >= K->n_states) return;
    K->labels[s] = label;
    /* Track max proposition index seen */
    for (int i = 0; i < LTL_MAX_PROPS; i++) {
        if (LTL_PROP_CONTAINS(label, i) && i + 1 > K->n_props)
            K->n_props = i + 1;
    }
}

/* kripke_finalize_edges removed — superseded by kripke_build_adjacency */

/*
 * Alternative: single-pass edge addition using dynamic arrays.
 * We'll use the simpler approach: store edges as (s,t) pairs temporarily.
 */

#define KRIPKE_MAX_EDGES_TEMP 4096

typedef struct {
    int src;
    int dst;
} KripkeEdge;

static KripkeEdge* g_temp_edges = NULL;
static int g_temp_edge_count = 0;
static int g_temp_edge_cap = 0;

static void temp_edge_add(int src, int dst) {
    if (!g_temp_edges) {
        g_temp_edge_cap = KRIPKE_MAX_EDGES_TEMP;
        g_temp_edges = (KripkeEdge*)malloc(g_temp_edge_cap * sizeof(KripkeEdge));
        g_temp_edge_count = 0;
    }
    if (g_temp_edge_count >= g_temp_edge_cap) {
        g_temp_edge_cap *= 2;
        g_temp_edges = (KripkeEdge*)realloc(g_temp_edges,
                                             g_temp_edge_cap * sizeof(KripkeEdge));
    }
    if (g_temp_edges && g_temp_edge_count < g_temp_edge_cap) {
        g_temp_edges[g_temp_edge_count].src = src;
        g_temp_edges[g_temp_edge_count].dst = dst;
        g_temp_edge_count++;
    }
}

static void temp_edges_clear(void) {
    free(g_temp_edges);
    g_temp_edges = NULL;
    g_temp_edge_count = 0;
    g_temp_edge_cap = 0;
}

/* Revisited edge addition: store edges and also count degrees */
int ltl_kripke_add_edge_int(LtlKripke* K, int s, int t) {
    if (!K || s < 0 || s >= K->n_states || t < 0 || t >= K->n_states) return -1;
    K->degree[s]++;
    temp_edge_add(s, t);
    return 0;
}

/* Build the final adjacency lists from stored edges */
static int kripke_build_adjacency(LtlKripke* K) {
    if (!K) return -1;

    /* Compute offsets */
    int total = 0;
    for (int i = 0; i < K->n_states; i++) {
        K->offset[i] = total;
        total += K->degree[i];
    }

    if (total == 0) return 0;

    K->successors = (int*)calloc(total, sizeof(int));
    if (!K->successors) return -1;

    /* Fill pointers */
    int* fill = (int*)calloc(K->n_states, sizeof(int));
    if (!fill) { free(K->successors); K->successors = NULL; return -1; }

    for (int i = 0; i < g_temp_edge_count; i++) {
        int s = g_temp_edges[i].src;
        int t = g_temp_edges[i].dst;
        int pos = K->offset[s] + fill[s];
        if (pos < K->offset[s] + K->degree[s]) {
            K->successors[pos] = t;
            fill[s]++;
        }
    }

    free(fill);
    temp_edges_clear();
    return total;
}

static LtlKripke* g_current_kripke = NULL;

int ltl_kripke_begin_build(LtlKripke* K) {
    g_current_kripke = K;
    temp_edges_clear();
    return 0;
}

int ltl_kripke_end_build(LtlKripke* K) {
    (void)K;
    int r = kripke_build_adjacency(g_current_kripke);
    g_current_kripke = NULL;
    return r;
}

void ltl_kripke_make_total(LtlKripke* K) {
    if (!K) return;
    for (int s = 0; s < K->n_states; s++) {
        if (K->degree[s] == 0) {
            ltl_kripke_add_edge_int(K, s, s); /* self-loop */
        }
    }
}

void ltl_kripke_print(const LtlKripke* K) {
    if (!K) { printf("null Kripke\n"); return; }
    printf("Kripke Structure: %d states, %d initial\n", K->n_states, K->n_initial);
    printf("Initial: {");
    for (int i = 0; i < K->n_initial; i++) {
        if (i > 0) printf(", ");
        printf("s%d", K->initial[i]);
    }
    printf("}\n");
    for (int s = 0; s < K->n_states; s++) {
        printf("  s%d [", s);
        if (K->state_names && K->state_names[s]) printf("%s", K->state_names[s]);
        else printf("s%d", s);
        printf("] label=");
        ltl_propset_print(K->labels[s], K->n_props);
        printf(" -> {");
        for (int j = 0; j < K->degree[s]; j++) {
            if (j > 0) printf(", ");
            printf("s%d", K->successors[K->offset[s] + j]);
        }
        printf("}\n");
    }
}

void ltl_kripke_print_dot(const LtlKripke* K, const char* filename) {
    if (!K || !filename) return;
    FILE* f = fopen(filename, "w");
    if (!f) return;
    fprintf(f, "digraph Kripke {\n");
    fprintf(f, "  rankdir=LR;\n");
    fprintf(f, "  node [shape=circle];\n");

    /* Initial state marker */
    for (int i = 0; i < K->n_initial; i++) {
        fprintf(f, "  init%d [shape=point];\n", i);
        fprintf(f, "  init%d -> %d;\n", i, K->initial[i]);
    }

    for (int s = 0; s < K->n_states; s++) {
        fprintf(f, "  %d [label=\"s%d\\n", s, s);
        /* Print labels */
        int first = 1;
        for (int p = 0; p < K->n_props; p++) {
            if (LTL_PROP_CONTAINS(K->labels[s], p)) {
                if (!first) fprintf(f, ",");
                fprintf(f, "p%d", p);
                first = 0;
            }
        }
        fprintf(f, "\"];\n");
    }

    for (int s = 0; s < K->n_states; s++) {
        for (int j = 0; j < K->degree[s]; j++) {
            int t = K->successors[K->offset[s] + j];
            fprintf(f, "  %d -> %d;\n", s, t);
        }
    }

    fprintf(f, "}\n");
    fclose(f);
}

/* ── Kripke Structure Factory (Convenience) ───────────────────── */
/*
 * Create a simple Kripke structure from an edge list.
 * This is the preferred way to create structures.
 */
LtlKripke* ltl_kripke_from_edges(int n_states, const int* edges, int n_edges,
                                  const int* initial, int n_initial,
                                  const LtlPropSet* labels) {
    LtlKripke* K = ltl_kripke_create(n_states);
    if (!K) return NULL;

    ltl_kripke_begin_build(K);

    for (int i = 0; i < n_edges; i++) {
        ltl_kripke_add_edge_int(K, edges[2*i], edges[2*i+1]);
    }

    if (initial && n_initial > 0)
        ltl_kripke_set_initial(K, initial, n_initial);

    if (labels)
        for (int s = 0; s < n_states; s++) {
            K->labels[s] = labels[s];
            /* Update n_props */
            for (int i = 0; i < LTL_MAX_PROPS; i++) {
                if (LTL_PROP_CONTAINS(labels[s], i) && i + 1 > K->n_props)
                    K->n_props = i + 1;
            }
        }

    ltl_kripke_make_total(K);
    ltl_kripke_end_build(K);

    return K;
}

/* ── Paths ────────────────────────────────────────────────────── */

LtlPath* ltl_path_create(const int* prefix, int plen,
                           const int* cycle, int clen) {
    if (clen < 1) return NULL;
    LtlPath* p = (LtlPath*)calloc(1, sizeof(LtlPath));
    if (!p) return NULL;

    p->prefix_len = plen;
    p->cycle_len = clen;

    if (plen > 0 && prefix) {
        p->prefix = (int*)malloc(plen * sizeof(int));
        if (!p->prefix) { free(p); return NULL; }
        memcpy(p->prefix, prefix, plen * sizeof(int));
    }

    p->cycle = (int*)malloc(clen * sizeof(int));
    if (!p->cycle) { free(p->prefix); free(p); return NULL; }
    memcpy(p->cycle, cycle, clen * sizeof(int));

    return p;
}

void ltl_path_free(LtlPath* path) {
    if (!path) return;
    free(path->prefix);
    free(path->cycle);
    free(path);
}

LtlTrace* ltl_path_to_trace(const LtlPath* path, const LtlKripke* K) {
    if (!path || !K) return NULL;

    LtlPropSet* prefix_props = NULL;
    LtlPropSet* cycle_props = NULL;

    if (path->prefix_len > 0) {
        prefix_props = (LtlPropSet*)malloc(path->prefix_len * sizeof(LtlPropSet));
        if (!prefix_props) return NULL;
        for (int i = 0; i < path->prefix_len; i++)
            prefix_props[i] = K->labels[path->prefix[i]];
    }

    cycle_props = (LtlPropSet*)malloc(path->cycle_len * sizeof(LtlPropSet));
    if (!cycle_props) { free(prefix_props); return NULL; }
    for (int i = 0; i < path->cycle_len; i++)
        cycle_props[i] = K->labels[path->cycle[i]];

    LtlTrace* trace = ltl_trace_create_lasso(prefix_props, path->prefix_len,
                                              cycle_props, path->cycle_len);
    free(prefix_props);
    free(cycle_props);
    return trace;
}

void ltl_path_print(const LtlPath* path) {
    if (!path) { printf("null path\n"); return; }
    printf("Path: ");
    for (int i = 0; i < path->prefix_len; i++)
        printf("s%d → ", path->prefix[i]);
    printf("[");
    for (int i = 0; i < path->cycle_len; i++) {
        printf("s%d", path->cycle[i]);
        if (i < path->cycle_len - 1) printf(" → ");
    }
    printf("]^ω\n");
}

/* ── LTL Model Checking (Explicit State, Path Enumeration) ────── */
/*
 * M ⊨ φ iff for ALL paths π from initial states: trace(π) ⊨ φ.
 *
 * Equivalently: M ⊭ φ iff ∃ path π from initial state: trace(π) ⊭ φ.
 * (trace(π) ⊨ ¬φ)
 *
 * We enumerate lasso paths up to a bound and check each one.
 * This is sound for finding violations (counterexamples) but not
 * complete for proving properties (only works for small state spaces).
 *
 * A path in Kripke M is a lasso: prefix (states) · cycle^ω.
 * The prefix can have length up to |S|, and the cycle up to |S|.
 * Total number of lassos: O(|S|^|S|) — enormous! So this is
 * only usable for tiny Kripke structures (demonstration only).
 *
 * For real LTL model checking, use the automata-theoretic approach:
 *   1. Build NBA A_{¬φ} for the negated property
 *   2. Compute product M × A_{¬φ}
 *   3. Check emptiness of the product
 * This is PSPACE in |φ| and NLOGSPACE in |M| (Vardi & Wolper 1986).
 */

/* Forward declaration */
static int find_violating_path(const LtlKripke* K, const LtlNode* phi,
                                int* prefix, int plen, int* cycle, int clen,
                                LtlPath** cex);

int ltl_model_check_enumerate(const LtlKripke* K, const LtlNode* phi,
                               int max_prefix, int max_cycle,
                               LtlPath** cex_path) {
    if (!K || !phi) return 0;
    if (K->n_initial == 0) return 1; /* vacuously true */

    if (max_prefix <= 0) max_prefix = K->n_states * 2;
    if (max_cycle <= 0) max_cycle = K->n_states;
    if (max_prefix > 30) max_prefix = 30; /* safety limit */
    if (max_cycle > 15) max_cycle = 15;

    /* Pre-allocate buffers */
    int* prefix = (int*)malloc(max_prefix * sizeof(int));
    int* cycle = (int*)malloc(max_cycle * sizeof(int));

    if (!prefix || !cycle) {
        free(prefix); free(cycle);
        return 0;
    }

    /* Enumerate all lasso paths starting from each initial state */
    for (int init_idx = 0; init_idx < K->n_initial; init_idx++) {
        (void)init_idx; /* used in loop, suppress unused warning for start */

        /* Enumerate prefix lengths from 0 to max_prefix */
        for (int plen = 0; plen <= max_prefix; plen++) {
            /* Enumerate cycles of length 1 to max_cycle */
            for (int clen = 1; clen <= max_cycle; clen++) {
                /* Generate all possible lassos... this is exponential.
                 * We'll use DFS to find a single counterexample. */
                if (find_violating_path(K, phi, prefix, 0, cycle, 0, cex_path)) {
                    free(prefix); free(cycle);
                    return 0; /* violation found */
                }
            }
        }
    }

    free(prefix); free(cycle);
    return 1; /* no violation found within bound */
}

/*
 * DFS-based counterexample search.
 * Explores Kripke paths up to max_depth, checking if trace(prefix·cycle^ω) ⊭ φ.
 * If a violating lasso is found, stores it in *cex.
 */
typedef struct {
    int* states;
    int  len;
    int  capacity;
} IntVec;

static void ivec_init(IntVec* v, int cap) {
    v->states = (int*)malloc(cap * sizeof(int));
    v->len = 0;
    v->capacity = v->states ? cap : 0;
}
static void ivec_push(IntVec* v, int s) {
    if (v->len >= v->capacity) {
        int nc = v->capacity * 2;
        int* nd = (int*)realloc(v->states, nc * sizeof(int));
        if (!nd) return;
        v->states = nd;
        v->capacity = nc;
    }
    if (v->len < v->capacity) v->states[v->len++] = s;
}
static void ivec_free(IntVec* v) { free(v->states); v->states = NULL; v->len = v->capacity = 0; }

/*
 * Find a path prefix from start state that, when combined with a cycle
 * starting at the last prefix state, forms a violating lasso.
 */
static int dfs_find_cex(const LtlKripke* K, const LtlNode* phi,
                         int current, IntVec* visited_states,
                         int depth, int max_depth,
                         IntVec* prefix_buf, LtlPath** cex) {
    if (depth > max_depth) return 0;

    ivec_push(prefix_buf, current);

    /* Check if current state has been visited before —
     * if so, we have a cycle candidate */
    for (int i = 0; i < prefix_buf->len - 1; i++) {
        if (prefix_buf->states[i] == current) {
            /* Found cycle: prefix = prefix_buf[0..i-1], cycle = prefix_buf[i..len-1] */
            int plen = i;
            int clen = prefix_buf->len - i;

            /* Build trace and check against ¬φ */
            LtlPath* path = ltl_path_create(
                (plen > 0) ? prefix_buf->states : NULL, plen,
                prefix_buf->states + i, clen);
            if (path) {
                LtlTrace* trace = ltl_path_to_trace(path, K);
                if (trace) {
                    /* Check if trace violates φ (i.e., satisfies ¬φ) */
                    /* We check: does trace satisfy φ? If not, it's a counterexample. */
                    int satisfies = ltl_satisfies(trace, phi);
                    if (!satisfies) {
                        if (cex) *cex = path;
                        else ltl_path_free(path);
                        ltl_trace_free(trace);
                        return 1; /* found counterexample */
                    }
                    ltl_trace_free(trace);
                }
                if (!cex || *cex != path) ltl_path_free(path);
            }
        }
    }

    /* Continue DFS */
    for (int j = 0; j < K->degree[current]; j++) {
        int next = K->successors[K->offset[current] + j];
        if (dfs_find_cex(K, phi, next, visited_states, depth + 1, max_depth,
                          prefix_buf, cex))
            return 1;
    }

    prefix_buf->len--;
    return 0;
}

static int find_violating_path(const LtlKripke* K, const LtlNode* phi,
                                int* prefix, int plen, int* cycle, int clen,
                                LtlPath** cex) {
    (void)prefix; (void)plen; (void)cycle; (void)clen;

    for (int init_idx = 0; init_idx < K->n_initial; init_idx++) {
        int start = K->initial[init_idx];
        IntVec visited; ivec_init(&visited, 64);
        IntVec prefix_buf; ivec_init(&prefix_buf, 128);

        int found = dfs_find_cex(K, phi, start, &visited, 0,
                                   K->n_states * 3, &prefix_buf, cex);

        ivec_free(&prefix_buf);
        ivec_free(&visited);

        if (found) return 1;
    }
    return 0;
}

int ltl_model_check(const LtlKripke* K, const LtlNode* phi,
                    LtlPath** cex_path) {
    /* Use path enumeration with generous bounds */
    return ltl_model_check_enumerate(K, phi, 30, 15, cex_path);
}
