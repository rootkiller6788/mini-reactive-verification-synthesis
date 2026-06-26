# LTL Knowledge Graph — Nine-Level Knowledge Coverage

## L1: Definitions

| # | Entry | Status | Location |
|---|-------|--------|----------|
| 1 | LTL Syntax (BNF): propositions, Boolean connectives, temporal operators | Complete | `include/ltl_ast.h` — `LtlNodeType` enum with 14 constructors |
| 2 | LTL Semantics over infinite traces: σ, i ⊨ φ | Complete | `include/ltl_semantics.h`, `src/ltl_semantics.c` — `ltl_evaluate()` |
| 3 | Trace / ω-word: finite representation as lasso | Complete | `include/ltl_semantics.h` — `LtlTrace` struct |
| 4 | Kripke Structure: M = (S, I, →, L) | Complete | `include/ltl_semantics.h` — `LtlKripke` struct |
| 5 | Path in Kripke Structure: infinite state sequence | Complete | `include/ltl_semantics.h` — `LtlPath` struct |
| 6 | Until operator: φ U ψ | Complete | `include/ltl_ast.h`, `src/ltl_semantics.c` |
| 7 | Release operator: φ R ψ (dual of Until) | Complete | `include/ltl_ast.h`, `src/ltl_semantics.c` |
| 8 | Weak Until: φ W ψ ≡ (φ U ψ) ∨ G φ | Complete | `include/ltl_ast.h`, `src/ltl_semantics.c` |
| 9 | Fischer-Ladner Closure | Complete | `include/ltl_ast.h`, `src/ltl_ast.c` |

## L2: Core Concepts

| # | Entry | Status | Location |
|---|-------|--------|----------|
| 1 | Safety vs Liveness (Alpern & Schneider 1985) | Complete | `include/ltl_equiv.h`, `src/ltl_equiv.c` — `ltl_is_safety_fragment()`, `ltl_is_liveness_fragment()` |
| 2 | Obligation fragment (Chang, Manna, Pnueli 1992) | Complete | `include/ltl_equiv.h`, `src/ltl_equiv.c` |
| 3 | LTL = Star-Free ω-Regular (Thomas 1979) | Documented | `include/ltl_ast.h` header |
| 4 | LTL = FO over (ℕ, <) (Kamp 1968) | Documented | `include/ltl_ast.h` header |
| 5 | Satisfaction relation formal semantics | Complete | `src/ltl_semantics.c` — `ltl_evaluate()` |
| 6 | Bounded semantics for runtime monitoring | Complete | `src/ltl_semantics.c` — `ltl_evaluate_bounded()` |
| 7 | LTL monitoring step expansion | Complete | `src/ltl_semantics.c` — `ltl_monitor_step()` |

## L3: Mathematical Structures

| # | Entry | Status | Location |
|---|-------|--------|----------|
| 1 | LTL AST: 14-node-type tree structure | Complete | `include/ltl_ast.h`, `src/ltl_ast.c` |
| 2 | Proposition set as bitmask (2^AP) | Complete | `include/ltl_semantics.h` — `LtlPropSet` (uint64_t) |
| 3 | Lasso representation: prefix · cycle^ω | Complete | `include/ltl_semantics.h`, `src/ltl_semantics.c` |
| 4 | Kripke structure as adjacency list | Complete | `include/ltl_semantics.h`, `src/ltl_semantics.c` |
| 5 | Subformula set / closure computation | Complete | `include/ltl_ast.h`, `src/ltl_ast.c` |
| 6 | Tableau node as formula set + X-obligations | Complete | `src/ltl_model_check.c` |
| 7 | Formula expansion fixed-point structure | Complete | `src/ltl_equiv.c` — `ltl_expand()` |
| 8 | S-expression representation | Complete | `src/ltl_ast.c` — `ltl_parse()`, `ltl_to_sexpr()` |

## L4: Fundamental Laws

| # | Theorem / Law | Status | Location |
|---|---------------|--------|----------|
| 1 | Duality: ¬X φ ≡ X ¬φ | Complete | `src/ltl_equiv.c` — NNF conversion |
| 2 | Duality: ¬F φ ≡ G ¬φ | Complete | `src/ltl_equiv.c` |
| 3 | Duality: ¬G φ ≡ F ¬φ | Complete | `src/ltl_equiv.c` |
| 4 | Duality: ¬(φ U ψ) ≡ (¬φ) R (¬ψ) | Complete | `src/ltl_equiv.c` — `ltl_check_until_release_duality()` |
| 5 | Duality: ¬(φ R ψ) ≡ (¬φ) U (¬ψ) | Complete | `src/ltl_equiv.c` |
| 6 | Expansion: φ U ψ ≡ ψ ∨ (φ ∧ X(φ U ψ)) | Complete | `src/ltl_equiv.c` — `ltl_expand()` |
| 7 | Expansion: G φ ≡ φ ∧ X(G φ) | Complete | `src/ltl_equiv.c` |
| 8 | Expansion: F φ ≡ φ ∨ X(F φ) | Complete | `src/ltl_equiv.c` |
| 9 | Idempotence: F F φ ≡ F φ | Complete | `src/ltl_equiv.c` — `ltl_simplify()` |
| 10 | Idempotence: G G φ ≡ G φ | Complete | `src/ltl_equiv.c` |
| 11 | LTL satisfiability is PSPACE-complete (Sistla & Clarke 1985) | Verified | `src/ltl_model_check.c` — `ltl_is_satisfiable()` |
| 12 | LTL model checking is PSPACE-complete | Documented | `src/ltl_model_check.c` header |

