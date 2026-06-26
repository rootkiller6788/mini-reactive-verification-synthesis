/**
 * ctl_ast.c — CTL Formula AST Implementation
 *
 * Full implementation of CTL formula construction, destruction,
 * traversal, printing, normalization, and analysis utilities.
 *
 * Knowledge: L1 (Definitions), L3 (Mathematical Structures)
 * Reference: Clarke, Emerson, Sistla (1986) ACM TOPLAS
 *            Baier & Katoen "Principles of Model Checking" (2008), Ch 6
 */

#include "ctl_ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ═══════════════════════════════════════════════════════════════════════
 * Internal Helpers
 * ═══════════════════════════════════════════════════════════════════════ */

static ctl_formula *ctl_alloc_node(ctl_formula_type t) {
    ctl_formula *f = (ctl_formula *)calloc(1, sizeof(ctl_formula));
    if (f) f->type = t;
    return f;
}

static char *ctl_strdup_safe(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *d = (char *)malloc(len + 1);
    if (d) memcpy(d, s, len + 1);
    return d;
}

static ctl_formula *mk_binary(ctl_formula_type t,
                               ctl_formula *c1, ctl_formula *c2) {
    if (!c1 || !c2) { ctl_free_formula(c1); ctl_free_formula(c2); return NULL; }
    ctl_formula *f = ctl_alloc_node(t);
    if (!f) { ctl_free_formula(c1); ctl_free_formula(c2); return NULL; }
    f->data.binary.left  = c1;
    f->data.binary.right = c2;
    return f;
}

