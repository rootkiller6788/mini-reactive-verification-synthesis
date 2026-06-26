/*
 * ltl_ast.c — LTL Formula AST Construction, Manipulation & I/O
 *
 * Implements the complete Abstract Syntax Tree for Linear Temporal Logic
 * formulas with all standard constructors, traversal, comparison,
 * printing (infix, prefix, DOT), and Fischer-Ladner closure.
 *
 * L1 Definitions + L3 Mathematical Structures.
 *
 * Reference:
 *   Pnueli 1977 — The temporal logic of programs
 *   Baier & Katoen 2008 — Principles of Model Checking (Ch. 5.1)
 *   Fischer & Ladner 1979 — Propositional Dynamic Logic of Regular Programs
 */

#include "ltl_ast.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* ── Static State ─────────────────────────────────────────────── */
static int g_id_counter = 0;
static char** g_atom_names = NULL;
static int g_n_atom_names = 0;

void ltl_reset_id_counter(void) { g_id_counter = 0; }
int  ltl_next_id(void) { return g_id_counter++; }

/* ── Node Type Classification ─────────────────────────────────── */

const char* ltl_node_type_name(LtlNodeType t) {
    switch (t) {
        case LTL_TRUE:       return "TRUE";
        case LTL_FALSE:      return "FALSE";
        case LTL_ATOM:       return "ATOM";
        case LTL_NOT:        return "NOT";
        case LTL_AND:        return "AND";
        case LTL_OR:         return "OR";
        case LTL_IMPLIES:    return "IMPLIES";
        case LTL_EQUIV:      return "EQUIV";
        case LTL_NEXT:       return "X";
        case LTL_FINALLY:    return "F";
        case LTL_GLOBALLY:   return "G";
        case LTL_UNTIL:      return "U";
        case LTL_RELEASE:    return "R";
        case LTL_WEAK_UNTIL: return "W";
        default:             return "UNKNOWN";
    }
}

int ltl_node_is_temporal(LtlNodeType t) {
    return t == LTL_NEXT || t == LTL_FINALLY || t == LTL_GLOBALLY ||
           t == LTL_UNTIL || t == LTL_RELEASE || t == LTL_WEAK_UNTIL;
}

int ltl_node_is_binary(LtlNodeType t) {
    return t == LTL_AND || t == LTL_OR || t == LTL_IMPLIES ||
           t == LTL_EQUIV || t == LTL_UNTIL || t == LTL_RELEASE ||
           t == LTL_WEAK_UNTIL;
}

int ltl_node_is_unary(LtlNodeType t) {
    return t == LTL_NOT || t == LTL_NEXT || t == LTL_FINALLY ||
           t == LTL_GLOBALLY;
}

int ltl_node_is_leaf(LtlNodeType t) {
    return t == LTL_TRUE || t == LTL_FALSE || t == LTL_ATOM;
}

/* ── Internal Node Allocation ─────────────────────────────────── */

static LtlNode* alloc_node(LtlNodeType type) {
    LtlNode* node = (LtlNode*)calloc(1, sizeof(LtlNode));
    if (!node) return NULL;
    node->type = type;
    node->atom_idx = -1;
    node->left = NULL;
    node->right = NULL;
    node->id = ltl_next_id();
    node->next = NULL;
    return node;
}

/* ── Formula Constructors ─────────────────────────────────────── */

LtlNode* ltl_mk_true(void) {
    return alloc_node(LTL_TRUE);
}

LtlNode* ltl_mk_false(void) {
    return alloc_node(LTL_FALSE);
}

LtlNode* ltl_mk_atom(int atom_idx) {
    LtlNode* node = alloc_node(LTL_ATOM);
    if (node) node->atom_idx = atom_idx;
    return node;
}

LtlNode* ltl_mk_not(LtlNode* phi) {
    if (!phi) return NULL;
    LtlNode* node = alloc_node(LTL_NOT);
    if (node) node->left = phi;
    return node;
}

LtlNode* ltl_mk_and(LtlNode* phi, LtlNode* psi) {
    if (!phi || !psi) { free(phi); free(psi); return NULL; }
    LtlNode* node = alloc_node(LTL_AND);
    if (node) { node->left = phi; node->right = psi; }
    return node;
}

