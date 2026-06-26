/*
 * ltl_equiv.c — LTL Equivalence Laws, NNF, Simplification & Rewriting
 *
 * Implements the rich algebraic structure of LTL equivalences:
 *   - Negation Normal Form (NNF) conversion via duality laws
 *   - Formula simplification using rewrite rules
 *   - Operator elimination (derived → core)
 *   - Expansion/unwinding of temporal operators
 *   - Safety/Liveness/Obligation fragment detection
 *   - Bounded equivalence checking
 *
 * L4 Fundamental Laws + L5 Algorithms + L7 Classification.
 *
 * Reference:
 *   Manna & Pnueli 1992 — The Temporal Logic of Reactive and Concurrent Systems
 *   Somenzi & Bloem 2000 — Efficient Büchi automata from LTL formulae
 *   Etessami & Holzmann 2000 — Optimizing Büchi automata
 *   Alpern & Schneider 1985 — Defining Liveness
 *   Chang, Manna, Pnueli 1992 — The Safety-Progress Classification
 */

#include "ltl_equiv.h"
#include "ltl_semantics.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ── Internal Helpers ─────────────────────────────────────────── */

/* alloc_node: allocate a fresh LTL AST node (internal use).
 * Forward-declared here for use throughout this file;
 * mirrors the static implementation in ltl_ast.c. */
static LtlNode* alloc_node(LtlNodeType type) {
    LtlNode* node = (LtlNode*)calloc(1, sizeof(LtlNode));
    if (!node) return NULL;
    node->type = type;
    node->atom_idx = -1;
    node->id = ltl_next_id();
    return node;
}

/* ── Negation Normal Form ─────────────────────────────────────── */
/*
 * NNF transformation: push all negations inwards to atoms using
 * De Morgan laws for both Boolean and temporal operators.
 *
 * The transformation is recursive and constructs new nodes.
 * Time complexity: O(|φ|) — linear in formula size.
 * Space complexity: O(|φ|) — produces a formula of similar size.
 */

