/*
 * omega_operations.c — Operations on Büchi Automata
 *
 * Implements union, intersection, complement, product with Kripke
 * structures, language containment, equivalence, and trimming.
 *
 * L4 Fundamental Laws: closure properties of ω-regular languages.
 * L5 Algorithms: product constructions, complementation.
 *
 * Reference:
 *   Thomas 1990 — Automata on infinite objects
 *   Vardi & Wolper 1986 — An automata-theoretic approach
 *   Kupferman & Vardi 2001 — Weak alternating automata
 *   Baier & Katoen 2008 — Principles of Model Checking, §4.3
 */

#include "omega_operations.h"
#include "buchi_emptiness.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ================================================================
 * Helper: compute alphabet union
 * ================================================================ */

static void compute_alphabet_union(const BuchiAutomaton* A1,
                                    const BuchiAutomaton* A2,
                                    BuchiSymbol** out_alpha, int* out_size) {
    int max_sym = 0;
    for (int i = 0; i < A1->alphabet_size; i++)
        if (A1->alphabet[i] > max_sym) max_sym = A1->alphabet[i];
    for (int i = 0; i < A2->alphabet_size; i++)
        if (A2->alphabet[i] > max_sym) max_sym = A2->alphabet[i];

    uint8_t* present = (uint8_t*)calloc(max_sym + 1, sizeof(uint8_t));
    if (!present) { *out_size = 0; *out_alpha = NULL; return; }

    for (int i = 0; i < A1->alphabet_size; i++)
        present[A1->alphabet[i]] = 1;
    for (int i = 0; i < A2->alphabet_size; i++)
        present[A2->alphabet[i]] = 1;

    int count = 0;
    for (int s = 0; s <= max_sym; s++)
        if (present[s]) count++;

    BuchiSymbol* alpha = (BuchiSymbol*)malloc(count * sizeof(BuchiSymbol));
    int idx = 0;
    for (int s = 0; s <= max_sym; s++)
        if (present[s]) alpha[idx++] = s;

    free(present);
    *out_alpha = alpha;
    *out_size = count;
}

/* ================================================================
 * Union: L(A₁) ∪ L(A₂)
 * ================================================================ */

BuchiAutomaton* buchi_union(const BuchiAutomaton* A1,
                             const BuchiAutomaton* A2) {
    if (!A1 || !A2) return NULL;

    int n1 = A1->n_states;
    int n2 = A2->n_states;
    int n_new = n1 + n2 + 1; /* +1 for new initial state */
    int new_q0 = 0;
    int offset1 = 1;
    int offset2 = 1 + n1;

    BuchiSymbol* alpha_union;
    int alpha_size;
    compute_alphabet_union(A1, A2, &alpha_union, &alpha_size);

    BuchiAutomaton* U = buchi_create(n_new, new_q0, alpha_size);
    buchi_set_alphabet(U, alpha_union, alpha_size);
    free(alpha_union);

    char name[256];
    snprintf(name, sizeof(name), "(%s)∪(%s)",
             A1->name ? A1->name : "A1",
             A2->name ? A2->name : "A2");
    buchi_set_name(U, name);

    /* Copy transitions from A1 (offset by 1) */
    for (int s = 0; s < n1; s++) {
        for (int a = 0; a < A1->alphabet_size; a++) {
            BuchiTransitionSet* ts = A1->delta[s][a];
            if (ts) {
                for (int i = 0; i < ts->count; i++) {
                    buchi_add_transition(U, offset1 + s, A1->alphabet[a],
                                         offset1 + ts->data[i].to);
                }
            }
        }
    }

    /* Copy transitions from A2 (offset by offset2) */
    for (int s = 0; s < n2; s++) {
        for (int a = 0; a < A2->alphabet_size; a++) {
            BuchiTransitionSet* ts = A2->delta[s][a];
            if (ts) {
                for (int i = 0; i < ts->count; i++) {
                    buchi_add_transition(U, offset2 + s, A2->alphabet[a],
                                         offset2 + ts->data[i].to);
                }
            }
        }
    }

    /* ε-transitions from new q0:
     * For each symbol in alphabet, from q0 go wherever A1.q0 or A2.q0 go */
    for (int ai = 0; ai < alpha_size; ai++) {
        BuchiSymbol sym = U->alphabet[ai];

        /* From new q0 to successors of A1.q0 on sym */
        int a1_idx = buchi_sym_index(A1, sym);
        if (a1_idx >= 0) {
            BuchiTransitionSet* ts = A1->delta[A1->q0][a1_idx];
            if (ts) {
                for (int i = 0; i < ts->count; i++)
                    buchi_add_transition(U, new_q0, sym, offset1 + ts->data[i].to);
            }
        }

        /* From new q0 to successors of A2.q0 on sym */
        int a2_idx = buchi_sym_index(A2, sym);
        if (a2_idx >= 0) {
            BuchiTransitionSet* ts = A2->delta[A2->q0][a2_idx];
            if (ts) {
                for (int i = 0; i < ts->count; i++)
                    buchi_add_transition(U, new_q0, sym, offset2 + ts->data[i].to);
            }
        }
    }

    /* Copy accepting states */
    for (int i = 0; i < A1->n_accepting; i++)
        buchi_add_accepting(U, offset1 + A1->accepting[i]);
    for (int i = 0; i < A2->n_accepting; i++)
        buchi_add_accepting(U, offset2 + A2->accepting[i]);

    return U;
}

