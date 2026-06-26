# mini-buchi-automata-on-words

Nondeterministic Büchi Automata on Infinite Words — Complete Implementation

## Module Status: COMPLETE ✅

- **L1 Definitions**: Complete (9 core definitions)
- **L2 Core Concepts**: Complete (10 concepts)
- **L3 Mathematical Structures**: Complete (8 structures)
- **L4 Fundamental Laws**: Complete (9 theorems)
- **L5 Algorithms/Methods**: Complete (9 algorithms)
- **L6 Canonical Problems**: Complete (5 problems)
- **L7 Applications**: Complete (4 applications)
- **L8 Advanced Topics**: Partial (1 complete, 2 partial, 2 missing)
- **L9 Research Frontiers**: Partial (documented only)

**Code lines (include/ + src/): 3259** ≥ 3000 ✅

## Quick Start

```bash
make          # Build library + tests + examples
make test     # Run all tests
make examples # Build examples
make run-examples  # Run all examples
make clean    # Clean build artifacts
```

## Core Definitions (L1)

| Definition | Notation | Structure |
|-----------|----------|-----------|
| Nondeterministic Büchi Automaton | A = (Q, Σ, δ, q₀, F) | `BuchiAutomaton` |
| ω-word (infinite word) | w ∈ Σ^ω | `OmegaWord` |
| Lasso representation | u·v^ω | `omega_make_lasso()` |
| Periodic ω-word | v^ω | `omega_make_periodic()` |
| Transition relation | δ ⊆ Q × Σ × Q | `BuchiTransitionSet` |
| Run | ρ = q₀ q₁ q₂ ... | `BuchiRun` |
| Infinitely often | Inf(ρ) | SCC-based computation |
| Büchi acceptance | Inf(ρ) ∩ F ≠ ∅ | `buchi_run_is_accepting()` |
| Generalized Büchi | ∀i: Inf(ρ) ∩ F_i ≠ ∅ | `gba_is_empty()` |

## Core Theorems (L4)

| Theorem | Statement | Implementation |
|---------|-----------|---------------|
| NBA Emptiness Decidability | L(A)=∅ is NLOGSPACE-decidable | `buchi_is_empty()` |
| Union Closure | L₁∪L₂ is ω-regular | `buchi_union()` |
| Intersection Closure | L₁∩L₂ is ω-regular | `buchi_intersect()` |
| Projection Closure | ∃a.L is ω-regular | `buchi_project()` |
| Complementation | Σ^ω\L is ω-regular (KV 2001) | `buchi_complement_kv()` |
| Rabin-Streett Duality | ¬Rabin(E,F) = Streett(F,E) | `rabin_complement()` |
| Büchi⊂Rabin | Büchi(F) = Rabin({},F) | `buchi_to_rabin()` |
| Parity→Rabin | O(k) Rabin pairs | `parity_to_rabin()` |
| GBA→NBA | Degeneralization: O(k·|Q|) | `gba_degeneralize()` |

## Core Algorithms (L5)

| Algorithm | Complexity | Implementation |
|-----------|-----------|---------------|
| Tarjan SCC | O(\|Q\|+\|δ\|) | `buchi_scc_decompose()` |
| SCC-based emptiness | O(\|Q\|+\|δ\|) | `buchi_is_empty()` |
| Counterexample extraction | O(\|Q\|+\|δ\|) | `buchi_find_accepting_lasso()` |
| Nested DFS (CVWY 1992) | O(\|Q\|+\|δ\|) | `nested_dfs_check()` |
| GBA degeneralization | O(k·\|Q\|²) | `gba_degeneralize()` |
| KV complementation | O(2^\|Q\|) | `buchi_complement_kv()` |
| Union construction | O(\|Q₁\|+\|Q₂\|) | `buchi_union()` |
| Intersection construction | O(\|Q₁\|×\|Q₂\|) | `buchi_intersect()` |
| Trimming | O(\|Q\|+\|δ\|) | `buchi_trim()` |

## Canonical Problems (L6)

- **Eventually-a**: {w | ∃i: w[i]=a} — 2-state NBA
- **Infinitely-often-a**: {w | |{i: w[i]=a}| = ∞} — 2-state NBA
- **Eventually-always-a**: {w | ∃i∀j≥i: w[j]=a} — 2-state NBA
- **Mutual Exclusion Verification**: Safety + Liveness model checking
- **Ladder Automaton Family**: Parameterized NBA with known structure

