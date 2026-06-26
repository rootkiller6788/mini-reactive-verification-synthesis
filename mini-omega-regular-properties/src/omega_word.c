/**
 * omega_word.c -- Omega-Word Operations
 *
 * Implements fundamental operations on omega-words (infinite sequences
 * over a finite alphabet) and their ultimately periodic representations.
 *
 * References:
 *   - Perrin & Pin (2004) "Infinite Words", Chapters 1-3
 *   - Thomas, W. (1990) "Automata on infinite objects"
 *
 * Knowledge Coverage:
 *   L1: omega-word definition (N -> Sigma)
 *   L2: ultimately periodic representation
 *   L3: prefix-period decomposition, cylinder sets
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "omega_regular.h"

bool omega_word_create(omega_word *ow,
                       const uint8_t *prefix, size_t plen,
                       const uint8_t *period, size_t rlen)
{
    if (!ow) return false;
    if (plen > OR_MAX_PREFIX || rlen > OR_MAX_PREFIX) return false;
    memset(ow, 0, sizeof(*ow));
    if (prefix && plen > 0) {
        memcpy(ow->prefix, prefix, plen);
        ow->prefix_len = plen;
    }
    if (period && rlen > 0) {
        memcpy(ow->period, period, rlen);
        ow->period_len = rlen;
    }
    return true;
}

uint8_t omega_word_index(const omega_word *ow, size_t i)
{
    if (!ow) return 0;
    if (i < ow->prefix_len) {
        return ow->prefix[i];
    }
    if (ow->period_len == 0) {
        /* Ultimately constant: repeat the last symbol of prefix forever. */
        return (ow->prefix_len > 0) ? ow->prefix[ow->prefix_len - 1] : 0;
    }
    size_t offset = i - ow->prefix_len;
    return ow->period[offset % ow->period_len];
}

/**
 * gcd -- Euclidean algorithm for greatest common divisor.
 * Foundation for periodic comparison: the alignment of two periodic
 * sequences is determined by the lcm of their periods.
 */
static size_t gcd(size_t x, size_t y)
{
    while (y != 0) {
        size_t t = y;
        y = x % y;
        x = t;
    }
    return x;
}

static size_t lcm(size_t x, size_t y)
{
    if (x == 0 || y == 0) return 0;
    return (x / gcd(x, y)) * y;
}

/**
 * omega_word_compare_up_to -- Compare two omega-words position by
 * position up to a given bound.
 *
 * Finite-verification principle: for ultimately periodic words, we
 * need only compare up to a computable bound to decide equality.
 *
 * Theorem (Perrin & Pin 2004, Prop. 1.1):
 *   u.v^omega = u'.v'^omega
 *   iff they agree on the first
 *   max(|u|,|u'|) + lcm(|v|,|v'|) positions.
 */
static bool omega_word_compare_up_to(const omega_word *a,
                                      const omega_word *b,
                                      size_t limit)
{
    for (size_t i = 0; i < limit; i++) {
        if (omega_word_index(a, i) != omega_word_index(b, i)) return false;
    }
    return true;
}

bool omega_word_equivalent(const omega_word *a, const omega_word *b)
{
    if (!a || !b) return false;

    /* Both ultimately constant */
    if (a->period_len == 0 && b->period_len == 0) {
        if (a->prefix_len > 0 && b->prefix_len > 0) {
            if (a->prefix[a->prefix_len - 1] != b->prefix[b->prefix_len - 1])
                return false;
        }
        size_t longer = (a->prefix_len > b->prefix_len)
                        ? a->prefix_len : b->prefix_len;
        return omega_word_compare_up_to(a, b, longer);
    }

    /* General case */
    size_t max_prefix = (a->prefix_len > b->prefix_len)
                        ? a->prefix_len : b->prefix_len;
    size_t p_lcm = lcm(a->period_len, b->period_len);
    if (p_lcm == 0) p_lcm = 1;
    size_t check_len = max_prefix + 2 * p_lcm;
    if (check_len > 1000000) check_len = 1000000;

    return omega_word_compare_up_to(a, b, check_len);
}

