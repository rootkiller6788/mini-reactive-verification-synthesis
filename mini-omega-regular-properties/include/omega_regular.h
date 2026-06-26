/**
 * omega_regular.h — Core ω-Regular Language Definitions
 *
 * Defines ω-words (infinite sequences over an alphabet Σ), ω-regular languages,
 * ω-regular expressions, and fundamental operations on these objects.
 *
 * Key References:
 *   - Büchi, J.R. "On a decision method in restricted second order arithmetic" (1962)
 *   - Thomas, W. "Automata on infinite objects" (Handbook of TCS, 1990)
 *   - Vardi, M.Y. & Wolper, P. "An automata-theoretic approach to automatic
 *     program verification" (LICS 1986)
 *
 * Knowledge Coverage:
 *   L1: ω-word, ω-language, ω-regular language definitions
 *   L2: closure properties, safety vs liveness
 *   L3: ω-words as N→Σ functions, prefix/limit properties
 */

#ifndef OMEGA_REGULAR_H
#define OMEGA_REGULAR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

/* ---------------------------------------------------------------------------
 * L1: Core Definitions — ω-words, ω-languages, ω-regular expressions
 * ------------------------------------------------------------------------- */

/** Maximum alphabet size (symbols numbered 0 to MAX_ALPHABET-1) */
#define OR_MAX_ALPHABET 32

/** Maximum length of a finite prefix we track in memory */
#define OR_MAX_PREFIX  1024

/**
 * omega_word — Represents a finite prefix of an ω-word.
 *
 * An ω-word is a function w: N → Σ. Since we cannot store infinite objects,
 * we represent them via an explicit finite prefix and a suffix pattern that
 * repeats indefinitely (ultimately periodic words are dense in ω-regular
 * languages and suffice for all algorithmic purposes per Büchi 1962).
 */
typedef struct {
    uint8_t prefix[OR_MAX_PREFIX];  /** Finite prefix of the ω-word */
    size_t  prefix_len;             /** Length of the prefix portion */
    uint8_t period[OR_MAX_PREFIX];  /** Repeated suffix (periodic part) */
    size_t  period_len;             /** Length of the period (0 = ultimately constant) */
} omega_word;

/**
 * omega_language — A set of ω-words, characterized by its defining automaton
 * or expression. We store it as a tagged union.
 */
typedef enum {
    OR_LANG_BUCHI,      /** Defined by a Büchi automaton */
    OR_LANG_LTL,        /** Defined by an LTL formula */
    OR_LANG_REGEX,      /** Defined by an ω-regular expression */
    OR_LANG_COMBI       /** Combined via set operations */
} or_lang_type_t;

/**
 * omega_regular_expression — Syntax tree for ω-regular expressions.
 *
 * An ω-regular expression is built from:
 *   - finite regular expressions r (over Σ), and
 *   - the ω-power operator r^ω (infinite repetition of r)
 *   - union, intersection, complement of ω-languages
 *
 * Theorem (Büchi 1962): A language L ⊆ Σ^ω is ω-regular iff it can be
 * described by an ω-regular expression.
 */
typedef enum {
    ORE_EMPTY,          /** ∅ — empty ω-language */
    ORE_EPSILON,        /** Not used for ω-languages (ε is a finite word) */
    ORE_LETTER,         /** Single letter a ∈ Σ */
    ORE_UNION,          /** L1 ∪ L2 */
    ORE_INTERSECTION,   /** L1 ∩ L2 */
    ORE_COMPLEMENT,     /** ¬L = Σ^ω \ L */
    ORE_CONCAT,         /** r · L where r is a regular language and L is ω-regular */
    ORE_OMEGA           /** r^ω — infinite repetition of regular language r */
} ore_op_t;

typedef struct ore_node ore_node_t;
struct ore_node {
    ore_op_t    op;
    uint8_t     letter;         /** For ORE_LETTER */
    ore_node_t *left;           /** Left child */
    ore_node_t *right;          /** Right child */
    /* For ORE_CONCAT and ORE_OMEGA, the finite regular sub-expression is
     * represented by an internal DFA index; we store the DFA id here. */
    int         dfa_id;
};

/* ---------------------------------------------------------------------------
 * L2: Safety vs Liveness Properties
 * ------------------------------------------------------------------------- */

/**
 * property_type — Classification of ω-regular properties.
 *
 * Alpern & Schneider (1987) "Defining Liveness":
 *   - Safety: "something bad never happens" — every violation has a finite
 *     prefix that already condemns the word.
 *   - Liveness: "something good eventually happens" — any finite prefix
 *     can be extended to satisfy the property.
 *   - Every property is the intersection of a safety and a liveness property.
 */
typedef enum {
    PROP_SAFETY,        /** Safety property (prefix-closed + limit-closed) */
    PROP_LIVENESS,      /** Liveness property (dense in Σ^ω) */
    PROP_GUARANTEE,     /** Guarantee: eventually something (∃ prefix) */
    PROP_RESPONSE,      /** Response: always, if p then eventually q */
    PROP_PERSISTENCE,   /** Persistence: eventually always p */
    PROP_RECURRENCE,    /** Recurrence: infinitely often p (Büchi) */
    PROP_GENERAL        /** General ω-regular (safety ∩ liveness) */
} prop_type_t;

/* ---------------------------------------------------------------------------
 * L3: Fundamental Operations on ω-words
 * ------------------------------------------------------------------------- */

/**
 * omega_word_create — Create an ω-word from prefix + period.
 *
 * The resulting ω-word is u·v^ω where u = prefix[0..plen-1] and
 * v = period[0..rlen-1].
 *
 * Returns: true on success (lengths within bounds).
 */