## Acceptance Conditions (L2-L3)

| Condition | Formula | Check |
|-----------|---------|-------|
| Büchi | Inf(ρ) ∩ F ≠ ∅ | `buchi_run_is_accepting()` |
| co-Büchi | Inf(ρ) ∩ F = ∅ | dual of Büchi |
| Rabin | ∃i: Inf∩E_i=∅ ∧ Inf∩F_i≠∅ | `rabin_check()` |
| Streett | ∀i: Inf∩G_i≠∅ → Inf∩R_i≠∅ | `streett_check()` |
| Parity (min-even) | min{color(q): q∈Inf(ρ)} is even | `parity_check()` |
| Muller | Inf(ρ) ∈ F | `muller_check()` |
| Gen. Büchi | ∀i: Inf∩F_i≠∅ | `gbuchi_check()` |

## Nine-School Course Mapping

| School | Course | Relevance |
|--------|--------|-----------|
| MIT | 6.045/18.400, 6.841 | Omega-automata definitions, complementation |
| Stanford | CS254 | Acceptance conditions, Rabin/Streett/Parity |
| Berkeley | CS172, CS278 | NBA emptiness, omega-regular closure |
| CMU | 15-455, 15-855 | SCC-based emptiness, Nested DFS |
| Princeton | COS 522, COS 551 | Model checking product, GBA degeneralization |
| Caltech | CS 151, CS 154 | Omega-word representations, containment |
| Cambridge | Part II/III Complexity | Büchi acceptance, Rabin-Streett duality |
| Oxford | Comp/Adv Complexity | Parity games connection, determinization |
| ETH | 263-4650, 252-0400 | Omega-automata hierarchy, LTL/NBA link |

## File Structure

```
mini-buchi-automata-on-words/
├── Makefile                  # Build system
├── README.md                 # This file
├── include/
│   ├── buchi.h               # Core Büchi automaton API
│   ├── buchi_emptiness.h     # Emptiness checking API
│   ├── omega_automata.h      # Acceptance conditions API
│   └── omega_operations.h    # Operations on NBA API
├── src/
│   ├── buchi_core.c          # NBA construction, SCC, acceptance
│   ├── buchi_emptiness.c     # SCC/Nested DFS emptiness, GBA
│   ├── omega_operations.c    # Union, intersection, complement, trim
│   ├── acceptance_conditions.c # Rabin, Streett, Parity, Muller, GBuchi
│   └── buchi_formal.lean     # Lean 4 formalization
├── tests/
│   ├── test_buchi_core.c     # Core API tests
│   ├── test_emptiness.c      # Emptiness checking tests
│   ├── test_operations.c     # Operation tests
│   └── test_acceptance.c     # Acceptance condition tests
├── examples/
│   ├── example_mutex_verification.c  # Model checking example
│   ├── example_nba_operations.c      # Operations demonstration
│   └── example_emptiness_demo.c      # Emptiness demo
├── demos/
│   └── demo_dot_visualizer.c # Graphviz DOT visualization
├── benches/
│   └── bench_emptiness.c     # Performance benchmarks
└── docs/
    ├── knowledge-graph.md    # L1-L9 knowledge coverage
    ├── coverage-report.md    # Coverage assessment
    ├── gap-report.md         # Missing knowledge points
    ├── course-alignment.md   # Nine-school curriculum mapping
    └── course-tree.md        # Prerequisite dependency tree
```

## References

- Büchi, J.R. (1962). "On a decision method in restricted second order arithmetic"
- Landweber, L.H. (1969). "Decision problems of ω-automata"
- Thomas, W. (1990). "Automata on infinite objects" (Handbook of TCS)
- Courcoubetis, Vardi, Wolper, Yannakakis (1992). "Memory-efficient algorithms for verification of temporal properties" (FMSD)
- Kupferman, O. & Vardi, M.Y. (2001). "Weak alternating automata are not that weak" (ACM TOCL)
- Gradel, Thomas, Wilke (2002). "Automata, Logics, and Infinite Games"
- Baier, C. & Katoen, J.P. (2008). "Principles of Model Checking" (Ch. 4)
