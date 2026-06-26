/**
 * ltl_eval.c -- LTL Semantic Evaluation + Printing
 *
 * Implements Kripke-style semantics for LTL on omega-words,
 * and formula printing.
 *
 * Knowledge: L2 LTL semantics, L3 Kripke/Tarskian evaluation.
 */

#include <stdlib.h>
#include <stdio.h>
#include "ltl_formula.h"

bool ltl_eval(const ltl_node_t *phi, size_t pos,
              ltl_assign_fn assign, void *ctx)
{
    if (!phi) return false;

    switch (phi->op) {
    case LTL_TRUE:   return true;
    case LTL_FALSE:  return false;
    case LTL_ATOM:   return assign ? assign(phi->atom_id, pos, ctx) : false;
    case LTL_NOT:    return !ltl_eval(phi->left, pos, assign, ctx);
    case LTL_AND:
        return ltl_eval(phi->left, pos, assign, ctx)
            && ltl_eval(phi->right, pos, assign, ctx);
    case LTL_OR:
        return ltl_eval(phi->left, pos, assign, ctx)
            || ltl_eval(phi->right, pos, assign, ctx);
    case LTL_IMPLIES:
        return !ltl_eval(phi->left, pos, assign, ctx)
            || ltl_eval(phi->right, pos, assign, ctx);
    case LTL_NEXT:
        return ltl_eval(phi->left, pos + 1, assign, ctx);
    case LTL_UNTIL: {
        size_t limit = pos + 1000;
        for (size_t k = pos; k < limit; k++) {
            if (!ltl_eval(phi->right, k, assign, ctx)) continue;
            bool all_left = true;
            for (size_t j = pos; j < k && all_left; j++)
                if (!ltl_eval(phi->left, j, assign, ctx))
                    all_left = false;
            if (all_left) return true;
        }
        return false;
    }
    case LTL_RELEASE: {
        size_t limit = pos + 1000;
        for (size_t k = pos; k < limit; k++) {
            if (ltl_eval(phi->left, k, assign, ctx)) {
                bool all_right = true;
                for (size_t j = pos; j <= k && all_right; j++)
                    if (!ltl_eval(phi->right, j, assign, ctx))
                        all_right = false;
                if (all_right) return true;
            }
            if (!ltl_eval(phi->right, k, assign, ctx)) return false;
        }
        return true;
    }
    case LTL_EVENTUALLY:
        return ltl_eval(ltl_make_until(ltl_make_true(), phi->left),
                        pos, assign, ctx);
    case LTL_ALWAYS:
        return !ltl_eval(ltl_make_eventually(ltl_make_not(phi->left)),
                         pos, assign, ctx);
    case LTL_WEAK_UNTIL:
        return ltl_eval(ltl_make_until(phi->left, phi->right),
                        pos, assign, ctx)
            || ltl_eval(ltl_make_always(phi->left), pos, assign, ctx);
    default:
        return false;
    }
}

void ltl_print(const ltl_node_t *root, FILE *fp)
{
    if (!root || !fp) return;
    switch (root->op) {
    case LTL_TRUE:       fprintf(fp, "true"); break;
    case LTL_FALSE:      fprintf(fp, "false"); break;
    case LTL_ATOM:       fprintf(fp, "p%d", root->atom_id); break;
    case LTL_NOT:
        fprintf(fp, "!("); ltl_print(root->left, fp);
        fprintf(fp, ")"); break;
    case LTL_AND:
        fprintf(fp, "("); ltl_print(root->left, fp);
        fprintf(fp, " & "); ltl_print(root->right, fp);
        fprintf(fp, ")"); break;
    case LTL_OR:
        fprintf(fp, "("); ltl_print(root->left, fp);
        fprintf(fp, " | "); ltl_print(root->right, fp);
        fprintf(fp, ")"); break;
    case LTL_IMPLIES:
        fprintf(fp, "("); ltl_print(root->left, fp);
        fprintf(fp, " -> "); ltl_print(root->right, fp);
        fprintf(fp, ")"); break;
    case LTL_NEXT:
        fprintf(fp, "X("); ltl_print(root->left, fp);
        fprintf(fp, ")"); break;
    case LTL_UNTIL:
        fprintf(fp, "("); ltl_print(root->left, fp);
        fprintf(fp, " U "); ltl_print(root->right, fp);
        fprintf(fp, ")"); break;
    case LTL_RELEASE:
        fprintf(fp, "("); ltl_print(root->left, fp);
        fprintf(fp, " R "); ltl_print(root->right, fp);
        fprintf(fp, ")"); break;
    case LTL_EVENTUALLY:
        fprintf(fp, "F("); ltl_print(root->left, fp);
        fprintf(fp, ")"); break;
    case LTL_ALWAYS:
        fprintf(fp, "G("); ltl_print(root->left, fp);
        fprintf(fp, ")"); break;
    case LTL_WEAK_UNTIL:
        fprintf(fp, "("); ltl_print(root->left, fp);
        fprintf(fp, " W "); ltl_print(root->right, fp);
        fprintf(fp, ")"); break;
    default: fprintf(fp, "?"); break;
    }
}
