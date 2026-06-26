# Course Tree — mini-grg1-synthesis-algorithm

## Prerequisite Dependencies

This module depends on concepts from:

| Prerequisite Module | Concepts Used |
|--------------------|---------------|
| mini-boolean-circuits-model | Boolean function representation, BDD structure |
| mini-p-np-np-completeness | Complexity classes, reduction concept |
| mini-cook-levin-theorem | SAT encoding, reduction to Boolean formulas |

## Internal Dependency Tree

```
grg1_types.h ──────────────────────────────────────────────────┐
  │ (all modules depend on basic types)                         │
  ├── grg1_spec.h / grg1_spec.c                                 │
  │     │ (specification construction, validation)              │
  │     └── grg1_game.h / grg1_game.c                           │
  │           │ (game construction from spec, CPre, attractor)  │
  │           ├── grg1_fixpoint.h / grg1_fixpoint.c              │
  │           │     │ (LFP, GFP, nested fixpoint)               │
  │           │     └── grg1_synthesis.h / grg1_synthesis.c     │
  │           │           │ (winning region, strategy extract)  │
  │           │           └── examples/                         │
  │           │                                                 │
  │           └── grg1_bdd.c (independent symbolic path) ───────┘
  │
  └── grg1_lean.lean (formal verification, independent)
```

## Implementation Order

1. `grg1_types.h` + `grg1_types.c` — Core data types and memory management
2. `grg1_spec.h` + `grg1_spec.c` — Specification representation
3. `grg1_game.h` + `grg1_game.c` — Game graph and CPre operators
4. `grg1_fixpoint.h` + `grg1_fixpoint.c` — Fixpoint computation
5. `grg1_synthesis.h` + `grg1_synthesis.c` — Synthesis algorithm
6. `grg1_bdd.c` — Symbolic BDD operations (parallel path)
7. `grg1_lean.lean` — Formalization (independent, cross-cuts all)
8. `tests/test_grg1.c` — Integration tests
9. `examples/` — End-to-end demonstrations

## File Dependency Graph (Compilation Order)

```
build/grg1_types.o      ← src/grg1_types.c, include/grg1_types.h
build/grg1_spec.o       ← src/grg1_spec.c, include/grg1_spec.h, include/grg1_types.h
build/grg1_game.o       ← src/grg1_game.c, include/grg1_game.h, include/grg1_spec.h
build/grg1_fixpoint.o   ← src/grg1_fixpoint.c, include/grg1_fixpoint.h, include/grg1_game.h
build/grg1_synthesis.o  ← src/grg1_synthesis.c, include/grg1_synthesis.h, [all above]
build/grg1_bdd.o        ← src/grg1_bdd.c, include/grg1_types.h
libgrg1.a               ← all .o files
tests/test_grg1         ← tests/test_grg1.c + libgrg1.a
examples/example_*      ← examples/*.c + libgrg1.a
```