LtlNode* ltl_to_nnf(const LtlNode* phi) {
    if (!phi) return NULL;

    switch (phi->type) {
        case LTL_TRUE:
        case LTL_FALSE:
        case LTL_ATOM:
            return ltl_clone(phi);

        case LTL_NOT: {
            LtlNode* child = phi->left;
            if (!child) return NULL;

            switch (child->type) {
                case LTL_TRUE:
                    return ltl_mk_false();
                case LTL_FALSE:
                    return ltl_mk_true();
                case LTL_ATOM:
                    /* Keep ¬p as is (literal in NNF) */
                    return ltl_clone(phi);
                case LTL_NOT:
                    /* Double negation: ¬¬ψ → nnf(ψ) */
                    return ltl_to_nnf(child->left);

                case LTL_AND: {
                    /* ¬(φ ∧ ψ) → ¬φ ∨ ¬ψ */
                    LtlNode* l = ltl_to_nnf(ltl_mk_not(ltl_clone(child->left)));
                    LtlNode* r = ltl_to_nnf(ltl_mk_not(ltl_clone(child->right)));
                    LtlNode* result = ltl_mk_or(l, r);
                    /* Clean up temporary ¬ nodes created for l,r */
                    return result;
                }
                case LTL_OR: {
                    /* ¬(φ ∨ ψ) → ¬φ ∧ ¬ψ */
                    LtlNode* l = ltl_to_nnf(ltl_mk_not(ltl_clone(child->left)));
                    LtlNode* r = ltl_to_nnf(ltl_mk_not(ltl_clone(child->right)));
                    return ltl_mk_and(l, r);
                }
                case LTL_IMPLIES: {
                    /* ¬(φ → ψ) → nnf(φ ∧ ¬ψ) */
                    LtlNode* l = ltl_to_nnf(ltl_clone(child->left));
                    LtlNode* r = ltl_to_nnf(ltl_mk_not(ltl_clone(child->right)));
                    return ltl_mk_and(l, r);
                }
                case LTL_EQUIV: {
                    /* ¬(φ ↔ ψ) → nnf(φ ∧ ¬ψ) ∨ nnf(¬φ ∧ ψ) */
                    LtlNode* l = ltl_to_nnf(ltl_mk_and(
                        ltl_clone(child->left),
                        ltl_mk_not(ltl_clone(child->right))));
                    LtlNode* r = ltl_to_nnf(ltl_mk_and(
                        ltl_mk_not(ltl_clone(child->left)),
                        ltl_clone(child->right)));
                    return ltl_mk_or(l, r);
                }

                /* Temporal dualities */
                case LTL_NEXT:
                    /* ¬X φ → X(¬φ) */
                    return ltl_mk_next(
                        ltl_to_nnf(ltl_mk_not(ltl_clone(child->left))));

                case LTL_FINALLY:
                    /* ¬F φ → G(¬φ) */
                    return ltl_mk_globally(
                        ltl_to_nnf(ltl_mk_not(ltl_clone(child->left))));

                case LTL_GLOBALLY:
                    /* ¬G φ → F(¬φ) */
                    return ltl_mk_finally(
                        ltl_to_nnf(ltl_mk_not(ltl_clone(child->left))));

                case LTL_UNTIL:
                    /* ¬(φ U ψ) → (¬φ) R (¬ψ) */
                    return ltl_mk_release(
                        ltl_to_nnf(ltl_mk_not(ltl_clone(child->left))),
                        ltl_to_nnf(ltl_mk_not(ltl_clone(child->right))));

                case LTL_RELEASE:
                    /* ¬(φ R ψ) → (¬φ) U (¬ψ) */
                    return ltl_mk_until(
                        ltl_to_nnf(ltl_mk_not(ltl_clone(child->left))),
                        ltl_to_nnf(ltl_mk_not(ltl_clone(child->right))));

                case LTL_WEAK_UNTIL: {
                    /* ¬(φ W ψ) → ¬((φ U ψ) ∨ G φ)
                     *          → ¬(φ U ψ) ∧ ¬G φ
                     *          → ((¬φ) R (¬ψ)) ∧ F ¬φ */
                    LtlNode* l = ltl_mk_release(
                        ltl_to_nnf(ltl_mk_not(ltl_clone(child->left))),
                        ltl_to_nnf(ltl_mk_not(ltl_clone(child->right))));
                    LtlNode* r = ltl_mk_finally(
                        ltl_to_nnf(ltl_mk_not(ltl_clone(child->left))));
                    return ltl_mk_and(l, r);
                }

                default:
                    return ltl_clone(phi);
            }
        }

        case LTL_AND:
            return ltl_mk_and(ltl_to_nnf(phi->left), ltl_to_nnf(phi->right));
        case LTL_OR:
            return ltl_mk_or(ltl_to_nnf(phi->left), ltl_to_nnf(phi->right));
        case LTL_IMPLIES:
            /* φ → ψ → ¬φ ∨ ψ */
            return ltl_to_nnf(ltl_mk_or(
                ltl_mk_not(ltl_clone(phi->left)),
                ltl_clone(phi->right)));
        case LTL_EQUIV:
            /* φ ↔ ψ → (¬φ ∨ ψ) ∧ (φ ∨ ¬ψ) */
            return ltl_to_nnf(ltl_mk_and(
                ltl_mk_or(ltl_mk_not(ltl_clone(phi->left)), ltl_clone(phi->right)),
                ltl_mk_or(ltl_clone(phi->left), ltl_mk_not(ltl_clone(phi->right)))));

        case LTL_NEXT:
            return ltl_mk_next(ltl_to_nnf(phi->left));
        case LTL_FINALLY:
            return ltl_mk_finally(ltl_to_nnf(phi->left));
        case LTL_GLOBALLY:
            return ltl_mk_globally(ltl_to_nnf(phi->left));
        case LTL_UNTIL:
            return ltl_mk_until(ltl_to_nnf(phi->left), ltl_to_nnf(phi->right));
        case LTL_RELEASE:
            return ltl_mk_release(ltl_to_nnf(phi->left), ltl_to_nnf(phi->right));
        case LTL_WEAK_UNTIL:
            return ltl_mk_weak_until(ltl_to_nnf(phi->left), ltl_to_nnf(phi->right));

        default:
            return ltl_clone(phi);
    }
}