/* ================================================================
 * Intersection: L(A₁) ∩ L(A₂)
 * ================================================================ */

BuchiAutomaton* buchi_intersect(const BuchiAutomaton* A1,
                                 const BuchiAutomaton* A2) {
    if (!A1 || !A2) return NULL;

    int n1 = A1->n_states;
    int n2 = A2->n_states;

    /* Standard construction (Choueka 1974, Baier & Katoen 2008, p.192):
     * Build a Generalized Buchi Automaton with states Q₁×Q₂×{0,1}
     * and 2 acceptance sets, then degeneralize to NBA.
     *
     * Flag semantics:
     *   0: waiting/just saw acceptance of A1 (next: look for A2 accepting)
     *   1: waiting/just saw acceptance of A2 (next: look for A1 accepting)
     *
     * Transition: flag flips from 0->1 when A1's accepting state is seen,
     *             and from 1->0 when A2's accepting state is seen.
     *
     * Accepting Set 0: flag=0 and s1 ∈ F₁
     * Accepting Set 1: flag=1 and s2 ∈ F₂
     *
     * A run is GBA-accepting iff both sets are visited ∞-often,
     * which is equivalent to Inf(ρ)∩F₁≠∅ and Inf(ρ)∩F₂≠∅. */

    int n_gba = n1 * n2 * 2;

    /* Build union alphabet */
    BuchiSymbol* alpha_union;
    int alpha_size;
    compute_alphabet_union(A1, A2, &alpha_union, &alpha_size);

    /* Build the GBA base automaton (without acceptance yet) */
    BuchiAutomaton* G = buchi_create(n_gba,
         (A1->q0 * n2 + A2->q0) * 2 + 0, alpha_size);
    buchi_set_alphabet(G, alpha_union, alpha_size);

    for (int s1 = 0; s1 < n1; s1++) {
        for (int s2 = 0; s2 < n2; s2++) {
            for (int flag = 0; flag < 2; flag++) {
                for (int ai = 0; ai < alpha_size; ai++) {
                    BuchiSymbol sym = G->alphabet[ai];
                    int i1 = buchi_sym_index(A1, sym);
                    int i2 = buchi_sym_index(A2, sym);
                    BuchiTransitionSet* ts1 = (i1 >= 0) ? A1->delta[s1][i1] : NULL;
                    BuchiTransitionSet* ts2 = (i2 >= 0) ? A2->delta[s2][i2] : NULL;
                    if (!ts1 || !ts2) continue;

                    for (int t1 = 0; t1 < ts1->count; t1++) {
                        for (int t2 = 0; t2 < ts2->count; t2++) {
                            int next_s1 = ts1->data[t1].to;
                            int next_s2 = ts2->data[t2].to;

                            /* Flag depends on the FROM state (s1, s2),
                             * not the TO state (next_s1, next_s2).
                             * (Baier & Katoen 2008, p.192, Algorithm 4.36) */
                            int next_flag = flag;
                            if (flag == 0 && A1->is_accepting[s1])
                                next_flag = 1;
                            else if (flag == 1 && A2->is_accepting[s2])
                                next_flag = 0;

                            int from = (s1 * n2 + s2) * 2 + flag;
                            int to = (next_s1 * n2 + next_s2) * 2 + next_flag;
                            buchi_add_transition(G, from, sym, to);
                        }
                    }
                }
            }
        }
    }

    /* Collect GBA acceptance sets */
    int F0_count = 0, F1_count = 0;
    for (int s1 = 0; s1 < n1; s1++)
        for (int s2 = 0; s2 < n2; s2++) {
            if (A1->is_accepting[s1]) F0_count++;
            if (A2->is_accepting[s2]) F1_count++;
        }

    BuchiState* F0 = (BuchiState*)malloc(F0_count * sizeof(BuchiState));
    BuchiState* F1 = (BuchiState*)malloc(F1_count * sizeof(BuchiState));
    int f0i = 0, f1i = 0;
    for (int s1 = 0; s1 < n1; s1++) {
        for (int s2 = 0; s2 < n2; s2++) {
            int base = (s1 * n2 + s2) * 2;
            if (A1->is_accepting[s1]) F0[f0i++] = base + 0;
            if (A2->is_accepting[s2]) F1[f1i++] = base + 1;
        }
    }

    /* Degeneralize to NBA */
    GBA* gba = gba_create(G, 2);
    gba_add_accepting_set(gba, 0, F0, F0_count);
    gba_add_accepting_set(gba, 1, F1, F1_count);

    BuchiAutomaton* result = gba_degeneralize(gba);

    char name[256];
    snprintf(name, sizeof(name), "(%s)∩(%s)",
             A1->name ? A1->name : "A1",
             A2->name ? A2->name : "A2");
    buchi_set_name(result, name);

    /* Cleanup */
    free(alpha_union);
    free(F0); free(F1);
    gba_free(gba);
    buchi_free(G);

    return result;
}

