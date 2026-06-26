# Course Tree — mini-buchi-automata-on-words

## Prerequisites

This module depends on the following prerequisite knowledge:

```
mini-buchi-automata-on-words
│
├── Finite Automata Theory
│   ├── DFA/NFA on finite words
│   ├── Regular languages
│   └── Automaton product constructions
│
├── Graph Theory
│   ├── Directed graphs
│   ├── SCC decomposition (Tarjan)
│   └── DFS/BFS traversal
│
├── Complexity Theory Basics
│   ├── NLOGSPACE, PSPACE
│   ├── Reachability in directed graphs
│   └── Subset construction
│
├── Formal Languages
│   ├── Omega-regular expressions
│   ├── Kleene's theorem (finite words)
│   └── Myhill-Nerode theorem
│
└── Mathematical Logic
    ├── Propositional logic
    ├── Linear Temporal Logic (LTL basics)
    └── Monadic Second-Order Logic (S1S)
```

## Downstream Dependencies

Modules that depend on this one:

```
mini-buchi-automata-on-words
│
├── Model Checking (SPIN-like)
│   ├── LTL-to-NBA translation
│   ├── On-the-fly emptiness
│   └── Counterexample-guided abstraction
│
├── Omega-Regular Games
│   ├── Parity games
│   ├── Rabin/Streett games
│   └── Church synthesis problem
│
├── Automata-Based Verification
│   ├── Runtime monitoring
│   ├── Assume-guarantee reasoning
│   └── Compositional verification
│
└── Advanced Omega-Automata
    ├── Safra determinization
    ├── Optimal complementation
    └── Limit-deterministic automata
```

## Learning Path

1. **Start**: L1 Definitions — understand the NBA 5-tuple
2. **Core**: L2 Concepts — acceptance conditions for infinite words
3. **Structures**: L3 — SCC decomposition, Nested DFS
4. **Theory**: L4 — closure properties, emptiness decidability
5. **Practice**: L5 Algorithms — implement emptiness + operations
6. **Problems**: L6 — canonical languages in NBA form
7. **Applications**: L7 — model checking, safety/liveness
8. **Advanced**: L8 — complementation, GBA
9. **Frontiers**: L9 — current research directions