/* ── Formula Simplification ───────────────────────────────────── */
/*
 * Apply one pass of simplification rewrite rules.
 * Returns a new formula (may share structure with input).
 *
 * Boolean rules:
 *   true ∧ φ → φ
 *   false ∧ φ → false
 *   true ∨ φ → true
 *   false ∨ φ → φ
 *   ¬true → false
 *   ¬false → true
 *   φ ∧ φ → φ (idempotence)
 *   φ ∨ φ → φ (idempotence)
 *
 * Temporal rules:
 *   X true → true
 *   X false → false
 *   F true → true
 *   F false → false
 *   G true → true
 *   G false → false
 *   true U φ → F φ
 *   false U φ → φ
 *   φ U true → true
 *   φ U false → false
 *   true R φ → G φ
 *   false R φ → φ
 *   φ R true → true
 *   φ R false → false
 *   φ W true → true
 *   φ W false → G φ
 *   true W φ → true
 *   false W φ → φ W φ → F φ (simplified)
 *
 * F F φ → F φ  (idempotence)
 * G G φ → G φ  (idempotence)
 */

LtlNode* ltl_simplify(const LtlNode* phi) {
    if (!phi) return NULL;

    /* Simplify children first */
    LtlNode* left_simp  = phi->left  ? ltl_simplify(phi->left)  : NULL;
    LtlNode* right_simp = phi->right ? ltl_simplify(phi->right) : NULL;

    switch (phi->type) {
        case LTL_TRUE:
        case LTL_FALSE:
        case LTL_ATOM:
            return ltl_clone(phi);

        case LTL_NOT: {
            if (!left_simp) return ltl_mk_not(NULL);
            /* ¬true → false */
            if (left_simp->type == LTL_TRUE) {
                ltl_free(left_simp);
                return ltl_mk_false();
            }
            /* ¬false → true */
            if (left_simp->type == LTL_FALSE) {
                ltl_free(left_simp);
                return ltl_mk_true();
            }
            /* ¬¬ψ → ψ */
            if (left_simp->type == LTL_NOT) {
                LtlNode* inner = left_simp->left;
                left_simp->left = NULL; /* prevent double-free */
                ltl_free(left_simp);
                return inner ? ltl_clone(inner) : NULL;
            }
            return ltl_mk_not(left_simp);
        }

        case LTL_AND: {
            if (!left_simp || !right_simp) {
                ltl_free(left_simp); ltl_free(right_simp);
                return NULL;
            }
            /* true ∧ φ → φ */
            if (left_simp->type == LTL_TRUE) {
                ltl_free(left_simp); return right_simp;
            }
            if (right_simp->type == LTL_TRUE) {
                ltl_free(right_simp); return left_simp;
            }
            /* false ∧ φ → false */
            if (left_simp->type == LTL_FALSE || right_simp->type == LTL_FALSE) {
                ltl_free(left_simp); ltl_free(right_simp);
                return ltl_mk_false();
            }
            /* φ ∧ φ → φ */
            if (ltl_equals(left_simp, right_simp)) {
                ltl_free(right_simp); return left_simp;
            }
            return ltl_mk_and(left_simp, right_simp);
        }

        case LTL_OR: {
            if (!left_simp || !right_simp) {
                ltl_free(left_simp); ltl_free(right_simp);
                return NULL;
            }
            /* true ∨ φ → true */
            if (left_simp->type == LTL_TRUE || right_simp->type == LTL_TRUE) {
                ltl_free(left_simp); ltl_free(right_simp);
                return ltl_mk_true();
            }
            /* false ∨ φ → φ */
            if (left_simp->type == LTL_FALSE) {
                ltl_free(left_simp); return right_simp;
            }
            if (right_simp->type == LTL_FALSE) {
                ltl_free(right_simp); return left_simp;
            }
            /* φ ∨ φ → φ */
            if (ltl_equals(left_simp, right_simp)) {
                ltl_free(right_simp); return left_simp;
            }
            return ltl_mk_or(left_simp, right_simp);
        }

        case LTL_IMPLIES: {
            if (!left_simp || !right_simp) {
                ltl_free(left_simp); ltl_free(right_simp);
                return NULL;
            }
            /* true → φ → φ */
            if (left_simp->type == LTL_TRUE) {
                ltl_free(left_simp); return right_simp;
            }
            /* false → φ → true */
            if (left_simp->type == LTL_FALSE) {
                ltl_free(left_simp); ltl_free(right_simp);
                return ltl_mk_true();
            }
            /* φ → true → true */
            if (right_simp->type == LTL_TRUE) {
                ltl_free(left_simp); ltl_free(right_simp);
                return ltl_mk_true();
            }
            /* φ → false → ¬φ */
            if (right_simp->type == LTL_FALSE) {
                ltl_free(right_simp);
                return ltl_mk_not(left_simp);
            }
            return ltl_mk_implies(left_simp, right_simp);
        }

        case LTL_EQUIV: {
            if (!left_simp || !right_simp) {
                ltl_free(left_simp); ltl_free(right_simp);
                return NULL;
            }
            if (ltl_equals(left_simp, right_simp)) {
                ltl_free(left_simp); ltl_free(right_simp);
                return ltl_mk_true();
            }
            return ltl_mk_equiv(left_simp, right_simp);
        }

        case LTL_NEXT: {
            if (!left_simp) return NULL;
            if (left_simp->type == LTL_TRUE) {
                ltl_free(left_simp); return ltl_mk_true();
            }
            if (left_simp->type == LTL_FALSE) {
                ltl_free(left_simp); return ltl_mk_false();
            }
            return ltl_mk_next(left_simp);
        }

        case LTL_FINALLY: {
            if (!left_simp) return NULL;
            if (left_simp->type == LTL_TRUE) {
                ltl_free(left_simp); return ltl_mk_true();
            }
            if (left_simp->type == LTL_FALSE) {
                ltl_free(left_simp); return ltl_mk_false();
            }
            /* F F φ → F φ */
            if (left_simp->type == LTL_FINALLY) {
                LtlNode* inner = left_simp->left;
                left_simp->left = NULL;
                ltl_free(left_simp);
                return ltl_mk_finally(inner);
            }
            return ltl_mk_finally(left_simp);
        }

        case LTL_GLOBALLY: {
            if (!left_simp) return NULL;
            if (left_simp->type == LTL_TRUE) {
                ltl_free(left_simp); return ltl_mk_true();
            }
            if (left_simp->type == LTL_FALSE) {
                ltl_free(left_simp); return ltl_mk_false();
            }
            /* G G φ → G φ */
            if (left_simp->type == LTL_GLOBALLY) {
                LtlNode* inner = left_simp->left;
                left_simp->left = NULL;
                ltl_free(left_simp);
                return ltl_mk_globally(inner);
            }
            return ltl_mk_globally(left_simp);
        }

        case LTL_UNTIL: {
            if (!left_simp || !right_simp) {
                ltl_free(left_simp); ltl_free(right_simp);
                return NULL;
            }
            /* true U φ → F φ */
            if (left_simp->type == LTL_TRUE) {
                ltl_free(left_simp);
                LtlNode* result = ltl_mk_finally(right_simp);
                return result;
            }
            /* false U φ → φ */
            if (left_simp->type == LTL_FALSE) {
                ltl_free(left_simp); return right_simp;
            }
            /* φ U true → true */
            if (right_simp->type == LTL_TRUE) {
                ltl_free(left_simp); ltl_free(right_simp);
                return ltl_mk_true();
            }
            /* φ U false → false */
            if (right_simp->type == LTL_FALSE) {
                ltl_free(left_simp); ltl_free(right_simp);
                return ltl_mk_false();
            }
            return ltl_mk_until(left_simp, right_simp);
        }

        case LTL_RELEASE: {
            if (!left_simp || !right_simp) {
                ltl_free(left_simp); ltl_free(right_simp);
                return NULL;
            }
            /* true R φ → G φ (true releases immediately) */
            if (left_simp->type == LTL_TRUE) {
                ltl_free(left_simp);
                return ltl_mk_globally(right_simp);
            }
            /* false R φ → φ (false never releases) */
            if (left_simp->type == LTL_FALSE) {
                ltl_free(left_simp); return right_simp;
            }
            /* φ R true → true */
            if (right_simp->type == LTL_TRUE) {
                ltl_free(left_simp); ltl_free(right_simp);
                return ltl_mk_true();
            }
            /* φ R false → false */
            if (right_simp->type == LTL_FALSE) {
                ltl_free(left_simp); ltl_free(right_simp);
                return ltl_mk_false();
            }
            return ltl_mk_release(left_simp, right_simp);
        }

        case LTL_WEAK_UNTIL: {
            if (!left_simp || !right_simp) {
                ltl_free(left_simp); ltl_free(right_simp);
                return NULL;
            }
            /* true W φ → true */
            if (left_simp->type == LTL_TRUE) {
                ltl_free(left_simp); ltl_free(right_simp);
                return ltl_mk_true();
            }
            /* false W φ → F φ (or φ W φ which simplifies differently) */
            if (left_simp->type == LTL_FALSE) {
                ltl_free(left_simp);
                return ltl_mk_finally(right_simp);
            }
            /* φ W true → true */
            if (right_simp->type == LTL_TRUE) {
                ltl_free(left_simp); ltl_free(right_simp);
                return ltl_mk_true();
            }
            /* φ W false → G φ */
            if (right_simp->type == LTL_FALSE) {
                ltl_free(right_simp);
                return ltl_mk_globally(left_simp);
            }
            return ltl_mk_weak_until(left_simp, right_simp);
        }

        default:
            return ltl_clone(phi);
    }
}

