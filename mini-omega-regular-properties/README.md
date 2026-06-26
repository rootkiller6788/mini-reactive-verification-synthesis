# mini-omega-regular-properties

Omega-Regular Properties and Buechi Automata on Infinite Words.

## Module Status: COMPLETE

- L1-L6: Complete
- L7: Complete (3 applications: LTL model checking, reactive verification, on-the-fly emptiness)
- L8: Partial (2/5 advanced topics: parity acceptance, DBA complement)
- L9: Partial (documented, not implemented per SKILL.md allowance)

## Core Definitions (L1)

| Term | Definition |
|------|-----------|
| omega-word | Function w: N -> Sigma (infinite sequence over alphabet) |
| omega-language | Set L subset Sigma^omega of infinite words |
| omega-regular language | Language definable by omega-regular expression or recognized by NBA |
| NBA | Nondeterministic Buechi Automaton: A = (Q, Sigma, delta, q0, F) |
| Buechi acceptance | Run r is accepting iff Inf(r) cap F != empty |
| omega-regular expr | Built from letters + union + intersection + complement + omega-power + concat |
| LTL | Linear Temporal Logic: p | !p | p&q | X p | p U q (Pnueli 1977) |
| Safety property | Prefix-closed + limit-closed (Alpern & Schneider 1987) |
| Liveness property | Dense in Sigma^omega (Alpern & Schneider 1987) |

## Core Theorems (L4)

| Theorem | Statement | Reference |
|---------|-----------|-----------|
| Buechi's Theorem | L is omega-regular iff L = L(A) for some NBA | Buechi 1962 |
| Closure | omega-regular languages closed under union, intersection, complement | Buechi 1962 |
| Emptiness | L(A) = empty is decidable in O(|Q|+|delta|) via Tarjan SCC | Buechi 1962 |
| LTL-to-NBA | For every LTL formula phi, exists NBA A_phi with 2^O(|phi|) states | Vardi & Wolper 1986 |
| Alpern-Schneider | Every property = safety intersect liveness | Alpern & Schneider 1987 |
| Safra | NBA -> DPA in 2^O(n log n) states | Safra 1988 |

## Core Algorithms (L5)

| Algorithm | Function | Complexity |
|-----------|----------|------------|
| Tarjan SCC emptiness | nba_is_empty() | O(|Q| + |delta|) |
| Couvreur on-the-fly | nba_emptiness_couvreur() | O(|Q| + |delta|) |
| NBA union | nba_union() | O(|A1| + |A2|) |
| NBA intersection (2-copy) | nba_intersection() | O(|A1| * |A2|) |
| NBA complement (DBA) | nba_complement() | O(|A|) |
| Expression to NBA | ore_to_nba() | poly (excl. complement) |
| LTL to NBA (tableau) | ltl_to_nba() | 2^O(|phi|) |
| Omega-word equivalence | omega_word_equivalent() | O(lcm of periods) |
| NBA trim | nba_trim() | O(|Q| + |delta|) |

## Canonical Problems (L6)

- GFp: Infinitely often p (Buechi acceptance)
- FGp: Eventually always p (persistence)
- G(request -> F response): Request-response property
- Mutual exclusion: G !(cs1 & cs2) & liveness
- Fairness: GF enabled -> GF executed
- Property classification: safety/liveness/guarantee/response/persistence/recurrence

## File Structure

```
mini-omega-regular-properties/
  Makefile              # make test runs all tests
  README.md             # This file
  include/
    omega_regular.h     # omega-word, omega-regular expression types
    buchi_automaton.h   # NBA data structures and operations
    ltl_formula.h       # LTL syntax, semantics, and NBA translation
  src/
    omega_word.c        # omega-word operations (L1-L3)
    omega_expression.c  # Expression trees + DFA library + property classification
    buchi_core.c        # NBA construction (L1-L3)
    buchi_run.c         # Run simulation and acceptance (L2, L5)
    buchi_emptiness.c   # Tarjan SCC emptiness (L4-L5)
    buchi_cycle.c       # Couvreur emptiness + cycle extraction + trim (L5)
    buchi_union.c       # NBA union (L2, L5)
    buchi_intersection.c # NBA intersection (L2, L5)
    buchi_complement.c  # NBA complement (DBA) + inclusion (L2, L5, L8)
    ore_to_nba.c        # Expression to NBA (L4-L6)
    ltl_formula.c       # LTL formula construction (L1, L3)
    ltl_eval.c          # LTL semantics + printing (L2-L3)
    ltl_to_nba.c        # LTL to NBA translation (L4-L5, L7)
    model_checking.c    # LTL model checking (L7)
    omega_closure.c     # Closure properties (L2, L5)
    omega_parity.c      # Parity/Rabin/Streett acceptance (L8)
  tests/
    test_omega_word.c   # Omega word tests (6 tests)
    test_buchi.c        # NBA tests (10 tests)
  examples/
    example_buchi.c     # NBA construction and emptiness
    example_ltl.c       # LTL formula evaluation
    example_omega_regular.c # Expression operations
  docs/
    knowledge-graph.md
    coverage-report.md
    gap-report.md
    course-alignment.md
    course-tree.md
```

## Nine-School Course Mapping

| School | Course | Omega-Regular Topics |
|--------|--------|---------------------|
| MIT | 6.045/18.400 Automata | NBA, Buechi acceptance |
| Stanford | CS254 Complexity | omega-regular properties |
| Berkeley | CS172 Computability | NBA emptiness |
| CMU | 15-455/855 Complexity | LTL model checking |
| Princeton | COS 522 Complexity | Automata-theoretic verification |
| Caltech | CS 151 Complexity | Infinite word automata |
| Cambridge | Part II Complexity | Buechi automata |
| Oxford | Computational Complexity | LTL, verification |
| ETH | 263-4650 Advanced | Omega-automata theory |

## Building and Testing

```bash
make          # Compile all source files
make test     # Build and run all tests
make examples # Build example programs
make clean    # Remove build artifacts
```

## References

- Buechi, J.R. (1962). On a decision method in restricted second order arithmetic.
- Pnueli, A. (1977). The temporal logic of programs. FOCS.
- Vardi, M.Y. & Wolper, P. (1986). An automata-theoretic approach to automatic program verification. LICS.
- Alpern, B. & Schneider, F.B. (1987). Defining Liveness. IPL.
- Thomas, W. (1990). Automata on infinite objects. Handbook of TCS.
- Clarke, E.M., Grumberg, O. & Peled, D. (1999). Model Checking. MIT Press.
- Perrin, D. & Pin, J.-E. (2004). Infinite Words. Elsevier.
- Baier, C. & Katoen, J.-P. (2008). Principles of Model Checking. MIT Press.
