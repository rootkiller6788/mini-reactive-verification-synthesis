/*
 * omega_automata.h — General ω-Automata Framework
 *
 * L1 Definitions:
 *   An ω-automaton accepts infinite words (ω-words). The acceptance
 *   condition determines which infinite runs are accepting.
 *
 * L2 Core Concepts — Acceptance Conditions:
 *   Büchi:      Inf(ρ) ∩ F ≠ ∅                (some F-state appears ∞-often)
 *   co-Büchi:   Inf(ρ) ∩ F = ∅                (no F-state appears ∞-often)
 *   Rabin:      ∨ᵢ (Inf(ρ) ∩ Eᵢ = ∅ ∧ Inf(ρ) ∩ Fᵢ ≠ ∅)  (Rabin pairs)
 *   Streett:    ∧ᵢ (Inf(ρ) ∩ Eᵢ ≠ ∅ → Inf(ρ) ∩ Fᵢ ≠ ∅)  (Streett pairs)
 *   Parity:     min{color(q) : q ∈ Inf(ρ)} is even         (min-even parity)
 *   Muller:     Inf(ρ) ∈ F           where F ⊆ 2^Q        (Muller family)
 *
 * L3 Mathematical Structures:
 *   The expressive power hierarchy for deterministic ω-automata:
 *     DBA ⊊ DcoBA ⊊ DRA = DSA = DMA = DPA
 *   Nondeterministic: NBA = NMA = NRA = NSA = NPA (all ω-regular)
 *
 * Key Results:
 *   Determinization: NBA → DRA (Safra 1988, Schewe 2009)
 *   Complementation: NBA → NBA (nondeterministic, Büchi 1962 via S1S)
 *                    Optimal: O((0.96n)^n) states (Schewe 2009)
 *
 * Reference:
 *   Landweber 1969 — Decision problems of ω-automata
 *   Safra 1988 — On the complexity of ω-automata
 *   Gradel, Thomas, Wilke 2002 — Automata, Logics, and Infinite Games
 *
 * Courses: MIT 6.841, Stanford CS254, CMU 15-855, Oxford Adv. Complexity
 */

#ifndef OMEGA_AUTOMATA_H
#define OMEGA_AUTOMATA_H

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

/* ── Acceptance condition types ───────────────────────────── */
typedef enum {
    ACC_BUCHI = 0,
    ACC_COBUCHI,
    ACC_RABIN,
    ACC_STREETT,
    ACC_PARITY_MIN_EVEN,
    ACC_PARITY_MAX_EVEN,
    ACC_PARITY_MIN_ODD,
    ACC_PARITY_MAX_ODD,
    ACC_MULLER,
    ACC_GENERALIZED_BUCHI
} AccConditionType;

const char* acc_condition_name(AccConditionType t);
int acc_condition_is_dual(AccConditionType a, AccConditionType b);

/* ── Rabin pair ───────────────────────────────────────────── */
typedef struct {
    int* E;   /* "green" set: states that should appear finitely often */
    int  E_size;
    int* F;   /* "red" set: states that should appear infinitely often */
    int  F_size;
} RabinPair;

/* A Rabin condition is satisfied if for SOME pair i:
 *   Inf(ρ) ∩ Eᵢ = ∅ AND Inf(ρ) ∩ Fᵢ ≠ ∅ */
typedef struct {
    RabinPair* pairs;
    int        n_pairs;
} RabinCondition;

/* ── Streett pair ─────────────────────────────────────────── */
typedef struct {
    int* G;  /* "request" set */
    int  G_size;
    int* R;  /* "response" set */
    int  R_size;
} StreettPair;

/* A Streett condition is satisfied if for ALL pairs i:
 *   Inf(ρ) ∩ Gᵢ ≠ ∅ → Inf(ρ) ∩ Rᵢ ≠ ∅
 *   (if request happens infinitely often, response must too) */
typedef struct {
    StreettPair* pairs;
    int          n_pairs;
} StreettCondition;

/* ── Parity condition ─────────────────────────────────────── */
typedef struct {
    int* colors;     /* colors[s] = priority ∈ {0,1,...,max_color} */
    int  n_states;
    int  max_color;
    AccConditionType parity_type; /* min-even, max-even, etc. */
} ParityCondition;

/* ── Muller condition ──────────────────────────────────────── */
typedef struct {
    int** accepting_sets;  /* array of accepting state subsets */
    int*   set_sizes;
    int    n_sets;
} MullerCondition;

/* ── Generalized Büchi ────────────────────────────────────── */
typedef struct {
    int** F_sets;   /* array of accepting state sets */
    int*   F_sizes;
    int    n_sets;  /* >= 1 */
} GeneralizedBuchiCondition;

/* ── Acceptance condition constructors ─────────────────────── */
RabinCondition*         rabin_create(int n_pairs);
void                    rabin_free(RabinCondition* rc);
int                     rabin_check(const RabinCondition* rc,
                                     const int* inf_set, int inf_size);
StreettCondition*       streett_create(int n_pairs);
void                    streett_free(StreettCondition* sc);
int                     streett_check(const StreettCondition* sc,
                                       const int* inf_set, int inf_size);
ParityCondition*        parity_create(int n_states, AccConditionType typ);
void                    parity_free(ParityCondition* pc);
int                     parity_check(const ParityCondition* pc,
                                      const int* inf_set, int inf_size);
MullerCondition*        muller_create(int n_sets);
void                    muller_free(MullerCondition* mc);
int                     muller_check(const MullerCondition* mc,
                                      const int* inf_set, int inf_size);
GeneralizedBuchiCondition* gbuchi_create(int n_sets);
void                       gbuchi_free(GeneralizedBuchiCondition* gbc);
int                        gbuchi_check(const GeneralizedBuchiCondition* gbc,
                                         const int* inf_set, int inf_size);

/* ── Conversion between acceptance conditions ──────────────── */
/*
 * Rabin → Streett: dualize (swap E↔G, F↔R, ∨↔∧)
 * Streett → Rabin: dualize
 * Parity → Rabin: O(k) pairs where k = max_color+1
 * Rabin → Parity: O(k) states, k = number of Rabin pairs
 * Büchi → Rabin: 1 pair ({}, F)
 * Müller → Rabin: exponential blowup in general
 *
 * All conversions preserve the recognized language
 * (up to automaton restructuring).
 */
int rabin_to_streett_pair_count(const RabinCondition* rc);
StreettCondition* rabin_to_streett(const RabinCondition* rc);
RabinCondition* parity_to_rabin(const ParityCondition* pc,
                                 int min_even);
ParityCondition* rabin_to_parity(const RabinCondition* rc);
RabinCondition* buchi_to_rabin(const int* F, int F_size,
                                int n_states);
RabinCondition* gbuchi_to_rabin(const GeneralizedBuchiCondition* gbc,
                                 int n_states);

/* ── Complement of acceptance conditions ──────────────────── */
RabinCondition* rabin_complement(const RabinCondition* rc);

#endif /* OMEGA_AUTOMATA_H */