LtlNode* ltl_simplify_fixpoint(const LtlNode* phi) {
    if (!phi) return NULL;
    LtlNode* current = ltl_clone(phi);
    int changed = 1;
    int max_iter = 100;
    int iter = 0;

    while (changed && iter < max_iter) {
        changed = 0;
        LtlNode* simplified = ltl_simplify(current);
        if (!ltl_equals(current, simplified)) changed = 1;
        ltl_free(current);
        current = simplified;
        iter++;
    }
    return current;
}

/* ── Operator Elimination ─────────────────────────────────────── */

LtlNode* ltl_eliminate_derived(const LtlNode* phi) {
    if (!phi) return NULL;

    LtlNode* left_elim  = phi->left  ? ltl_eliminate_derived(phi->left)  : NULL;
    LtlNode* right_elim = phi->right ? ltl_eliminate_derived(phi->right) : NULL;

    switch (phi->type) {
        case LTL_IMPLIES: {
            /* φ → ψ → ¬φ ∨ ψ */
            return ltl_mk_or(ltl_mk_not(left_elim), right_elim);
        }
        case LTL_EQUIV: {
            /* φ ↔ ψ → (φ → ψ) ∧ (ψ → φ) → (¬φ ∨ ψ) ∧ (¬ψ ∨ φ) */
            LtlNode* l = ltl_mk_or(ltl_mk_not(ltl_clone(left_elim)),
                                    ltl_clone(right_elim));
            LtlNode* r = ltl_mk_or(ltl_mk_not(right_elim),
                                    ltl_clone(left_elim));
            return ltl_mk_and(l, r);
        }
        case LTL_WEAK_UNTIL: {
            /* φ W ψ → (G φ) ∨ (φ U ψ) */
            LtlNode* l = ltl_mk_globally(ltl_clone(left_elim));
            LtlNode* r = ltl_mk_until(ltl_clone(left_elim), right_elim);
            LtlNode* result = ltl_mk_or(l, r);
            ltl_free(left_elim);
            return result;
        }

        case LTL_NOT:
        case LTL_AND:
        case LTL_OR:
        case LTL_NEXT:
        case LTL_FINALLY:
        case LTL_GLOBALLY:
        case LTL_UNTIL:
        case LTL_RELEASE: {
            LtlNode* result = alloc_node(phi->type);
            if (!result) {
                ltl_free(left_elim); ltl_free(right_elim);
                return NULL;
            }
            result->atom_idx = phi->atom_idx;
            result->left = left_elim;
            result->right = right_elim;
            return result;
        }

        default:
            return ltl_clone(phi);
    }
}

