# Course Tree: LTL-to-Buchi Translation

## Prerequisites

```
Propositional Logic
    |
    v
Modal Logic Basics
    |
    v
Finite Automata (DFA, NFA)
    |
    v
omega-Automata (Buchi acceptance)
    |
    +-- LTL Syntax and Semantics (Pnueli 1977)
    |
    +-- LTL-to-Buchi Translation (Vardi-Wolper 1986)
    |       |
    |       +-- Tableau Construction (Wolper-Vardi-Sistla 1983)
    |       +-- GPVW On-the-Fly (Gerth-Peled-Vardi-Wolper 1995)
    |       +-- Degeneralization (Choueka 1974)
    |
    +-- Model Checking with Automata
            |
            +-- NDFS Emptiness (CVWY 1990)
            +-- Counterexample Generation
            +-- Symbolic Model Checking (BDDs)
```

## Dependencies within this module

```
ltl.h  (LTL formula types)
  |
  +--> ltl.c (formula construction, NNF, simplification, FL closure)
  |
  +--> ltl_property.c (safety/liveness classification)
  
buchi.h (Buchi automaton types)
  |
  +--> buchi.c (automaton construction, label ops, NDFS, SCC)

ltl_to_buchi.h (translation types)
  |
  +--> ltl_to_buchi.c (tableau, GPVW, pattern translation, model checking)
         depends on: ltl.h, buchi.h
```