bool omega_word_inf_often(const omega_word *ow, uint8_t symbol)
{
    if (!ow) return false;
    /* In u.v^omega, a symbol appears infinitely often iff it is in v. */
    for (size_t i = 0; i < ow->period_len; i++) {
        if (ow->period[i] == symbol) return true;
    }
    return false;
}

bool omega_word_eventually(const omega_word *ow, uint8_t symbol)
{
    if (!ow) return false;
    for (size_t i = 0; i < ow->prefix_len; i++)
        if (ow->prefix[i] == symbol) return true;
    for (size_t i = 0; i < ow->period_len; i++)
        if (ow->period[i] == symbol) return true;
    return false;
}

bool omega_word_always_from(const omega_word *ow, const bool *symbol_set,
                            size_t set_size, size_t from_pos)
{
    if (!ow || !symbol_set) return false;
    /* Check one complete period cycle after from_pos. */
    size_t check_end = from_pos + ow->period_len + 1;
    if (ow->period_len == 0)
        check_end = from_pos + 1;
    if (check_end > from_pos + OR_MAX_PREFIX)
        check_end = from_pos + OR_MAX_PREFIX;

    for (size_t i = from_pos; i < check_end; i++) {
        uint8_t sym = omega_word_index(ow, i);
        if (sym >= set_size || !symbol_set[sym]) return false;
    }
    return true;
}

void omega_word_print(const omega_word *ow, FILE *fp)
{
    if (!ow || !fp) return;
    fprintf(fp, "w-word: ");
    for (size_t i = 0; i < ow->prefix_len; i++)
        fprintf(fp, "%c", (char)ow->prefix[i]);
    fprintf(fp, " . (");
    for (size_t i = 0; i < ow->period_len; i++)
        fprintf(fp, "%c", (char)ow->period[i]);
    fprintf(fp, ")^w\n");
}

/**
 * omega_word_prefix_extract -- Extract a length-n prefix of the omega-word.
 *
 * Knowledge point: Cylinder sets. The cylinder C_w = {alpha in Sigma^omega :
 * w < alpha} is the set of all omega-words with prefix w. Cylinder sets
 * form a basis for the Cantor topology on Sigma^omega, which is a Polish
 * space (completely metrizable, separable).
 */
size_t omega_word_prefix_extract(const omega_word *ow, size_t length,
                                  uint8_t *buf, size_t buf_size)
{
    if (!ow || !buf || buf_size == 0) return 0;
    size_t n = (length < buf_size) ? length : buf_size;
    for (size_t i = 0; i < n; i++)
        buf[i] = omega_word_index(ow, i);
    return n;
}

/**
 * omega_word_period_shift -- Drop first k symbols and return the shifted
 * omega-word, which remains ultimately periodic.
 *
 * Knowledge point: The shift (suffix) operator on omega-words.
 * omega-regular languages are closed under shift. In LTL model checking,
 * "X phi" (next phi) means phi holds on the shifted word.
 */
bool omega_word_period_shift(const omega_word *ow, size_t k, omega_word *result)
{
    if (!ow || !result) return false;

    size_t max_check = k + ow->period_len * 2 + ow->prefix_len;
    size_t extract_count = (max_check < OR_MAX_PREFIX) ? max_check : OR_MAX_PREFIX;
    uint8_t temp[OR_MAX_PREFIX];
    for (size_t i = 0; i < extract_count; i++)
        temp[i] = omega_word_index(ow, k + i);

    /* Detect smallest period in shifted sequence */
    size_t detected_period = 0;
    for (size_t p = 1; p <= extract_count / 2 && p <= OR_MAX_PREFIX / 2; p++) {
        bool is_period = true;
        for (size_t j = p; j < extract_count; j++) {
            if (temp[j] != temp[j % p]) { is_period = false; break; }
        }
        if (is_period) { detected_period = p; break; }
    }

    if (detected_period == 0) {
        memcpy(result->prefix, temp, extract_count);
        result->prefix_len = extract_count;
        result->period_len = 0;
    } else {
        result->prefix_len = 0;
        memcpy(result->period, temp, detected_period);
        result->period_len = detected_period;
    }
    return true;
}
