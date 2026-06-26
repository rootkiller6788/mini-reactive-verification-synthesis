/**
 * test_buchi.c -- Tests for Buechi Automata Operations
 *
 * Uses numeric symbols (0..alphabet_size-1) for transitions.
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "omega_regular.h"
#include "buchi_automaton.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  TEST %s... ", name); } while(0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)

int main(void)
{
    printf("=== Test Buechi Automata ===\n");

    TEST("nba_init");
    {
        nba_t nba;
        nba_init(&nba, 4);
        assert(nba.nstates == 0);
        assert(nba.ntrans == 0);
        assert(nba.initial == -1);
        PASS();
    }

    TEST("nba_add_state");
    {
        nba_t nba;
        nba_init(&nba, 4);
        int q0 = nba_add_state(&nba, true, false, "init");
        int q1 = nba_add_state(&nba, false, true, "accept");
        assert(q0 == 0);
        assert(q1 == 1);
        assert(nba.nstates == 2);
        assert(nba.initial == 0);
        assert(nba.states[1].is_accepting);
        PASS();
    }

    TEST("nba_add_transition");
    {
        nba_t nba;
        nba_init(&nba, 4);
        int q0 = nba_add_state(&nba, true, false, "q0");
        int q1 = nba_add_state(&nba, false, true, "q1");
        bool ok = nba_add_transition(&nba, q0, 0, q1);
        assert(ok);
        assert(nba.ntrans == 1);
        PASS();
    }

    TEST("nba_accepts_self_loop");
    {
        nba_t nba;
        nba_init(&nba, 4);
        int q0 = nba_add_state(&nba, true, true, "q0");
        nba_add_transition(&nba, q0, 0, q0);
        nba_add_transition(&nba, q0, 1, q0);

        omega_word w;
        uint8_t period[] = {0};
        omega_word_create(&w, NULL, 0, period, 1);
        assert(nba_accepts(&nba, &w));
        PASS();
    }

    TEST("nba_empty_no_accepting_cycle");
    {
        nba_t nba;
        nba_init(&nba, 4);
        int q0 = nba_add_state(&nba, true, false, "q0");
        nba_add_transition(&nba, q0, 0, q0);
        assert(nba_is_empty(&nba));
        PASS();
    }

    TEST("nba_nonempty_accepting_cycle");
    {
        nba_t nba;
        nba_init(&nba, 4);
        int q0 = nba_add_state(&nba, true, true, "q0");
        nba_add_transition(&nba, q0, 0, q0);
        assert(!nba_is_empty(&nba));
        PASS();
    }

    TEST("nba_union_basic");
    {
        nba_t a1, a2, result;
        nba_init(&a1, 4);
        nba_init(&a2, 4);
        int q0 = nba_add_state(&a1, true, true, "a1");
        nba_add_transition(&a1, q0, 0, q0);
        int q1 = nba_add_state(&a2, true, true, "a2");
        nba_add_transition(&a2, q1, 0, q1);
        bool ok = nba_union(&a1, &a2, &result);
        assert(ok);
        assert(result.nstates >= 3);
        PASS();
    }

    TEST("nba_intersection_basic");
    {
        nba_t a1, a2, result;
        nba_init(&a1, 4);
        nba_init(&a2, 4);
        int q0 = nba_add_state(&a1, true, true, "a1");
        nba_add_transition(&a1, q0, 0, q0);
        int q1 = nba_add_state(&a2, true, true, "a2");
        nba_add_transition(&a2, q1, 0, q1);
        bool ok = nba_intersection(&a1, &a2, &result);
        assert(ok);
        assert(result.nstates > 0);
        PASS();
    }

    TEST("nba_emptiness_couvreur");
    {
        nba_t nba;
        nba_init(&nba, 4);
        int q0 = nba_add_state(&nba, true, true, "q0");
        nba_add_transition(&nba, q0, 0, q0);
        int count = 0;
        nba_emptiness_couvreur(&nba, &count);
        assert(count > 0);
        PASS();
    }

    TEST("nba_trim");
    {
        nba_t nba;
        nba_init(&nba, 4);
        int q0 = nba_add_state(&nba, true, true, "q0");
        nba_add_transition(&nba, q0, 0, q0);
        int removed = nba_trim(&nba);
        assert(removed == 0);
        PASS();
    }

    printf("\n=== Results: %d/%d tests passed ===\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