/* ================================================================
 * Projection
 * ================================================================ */

BuchiAutomaton* buchi_project(const BuchiAutomaton* A,
                               SymbolProjection proj,
                               const BuchiSymbol* new_alphabet,
                               int new_alpha_size) {
    if (!A || !proj || !new_alphabet || new_alpha_size <= 0) return NULL;

    BuchiAutomaton* P = buchi_create(A->n_states, A->q0, new_alpha_size);
    buchi_set_alphabet(P, new_alphabet, new_alpha_size);

    char name[256];
    snprintf(name, sizeof(name), "proj(%s)",
             A->name ? A->name : "A");
    buchi_set_name(P, name);

    for (int s = 0; s < A->n_states; s++) {
        for (int a = 0; a < A->alphabet_size; a++) {
            BuchiTransitionSet* ts = A->delta[s][a];
            if (ts) {
                BuchiSymbol projected_sym = proj(A->alphabet[a]);
                for (int i = 0; i < ts->count; i++) {
                    buchi_add_transition(P, s, projected_sym, ts->data[i].to);
                }
            }
        }
    }

    for (int i = 0; i < A->n_accepting; i++)
        buchi_add_accepting(P, A->accepting[i]);

    return P;
}

/* ================================================================
 * Rank-based Complementation (Kupferman & Vardi 2001)
 * ================================================================ */