bool omega_word_create(omega_word *ow,
                       const uint8_t *prefix, size_t plen,
                       const uint8_t *period, size_t rlen);

/**
 * omega_word_index — Return the i-th symbol of the ω-word.
 *
 * For i < prefix_len, returns prefix[i].
 * For i >= prefix_len, returns period[(i - prefix_len) % period_len].
 * If period_len == 0, returns the last symbol of prefix (ultimately constant).
 */
uint8_t omega_word_index(const omega_word *ow, size_t i);

/**
 * omega_word_equivalent — Check if two ultimately periodic ω-words
 * represent the same infinite sequence.
 *
 * Theorem: Two ultimately periodic words u·v^ω and u'·v'^ω are equal
 * iff there exists k such that u·v^k·v'^ω = u'·v'^ω (finite check).
 *
 * Reference: Perrin & Pin "Infinite Words" (2004), Chap 1.
 */
bool omega_word_equivalent(const omega_word *a, const omega_word *b);

/**
 * omega_word_contains — Check if a symbol appears infinitely often.
 */
bool omega_word_inf_often(const omega_word *ow, uint8_t symbol);

/**
 * omega_word_eventually — Check if a symbol appears at all.
 */
bool omega_word_eventually(const omega_word *ow, uint8_t symbol);

/**
 * omega_word_always_from — Check if from some point on, all symbols
 * are from a given set.
 */
bool omega_word_always_from(const omega_word *ow, const bool *symbol_set,
                            size_t set_size, size_t from_pos);

/* ---------------------------------------------------------------------------
 * ω-Regular Expression Operations
 * ------------------------------------------------------------------------- */

/**
 * ore_make_letter — Create a leaf node for a single letter.
 */
ore_node_t *ore_make_letter(uint8_t letter);

/**
 * ore_make_union — Create a union node L1 ∪ L2.
 */
ore_node_t *ore_make_union(ore_node_t *left, ore_node_t *right);

/**
 * ore_make_intersection — Create an intersection node L1 ∩ L2.
 */
ore_node_t *ore_make_intersection(ore_node_t *left, ore_node_t *right);

/**
 * ore_make_complement — Create a complement node ¬L.
 */
ore_node_t *ore_make_complement(ore_node_t *left);

/**
 * ore_make_omega — Create an ω-power node r^ω, where r is identified
 * by a DFA id that we manage externally.
 */
ore_node_t *ore_make_omega(int dfa_id);

/**
 * ore_make_concat — Create a concatenation node r·L.
 */
ore_node_t *ore_make_concat(int dfa_id, ore_node_t *right);

/**
 * ore_free — Recursively free an ω-regular expression tree.
 */
void ore_free(ore_node_t *root);

/**
 * ore_membership_ult_periodic — Check if an ultimately periodic ω-word
 * belongs to the ω-regular language defined by the expression.
 *
 * This is decidable because ultimately periodic words have a finite
 * representation and automata acceptance can be checked combinatorially.
 *
 * Returns: true if the word is in the language.
 */
bool ore_membership(const ore_node_t *expr, const omega_word *w);

/**
 * ore_is_empty — Check if the ω-regular language is empty.
 * Decidable by NBA emptiness check.
 */
bool ore_is_empty(const ore_node_t *expr);

/**
 * ore_is_universal — Check if L = Σ^ω.
 */
bool ore_is_universal(const ore_node_t *expr, size_t alphabet_size);

/* ---------------------------------------------------------------------------
 * Additional Expression Tree Operations
 * ------------------------------------------------------------------------- */

size_t ore_size(const ore_node_t *root);
size_t ore_depth(const ore_node_t *root);
ore_node_t *ore_clone(const ore_node_t *root);
bool ore_is_syntactically_equal(const ore_node_t *a, const ore_node_t *b);
void ore_print(const ore_node_t *root, FILE *fp);
void ore_count_operators(const ore_node_t *root, int counts[8]);
prop_type_t ore_classify_property(const ore_node_t *expr);
bool ore_is_safety_syntactic(const ore_node_t *expr);
bool ore_language_equal(const ore_node_t *e1, const ore_node_t *e2, size_t alphabet_size);

/* ---------------------------------------------------------------------------
 * Note: ore_to_nba() declared in buchi_automaton.h (requires NBA type)
 * ------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
 * Internal DFA Library (for omega-power and concatenation)
 * ------------------------------------------------------------------------- */

/* ---- DFA for finite regular sub-expressions ---- */
#define MAX_DFA_STATES 64
#define MAX_DFA_TABLE  16

struct _simple_dfa {
    int     nstates;
    int     initial;
    bool    accepting[MAX_DFA_STATES];
    int     trans[MAX_DFA_STATES][OR_MAX_ALPHABET];
    size_t  alphabet_size;
};
typedef struct _simple_dfa simple_dfa_t;

const simple_dfa_t *simple_dfa_get(int dfa_id);
int simple_dfa_register(simple_dfa_t *dfa);
int simple_dfa_symbol_set_dfa(const bool *symbol_set, size_t alphabet_size);
int simple_dfa_all_dfa(size_t alphabet_size);
int simple_dfa_empty_dfa(size_t alphabet_size);
uint64_t dfa_accepting_mask(const simple_dfa_t *dfa);

/* ---------------------------------------------------------------------------
 * Additional Omega Word Operations
 * ------------------------------------------------------------------------- */

void omega_word_print(const omega_word *ow, FILE *fp);
size_t omega_word_prefix_extract(const omega_word *ow, size_t length, uint8_t *buf, size_t buf_size);
bool omega_word_period_shift(const omega_word *ow, size_t k, omega_word *result);

#endif /* OMEGA_REGULAR_H */