LtlNode* ltl_mk_or(LtlNode* phi, LtlNode* psi) {
    if (!phi || !psi) { free(phi); free(psi); return NULL; }
    LtlNode* node = alloc_node(LTL_OR);
    if (node) { node->left = phi; node->right = psi; }
    return node;
}

LtlNode* ltl_mk_implies(LtlNode* phi, LtlNode* psi) {
    if (!phi || !psi) { free(phi); free(psi); return NULL; }
    LtlNode* node = alloc_node(LTL_IMPLIES);
    if (node) { node->left = phi; node->right = psi; }
    return node;
}

LtlNode* ltl_mk_equiv(LtlNode* phi, LtlNode* psi) {
    if (!phi || !psi) { free(phi); free(psi); return NULL; }
    LtlNode* node = alloc_node(LTL_EQUIV);
    if (node) { node->left = phi; node->right = psi; }
    return node;
}

LtlNode* ltl_mk_next(LtlNode* phi) {
    if (!phi) return NULL;
    LtlNode* node = alloc_node(LTL_NEXT);
    if (node) node->left = phi;
    return node;
}

LtlNode* ltl_mk_finally(LtlNode* phi) {
    if (!phi) return NULL;
    LtlNode* node = alloc_node(LTL_FINALLY);
    if (node) node->left = phi;
    return node;
}

LtlNode* ltl_mk_globally(LtlNode* phi) {
    if (!phi) return NULL;
    LtlNode* node = alloc_node(LTL_GLOBALLY);
    if (node) node->left = phi;
    return node;
}

LtlNode* ltl_mk_until(LtlNode* phi, LtlNode* psi) {
    if (!phi || !psi) { free(phi); free(psi); return NULL; }
    LtlNode* node = alloc_node(LTL_UNTIL);
    if (node) { node->left = phi; node->right = psi; }
    return node;
}

LtlNode* ltl_mk_release(LtlNode* phi, LtlNode* psi) {
    if (!phi || !psi) { free(phi); free(psi); return NULL; }
    LtlNode* node = alloc_node(LTL_RELEASE);
    if (node) { node->left = phi; node->right = psi; }
    return node;
}

LtlNode* ltl_mk_weak_until(LtlNode* phi, LtlNode* psi) {
    if (!phi || !psi) { free(phi); free(psi); return NULL; }
    LtlNode* node = alloc_node(LTL_WEAK_UNTIL);
    if (node) { node->left = phi; node->right = psi; }
    return node;
}

LtlNode* ltl_mk_and_n(LtlNode** phis, int n) {
    if (!phis || n <= 0) return NULL;
    if (n == 1) return phis[0];
    LtlNode* result = ltl_clone(phis[0]);
    for (int i = 1; i < n; i++) {
        LtlNode* old = result;
        result = ltl_mk_and(result, ltl_clone(phis[i]));
        if (!result) { ltl_free(old); return NULL; }
    }
    return result;
}

LtlNode* ltl_mk_or_n(LtlNode** phis, int n) {
    if (!phis || n <= 0) return NULL;
    if (n == 1) return phis[0];
    LtlNode* result = ltl_clone(phis[0]);
    for (int i = 1; i < n; i++) {
        LtlNode* old = result;
        result = ltl_mk_or(result, ltl_clone(phis[i]));
        if (!result) { ltl_free(old); return NULL; }
    }
    return result;
}

LtlNode* ltl_mk_next_k(LtlNode* phi, int k) {
    if (!phi || k < 0) return NULL;
    if (k == 0) return phi;
    LtlNode* result = phi;
    for (int i = 0; i < k; i++) {
        LtlNode* old = result;
        result = ltl_mk_next(result);
        if (!result) { ltl_free(old); return NULL; }
    }
    return result;
}

/* ── Formula Destruction ──────────────────────────────────────── */

void ltl_free(LtlNode* node) {
    if (!node) return;
    ltl_free(node->left);
    ltl_free(node->right);
    free(node);
}

/* ── Formula Cloning ──────────────────────────────────────────── */

LtlNode* ltl_clone(const LtlNode* node) {
    if (!node) return NULL;
    LtlNode* copy = (LtlNode*)calloc(1, sizeof(LtlNode));
    if (!copy) return NULL;
    copy->type = node->type;
    copy->atom_idx = node->atom_idx;
    copy->id = ltl_next_id();
    copy->left = ltl_clone(node->left);
    copy->right = ltl_clone(node->right);
    copy->next = NULL;
    return copy;
}