/*
 * The KV rank-based construction tracks levels/ranks of states.
 * States are ordered by how many times they can "escape" from
 * the Büchi acceptance condition. The complement automaton
 * accepts runs where the ranks eventually stabilize.
 *
 * For full details see: Kupferman & Vardi, "Weak alternating
 * automata are not that weak", ACM TOCL 2001.
 *
 * Simplified implementation: subset construction with breakpoint.
 * Let D = 2^Q × 2^Q (breakpoint construction).
 * State (S, O) means: S = all reachable states, O = "obligation"
 * set that must eventually be empty (all accepting states visited).
 *
 * The complement NBA accepts if from some point onwards,
 * the obligation set O permanently becomes empty.
 */

BuchiAutomaton* buchi_complement_kv(const BuchiAutomaton* A) {
    if (!A) return NULL;

    /* We implement the Miyano-Hayashi construction:
     * States are pairs (R, S) with S ⊆ R ⊆ Q.
     * R tracks all current states (subset construction).
     * S tracks states that still owe an accepting visit.
     * Accepting: S = ∅ (obligation fulfilled).
     *
     * This gives a deterministic Streett automaton
     * with 2^{2|Q|} states in worst case.
     * We convert to NBA by considering the breakpoint
     * as the acceptance condition.
     */

    int n = A->n_states;
    int alpha_n = A->alphabet_size;

    /* For small automata (n <= 6), enumerate all subset pairs.
     * For larger, this becomes impractical. */
    if (n > 8) {
        fprintf(stderr, "buchi_complement_kv: automaton too large (%d states), "
                "use Safra-based complementation\n", n);
        return NULL;
    }

    /* Enumerate all (R, S) pairs where S ⊆ R ⊆ Q.
     * Build a mapping from (R_bits, S_bits) to state index. */
    int max_subsets = 1 << n;
    int total_pairs = 0;
    int** pair_index = (int**)malloc(max_subsets * sizeof(int*));
    for (int r = 0; r < max_subsets; r++)
        pair_index[r] = (int*)malloc(max_subsets * sizeof(int));

    /* Initialize: -1 means invalid pair */
    for (int r = 0; r < max_subsets; r++)
        for (int s = 0; s < max_subsets; s++)
            pair_index[r][s] = -1;

    /* Assign indices to valid pairs */
    int idx = 0;
    for (int R_bits = 1; R_bits < max_subsets; R_bits++) {
        for (int S_bits = 0; S_bits < max_subsets; S_bits++) {
            if ((S_bits & ~R_bits) == 0) {
                pair_index[R_bits][S_bits] = idx++;
            }
        }
    }
    total_pairs = idx;

    if (total_pairs == 0) {
        for (int r = 0; r < max_subsets; r++) free(pair_index[r]);
        free(pair_index);
        return NULL;
    }

    BuchiAutomaton* C = buchi_create(total_pairs, 0, alpha_n);
    buchi_set_alphabet(C, A->alphabet, alpha_n);

    char name[256];
    snprintf(name, sizeof(name), "not(%s)", A->name ? A->name : "A");
    buchi_set_name(C, name);

    /* Initial state: R0 = {q0}, S0 = R0 \ F */
    int R0_bits = 1 << A->q0;
    int S0_bits = R0_bits;
    for (int q = 0; q < n; q++)
        if (A->is_accepting[q])
            S0_bits &= ~(1 << q);
    C->q0 = pair_index[R0_bits][S0_bits];

    /* For each valid pair, compute transitions */
    for (int R_bits = 1; R_bits < max_subsets; R_bits++) {
        for (int S_bits = 0; S_bits < max_subsets; S_bits++) {
            if ((S_bits & ~R_bits) != 0) continue;
            int from = pair_index[R_bits][S_bits];
            if (from < 0) continue;

            /* Mark accepting: S = ∅ (all obligations fulfilled) */
            if (S_bits == 0) {
                buchi_add_accepting(C, from);
            }

            /* For each symbol, compute R' and S' */
            for (int ai = 0; ai < alpha_n; ai++) {
                int sym_idx = buchi_sym_index(A, A->alphabet[ai]);
                if (sym_idx < 0) continue;

                /* Compute R' = ∪_{q∈R} δ(q, a) */
                int Rp_bits = 0;
                for (int q = 0; q < n; q++) {
                    if (R_bits & (1 << q)) {
                        BuchiTransitionSet* ts = A->delta[q][sym_idx];
                        if (ts) {
                            for (int j = 0; j < ts->count; j++)
                                Rp_bits |= (1 << ts->data[j].to);
                        }
                    }
                }
                if (Rp_bits == 0) continue;

                /* Compute S' */
                int Sp_bits;
                if (S_bits != 0) {
                    /* S' = ∪_{q∈S} δ(q,a) \ F */
                    Sp_bits = 0;
                    for (int q = 0; q < n; q++) {
                        if (S_bits & (1 << q)) {
                            BuchiTransitionSet* ts = A->delta[q][sym_idx];
                            if (ts) {
                                for (int j = 0; j < ts->count; j++) {
                                    int t = ts->data[j].to;
                                    if (!A->is_accepting[t])
                                        Sp_bits |= (1 << t);
                                }
                            }
                        }
                    }
                } else {
                    /* S = ∅: start new obligation cycle S' = R' \ F */
                    Sp_bits = Rp_bits;
                    for (int q = 0; q < n; q++)
                        if (A->is_accepting[q])
                            Sp_bits &= ~(1 << q);
                }

                int to = pair_index[Rp_bits][Sp_bits];
                if (to >= 0) {
                    buchi_add_transition(C, from, A->alphabet[ai], to);
                }
            }
        }
    }

    for (int r = 0; r < max_subsets; r++) free(pair_index[r]);
    free(pair_index);
    return C;
}

