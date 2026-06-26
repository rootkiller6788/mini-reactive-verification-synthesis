# LTL Course Tree — Prerequisite Dependencies

## Prerequisites (What You Need Before This Module)

```
Propositional Logic
    ├── Syntax (atoms, Boolean connectives)
    ├── Semantics (truth tables)
    └── Normal Forms (CNF, DNF)

Automata Theory (Finite Words)
    ├── DFA/NFA (states, transitions, acceptance)
    ├── Regular languages
    └── Closure properties

First-Order Logic (Basic)
    ├── Quantifiers (∀, ∃)
    └── Interpretations/Models

Graph Theory
    ├── Graph traversal (DFS/BFS)
    └── Tarjan's SCC algorithm
```

## This Module Provides

```
mini-linear-temporal-logic-ltl (THIS MODULE)
    ├── LTL Syntax (14 operators)
    ├── LTL Semantics (trace-based, Kripke-based)
    ├── LTL Equivalence Laws (duality, expansion, idempotence)
    ├── LTL Normal Forms (NNF, core fragment)
    ├── Formula Simplification (30+ rewrite rules)
    ├── Tableau Construction (Gerth et al. 1995)
    ├── Explicit-State Model Checking
    ├── LTL Satisfiability Checking
    ├── Specification Pattern Library (13 patterns)
    └── Safety/Liveness/Obligation Classification
```

## Dependents (Modules That Need This)

```
mini-ltl-to-buchi-translation
    └── Needs: LTL AST, NNF, formula expansion
    └── Uses: ltl_to_nnf(), ltl_expand(), LtlNode

mini-omega-regular-properties
    └── Needs: LTL ⊆ ω-regular theorem
    └── Uses: LTL syntax, semantics

mini-reactive-synthesis-pnueli
    └── Needs: LTL specifications, fairness patterns
    └── Uses: ltl_pattern_compassion(), ltl_pattern_justice()

mini-computation-tree-logic-ctl
    └── Needs: Branching-time vs linear-time comparison
    └── Uses: LTL for expressiveness comparison

mini-symbolic-model-checking-obdd
    └── Needs: LTL model checking problem
    └── Uses: Model checking API, counterexample extraction
```

## Research Frontiers Connection

```
L9 Research Frontiers:
    ├── LTL Synthesis (Pnueli & Rosner 1989)
    │   └── This module provides: fairness (compassion/justice) patterns
    ├── LTL+Past Operators
    │   └── This module provides: pure-future fragment check
    ├── Probabilistic LTL
    │   └── This module provides: bounded semantics infrastructure
    └── Runtime Verification
        └── This module provides: monitor_step() algorithm
```
