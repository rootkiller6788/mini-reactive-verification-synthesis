/**
 * example_buchi.c -- Buechi Automaton Construction and Emptiness Check
 *
 * Demonstrates:
 *   1. Building an NBA for "infinitely often symbol 0"
 *   2. Checking emptiness (non-empty: there exist words with inf often 0)
 *   3. Building an NBA for "always symbol 0" and checking universality
 */

#include <stdio.h>
#include <assert.h>
#include "omega_regular.h"
#include "buchi_automaton.h"

int main(void)
{
    printf("=== Buechi Automaton Example ===\n\n");

    /* Example 1: NBA for "infinitely often symbol 0" over alphabet {0,1} */
    printf("1. Building NBA for GF(symbol=0) over alphabet {0,1}\n");
    nba_t nba_inf;
    nba_init(&nba_inf, 2);

    int q0 = nba_add_state(&nba_inf, true, false, "q0");
    int q1 = nba_add_state(&nba_inf, false, true, "q1");

    /* On symbol 0: go to accepting state. On symbol 1: stay in q0 */
    nba_add_transition(&nba_inf, q0, 0, q1);
    nba_add_transition(&nba_inf, q0, 1, q0);
    nba_add_transition(&nba_inf, q1, 0, q1);
    nba_add_transition(&nba_inf, q1, 1, q0);

    printf("   NBA has %d states, %d transitions\n",
           nba_inf.nstates, nba_inf.ntrans);

    /* Check emptiness */
    bool empty = nba_is_empty(&nba_inf);
    printf("   Emptiness check: %s (expected: non-empty)\n",
           empty ? "EMPTY" : "NON-EMPTY");
    assert(!empty);

    /* Example 2: Check a specific omega-word */
    printf("\n2. Checking omega-word (0,1,0,1,...)^omega\n");
    omega_word w;
    uint8_t period[] = {0, 1};
    omega_word_create(&w, NULL, 0, period, 2);
    bool accepts = nba_accepts(&nba_inf, &w);
    printf("   Acceptance: %s (expected: YES, 0 appears inf often)\n",
           accepts ? "YES" : "NO");
    assert(accepts);

    /* Example 3: NBA for "always symbol 0" */
    printf("\n3. Building NBA for G(symbol=0)\n");
    nba_t nba_always;
    nba_init(&nba_always, 2);
    int a0 = nba_add_state(&nba_always, true, true, "acc");
    nba_add_transition(&nba_always, a0, 0, a0);
    /* No transition on symbol 1 -> dead/empty */

    printf("   Universality check: %s (expected: NOT universal)\n",
           nba_is_universal(&nba_always) ? "UNIVERSAL" : "NOT UNIVERSAL");

    /* Example 4: Couvreur emptiness */
    printf("\n4. Couvreur on-the-fly emptiness\n");
    int scc_count = 0;
    nba_emptiness_couvreur(&nba_inf, &scc_count);
    printf("   Accepting SCC count: %d (expected: >= 1)\n", scc_count);
    assert(scc_count >= 1);

    /* Example 5: Trim */
    printf("\n5. Trimming NBA\n");
    nba_t test_nba;
    nba_init(&test_nba, 2);
    int t0 = nba_add_state(&test_nba, true, false, "init");
    int t1 = nba_add_state(&test_nba, false, true, "good");
    int t2 = nba_add_state(&test_nba, false, false, "dead");
    nba_add_transition(&test_nba, t0, 0, t1);
    nba_add_transition(&test_nba, t1, 0, t1);
    /* state t2 is unreachable */
    int removed = nba_trim(&test_nba);
    printf("   Removed states: %d (expected: 1, the dead state)\n", removed);
    assert(removed >= 1);

    printf("\n=== All examples completed successfully ===\n");
    return 0;
}