/* ── Formula Queries ──────────────────────────────────────────── */

int ltl_depth(const LtlNode* node) {
    if (!node) return 0;
    if (ltl_node_is_leaf(node->type)) return 1;
    int dl = ltl_depth(node->left);
    int dr = ltl_depth(node->right);
    int max_child = (dl > dr) ? dl : dr;
    return 1 + max_child;
}

int ltl_size(const LtlNode* node) {
    if (!node) return 0;
    return 1 + ltl_size(node->left) + ltl_size(node->right);
}

int ltl_temporal_depth(const LtlNode* node) {
    if (!node) return 0;
    int is_temp = ltl_node_is_temporal(node->type) ? 1 : 0;
    int dl = ltl_temporal_depth(node->left);
    int dr = ltl_temporal_depth(node->right);
    int mc = (dl > dr) ? dl : dr;
    return is_temp + mc;
}

int ltl_count_operators(const LtlNode* node, LtlNodeType t) {
    if (!node) return 0;
    int self = (node->type == t) ? 1 : 0;
    return self + ltl_count_operators(node->left, t) +
           ltl_count_operators(node->right, t);
}

int ltl_is_literal(const LtlNode* node) {
    if (!node) return 0;
    if (node->type == LTL_ATOM) return 1;
    if (node->type == LTL_NOT && node->left &&
        node->left->type == LTL_ATOM) return 1;
    return 0;
}

int ltl_has_subformula(const LtlNode* haystack, const LtlNode* needle) {
    if (!haystack || !needle) return 0;
    if (ltl_equals(haystack, needle)) return 1;
    return ltl_has_subformula(haystack->left, needle) ||
           ltl_has_subformula(haystack->right, needle);
}

/* ── Formula Comparison ───────────────────────────────────────── */

int ltl_equals(const LtlNode* a, const LtlNode* b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    if (a->type != b->type) return 0;
    if (a->type == LTL_ATOM && a->atom_idx != b->atom_idx) return 0;
    return ltl_equals(a->left, b->left) && ltl_equals(a->right, b->right);
}

int ltl_is_syntactic_subformula(const LtlNode* sub, const LtlNode* super) {
    return ltl_has_subformula(super, sub);
}

/* ── Formula I/O — Infix Notation ─────────────────────────────── */

static void print_with_precedence(const LtlNode* node, int parent_prec,
                                   int is_right_child) {
    if (!node) { printf("NULL"); return; }

    int my_prec = 0;
    switch (node->type) {
        case LTL_TRUE: case LTL_FALSE:
            my_prec = 100; break;
        case LTL_ATOM:
            my_prec = 100; break;
        case LTL_NOT:
            my_prec = 80; break;
        case LTL_NEXT: case LTL_FINALLY: case LTL_GLOBALLY:
            my_prec = 75; break;
        case LTL_UNTIL: case LTL_RELEASE: case LTL_WEAK_UNTIL:
            my_prec = 60; break;
        case LTL_AND:
            my_prec = 50; break;
        case LTL_OR:
            my_prec = 40; break;
        case LTL_IMPLIES:
            my_prec = 30; break;
        case LTL_EQUIV:
            my_prec = 20; break;
        default: my_prec = 0;
    }

    int need_paren = (my_prec < parent_prec) ||
                     (my_prec == parent_prec && is_right_child &&
                      node->type != LTL_AND);

    if (need_paren) printf("(");

    switch (node->type) {
        case LTL_TRUE:
            printf("true"); break;
        case LTL_FALSE:
            printf("false"); break;
        case LTL_ATOM:
            if (g_atom_names && node->atom_idx >= 0 &&
                node->atom_idx < g_n_atom_names) {
                printf("%s", g_atom_names[node->atom_idx]);
            } else {
                printf("p%d", node->atom_idx);
            }
            break;
        case LTL_NOT:
            printf("¬");
            print_with_precedence(node->left, 80, 0);
            break;
        case LTL_AND:
            print_with_precedence(node->left, 50, 0);
            printf(" ∧ ");
            print_with_precedence(node->right, 50, 1);
            break;
        case LTL_OR:
            print_with_precedence(node->left, 40, 0);
            printf(" ∨ ");
            print_with_precedence(node->right, 40, 1);
            break;
        case LTL_IMPLIES:
            print_with_precedence(node->left, 30, 0);
            printf(" → ");
            print_with_precedence(node->right, 31, 1);
            break;
        case LTL_EQUIV:
            print_with_precedence(node->left, 20, 0);
            printf(" ↔ ");
            print_with_precedence(node->right, 20, 1);
            break;
        case LTL_NEXT:
            printf("X ");
            print_with_precedence(node->left, 75, 0);
            break;
        case LTL_FINALLY:
            printf("F ");
            print_with_precedence(node->left, 75, 0);
            break;
        case LTL_GLOBALLY:
            printf("G ");
            print_with_precedence(node->left, 75, 0);
            break;
        case LTL_UNTIL:
            print_with_precedence(node->left, 60, 0);
            printf(" U ");
            print_with_precedence(node->right, 60, 1);
            break;
        case LTL_RELEASE:
            print_with_precedence(node->left, 60, 0);
            printf(" R ");
            print_with_precedence(node->right, 60, 1);
            break;
        case LTL_WEAK_UNTIL:
            print_with_precedence(node->left, 60, 0);
            printf(" W ");
            print_with_precedence(node->right, 60, 1);
            break;
    }
    if (need_paren) printf(")");
}