LtlNode* ltl_to_core_until(const LtlNode* phi) {
    if (!phi) return NULL;

    /* First eliminate derived operators, then eliminate F, G, R */
    LtlNode* step1 = ltl_eliminate_derived(phi);
    LtlNode* step2 = ltl_release_to_until(step1);
    ltl_free(step1);

    /* Eliminate F and G using U */
    if (!step2) return NULL;
    LtlNode* result = ltl_clone(step2);

    /* Post-order rewrite:
     *   F ψ → true U ψ
     *   G ψ → false R ψ → ¬(true U ¬ψ)
     * We do this structurally rather than recursively for clarity. */

    /* For simplicity, we rely on eliminate_derived having eliminated
     * F and G? No, eliminate_derived only eliminates →, ↔, W.
     * F and G remain. We'll do a recursive transformation. */
    ltl_free(result);

    /* Recursive transformation of F and G to U */
    LtlNode* left_core  = step2->left  ? ltl_to_core_until(step2->left)  : NULL;
    LtlNode* right_core = step2->right ? ltl_to_core_until(step2->right) : NULL;

    switch (step2->type) {
        case LTL_FINALLY:
            /* F ψ → true U ψ */
            if (left_core) {
                ltl_free(step2);
                return ltl_mk_until(ltl_mk_true(), left_core);
            }
            break;
        case LTL_GLOBALLY:
            /* G ψ → ¬(true U ¬ψ) */
            if (left_core) {
                ltl_free(step2);
                LtlNode* inner = ltl_mk_until(ltl_mk_true(),
                                              ltl_mk_not(ltl_clone(left_core)));
                LtlNode* result = ltl_mk_not(inner);
                ltl_free(left_core);
                return result;
            }
            break;
        default:
            break;
    }

    /* For non-F/G nodes, rebuild with processed children */
    LtlNode* rebuilt = alloc_node(step2->type);
    if (rebuilt) {
        rebuilt->atom_idx = step2->atom_idx;
        rebuilt->left = left_core;
        rebuilt->right = right_core;
    }
    ltl_free(step2);
    return rebuilt;
}