/* ================================================================
 * Product with Kripke Structure
 * ================================================================ */

BuchiAutomaton* buchi_product_ks(const BuchiAutomaton* A,
                                  const KripkeStructure* ks) {
    if (!A || !ks) return NULL;

    /* The product has states Q_A × Q_KS.
     * Transition: ((s_ks, s_ba), (t_ks, t_ba)) exists if
     *   t_ks is a successor of s_ks in KS, and
     *   t_ba ∈ δ(s_ba, L(t_ks)) in NBA. */
    int n_ks = ks->n_states;
    int n_ba = A->n_states;
    int n_total = n_ks * n_ba;
    int q0_idx = 0 * n_ba + A->q0;

    BuchiAutomaton* P = buchi_create(n_total, q0_idx, 1);
    BuchiSymbol sym0 = 0;
    buchi_set_alphabet(P, &sym0, 1);

    char name[256];
    snprintf(name, sizeof(name), "(%s)⊗KS",
             A->name ? A->name : "A");
    buchi_set_name(P, name);

    for (int s_ks = 0; s_ks < n_ks; s_ks++) {
        for (int s_ba = 0; s_ba < n_ba; s_ba++) {
            int from = s_ks * n_ba + s_ba;
            /* Get label of current KS state */
            int label = ks->labels[s_ks];
            /* Label is a symbol: find successors in BA */
            int sym_idx = buchi_sym_index(A, label);
            if (sym_idx < 0) continue;
            BuchiTransitionSet* ts = A->delta[s_ba][sym_idx];
            if (!ts) continue;

            for (int i = 0; i < ts->count; i++) {
                int t_ba = ts->data[i].to;
                /* For each KS successor */
                for (int j = 0; ks->successors[s_ks][j] >= 0; j++) {
                    int t_ks = ks->successors[s_ks][j];
                    int to = t_ks * n_ba + t_ba;
                    buchi_add_transition(P, from, sym0, to);
                }
            }
        }
    }

    /* Accepting: states where BA state is accepting */
    for (int s_ks = 0; s_ks < n_ks; s_ks++) {
        for (int s_ba = 0; s_ba < n_ba; s_ba++) {
            if (A->is_accepting[s_ba]) {
                buchi_add_accepting(P, s_ks * n_ba + s_ba);
            }
        }
    }

    return P;
}