## L5: Algorithms / Methods

| # | Algorithm | Status | Location |
|---|-----------|--------|----------|
| 1 | NNF conversion (linear time) | Complete | `src/ltl_equiv.c` — `ltl_to_nnf()` |
| 2 | Formula simplification (rewrite rules) | Complete | `src/ltl_equiv.c` — `ltl_simplify()`, `ltl_simplify_fixpoint()` |
| 3 | Operator elimination (derived → core) | Complete | `src/ltl_equiv.c` — `ltl_eliminate_derived()`, `ltl_to_core_until()` |
| 4 | Formula expansion/unwinding | Complete | `src/ltl_equiv.c` — `ltl_expand()`, `ltl_unwind()` |
| 5 | Tableau construction for LTL (Gerth et al. 1995) | Complete | `src/ltl_model_check.c` — `ltl_build_tableau()` |
| 6 | Explicit-state LTL model checking | Complete | `src/ltl_semantics.c` — `ltl_model_check()`, `ltl_model_check_enumerate()` |
| 7 | LTL satisfiability via tableau | Complete | `src/ltl_model_check.c` — `ltl_is_satisfiable()`, `ltl_find_witness()` |
| 8 | Trace-based semantic evaluation | Complete | `src/ltl_semantics.c` — `ltl_evaluate()` |
| 9 | Bounded semantics for monitoring | Complete | `src/ltl_semantics.c` — `ltl_evaluate_bounded()` |
| 10 | Fischer-Ladner closure computation | Complete | `src/ltl_ast.c` — `ltl_fischer_ladner_closure()` |
| 11 | Tableau product construction (M × A_{¬φ}) | Complete | `src/ltl_model_check.c` — `ltl_model_check_tableau()` |

## L6: Canonical Problems

| # | Problem | Status | Location |
|---|---------|--------|----------|
| 1 | LTL Model Checking: M ⊨ φ ? | Complete | `src/ltl_semantics.c`, `src/ltl_model_check.c` |
| 2 | LTL Satisfiability: ∃σ : σ ⊨ φ ? | Complete | `src/ltl_model_check.c` — `ltl_is_satisfiable()` |
| 3 | Counterexample extraction | Complete | `src/ltl_semantics.c` — `ltl_model_check(..., &cex_path)` |
| 4 | Witness trace generation | Complete | `src/ltl_model_check.c` — `ltl_find_witness()` |
| 5 | Response pattern verification | Complete | `examples/example_ltl_check.c` |
| 6 | Mutual exclusion verification | Complete | `examples/example_ltl_check.c` |
| 7 | Traffic light controller verification | Complete | `examples/example_ltl_check.c` |

## L7: Applications

| # | Application | Status | Location |
|---|-------------|--------|----------|
| 1 | Specification pattern library (Dwyer et al. 1999) | Complete | `include/ltl_patterns.h`, `src/ltl_patterns.c` — 13 patterns |
| 2 | Runtime LTL monitoring | Complete | `src/ltl_semantics.c` — `ltl_monitor_step()` |
| 3 | Fairness constraint specification | Complete | `src/ltl_patterns.c` — weak/strong fairness, justice, compassion |
| 4 | Real-time bounded properties | Complete | `src/ltl_patterns.c` — `ltl_pattern_deadline()`, `ltl_pattern_timed_response()` |
| 5 | Mutual exclusion specification | Complete | `src/ltl_patterns.c` — `ltl_pattern_mutex()` |
| 6 | Alternating bit protocol patterns | Complete | `src/ltl_patterns.c` — `ltl_pattern_alternating()` |
| 7 | Traffic light controller modeling | Complete | `examples/example_ltl_check.c` |
| 8 | 2-process mutual exclusion | Complete | `examples/example_ltl_check.c` |
| 9 | S-expression formula exchange | Complete | `src/ltl_ast.c` — `ltl_parse()`, `ltl_to_sexpr()` |

## L8: Advanced Topics

| # | Topic | Status | Location |
|---|-------|--------|----------|
| 1 | Tableau-based LTL decision procedure | Complete | `src/ltl_model_check.c` — full tableau construction |
| 2 | LTL safety fragment classification | Complete | `src/ltl_equiv.c` — `ltl_is_safety_fragment()` |
| 3 | LTL liveness fragment classification | Complete | `src/ltl_equiv.c` — `ltl_is_liveness_fragment()` |
| 4 | Obligation fragment (Chang, Manna, Pnueli 1992) | Complete | `src/ltl_equiv.c` — `ltl_is_obligation_fragment()` |
| 5 | Bounded equivalence checking | Complete | `src/ltl_equiv.c` — `ltl_check_equivalence_bounded()` |
| 6 | Operator elimination (core fragment: X, U only) | Complete | `src/ltl_equiv.c` — `ltl_to_core_until()` |

## L9: Research Frontiers

| # | Topic | Status | Location |
|---|-------|--------|----------|
| 1 | GR(1) reactive synthesis (preparation) | Partial | `src/ltl_patterns.c` — compassion/justice patterns |
| 2 | LTL synthesis (Pnueli & Rosner 1989) | Documented | docs/course-alignment.md |
| 3 | Runtime verification with LTL | Documented | `src/ltl_semantics.c` — monitor step infrastructure |
| 4 | Probabilistic LTL | Documented | docs/gap-report.md |