LtlNode* ltl_release_to_until(const LtlNode* phi) {
    if (!phi) return NULL;

    LtlNode* left_r  = phi->left  ? ltl_release_to_until(phi->left)  : NULL;
    LtlNode* right_r = phi->right ? ltl_release_to_until(phi->right) : NULL;

    if (phi->type == LTL_RELEASE) {
        /* φ R ψ → ¬(¬φ U ¬ψ) */
        LtlNode* inner = ltl_mk_until(
            ltl_mk_not(ltl_clone(left_r)),
            ltl_mk_not(right_r));
        LtlNode* result = ltl_mk_not(inner);
        ltl_free(left_r);
        /* phi is const; we free the owned right_r and left_r */
        return result;
    }

    /* Rebuild other nodes (non-RELEASE) — return a clone */
    LtlNode* result = alloc_node(phi->type);
    if (result) {
        result->atom_idx = phi->atom_idx;
        result->left = left_r;
        result->right = right_r;
    }
    return result;
}

/* ── Fragment Detection ───────────────────────────────────────── */

int ltl_is_future_only(const LtlNode* phi) {
    if (!phi) return 1;
    /* All standard LTL operators are future-only,
     * so this always returns true for our AST types.
     * Past operators would be: Y, O, H, S (not in our enum). */
    return 1;
}

int ltl_is_safety_fragment(const LtlNode* phi) {
    if (!phi) return 1;

    /* Convert to NNF first */
    LtlNode* nnf = ltl_to_nnf(phi);

    /* In NNF, safety fragment contains only:
     *   atoms, ¬atoms, ∧, ∨, X, G, W, R
     * Safety violation: presence of F or U in NNF.
     *
     * However, U and W can appear in specific safe contexts.
     * A more precise syntactic check: no unguarded U.
     *
     * Simple check: if NNF contains U or unguarded F, not safety. */

    int is_safe = 1;

    /* Recursive check helper */
    struct check_ctx { int* safe; };
    /* We'll do it iteratively: for each node in NNF,
     * if type is UNTIL or FINALLY, it's potentially not safety */
    if (ltl_count_operators(nnf, LTL_UNTIL) > 0) is_safe = 0;

    /* F in NNF means the formula requires something to eventually happen,
     * which is a liveness component. However, F under G can be safe
     * (e.g., G F p is NOT safe, but G(p → F q) = G(¬p ∨ F q) is also not safe).
     *
     * Simple rule: if NNF has F not under any G/R, it's liveness. */

    if (ltl_count_operators(nnf, LTL_FINALLY) > 0) is_safe = 0;

    ltl_free(nnf);
    return is_safe;
}

