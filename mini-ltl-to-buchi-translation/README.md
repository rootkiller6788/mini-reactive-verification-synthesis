# mini-ltl-to-buchi-translation

## Linear Temporal Logic to Buchi Automaton Translation

A comprehensive C library implementing the theory and practice of translating
Linear Temporal Logic (LTL) formulas into Buchi automata, the foundation of
automata-theoretic model checking.

### Module Status: COMPLETE

- **L1** Definitions: Complete (18 operator types, formula struct, Buchi struct)
- **L2** Core Concepts: Complete (construction, manipulation, metrics)
- **L3** Math Structures: Complete (NNF conversion, label algebra)
- **L4** Fundamental Laws: Complete (expansion laws, simplification, duality)
- **L5** Algorithms: Complete (tableau, GPVW, degeneralization, NDFS, SCC)
- **L6** Canonical Problems: Complete (6 standard LTL patterns, property classification)
- **L7** Applications: Complete (model checking, satisfiability, validity, DOT output)
- **L8** Advanced Topics: Partial (documented: Safra complementation, alternating automata)
- **L9** Research Frontiers: Partial (documented)

### Core Definitions (L1)

- **LTL Formula Syntax**: 16 operator types including Boolean (AND, OR, NOT, IMPLIES, EQUIV, XOR)
  and temporal (NEXT, FINALLY, GLOBALLY, UNTIL, RELEASE, WEAK_UNTIL, STRONG_RELEASE)
- **Buchi Automaton**: B = (Q, Sigma, delta, Q0, F) over omega-words
- **Generalized Buchi Automaton**: Multiple acceptance sets

### Core Theorems (L4)

1. **Fixed-point expansion laws** (Baier & Katoen Thm 5.18):
   - phi U psi == psi or (phi and X(phi U psi))
   - phi R psi == psi and (phi or X(phi R psi))

2. **Duality laws** (Baier & Katoen Def 5.20):
   - not(phi U psi) == (not phi) R (not psi)
   - not(F phi) == G (not phi)
   - not(X phi) == X (not phi)

3. **Fischer-Ladner closure theorem**: |FL(phi)| = O(|phi|)

4. **Buchi closure properties**: omega-regular languages closed under
   union, intersection, complementation

### Core Algorithms (L5)

1. **NNF Conversion**: Push negations inward, O(|phi|) time
2. **Formula Simplification**: Boolean + temporal rewrite rules
3. **Fischer-Ladner Closure**: Foundation for tableau construction
4. **Tableau Construction**: Wolper-Vardi-Sistla classic tableau
5. **GPVW On-the-Fly**: Gerth-Peled-Vardi-Wolper algorithm
6. **Degeneralization**: TGBA to NBA, O(k*|Q|) states
7. **NDFS Emptiness**: Courcoubetis-Vardi-Wolper-Yannakakis, O(|Q|+|delta|)
8. **SCC Computation**: Tarjan's algorithm

### Canonical Problems (L6)

- G(p -> F q) [response property]
- G F p [recurrence / strong fairness]
- F G p [persistence]
- p U q [until]
- G(!p || !q) [mutual exclusion / safety]
- G(p -> X q) [next-step response]

### Manna-Pnueli Safety-Progress Hierarchy

| Class | Example | Detection |
|-------|---------|-----------|
| Safety | G(!p) | `ltl_is_safety()` |
| Guarantee | F p | `ltl_is_liveness()` |
| Obligation | F p or G q | `ltl_is_obligation()` |
| Recurrence | G F p | `ltl_is_recurrence()` |
| Persistence | F G p | `ltl_is_persistence()` |
| Reactivity | G F p -> G F q | `ltl_classify()` |

### Nine-School Course Mapping

| School | Course | Key Concept |
|--------|--------|-------------|
| **MIT** | 6.045/18.400 | Automata theory -> omega-automata |
| **CMU** | 15-414 | LTL model checking with automata |
| **Oxford** | Computer-Aided Formal Verification | LTL-to-Buchi translation |
| **ETH** | 263-2800 | Model checking (LTL, Buchi, CTL) |
| **Stanford** | CS256 | Formal methods for reactive systems |
| **Berkeley** | CS172 | Computability & complexity (automata) |
| **Princeton** | COS 522 | Computational complexity (PSPACE model checking) |
| **Cambridge** | Part II Logic & Proof | Temporal logics and automata |
| **Caltech** | CS 154 | Limits of computation |

### Building and Testing

```bash
make          # Build library (build/libltlbuchi.a)
make test     # Build and run tests
make examples # Build example programs
make clean    # Remove build artifacts
```

### File Structure

```
mini-ltl-to-buchi-translation/
  include/
    ltl.h            - LTL formula types and API (327 lines)
    buchi.h           - Buchi automaton types and API (315 lines)
    ltl_to_buchi.h    - Translation API and types (269 lines)
  src/
    ltl.c             - LTL formula implementation (796 lines)
    buchi.c           - Buchi automaton implementation (593 lines)
    ltl_to_buchi.c    - Translation algorithms (937 lines)
    ltl_property.c    - Property classification (199 lines)
  tests/
    test_all.c        - Comprehensive test suite
  examples/
    example_mutex.c   - Mutual exclusion (safety)
    example_response.c - Response property (liveness)
    example_fairness.c - Fairness (recurrence)
  docs/
    knowledge-graph.md - L1-L9 knowledge coverage
    coverage-report.md - Detailed coverage assessment
    gap-report.md      - Missing topics and priorities
    course-alignment.md - Nine-school curriculum mapping
    course-tree.md      - Prerequisite dependency tree
```

### References

- Pnueli, "The Temporal Logic of Programs" (FOCS 1977)
- Vardi & Wolper, "An Automata-Theoretic Approach..." (LICS 1986)
- Gerth, Peled, Vardi, Wolper, "Simple On-the-Fly Verification..." (PSTV 1995)
- Baier & Katoen, "Principles of Model Checking" (2008)
- Clarke, Grumberg & Peled, "Model Checking" (1999)
- Manna & Pnueli, "Temporal Logic of Reactive Systems" (1992)
- Fischer & Ladner, "Propositional Dynamic Logic" (JCSS 1979)
- Courcoubetis, Vardi, Wolper, Yannakakis, "Memory-Efficient Algorithms..." (CAV 1990)