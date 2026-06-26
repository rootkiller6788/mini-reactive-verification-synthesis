/**
 * test_omega_word.c -- Tests for Omega Word Operations
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "omega_regular.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  TEST %s... ", name); } while(0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

int main(void)
{
    printf("=== Test Omega Word Operations ===\n");

    /* Test: create and index a simple periodic word */
    TEST("create_periodic");
    {
        omega_word w;
        uint8_t period[] = {'a', 'b'};
        bool ok = omega_word_create(&w, NULL, 0, period, 2);
        assert(ok);
        assert(omega_word_index(&w, 0) == 'a');
        assert(omega_word_index(&w, 1) == 'b');
        assert(omega_word_index(&w, 2) == 'a');
        assert(omega_word_index(&w, 3) == 'b');
        assert(omega_word_index(&w, 100) == 'a');
        PASS();
    }

    /* Test: create word with prefix and period */
    TEST("create_prefix_period");
    {
        omega_word w;
        uint8_t prefix[] = {'x', 'y'};
        uint8_t period[] = {'z'};
        bool ok = omega_word_create(&w, prefix, 2, period, 1);
        assert(ok);
        assert(omega_word_index(&w, 0) == 'x');
        assert(omega_word_index(&w, 1) == 'y');
        assert(omega_word_index(&w, 2) == 'z');
        assert(omega_word_index(&w, 3) == 'z');
        PASS();
    }

    /* Test: inf_often */
    TEST("inf_often");
    {
        omega_word w;
        uint8_t period[] = {'a', 'b', 'c'};
        omega_word_create(&w, NULL, 0, period, 3);
        assert(omega_word_inf_often(&w, 'a'));
        assert(omega_word_inf_often(&w, 'b'));
        assert(!omega_word_inf_often(&w, 'x'));
        PASS();
    }

    /* Test: eventually */
    TEST("eventually");
    {
        omega_word w;
        uint8_t prefix[] = {'a'};
        uint8_t period[] = {'b'};
        omega_word_create(&w, prefix, 1, period, 1);
        assert(omega_word_eventually(&w, 'a'));
        assert(omega_word_eventually(&w, 'b'));
        assert(!omega_word_eventually(&w, 'x'));
        PASS();
    }

    /* Test: equivalence */
    TEST("equivalence");
    {
        omega_word w1, w2;
        uint8_t p1[] = {'a'};
        uint8_t r1[] = {'b', 'c'};
        uint8_t p2[] = {'a'};
        uint8_t r2[] = {'b', 'c'};
        omega_word_create(&w1, p1, 1, r1, 2);
        omega_word_create(&w2, p2, 1, r2, 2);
        assert(omega_word_equivalent(&w1, &w2));
        PASS();
    }

    /* Test: null pointer safety */
    TEST("null_safety");
    {
        assert(!omega_word_create(NULL, NULL, 0, NULL, 0));
        assert(omega_word_index(NULL, 0) == 0);
        assert(!omega_word_inf_often(NULL, 'a'));
        assert(!omega_word_eventually(NULL, 'a'));
        PASS();
    }

    printf("\n=== Results: %d/%d tests passed ===\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