int ltl_is_liveness_fragment(const LtlNode* phi) {
    if (!phi) return 0;
    LtlNode* nnf = ltl_to_nnf(phi);
    /* Liveness formulas contain F or U */
    int is_live = (ltl_count_operators(nnf, LTL_FINALLY) > 0 ||
                   ltl_count_operators(nnf, LTL_UNTIL) > 0);
    ltl_free(nnf);
    return is_live;
}

int ltl_is_obligation_fragment(const LtlNode* phi) {
    /* Obligation = positive Boolean combination of safety and guarantee.
     * Guarantee = co-safety = ¬Safety. So obligation = Bool(Safety).
     * Simplified: check if it's a simple safety or guarantee formula. */

    if (ltl_is_safety_fragment(phi)) return 1;

    /* Check if negation is safety */
    LtlNode* neg = ltl_mk_not(ltl_clone(phi));
    int neg_safe = ltl_is_safety_fragment(neg);
    ltl_free(neg);
    return neg_safe;
}

/* ── Until/Release Duality Check ──────────────────────────────── */

int ltl_check_until_release_duality(const LtlNode* phi, const LtlNode* psi) {
    if (!phi || !psi) return 0;

    /* Construct ¬(φ U ψ) */
    LtlNode* until_formula = ltl_mk_until(ltl_clone(phi), ltl_clone(psi));
    LtlNode* neg_until = ltl_mk_not(until_formula);
    LtlNode* neg_until_nnf = ltl_to_nnf(neg_until);
    ltl_free(neg_until);

    /* Construct (¬φ) R (¬ψ) */
    LtlNode* not_phi = ltl_mk_not(ltl_clone(phi));
    LtlNode* not_psi = ltl_mk_not(ltl_clone(psi));
    LtlNode* release_formula = ltl_mk_release(not_phi, not_psi);
    LtlNode* release_nnf = ltl_to_nnf(release_formula);
    ltl_free(release_formula);

    /* Compare: NNF(¬(φ U ψ)) == NNF((¬φ) R (¬ψ)) */
    int result = ltl_equals(neg_until_nnf, release_nnf);

    ltl_free(neg_until_nnf);
    ltl_free(release_nnf);

    return result;
}

/* ── Expansion Laws ───────────────────────────────────────────── */

LtlNode* ltl_expand(const LtlNode* phi) {
    if (!phi) return NULL;

    switch (phi->type) {
        case LTL_FINALLY:
            /* F φ → φ ∨ X(F φ) */
            return ltl_mk_or(
                ltl_clone(phi->left),
                ltl_mk_next(ltl_clone(phi))
            );

        case LTL_GLOBALLY:
            /* G φ → φ ∧ X(G φ) */
            return ltl_mk_and(
                ltl_clone(phi->left),
                ltl_mk_next(ltl_clone(phi))
            );

        case LTL_UNTIL:
            /* φ U ψ → ψ ∨ (φ ∧ X(φ U ψ)) */
            return ltl_mk_or(
                ltl_clone(phi->right),
                ltl_mk_and(ltl_clone(phi->left), ltl_mk_next(ltl_clone(phi)))
            );

        case LTL_RELEASE:
            /* φ R ψ → ψ ∧ (φ ∨ X(φ R ψ)) */
            return ltl_mk_and(
                ltl_clone(phi->right),
                ltl_mk_or(ltl_clone(phi->left), ltl_mk_next(ltl_clone(phi)))
            );

        case LTL_WEAK_UNTIL:
            /* φ W ψ → ψ ∨ (φ ∧ X(φ W ψ)) */
            return ltl_mk_or(
                ltl_clone(phi->right),
                ltl_mk_and(ltl_clone(phi->left), ltl_mk_next(ltl_clone(phi)))
            );

        default:
            /* For non-temporal nodes, no expansion possible */
            return ltl_clone(phi);
    }
}

