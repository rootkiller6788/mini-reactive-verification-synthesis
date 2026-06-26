# Course Alignment — mini-grg1-synthesis-algorithm

## Nine-School Curriculum Mapping

| School | Course | Relevant Topics | Implementation |
|--------|--------|----------------|----------------|
| **MIT** | 6.045 Automata, Computability, Complexity | Church's problem, reactive synthesis | grg1_synthesis.c: Church's problem reduction |
| **Stanford** | CS254 Computational Complexity | LTL synthesis complexity, 2EXPTIME | grg1_synthesis.h: complexity documentation |
| **Berkeley** | CS172 Computability & Complexity | Game semantics, μ-calculus | grg1_fixpoint.c: μ-calculus fixpoint computation |
| **CMU** | 15-414 Bug Catching (Model Checking) | Temporal logic, GR(1) | grg1_spec.c: LTL fragment specification |
| **Princeton** | COS 522 Computational Complexity | Two-player games, determinacy | grg1_game.c: game graph + CPre |
| **Caltech** | CS 151 Complexity Theory | Alternation, game complexity | grg1_types.h: two-player game structure |
| **Cambridge** | Part II Complexity Theory | Infinite games, parity games | grg1_fixpoint.c: nested fixpoint for Streett |
| **Oxford** | Advanced Complexity Theory | Automata on infinite words | grg1_bdd.c: symbolic automata via BDD |
| **ETH** | 263-4650 Advanced Complexity | Synthesis, Church's problem | grg1_synthesis.c: full synthesis pipeline |

## Key References Implemented

| Reference | Implementation File |
|-----------|-------------------|
| Piterman, Pnueli, Sa'ar (VMCAI 2006) — GR(1) algorithm | grg1_synthesis.c |
| Bloem et al. (JCSS 2012) — GR(1) survey | grg1_spec.c, grg1_game.c |
| Bryant (IEEE TC 1986) — BDD | grg1_bdd.c |
| Emerson & Lei (LICS 1986) — μ-calculus model checking | grg1_fixpoint.c |
| Tarjan (SICOMP 1972) — SCC algorithm | grg1_game.c |
| Cousot & Cousot (POPL 1977) — Abstract interpretation | grg1_fixpoint.c |
| Zielonka (TCS 1998) — Infinite game solving | grg1_game.c |
| Alpern & Schneider (IPL 1985) — Safety-liveness | grg1_spec.c |

## Knowledge Dependency Tree

```
L1: Game Arena, State, Valuation
├── L2: Two-Player Game, Realizability
│   ├── L3: Lattice (P(S),⊆), BDD
│   │   ├── L4: Knaster-Tarski, CPre Monotonicity
│   │   │   ├── L5: Fixpoint Iteration, Synthesis Algorithm
│   │   │   │   ├── L6: Mutual Exclusion, Traffic Light
│   │   │   │   │   ├── L7: Strategy Verification, Visualization
│   │   │   │   │   │   ├── L8: Symbolic Synthesis, Acceleration
│   │   │   │   │   │   │   └── L9: Research Frontiers (documented)
```