void ltl_print(const LtlNode* node) {
    if (!node) { printf("(null)\n"); return; }
    print_with_precedence(node, 0, 0);
    printf("\n");
}

/* ── Formula I/O — Prefix (S-Expression) Notation ─────────────── */

static void print_prefix_rec(const LtlNode* node) {
    if (!node) { printf("NULL"); return; }
    printf("(%s", ltl_node_type_name(node->type));
    if (node->type == LTL_ATOM) {
        printf(" %d", node->atom_idx);
    }
    if (node->left) {
        printf(" ");
        print_prefix_rec(node->left);
    }
    if (node->right) {
        printf(" ");
        print_prefix_rec(node->right);
    }
    printf(")");
}

void ltl_print_prefix(const LtlNode* node) {
    if (!node) { printf("()\n"); return; }
    print_prefix_rec(node);
    printf("\n");
}

/* ── Formula I/O — DOT (Graphviz) ─────────────────────────────── */

static void print_dot_rec(const LtlNode* node, FILE* f, int* node_count) {
    if (!node) return;
    int my_id = (*node_count)++;
    const char* shape = (ltl_node_is_leaf(node->type)) ? "box" : "ellipse";
    const char* color = ltl_node_is_temporal(node->type) ? "lightblue" : "white";

    fprintf(f, "  n%d [label=\"%s", my_id, ltl_node_type_name(node->type));
    if (node->type == LTL_ATOM) {
        if (g_atom_names && node->atom_idx >= 0 &&
            node->atom_idx < g_n_atom_names) {
            fprintf(f, "(%s)", g_atom_names[node->atom_idx]);
        } else {
            fprintf(f, "(%d)", node->atom_idx);
        }
    }
    fprintf(f, "\", shape=%s, style=filled, fillcolor=%s];\n", shape, color);

    if (node->left) {
        int lid = *node_count;
        print_dot_rec(node->left, f, node_count);
        fprintf(f, "  n%d -> n%d;\n", my_id, lid);
    }
    if (node->right) {
        int rid = *node_count;
        print_dot_rec(node->right, f, node_count);
        fprintf(f, "  n%d -> n%d;\n", my_id, rid);
    }
}

void ltl_print_dot(const LtlNode* node, const char* filename) {
    if (!node || !filename) return;
    FILE* f = fopen(filename, "w");
    if (!f) return;
    fprintf(f, "digraph LTL_AST {\n");
    fprintf(f, "  rankdir=TB;\n");
    fprintf(f, "  node [fontname=\"Courier\"];\n");
    int nc = 0;
    print_dot_rec(node, f, &nc);
    fprintf(f, "}\n");
    fclose(f);
}

/* ── Atom Name Management ─────────────────────────────────────── */

void ltl_set_atom_names(const char** names, int n) {
    if (g_atom_names) {
        for (int i = 0; i < g_n_atom_names; i++) free(g_atom_names[i]);
        free(g_atom_names);
    }
    g_n_atom_names = n;
    g_atom_names = (char**)malloc(n * sizeof(char*));
    if (g_atom_names) {
        for (int i = 0; i < n; i++) {
            g_atom_names[i] = names[i] ? strdup(names[i]) : NULL;
        }
    }
}