/* ================================================================
 * Language Containment and Equivalence
 * ================================================================ */

int buchi_language_contained(const BuchiAutomaton* A1,
                              const BuchiAutomaton* A2) {
    if (!A1 || !A2) return 0;
    /* L(A1) ⊆ L(A2) iff L(A1) ∩ L(¬A2) = ∅ */
    BuchiAutomaton* notA2 = buchi_complement_kv(A2);
    if (!notA2) return 0; /* complementation failed due to size */

    BuchiAutomaton* inter = buchi_intersect(A1, notA2);
    buchi_free(notA2);
    if (!inter) return 0;

    int contained = buchi_is_empty(inter);
    buchi_free(inter);
    return contained;
}

int buchi_language_equivalent(const BuchiAutomaton* A1,
                               const BuchiAutomaton* A2) {
    return buchi_language_contained(A1, A2) &&
           buchi_language_contained(A2, A1);
}

/* ================================================================
 * Trimming
 * ================================================================ */

BuchiAutomaton* buchi_trim(const BuchiAutomaton* A) {
    if (!A) return NULL;

    int n = A->n_states;

    /* Step 1: Forward reachability from q0 */
    uint8_t* reachable = (uint8_t*)calloc(n, sizeof(uint8_t));
    int* queue = (int*)malloc(n * sizeof(int));
    int head = 0, tail = 0;
    queue[tail++] = A->q0;
    reachable[A->q0] = 1;

    while (head < tail) {
        int v = queue[head++];
        for (int a = 0; a < A->alphabet_size; a++) {
            BuchiTransitionSet* ts = A->delta[v][a];
            if (ts) {
                for (int i = 0; i < ts->count; i++) {
                    int w = ts->data[i].to;
                    if (!reachable[w]) {
                        reachable[w] = 1;
                        queue[tail++] = w;
                    }
                }
            }
        }
    }

    /* Step 2: Backward reachability to accepting SCCs */
    /* Find accepting SCCs first */
    BuchiSCCDecomp* scc = buchi_scc_decompose(A);
    uint8_t* can_accept = (uint8_t*)calloc(n, sizeof(uint8_t));
    if (scc) {
        for (int i = 0; i < scc->n_sccs; i++) {
            if (scc->scc_accepting[i] && scc->scc_nontrivial[i]) {
                /* Mark all states in this SCC */
                for (int s = 0; s < n; s++) {
                    if (scc->scc_id[s] == i)
                        can_accept[s] = 1;
                }
            }
        }
    }

    /* Backward reachability to accepting SCCs */
    uint8_t* can_reach_accept = (uint8_t*)calloc(n, sizeof(uint8_t));
    memcpy(can_reach_accept, can_accept, n);

    for (int iter = 0; iter < n; iter++) {
        int changed = 0;
        for (int s = 0; s < n; s++) {
            if (!can_reach_accept[s] && reachable[s]) {
                for (int a = 0; a < A->alphabet_size; a++) {
                    BuchiTransitionSet* ts = A->delta[s][a];
                    if (ts) {
                        for (int i = 0; i < ts->count; i++) {
                            if (can_reach_accept[ts->data[i].to]) {
                                can_reach_accept[s] = 1;
                                changed = 1;
                                break;
                            }
                        }
                    }
                    if (can_reach_accept[s]) break;
                }
            }
        }
        if (!changed) break;
    }

    /* Step 3: Build new automaton with only good states
     * (reachable AND can reach accepting SCC) */
    int* state_map = (int*)malloc(n * sizeof(int));
    int new_count = 0;
    for (int s = 0; s < n; s++) {
        if (reachable[s] && can_reach_accept[s])
            state_map[s] = new_count++;
        else
            state_map[s] = -1;
    }

    if (new_count == 0 || state_map[A->q0] < 0) {
        /* No useful states remain - return trivial empty automaton */
        BuchiAutomaton* empty = buchi_create(1, 0, A->alphabet_size);
        buchi_set_alphabet(empty, A->alphabet, A->alphabet_size);
        free(state_map); free(reachable); free(queue);
        free(can_accept); free(can_reach_accept);
        return empty;
    }

    BuchiAutomaton* T = buchi_create(new_count, state_map[A->q0],
                                      A->alphabet_size);
    buchi_set_alphabet(T, A->alphabet, A->alphabet_size);

    char name[256];
    snprintf(name, sizeof(name), "trim(%s)",
             A->name ? A->name : "A");
    buchi_set_name(T, name);

    for (int s = 0; s < n; s++) {
        if (state_map[s] < 0) continue;
        for (int a = 0; a < A->alphabet_size; a++) {
            BuchiTransitionSet* ts = A->delta[s][a];
            if (ts) {
                for (int i = 0; i < ts->count; i++) {
                    int t = ts->data[i].to;
                    if (state_map[t] >= 0) {
                        buchi_add_transition(T, state_map[s],
                                             A->alphabet[a], state_map[t]);
                    }
                }
            }
        }
    }

    for (int i = 0; i < A->n_accepting; i++) {
        int acc = A->accepting[i];
        if (state_map[acc] >= 0)
            buchi_add_accepting(T, state_map[acc]);
    }

    free(state_map); free(reachable); free(queue);
    free(can_accept); free(can_reach_accept);
    if (scc) buchi_scc_free(scc);

    return T;
}

