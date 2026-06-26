# CTL — Prerequisite Dependency Tree

```
mathematical logic
    |
propositional logic (AND, OR, NOT, IMPLIES, IFF)
    |
temporal logic foundations (time as sequences)
    |
    +-- linear temporal logic (LTL)
    |       |
    |   path formulas (X, F, G, U, R on linear traces)
    |
    +-- branching temporal logic (CTL)
            |
        state formulas + path quantifiers (A, E)
            |
        CTL syntax (10 temporal operators)
            |
        CTL semantics (Kripke structures)
            |
        CTL model checking (labeling algorithm)
            |
            +-- explicit-state (CES 1986)
            |
            +-- symbolic (BDD-based, McMillan 1993)
            |
            +-- bounded (SAT-based, Biere 1999)
            |
        CTL equivalences (adequate sets, normal forms)
            |
        CTL satisfiability (EXPTIME-complete)
            |
        CTL* (superset: CTL + LTL path formulas)
            |
            +-- CTL* model checking (PSPACE-complete)
            |
        PCTL (probabilistic CTL) [L9 — Research]
            |
        TCTL (timed CTL) [L9 — Research]
```

## Module Dependencies

This module depends on:
- Propositional logic (AND, OR, NOT, IMPLIES)
- Basic graph algorithms (BFS, DFS, SCC)
- Fixpoint theory (Knaster-Tarski, monotone operators)
- Bit-vector operations for state space representation

Depended upon by:
- Symbolic model checking
- Hardware verification (SMV, NuSMV)
- Software model checking (Java PathFinder, SLAM)
- Real-time systems verification (TCTL)
- Probabilistic systems (PCTL, PRISM)