static ctl_formula *mk_unary(ctl_formula_type t, ctl_formula *child) {
    if (!child) return NULL;
    ctl_formula *f = ctl_alloc_node(t);
    if (!f) { ctl_free_formula(child); return NULL; }
    f->data.unary.child = child;
    return f;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Constructors — Constants
 * ═══════════════════════════════════════════════════════════════════════ */

ctl_formula *ctl_mk_top(void) {
    return ctl_alloc_node(CTL_TOP);
}

ctl_formula *ctl_mk_bot(void) {
    return ctl_alloc_node(CTL_BOT);
}

ctl_formula *ctl_mk_atom(const char *name) {
    if (!name) return NULL;
    ctl_formula *f = ctl_alloc_node(CTL_ATOM);
    if (!f) return NULL;
    f->data.atom_name = ctl_strdup_safe(name);
    if (!f->data.atom_name) { free(f); return NULL; }
    return f;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Constructors — Boolean Connectives
 * ═══════════════════════════════════════════════════════════════════════ */

ctl_formula *ctl_mk_not(ctl_formula *child) {
    return mk_unary(CTL_NOT, child);
}

ctl_formula *ctl_mk_and(ctl_formula *c1, ctl_formula *c2) {
    return mk_binary(CTL_AND, c1, c2);
}

ctl_formula *ctl_mk_or(ctl_formula *c1, ctl_formula *c2) {
    return mk_binary(CTL_OR, c1, c2);
}

ctl_formula *ctl_mk_implies(ctl_formula *c1, ctl_formula *c2) {
    return mk_binary(CTL_IMPLIES, c1, c2);
}

ctl_formula *ctl_mk_iff(ctl_formula *c1, ctl_formula *c2) {
    return mk_binary(CTL_IFF, c1, c2);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Constructors — Temporal Operators
 * ═══════════════════════════════════════════════════════════════════════ */

ctl_formula *ctl_mk_ex(ctl_formula *child) { return mk_unary(CTL_EX, child); }
ctl_formula *ctl_mk_ef(ctl_formula *child) { return mk_unary(CTL_EF, child); }
ctl_formula *ctl_mk_eg(ctl_formula *child) { return mk_unary(CTL_EG, child); }
ctl_formula *ctl_mk_eu(ctl_formula *l, ctl_formula *r) { return mk_binary(CTL_EU, l, r); }
ctl_formula *ctl_mk_er(ctl_formula *l, ctl_formula *r) { return mk_binary(CTL_ER, l, r); }
ctl_formula *ctl_mk_ax(ctl_formula *child) { return mk_unary(CTL_AX, child); }
ctl_formula *ctl_mk_af(ctl_formula *child) { return mk_unary(CTL_AF, child); }
ctl_formula *ctl_mk_ag(ctl_formula *child) { return mk_unary(CTL_AG, child); }
ctl_formula *ctl_mk_au(ctl_formula *l, ctl_formula *r) { return mk_binary(CTL_AU, l, r); }
ctl_formula *ctl_mk_ar(ctl_formula *l, ctl_formula *r) { return mk_binary(CTL_AR, l, r); }

/* ═══════════════════════════════════════════════════════════════════════
 * Destructor
 * ═══════════════════════════════════════════════════════════════════════ */

void ctl_free_formula(ctl_formula *f) {
    if (!f) return;
    switch (f->type) {
    case CTL_TOP: case CTL_BOT: break;
    case CTL_ATOM: free(f->data.atom_name); break;
    case CTL_NOT: case CTL_EX: case CTL_AX:
    case CTL_EF: case CTL_AF: case CTL_EG: case CTL_AG:
        ctl_free_formula(f->data.unary.child); break;
    default:
        ctl_free_formula(f->data.binary.left);
        ctl_free_formula(f->data.binary.right); break;
    }
    free(f);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Clone (Deep Copy)
 * ═══════════════════════════════════════════════════════════════════════ */

ctl_formula *ctl_clone_formula(const ctl_formula *f) {
    if (!f) return NULL;
    switch (f->type) {
    case CTL_TOP:  return ctl_mk_top();
    case CTL_BOT:  return ctl_mk_bot();
    case CTL_ATOM: return ctl_mk_atom(f->data.atom_name);
    case CTL_NOT:  return ctl_mk_not(ctl_clone_formula(f->data.unary.child));
    case CTL_EX:   return ctl_mk_ex(ctl_clone_formula(f->data.unary.child));
    case CTL_AX:   return ctl_mk_ax(ctl_clone_formula(f->data.unary.child));
    case CTL_EF:   return ctl_mk_ef(ctl_clone_formula(f->data.unary.child));
    case CTL_AF:   return ctl_mk_af(ctl_clone_formula(f->data.unary.child));
    case CTL_EG:   return ctl_mk_eg(ctl_clone_formula(f->data.unary.child));
    case CTL_AG:   return ctl_mk_ag(ctl_clone_formula(f->data.unary.child));
    default:
        return mk_binary(f->type,
                         ctl_clone_formula(f->data.binary.left),
                         ctl_clone_formula(f->data.binary.right));
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * Size and Height Metrics
 * ═══════════════════════════════════════════════════════════════════════ */

size_t ctl_formula_height(const ctl_formula *f) {
    if (!f) return 0;
    switch (f->type) {
    case CTL_TOP: case CTL_BOT: case CTL_ATOM: return 1;
    case CTL_NOT: case CTL_EX: case CTL_AX:
    case CTL_EF: case CTL_AF: case CTL_EG: case CTL_AG:
        return 1 + ctl_formula_height(f->data.unary.child);
    default: {
        size_t lh = ctl_formula_height(f->data.binary.left);
        size_t rh = ctl_formula_height(f->data.binary.right);
        return 1 + (lh > rh ? lh : rh);
    }
    }
}

size_t ctl_formula_size(const ctl_formula *f) {
    if (!f) return 0;
    switch (f->type) {
    case CTL_TOP: case CTL_BOT: case CTL_ATOM: return 1;
    case CTL_NOT: case CTL_EX: case CTL_AX:
    case CTL_EF: case CTL_AF: case CTL_EG: case CTL_AG:
        return 1 + ctl_formula_size(f->data.unary.child);
    default:
        return 1 + ctl_formula_size(f->data.binary.left)
                 + ctl_formula_size(f->data.binary.right);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * Atom Collection
 * ═══════════════════════════════════════════════════════════════════════ */

size_t ctl_collect_atoms(const ctl_formula *f, const char **names,
                          size_t capacity) {
    if (!f) return 0;
    size_t count = 0;
    if (f->type == CTL_ATOM) {
        if (names && count < capacity) names[count] = f->data.atom_name;
        return 1;
    }
    if (f->type == CTL_TOP || f->type == CTL_BOT) return 0;
    switch (f->type) {
    case CTL_NOT: case CTL_EX: case CTL_AX:
    case CTL_EF: case CTL_AF: case CTL_EG: case CTL_AG:
        return ctl_collect_atoms(f->data.unary.child, names, capacity);
    default:
        break;
    }
    count = ctl_collect_atoms(f->data.binary.left, names, capacity);
    if (names && capacity > count) {
        count += ctl_collect_atoms(f->data.binary.right,
                                    names + count, capacity - count);
    } else {
        count += ctl_collect_atoms(f->data.binary.right, NULL, 0);
    }
    return count;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Structural Equality
 * ═══════════════════════════════════════════════════════════════════════ */

int ctl_formula_equal(const ctl_formula *a, const ctl_formula *b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    if (a->type != b->type) return 0;
    switch (a->type) {
    case CTL_TOP: case CTL_BOT: return 1;
    case CTL_ATOM:
        return strcmp(a->data.atom_name, b->data.atom_name) == 0;
    case CTL_NOT: case CTL_EX: case CTL_AX:
    case CTL_EF: case CTL_AF: case CTL_EG: case CTL_AG:
        return ctl_formula_equal(a->data.unary.child, b->data.unary.child);
    default:
        return ctl_formula_equal(a->data.binary.left, b->data.binary.left) &&
               ctl_formula_equal(a->data.binary.right, b->data.binary.right);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * Subformula Check
 * ═══════════════════════════════════════════════════════════════════════ */

int ctl_is_subformula(const ctl_formula *f, const ctl_formula *sub) {
    if (!f || !sub) return 0;
    if (ctl_formula_equal(f, sub)) return 1;
    switch (f->type) {
    case CTL_TOP: case CTL_BOT: case CTL_ATOM: return 0;
    case CTL_NOT: case CTL_EX: case CTL_AX:
    case CTL_EF: case CTL_AF: case CTL_EG: case CTL_AG:
        return ctl_is_subformula(f->data.unary.child, sub);
    default:
        return ctl_is_subformula(f->data.binary.left, sub) ||
               ctl_is_subformula(f->data.binary.right, sub);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * Immediate Subformulas & Type Name
 * ═══════════════════════════════════════════════════════════════════════ */

size_t ctl_immediate_subformulas(const ctl_formula *f,
                                  ctl_formula **out, size_t capacity) {
    if (!f) return 0;
    size_t n = 0;
    switch (f->type) {
    case CTL_TOP: case CTL_BOT: case CTL_ATOM: break;
    case CTL_NOT: case CTL_EX: case CTL_AX:
    case CTL_EF: case CTL_AF: case CTL_EG: case CTL_AG:
        if (out && n < capacity) out[n] = f->data.unary.child;
        n = 1; break;
    default:
        if (out && n < capacity) out[n] = f->data.binary.left;
        n++;
        if (out && n < capacity) out[n] = f->data.binary.right;
        n++; break;
    }
    return n;
}

const char *ctl_formula_type_name(ctl_formula_type t) {
    switch (t) {
    case CTL_TOP:     return "TOP";
    case CTL_BOT:     return "BOT";
    case CTL_ATOM:    return "ATOM";
    case CTL_NOT:     return "NOT";
    case CTL_AND:     return "AND";
    case CTL_OR:      return "OR";
    case CTL_IMPLIES: return "IMPLIES";
    case CTL_IFF:     return "IFF";
    case CTL_EX:      return "EX";
    case CTL_AX:      return "AX";
    case CTL_EF:      return "EF";
    case CTL_AF:      return "AF";
    case CTL_EG:      return "EG";
    case CTL_AG:      return "AG";
    case CTL_EU:      return "EU";
    case CTL_AU:      return "AU";
    case CTL_ER:      return "ER";
    case CTL_AR:      return "AR";
    }
    return "UNKNOWN";
}

/* ═══════════════════════════════════════════════════════════════════════
 * Pretty-Printing
 * ═══════════════════════════════════════════════════════════════════════ */

static int op_precedence(ctl_formula_type t) {
    switch (t) {
    case CTL_NOT: case CTL_EX: case CTL_AX:
    case CTL_EF: case CTL_AF: case CTL_EG: case CTL_AG:
        return 5;
    case CTL_AND:    return 4;
    case CTL_OR:     return 3;
    case CTL_IMPLIES: return 2;
    case CTL_IFF:    return 2;
    case CTL_EU: case CTL_AU: case CTL_ER: case CTL_AR:
        return 1;
    default: return 0;
    }
}

static int needs_parens(ctl_formula_type parent, ctl_formula_type child,
                        int is_right) {
    int pp = op_precedence(parent);
    int cp = op_precedence(child);
    if (cp < pp) return 1;
    if (cp == pp && parent != child) return 1;
    if (parent == CTL_IMPLIES && child == CTL_IMPLIES && !is_right) return 1;
    return 0;
}

static int print_to_buf(char *buf, size_t size, int *pos,
                         const ctl_formula *f, int parent_prec, int is_right) {
    if (!f) return *pos;
    int p = *pos;
    int paren = needs_parens((ctl_formula_type)parent_prec, f->type, is_right);
    if (paren && p < (int)size - 1) buf[p++] = '(';

    switch (f->type) {
    case CTL_TOP:  if (p < (int)size - 1) buf[p++] = 'T'; break;
    case CTL_BOT:  if (p < (int)size - 1) buf[p++] = 'F'; break;
    case CTL_ATOM: {
        const char *s = f->data.atom_name;
        while (*s && p < (int)size - 1) buf[p++] = *s++;
        break;
    }
    case CTL_NOT:
        if (p < (int)size - 1) buf[p++] = '!';
        p = print_to_buf(buf, size, &p, f->data.unary.child,
                          op_precedence(CTL_NOT), 0);
        break;
    case CTL_EX:
        if (p < (int)size - 2) { buf[p++] = 'E'; buf[p++] = 'X'; }
        p = print_to_buf(buf, size, &p, f->data.unary.child,
                          op_precedence(CTL_EX), 0);
        break;
    case CTL_AX:
        if (p < (int)size - 2) { buf[p++] = 'A'; buf[p++] = 'X'; }
        p = print_to_buf(buf, size, &p, f->data.unary.child,
                          op_precedence(CTL_AX), 0);
        break;
    case CTL_EF:
        if (p < (int)size - 2) { buf[p++] = 'E'; buf[p++] = 'F'; }
        p = print_to_buf(buf, size, &p, f->data.unary.child,
                          op_precedence(CTL_EF), 0);
        break;
    case CTL_AF:
        if (p < (int)size - 2) { buf[p++] = 'A'; buf[p++] = 'F'; }
        p = print_to_buf(buf, size, &p, f->data.unary.child,
                          op_precedence(CTL_AF), 0);
        break;
    case CTL_EG:
        if (p < (int)size - 2) { buf[p++] = 'E'; buf[p++] = 'G'; }
        p = print_to_buf(buf, size, &p, f->data.unary.child,
                          op_precedence(CTL_EG), 0);
        break;
    case CTL_AG:
        if (p < (int)size - 2) { buf[p++] = 'A'; buf[p++] = 'G'; }
        p = print_to_buf(buf, size, &p, f->data.unary.child,
                          op_precedence(CTL_AG), 0);
        break;
    case CTL_AND:
        p = print_to_buf(buf, size, &p, f->data.binary.left,
                          op_precedence(CTL_AND), 0);
        if (p < (int)size - 3) {
            buf[p++] = ' '; buf[p++] = '&'; buf[p++] = '&'; buf[p++] = ' ';
        }
        p = print_to_buf(buf, size, &p, f->data.binary.right,
                          op_precedence(CTL_AND), 1);
        break;
    case CTL_OR:
        p = print_to_buf(buf, size, &p, f->data.binary.left,
                          op_precedence(CTL_OR), 0);
        if (p < (int)size - 3) {
            buf[p++] = ' '; buf[p++] = '|'; buf[p++] = '|'; buf[p++] = ' ';
        }
        p = print_to_buf(buf, size, &p, f->data.binary.right,
                          op_precedence(CTL_OR), 1);
        break;
    case CTL_IMPLIES:
        p = print_to_buf(buf, size, &p, f->data.binary.left,
                          op_precedence(CTL_IMPLIES), 0);
        if (p < (int)size - 3) {
            buf[p++] = ' '; buf[p++] = '-'; buf[p++] = '>'; buf[p++] = ' ';
        }
        p = print_to_buf(buf, size, &p, f->data.binary.right,
                          op_precedence(CTL_IMPLIES), 1);
        break;
    case CTL_IFF:
        p = print_to_buf(buf, size, &p, f->data.binary.left,
                          op_precedence(CTL_IFF), 0);
        if (p < (int)size - 4) {
            buf[p++] = ' '; buf[p++] = '<'; buf[p++] = '-';
            buf[p++] = '>'; buf[p++] = ' ';
        }
        p = print_to_buf(buf, size, &p, f->data.binary.right,
                          op_precedence(CTL_IFF), 1);
        break;
    case CTL_EU:
        if (p < (int)size - 2) { buf[p++] = 'E'; buf[p++] = '['; }
        p = print_to_buf(buf, size, &p, f->data.binary.left, 1, 0);
        if (p < (int)size - 2) { buf[p++] = ' '; buf[p++] = 'U'; buf[p++] = ' '; }
        p = print_to_buf(buf, size, &p, f->data.binary.right, 1, 0);
        if (p < (int)size - 1) buf[p++] = ']';
        break;
    case CTL_AU:
        if (p < (int)size - 2) { buf[p++] = 'A'; buf[p++] = '['; }
        p = print_to_buf(buf, size, &p, f->data.binary.left, 1, 0);
        if (p < (int)size - 2) { buf[p++] = ' '; buf[p++] = 'U'; buf[p++] = ' '; }
        p = print_to_buf(buf, size, &p, f->data.binary.right, 1, 0);
        if (p < (int)size - 1) buf[p++] = ']';
        break;
    case CTL_ER:
        if (p < (int)size - 2) { buf[p++] = 'E'; buf[p++] = '['; }
        p = print_to_buf(buf, size, &p, f->data.binary.left, 1, 0);
        if (p < (int)size - 2) { buf[p++] = ' '; buf[p++] = 'R'; buf[p++] = ' '; }
        p = print_to_buf(buf, size, &p, f->data.binary.right, 1, 0);
        if (p < (int)size - 1) buf[p++] = ']';
        break;
    case CTL_AR:
        if (p < (int)size - 2) { buf[p++] = 'A'; buf[p++] = '['; }
        p = print_to_buf(buf, size, &p, f->data.binary.left, 1, 0);
        if (p < (int)size - 2) { buf[p++] = ' '; buf[p++] = 'R'; buf[p++] = ' '; }
        p = print_to_buf(buf, size, &p, f->data.binary.right, 1, 0);
        if (p < (int)size - 1) buf[p++] = ']';
        break;
    }

    if (paren && p < (int)size - 1) buf[p++] = ')';
    *pos = p;
    return p;
}

int ctl_snprint_formula(char *buf, size_t size, const ctl_formula *f) {
    if (!buf || size == 0) return 0;
    int pos = 0;
    print_to_buf(buf, size, &pos, f, 0, 0);
    if (pos < (int)size)
        buf[pos] = '\0';
    else
        buf[size - 1] = '\0';
    return pos;
}

void ctl_print_formula(const ctl_formula *f) {
    if (!f) { printf("(null)\n"); return; }
    char buf[4096];
    ctl_snprint_formula(buf, sizeof(buf), f);
    printf("%s\n", buf);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Hashing, Canonicalization, PNF, Temporal Detection, etc.
 * ═══════════════════════════════════════════════════════════════════════ */

unsigned long ctl_formula_hash(const ctl_formula *f) {
    if (!f) return 0;
    unsigned long hash = 5381UL;  /* djb2 initial hash */
    unsigned char t = (unsigned char)f->type;
    hash ^= t;
    hash *= 1099511628211ULL;
    switch (f->type) {
    case CTL_TOP: case CTL_BOT: break;
    case CTL_ATOM: {
        const char *s = f->data.atom_name;
        while (*s) {
            hash ^= (unsigned char)*s++;
            hash *= 1099511628211ULL;
        }
        break;
    }
    case CTL_NOT: case CTL_EX: case CTL_AX:
    case CTL_EF: case CTL_AF: case CTL_EG: case CTL_AG:
        hash ^= ctl_formula_hash(f->data.unary.child);
        hash *= 1099511628211ULL;
        break;
    default:
        hash ^= ctl_formula_hash(f->data.binary.left);
        hash *= 1099511628211ULL;
        hash ^= ctl_formula_hash(f->data.binary.right);
        hash *= 1099511628211ULL;
        break;
    }
    return hash;
}

ctl_formula *ctl_canonicalize_formula(const ctl_formula *f) {
    if (!f) return NULL;
    switch (f->type) {
    case CTL_TOP:  return ctl_mk_top();
    case CTL_BOT:  return ctl_mk_bot();
    case CTL_ATOM: return ctl_mk_atom(f->data.atom_name);
    case CTL_NOT: case CTL_EX: case CTL_AX:
    case CTL_EF: case CTL_AF: case CTL_EG: case CTL_AG:
        return mk_unary(f->type, ctl_canonicalize_formula(f->data.unary.child));
    default: {
        ctl_formula *l = ctl_canonicalize_formula(f->data.binary.left);
        ctl_formula *r = ctl_canonicalize_formula(f->data.binary.right);
        if (f->type == CTL_AND || f->type == CTL_OR || f->type == CTL_IFF) {
            if (ctl_formula_hash(l) > ctl_formula_hash(r)) {
                ctl_formula *tmp = l; l = r; r = tmp;
            }
        }
        return mk_binary(f->type, l, r);
    }
    }
}

ctl_formula *ctl_to_pnf(const ctl_formula *f) {
    if (!f) return NULL;
    switch (f->type) {
    case CTL_TOP: case CTL_BOT: case CTL_ATOM:
        return ctl_clone_formula(f);
    case CTL_NOT: {
        ctl_formula *c = f->data.unary.child;
        switch (c->type) {
        case CTL_TOP: return ctl_mk_bot();
        case CTL_BOT: return ctl_mk_top();
        case CTL_ATOM: return ctl_mk_not(ctl_mk_atom(c->data.atom_name));
        case CTL_NOT: return ctl_to_pnf(c->data.unary.child);
        case CTL_AND: {
            ctl_formula *na = ctl_to_pnf(ctl_mk_not(ctl_clone_formula(c->data.binary.left)));
            ctl_formula *nb = ctl_to_pnf(ctl_mk_not(ctl_clone_formula(c->data.binary.right)));
            return ctl_mk_or(na, nb);
        }
        case CTL_OR: {
            ctl_formula *na = ctl_to_pnf(ctl_mk_not(ctl_clone_formula(c->data.binary.left)));
            ctl_formula *nb = ctl_to_pnf(ctl_mk_not(ctl_clone_formula(c->data.binary.right)));
            return ctl_mk_and(na, nb);
        }
        case CTL_EX:
            return ctl_mk_ax(ctl_to_pnf(ctl_mk_not(ctl_clone_formula(c->data.unary.child))));
        case CTL_AX:
            return ctl_mk_ex(ctl_to_pnf(ctl_mk_not(ctl_clone_formula(c->data.unary.child))));
        case CTL_EF:
            return ctl_mk_ag(ctl_to_pnf(ctl_mk_not(ctl_clone_formula(c->data.unary.child))));
        case CTL_AF:
            return ctl_mk_eg(ctl_to_pnf(ctl_mk_not(ctl_clone_formula(c->data.unary.child))));
        case CTL_EG:
            return ctl_mk_af(ctl_to_pnf(ctl_mk_not(ctl_clone_formula(c->data.unary.child))));
        case CTL_AG:
            return ctl_mk_ef(ctl_to_pnf(ctl_mk_not(ctl_clone_formula(c->data.unary.child))));
        case CTL_EU: {
            ctl_formula *nb = ctl_to_pnf(ctl_mk_not(ctl_clone_formula(c->data.binary.right)));
            ctl_formula *na = ctl_to_pnf(ctl_mk_not(ctl_clone_formula(c->data.binary.left)));
            return ctl_mk_ar(nb, na);
        }
        case CTL_AU: {
            ctl_formula *nb = ctl_to_pnf(ctl_mk_not(ctl_clone_formula(c->data.binary.right)));
            ctl_formula *na = ctl_to_pnf(ctl_mk_not(ctl_clone_formula(c->data.binary.left)));
            return ctl_mk_er(nb, na);
        }
        case CTL_ER: {
            ctl_formula *na = ctl_to_pnf(ctl_mk_not(ctl_clone_formula(c->data.binary.left)));
            ctl_formula *nb = ctl_to_pnf(ctl_mk_not(ctl_clone_formula(c->data.binary.right)));
            return ctl_mk_au(na, nb);
        }
        case CTL_AR: {
            ctl_formula *na = ctl_to_pnf(ctl_mk_not(ctl_clone_formula(c->data.binary.left)));
            ctl_formula *nb = ctl_to_pnf(ctl_mk_not(ctl_clone_formula(c->data.binary.right)));
            return ctl_mk_eu(na, nb);
        }
        default: return ctl_mk_not(ctl_to_pnf(c));
        }
    }
    case CTL_EX: case CTL_AX: case CTL_EF: case CTL_AF:
    case CTL_EG: case CTL_AG:
        return mk_unary(f->type, ctl_to_pnf(f->data.unary.child));
    default:
        return mk_binary(f->type,
                         ctl_to_pnf(f->data.binary.left),
                         ctl_to_pnf(f->data.binary.right));
    }
}

unsigned int ctl_temporal_operators_used(const ctl_formula *f) {
    if (!f) return 0;
    unsigned int mask = 0;
    switch (f->type) {
    case CTL_EX: mask = 1u << 0; break;
    case CTL_AX: mask = 1u << 1; break;
    case CTL_EF: mask = 1u << 2; break;
    case CTL_AF: mask = 1u << 3; break;
    case CTL_EG: mask = 1u << 4; break;
    case CTL_AG: mask = 1u << 5; break;
    case CTL_EU: mask = 1u << 6; break;
    case CTL_AU: mask = 1u << 7; break;
    case CTL_ER: mask = 1u << 8; break;
    case CTL_AR: mask = 1u << 9; break;
    default: break;
    }
    switch (f->type) {
    case CTL_TOP: case CTL_BOT: case CTL_ATOM: return mask;
    case CTL_NOT: case CTL_EX: case CTL_AX:
    case CTL_EF: case CTL_AF: case CTL_EG: case CTL_AG:
        return mask | ctl_temporal_operators_used(f->data.unary.child);
    default:
        return mask | ctl_temporal_operators_used(f->data.binary.left)
                    | ctl_temporal_operators_used(f->data.binary.right);
    }
}

int ctl_is_state_formula(const ctl_formula *f) {
    if (!f) return 0;
    switch (f->type) {
    case CTL_TOP: case CTL_BOT: case CTL_ATOM: return 1;
    case CTL_NOT: return ctl_is_state_formula(f->data.unary.child);
    case CTL_AND: case CTL_OR: case CTL_IMPLIES: case CTL_IFF:
        return ctl_is_state_formula(f->data.binary.left) &&
               ctl_is_state_formula(f->data.binary.right);
    case CTL_EX: case CTL_AX: case CTL_EF: case CTL_AF:
    case CTL_EG: case CTL_AG:
        return ctl_is_state_formula(f->data.unary.child);
    case CTL_EU: case CTL_AU: case CTL_ER: case CTL_AR:
        return ctl_is_state_formula(f->data.binary.left) &&
               ctl_is_state_formula(f->data.binary.right);
    }
    return 1;
}

int ctl_alternation_depth(const ctl_formula *f) {
    if (!f) return 0;
    int this_quant = 0;
    switch (f->type) {
    case CTL_TOP: case CTL_BOT: case CTL_ATOM:
    case CTL_NOT: case CTL_AND: case CTL_OR:
    case CTL_IMPLIES: case CTL_IFF: this_quant = 0; break;
    case CTL_EX: case CTL_EF: case CTL_EG:
    case CTL_EU: case CTL_ER: this_quant = 1; break;
    case CTL_AX: case CTL_AF: case CTL_AG:
    case CTL_AU: case CTL_AR: this_quant = 2; break;
    }
    int child_depth = 0;
    switch (f->type) {
    case CTL_TOP: case CTL_BOT: case CTL_ATOM: child_depth = 0; break;
    case CTL_NOT: child_depth = ctl_alternation_depth(f->data.unary.child); break;
    case CTL_AND: case CTL_OR: case CTL_IMPLIES: case CTL_IFF: {
        int l = ctl_alternation_depth(f->data.binary.left);
        int r = ctl_alternation_depth(f->data.binary.right);
        child_depth = (l > r ? l : r); break;
    }
    case CTL_EX: case CTL_AX: case CTL_EF: case CTL_AF:
    case CTL_EG: case CTL_AG:
        child_depth = ctl_alternation_depth(f->data.unary.child); break;
    case CTL_EU: case CTL_AU: case CTL_ER: case CTL_AR: {
        int l = ctl_alternation_depth(f->data.binary.left);
        int r = ctl_alternation_depth(f->data.binary.right);
        child_depth = (l > r ? l : r); break;
    }
    }
    if (this_quant == 0) return child_depth;
    return (child_depth > 0 ? child_depth : 1);
}