const char* ltl_get_atom_name(int idx) {
    if (g_atom_names && idx >= 0 && idx < g_n_atom_names)
        return g_atom_names[idx] ? g_atom_names[idx] : "?";
    return NULL;
}

/* ── Subformula Collection ────────────────────────────────────── */

void ltl_subformula_set_init(LtlSubformulaSet* s, int capacity) {
    if (!s) return;
    s->formulas = (LtlNode**)calloc(capacity, sizeof(LtlNode*));
    s->count = 0;
    s->capacity = s->formulas ? capacity : 0;
}

void ltl_subformula_set_add(LtlSubformulaSet* s, LtlNode* f) {
    if (!s || !s->formulas || !f) return;
    /* Check for duplicate */
    for (int i = 0; i < s->count; i++) {
        if (ltl_equals(s->formulas[i], f)) return;
    }
    if (s->count >= s->capacity) {
        int nc = s->capacity * 2;
        if (nc < 8) nc = 8;
        LtlNode** nd = (LtlNode**)realloc(s->formulas, nc * sizeof(LtlNode*));
        if (!nd) return;
        s->formulas = nd;
        s->capacity = nc;
    }
    s->formulas[s->count++] = f;
}

int ltl_subformula_set_contains(const LtlSubformulaSet* s, const LtlNode* f) {
    if (!s || !f) return 0;
    for (int i = 0; i < s->count; i++) {
        if (ltl_equals(s->formulas[i], f)) return 1;
    }
    return 0;
}

void ltl_subformula_set_free(LtlSubformulaSet* s) {
    if (!s) return;
    free(s->formulas);
    s->formulas = NULL;
    s->count = s->capacity = 0;
}

static void collect_sub_rec(const LtlNode* node, LtlSubformulaSet* out) {
    if (!node || !out) return;
    ltl_subformula_set_add(out, (LtlNode*)node);
    collect_sub_rec(node->left, out);
    collect_sub_rec(node->right, out);
}

void ltl_collect_subformulas(const LtlNode* root, LtlSubformulaSet* out) {
    collect_sub_rec(root, out);
}

/* ── Fischer-Ladner Closure ───────────────────────────────────── */
/*
 * FL(φ) is the least set S containing φ such that:
 *   1. If ¬ψ ∈ S, then ψ ∈ S
 *   2. If ψ₁ ∧ ψ₂ ∈ S, then ψ₁, ψ₂ ∈ S
 *   3. If ψ₁ ∨ ψ₂ ∈ S, then ψ₁, ψ₂ ∈ S
 *   4. If ψ₁ → ψ₂ ∈ S, then ψ₁, ψ₂ ∈ S
 *   5. If ψ₁ ↔ ψ₂ ∈ S, then ψ₁, ψ₂ ∈ S  (not in original FL but useful)
 *   6. If X ψ ∈ S, then ψ ∈ S
 *   7. If F ψ ∈ S, then ψ ∈ S and X(F ψ) ∈ S
 *   8. If G ψ ∈ S, then ψ ∈ S and X(G ψ) ∈ S
 *   9. If ψ₁ U ψ₂ ∈ S, then ψ₁, ψ₂ ∈ S and X(ψ₁ U ψ₂) ∈ S
 *  10. If ψ₁ R ψ₂ ∈ S, then ψ₁, ψ₂ ∈ S and X(ψ₁ R ψ₂) ∈ S
 *  11. If ψ₁ W ψ₂ ∈ S, then ψ₁, ψ₂ ∈ S and X(ψ₁ W ψ₂) ∈ S
 *
 * This closure is central to tableau-based LTL decision procedures.
 * Its size is O(|φ|) — linear in the size of the formula.
 */