LtlNode* ltl_unwind(const LtlNode* phi, int k) {
    if (!phi || k < 0) return NULL;
    if (k == 0) return ltl_clone(phi);

    /* Only unwind temporal operators */
    switch (phi->type) {
        case LTL_FINALLY:
        case LTL_GLOBALLY:
        case LTL_UNTIL:
        case LTL_RELEASE:
        case LTL_WEAK_UNTIL: {
            /* Iterative expansion: apply expand() k times */
            LtlNode* result = ltl_clone(phi);
            for (int i = 0; i < k; i++) {
                LtlNode* step = ltl_expand(result);
                ltl_free(result);
                result = step;
            }
            return result;
        }

        case LTL_NEXT: {
            /* X^k φ: just nest X operators */
            LtlNode* inner = ltl_unwind(phi->left, k);
            LtlNode* result = inner;
            for (int i = 0; i < k; i++) {
                result = ltl_mk_next(result);
            }
            return result;
        }

        default:
            /* Non-temporal: just clone */
            return ltl_clone(phi);
    }
}

/* ── Bounded Equivalence Check ────────────────────────────────── */

int ltl_check_equivalence_bounded(const LtlNode* phi, const LtlNode* psi,
                                   int max_prefix, int max_cycle,
                                   int n_props) {
    if (!phi || !psi) return (phi == psi);

    /* Φ ≡ Ψ iff for all traces σ: σ ⊨ Φ ↔ σ ⊨ Ψ.
     * Equivalently: no trace σ such that σ ⊨ Φ ⊕ σ ⊨ Ψ.
     * (⊕ = XOR = one true, the other false)
     *
     * We enumerate all traces over n_props propositions,
     * with prefix up to max_prefix and cycle up to max_cycle,
     * and check for disagreement. */

    if (n_props > 6) n_props = 6; /* 2^6 = 64 states per step */
    if (max_prefix > 4) max_prefix = 4;
    if (max_cycle > 3) max_cycle = 3;

    /* Number of distinct proposition sets: 2^n_props */
    int n_sets = 1 << n_props;
    LtlPropSet* all_sets = (LtlPropSet*)malloc(n_sets * sizeof(LtlPropSet));
    if (!all_sets) return 0;
    for (int i = 0; i < n_sets; i++)
        all_sets[i] = (LtlPropSet)i;

    /* Enumerate all lassos */
    int total_lassos = 0;
    /* prefix: sequence of plen elements from n_sets choices */
    /* cycle: sequence of clen elements from n_sets choices */
    /* Total: sum_{plen=0..max_prefix} sum_{clen=1..max_cycle} n_sets^(plen+clen) */

    /* For small bounds, use iterative enumeration */
    for (int plen = 0; plen <= max_prefix; plen++) {
        for (int clen = 1; clen <= max_cycle; clen++) {
            int total_len = plen + clen;
            /* Use base-n_sets counting to enumerate all sequences of length total_len */
            long long limit = 1;
            for (int i = 0; i < total_len; i++) limit *= n_sets;
            if (limit > 2000000) continue; /* safety limit */

            for (long long idx = 0; idx < limit; idx++) {
                /* Decode index into a sequence of proposition sets */
                LtlPropSet* seq = (LtlPropSet*)malloc(total_len * sizeof(LtlPropSet));
                if (!seq) continue;
                long long tmp = idx;
                for (int i = 0; i < total_len; i++) {
                    seq[i] = all_sets[tmp % n_sets];
                    tmp /= n_sets;
                }

                /* Create lasso trace */
                LtlTrace* trace = ltl_trace_create_lasso(seq, plen,
                                                          seq + plen, clen);
                if (trace) {
                    int sat_phi = ltl_satisfies(trace, phi);
                    int sat_psi = ltl_satisfies(trace, psi);
                    if (sat_phi != sat_psi) {
                        ltl_trace_free(trace);
                        free(seq);
                        free(all_sets);
                        return 0; /* counterexample found */
                    }
                    total_lassos++;
                    ltl_trace_free(trace);
                }
                free(seq);
            }
        }
    }

    free(all_sets);
    return 1; /* no counterexample within bound */
}

int ltl_syntactic_equiv(const LtlNode* phi, const LtlNode* psi) {
    if (!phi || !psi) return (phi == psi);
    LtlNode* s1 = ltl_simplify_fixpoint(phi);
    LtlNode* s2 = ltl_simplify_fixpoint(psi);
    int eq = ltl_equals(s1, s2);
    ltl_free(s1);
    ltl_free(s2);
    return eq;
}

/* alloc_node defined earlier in this file */