/* ================================================================
 * Statistics
 * ================================================================ */

BuchiStats buchi_stats(const BuchiAutomaton* A) {
    BuchiStats bs;
    memset(&bs, 0, sizeof(bs));
    if (!A) return bs;

    bs.n_states = A->n_states;
    bs.n_trans = A->n_trans_total;
    bs.n_accepting = A->n_accepting;

    /* Count reachable */
    uint8_t* visited = (uint8_t*)calloc(A->n_states, sizeof(uint8_t));
    int* queue = (int*)malloc(A->n_states * sizeof(int));
    int head = 0, tail = 0;
    queue[tail++] = A->q0;
    visited[A->q0] = 1;

    while (head < tail) {
        int v = queue[head++];
        bs.n_reachable++;
        for (int a = 0; a < A->alphabet_size; a++) {
            BuchiTransitionSet* ts = A->delta[v][a];
            if (ts) {
                for (int i = 0; i < ts->count; i++) {
                    int w = ts->data[i].to;
                    if (!visited[w]) {
                        visited[w] = 1;
                        queue[tail++] = w;
                    }
                }
            }
        }
    }

    BuchiSCCDecomp* scc = buchi_scc_decompose(A);
    if (scc) {
        bs.n_sccs = scc->n_sccs;
        for (int i = 0; i < scc->n_sccs; i++) {
            if (scc->scc_accepting[i] && scc->scc_nontrivial[i])
                bs.n_accepting_sccs++;
        }
        buchi_scc_free(scc);
    }

    free(visited); free(queue);
    return bs;
}

void buchi_stats_print(const BuchiStats* bs) {
    if (!bs) return;
    printf("Buchi Automaton Statistics:\n");
    printf("  States: %d (reachable: %d)\n", bs->n_states, bs->n_reachable);
    printf("  Transitions: %d\n", bs->n_trans);
    printf("  Accepting states: %d\n", bs->n_accepting);
    printf("  SCCs: %d (accepting: %d)\n", bs->n_sccs, bs->n_accepting_sccs);
}