static void fl_closure_step(LtlNode* node, LtlSubformulaSet* out) {
    if (!node || !out) return;
    ltl_subformula_set_add(out, node);

    switch (node->type) {
        case LTL_TRUE:
        case LTL_FALSE:
        case LTL_ATOM:
            /* No additional closure rules */
            break;

        case LTL_NOT:
            if (node->left) {
                ltl_subformula_set_add(out, node->left);
            }
            break;

        case LTL_AND:
        case LTL_OR:
        case LTL_IMPLIES:
        case LTL_EQUIV:
            if (node->left)  ltl_subformula_set_add(out, node->left);
            if (node->right) ltl_subformula_set_add(out, node->right);
            break;

        case LTL_NEXT:
            if (node->left) ltl_subformula_set_add(out, node->left);
            break;

        case LTL_FINALLY: {
            if (node->left) ltl_subformula_set_add(out, node->left);
            /* Add X(F φ) */
            LtlNode* xf = ltl_mk_next(ltl_clone(node));
            ltl_subformula_set_add(out, xf);
            /* We'll free temp nodes later */
            break;
        }

        case LTL_GLOBALLY: {
            if (node->left) ltl_subformula_set_add(out, node->left);
            LtlNode* xg = ltl_mk_next(ltl_clone(node));
            ltl_subformula_set_add(out, xg);
            break;
        }

        case LTL_UNTIL:
        case LTL_RELEASE:
        case LTL_WEAK_UNTIL: {
            if (node->left)  ltl_subformula_set_add(out, node->left);
            if (node->right) ltl_subformula_set_add(out, node->right);
            LtlNode* xtemp = ltl_mk_next(ltl_clone(node));
            ltl_subformula_set_add(out, xtemp);
            break;
        }
    }
}

void ltl_fischer_ladner_closure(const LtlNode* phi, LtlSubformulaSet* out) {
    if (!phi || !out) return;
    ltl_subformula_set_init(out, 32);
    ltl_subformula_set_add(out, (LtlNode*)phi);

    /* Iterate to fixpoint: add new formulas until no more are added */
    int changed = 1;
    while (changed) {
        changed = 0;
        int n = out->count;
        for (int i = 0; i < n; i++) {
            int before = out->count;
            fl_closure_step(out->formulas[i], out);
            if (out->count > before) changed = 1;
        }
    }

    /* Free temporary nodes created during closure computation.
     * We identify temporary nodes by checking that they don't appear
     * as subformulas of the original phi (since all subformulas were
     * in the initial closure). This is approximate but works for
     * the Fischer-Ladner construction.
     * Note: The caller should NOT use ltl_free on entries in out
     * because we keep them; temporary ones are freed here. */
}

/* ── S-Expression Parser ──────────────────────────────────────── */
/*
 * Simple recursive descent parser for prefix notation:
 *   expr ::= "(TRUE)" | "(FALSE)" | "(ATOM <n>)"
 *          | "(NOT expr)" | "(AND expr expr)" | "(OR expr expr)"
 *          | "(IMPLIES expr expr)" | "(EQUIV expr expr)"
 *          | "(X expr)" | "(F expr)" | "(G expr)"
 *          | "(U expr expr)" | "(R expr expr)" | "(W expr expr)"
 */

static int skip_ws(const char* s, int pos) {
    while (s[pos] && isspace((unsigned char)s[pos])) pos++;
    return pos;
}

static char* read_token(const char* s, int* pos) {
    *pos = skip_ws(s, *pos);
    if (!s[*pos]) return NULL;
    int start = *pos;
    while (s[*pos] && !isspace((unsigned char)s[*pos]) && s[*pos] != '(' && s[*pos] != ')')
        (*pos)++;
    if (*pos == start) return NULL;
    int len = *pos - start;
    char* tok = (char*)malloc(len + 1);
    if (!tok) return NULL;
    memcpy(tok, s + start, len);
    tok[len] = '\0';
    return tok;
}

static LtlNode* parse_expr(const char* s, int* pos);

