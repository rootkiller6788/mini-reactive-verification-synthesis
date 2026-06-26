/*
 * buchi.h ¡ª Nondeterministic B¨¹chi Automaton on Infinite Words
 *
 * L1 Definition (B¨¹chi, 1962):
 *   An NBA is a 5-tuple A = (Q, ¦², ¦Ä, q?, F) where:
 *     Q:   finite set of states
 *     ¦²:   finite alphabet
 *     ¦Ä:   Q ¡Á ¦² ¡ú 2^Q, nondeterministic transition function
 *     q? ¡Ê Q: initial state
 *     F ? Q: set of accepting states
 *
 * L2 Core Concept:
 *   An ¦Ø-word w = a?a?a?... over ¦² is accepted by NBA A iff
 *   there exists an infinite run ¦Ñ = q? q? q? ... such that:
 *     (a) q??? ¡Ê ¦Ä(q?, a?) for all i ¡Ý 0 (valid run)
 *     (b) Inf(¦Ñ) ¡É F ¡Ù ? (some accepting state visited infinitely often)
 *
 * L3 Mathematical Structure:
 *   The acceptance condition uses the concept of "infinitely often"
 *   which is the defining feature of ¦Ø-automata. For deterministic
 *   automata on infinite words, the B¨¹chi condition is strictly weaker
 *   than Muller/Rabin/Streett/Parity conditions (Landweber, 1969).
 *
 * Key Theorem: NBA are closed under union, intersection, projection,
 *   but NOT under complement (Landweber 1969; complement requires
 *   deterministic automata with richer acceptance conditions).
 *
 * Reference:
 *   B¨¹chi 1962 ¡ª On a decision method in restricted second order arithmetic
 *   Thomas 1990 ¡ª Automata on infinite objects (Handbook)
 *   Gradel, Thomas, Wilke 2002 ¡ª Automata, Logics, and Infinite Games
 *   Baier & Katoen 2008 ¡ª Principles of Model Checking (Ch. 4)
 *
 * Courses: MIT 6.841, Stanford CS254, Berkeley CS278, CMU 15-855
 */

#ifndef BUCHI_H
#define BUCHI_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* ©¤©¤ Symbol type ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤ */
typedef int BuchiSymbol;

/* ©¤©¤ State identifiers ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤ */
typedef int BuchiState;

#define BUCHI_DEAD_STATE (-2)
#define BUCHI_INVALID_STATE (-1)

/* ©¤©¤ ¦Ø-word: infinite sequence of symbols (finite representation) ©¤©¤ */
typedef struct {
    BuchiSymbol* symbols;
    int           len;
    int*          period_start;
    int           period_len;
    int           is_periodic;
    int           is_lasso;
} OmegaWord;

OmegaWord* omega_make_prefix(const BuchiSymbol* prefix, int len);
OmegaWord* omega_make_lasso(const BuchiSymbol* prefix, int plen,
                             const BuchiSymbol* cycle, int clen);
OmegaWord* omega_make_periodic(const BuchiSymbol* period, int plen);
BuchiSymbol omega_get(const OmegaWord* w, int position);
void omega_print(const OmegaWord* w, int max_len);
void omega_free(OmegaWord* w);
int  omega_equals_up_to(const OmegaWord* a, const OmegaWord* b, int max_pos);

/* ©¤©¤ Transition relation ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤ */
typedef struct {
    BuchiState from;
    BuchiSymbol symbol;
    BuchiState to;
} BuchiTransition;

typedef struct {
    BuchiTransition* data;
    int              count;
    int              capacity;
} BuchiTransitionSet;

void buchi_trans_set_init(BuchiTransitionSet* ts, int cap);
void buchi_trans_set_add(BuchiTransitionSet* ts, BuchiState from,
                          BuchiSymbol sym, BuchiState to);
void buchi_trans_set_free(BuchiTransitionSet* ts);

/* ©¤©¤ B¨¹chi Automaton ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤ */
typedef struct {
    int              n_states;
    BuchiState       q0;
    int              n_accepting;
    BuchiState*      accepting;
    int              alphabet_size;
    BuchiSymbol*     alphabet;

    BuchiTransitionSet*** delta;
    int              n_trans_total;

    char*            name;
    uint8_t*         is_accepting;
} BuchiAutomaton;

/* ©¤©¤ Construction API ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤ */
BuchiAutomaton* buchi_create(int n_states, BuchiState q0, int alphabet_size);
void buchi_set_name(BuchiAutomaton* ba, const char* name);
void buchi_set_accepting(BuchiAutomaton* ba, const BuchiState* states, int n);
void buchi_add_accepting(BuchiAutomaton* ba, BuchiState s);
int  buchi_is_accepting(const BuchiAutomaton* ba, BuchiState s);
int  buchi_add_transition(BuchiAutomaton* ba, BuchiState from,
                           BuchiSymbol sym, BuchiState to);
void buchi_set_alphabet(BuchiAutomaton* ba, const BuchiSymbol* syms, int n);
int  buchi_sym_index(const BuchiAutomaton* ba, BuchiSymbol sym);
void buchi_free(BuchiAutomaton* ba);

int buchi_num_states(const BuchiAutomaton* ba);
int buchi_num_trans(const BuchiAutomaton* ba);
int buchi_has_transition(const BuchiAutomaton* ba, BuchiState from,
                          BuchiSymbol sym, BuchiState to);
const BuchiTransitionSet* buchi_post(const BuchiAutomaton* ba,
                                      BuchiState s, BuchiSymbol sym);

/* ©¤©¤ Run and Acceptance ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤ */
typedef struct {
    BuchiState* states;
    int         length;
    int         period_start;
    int         period_len;
    int         is_lasso;
} BuchiRun;

BuchiRun* buchi_run_create(int max_len);
void buchi_run_free(BuchiRun* run);
int  buchi_run_is_accepting(const BuchiRun* run, const BuchiAutomaton* ba);
int buchi_accepts_lasso(const BuchiAutomaton* ba, const OmegaWord* w);
int buchi_accepts(const BuchiAutomaton* ba, const OmegaWord* w,
                  int max_prefix_check);
void buchi_print(const BuchiAutomaton* ba);
void buchi_print_dot(const BuchiAutomaton* ba, const char* filename);
void buchi_print_run(const BuchiRun* run, int max_len);

/* ©¤©¤ SCC-based Acceptance ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤ */
typedef struct {
    uint8_t* visited;
    uint8_t* on_stack;
    int*     lowlink;
    int*     index;
    int      current_index;
    int*     stack;
    int      stack_top;
    int      n_sccs;
    int*     scc_id;
    int*     scc_accepting;
    int*     scc_nontrivial;
} BuchiSCCDecomp;

BuchiSCCDecomp* buchi_scc_decompose(const BuchiAutomaton* ba);
void buchi_scc_free(BuchiSCCDecomp* scc);
int  buchi_has_accepting_scc(const BuchiSCCDecomp* scc);

#endif /* BUCHI_H */