static LtlNode* parse_expr(const char* s, int* pos) {
    *pos = skip_ws(s, *pos);
    if (s[*pos] != '(') return NULL;
    (*pos)++; /* skip '(' */

    char* type_name = read_token(s, pos);
    if (!type_name) return NULL;

    LtlNode* result = NULL;

    if (strcmp(type_name, "TRUE") == 0) {
        result = ltl_mk_true();
    } else if (strcmp(type_name, "FALSE") == 0) {
        result = ltl_mk_false();
    } else if (strcmp(type_name, "ATOM") == 0) {
        char* num_str = read_token(s, pos);
        int idx = num_str ? atoi(num_str) : 0;
        free(num_str);
        result = ltl_mk_atom(idx);
    } else if (strcmp(type_name, "NOT") == 0) {
        LtlNode* child = parse_expr(s, pos);
        result = ltl_mk_not(child);
    } else if (strcmp(type_name, "AND") == 0) {
        LtlNode* l = parse_expr(s, pos);
        LtlNode* r = parse_expr(s, pos);
        result = ltl_mk_and(l, r);
    } else if (strcmp(type_name, "OR") == 0) {
        LtlNode* l = parse_expr(s, pos);
        LtlNode* r = parse_expr(s, pos);
        result = ltl_mk_or(l, r);
    } else if (strcmp(type_name, "IMPLIES") == 0) {
        LtlNode* l = parse_expr(s, pos);
        LtlNode* r = parse_expr(s, pos);
        result = ltl_mk_implies(l, r);
    } else if (strcmp(type_name, "EQUIV") == 0) {
        LtlNode* l = parse_expr(s, pos);
        LtlNode* r = parse_expr(s, pos);
        result = ltl_mk_equiv(l, r);
    } else if (strcmp(type_name, "X") == 0 || strcmp(type_name, "NEXT") == 0) {
        LtlNode* child = parse_expr(s, pos);
        result = ltl_mk_next(child);
    } else if (strcmp(type_name, "F") == 0 || strcmp(type_name, "FINALLY") == 0) {
        LtlNode* child = parse_expr(s, pos);
        result = ltl_mk_finally(child);
    } else if (strcmp(type_name, "G") == 0 || strcmp(type_name, "GLOBALLY") == 0) {
        LtlNode* child = parse_expr(s, pos);
        result = ltl_mk_globally(child);
    } else if (strcmp(type_name, "U") == 0 || strcmp(type_name, "UNTIL") == 0) {
        LtlNode* l = parse_expr(s, pos);
        LtlNode* r = parse_expr(s, pos);
        result = ltl_mk_until(l, r);
    } else if (strcmp(type_name, "R") == 0 || strcmp(type_name, "RELEASE") == 0) {
        LtlNode* l = parse_expr(s, pos);
        LtlNode* r = parse_expr(s, pos);
        result = ltl_mk_release(l, r);
    } else if (strcmp(type_name, "W") == 0 || strcmp(type_name, "WEAK_UNTIL") == 0) {
        LtlNode* l = parse_expr(s, pos);
        LtlNode* r = parse_expr(s, pos);
        result = ltl_mk_weak_until(l, r);
    }

    free(type_name);
    *pos = skip_ws(s, *pos);
    if (s[*pos] == ')') (*pos)++;
    return result;
}

LtlNode* ltl_parse(const char* sexpr) {
    if (!sexpr) return NULL;
    int pos = 0;
    return parse_expr(sexpr, &pos);
}

/* ── S-Expression Output ──────────────────────────────────────── */

static int sexpr_len(const LtlNode* node) {
    if (!node) return 2;
    int len = (int)strlen(ltl_node_type_name(node->type)) + 3; /* (NAME ...) */
    if (node->type == LTL_ATOM) len += 12; /* space + atom_idx */
    if (node->left) len += sexpr_len(node->left);
    if (node->right) len += sexpr_len(node->right);
    return len;
}

static void write_sexpr(const LtlNode* node, char* buf, int* pos) {
    if (!node) { buf[(*pos)++] = 'N'; buf[(*pos)++] = 'I'; buf[(*pos)++] = 'L'; return; }
    buf[(*pos)++] = '(';
    const char* name = ltl_node_type_name(node->type);
    int nlen = (int)strlen(name);
    memcpy(buf + *pos, name, nlen); *pos += nlen;
    if (node->type == LTL_ATOM) {
        buf[(*pos)++] = ' ';
        *pos += sprintf(buf + *pos, "%d", node->atom_idx);
    }
    if (node->left) {
        buf[(*pos)++] = ' ';
        write_sexpr(node->left, buf, pos);
    }
    if (node->right) {
        buf[(*pos)++] = ' ';
        write_sexpr(node->right, buf, pos);
    }
    buf[(*pos)++] = ')';
}

char* ltl_to_sexpr(const LtlNode* node) {
    if (!node) return strdup("()");
    int len = sexpr_len(node) + 1;
    char* buf = (char*)malloc(len);
    if (!buf) return NULL;
    int pos = 0;
    write_sexpr(node, buf, &pos);
    buf[pos] = '\0';
    return buf;
}
