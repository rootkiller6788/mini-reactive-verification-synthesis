# CTL Knowledge Graph — Nine-Layer Coverage

## L1: Definitions (Complete ✅)

| Entity | C Definition | Location |
|--------|-------------|----------|
| CTL Formula AST | `ctl_formula` struct + `ctl_formula_type` enum | include/ctl_ast.h:69-87 |
| Kripke Structure | `ctl_kripke` struct (S, S0, R, L) | include/ctl_kripke.h:73-84 |
| State | `ctl_state_id` typedef | include/ctl_kripke.h:27 |
| State Set | `ctl_state_set` bit-vector | include/ctl_kripke.h:127-131 |
| Label Set | `ctl_label_set` per-state proposition set | include/ctl_kripke.h:53-56 |
| Path Quantifiers | A (all paths), E (exists a path) | include/ctl_ast.h:9-10 |
| Temporal Operators | X, F, G, U, R | include/ctl_ast.h:10-12 |
| SAT Set | `ctl_mc_context` result | include/ctl_modelcheck.h:39-46 |
| Counterexample | `ctl_counterexample` struct | include/ctl_modelcheck.h:108-114 |
| Fairness Constraint | `ctl_fairness_constraint` | include/ctl_modelcheck.h:132-135 |

## L2: Core Concepts (Complete ✅)

| Concept | Implementation | Location |
|---------|---------------|----------|
| Model Checking Problem | `ctl_model_check()` | src/ctl_modelcheck.c:576-640 |
| State Explosion | Bit-vector state set operations | src/ctl_kripke.c |
| Fixpoint Characterization | `ctl_label_eg()`, `ctl_label_eu()` | src/ctl_modelcheck.c:69-153 |
| Bottom-up Labeling | `compute_sat()` recursive labeling | src/ctl_modelcheck.c:310-574 |
| CTL vs LTL | `ctl_is_ltl_expressible()` | src/ctl_equiv.c:654-701 |
| Adequate Set | `ctl_reduce_to_core()` to {EX, EG, EU} | src/ctl_equiv.c:390-445 |
| Bounded Model Checking | `ctl_bounded_model_check()` | src/ctl_modelcheck.c:719-835 |
| Fair Model Checking | `ctl_fair_model_check()` | src/ctl_modelcheck.c:855-918 |

## L3: Mathematical Structures (Complete ✅)

| Structure | Implementation | Location |
|-----------|---------------|----------|
| Kripke Structure (S, S0, R, L) | `ctl_kripke` with adjacency lists | src/ctl_kripke.c |
| Transition Relation R | Doubly-linked adjacency lists | src/ctl_kripke.c:138-175 |
| Labeling Function L | `ctl_label_set` bit vectors | src/ctl_kripke.c:22-69 |
| State Space 2^AP | Bit-vector state set | src/ctl_kripke.c:270-387 |
| Pre-image Pre∃ | `ctl_pre_image()` | src/ctl_kripke.c:393-405 |
| Universal Pre-image Pre∀ | `ctl_pre_image_all()` | src/ctl_kripke.c:407-424 |
| SCC Decomposition | Tarjan's algorithm | src/ctl_kripke.c:457-506 |
| Bottom SCCs | `ctl_bottom_sccs()` | src/ctl_kripke.c:512-537 |
| Fischer-Ladner Closure | `compute_closure()` (tableau) | src/ctl_sat.c:60-98 |

## L4: Fundamental Laws (Complete ✅)

| Theorem / Law | Code | Location |
|--------------|------|----------|
| CTL Model Checking Correctness | `ctl_model_check()` (Clarke-Emerson-Sistla 1986) | src/ctl_modelcheck.c |
| Duality Laws (EX↔AX, EF↔AG, etc.) | `ctl_dual()` | src/ctl_equiv.c:256-302 |
| Fixpoint Theorems (Knaster-Tarski) | `ctl_label_eg()`, `ctl_label_eu()` monotone convergence | src/ctl_modelcheck.c:69-153 |
| Expansion Laws | `ctl_expand()` | src/ctl_equiv.c:330-367 |
| De Morgan Laws for CTL | `ctl_to_pnf()` | src/ctl_ast.c |
| Emerson Adequate Set Theorem | `ctl_reduce_to_core()` | src/ctl_equiv.c:390-445 |
| Small Model Property | SAT search up to 2^|φ| | src/ctl_sat.c:150-272 |
| CTL SAT EXPTIME-completeness | `ctl_is_satisfiable()` | src/ctl_sat.c |

## L5: Algorithms/Methods (Complete ✅)

| Algorithm | Implementation | Complexity |
|-----------|---------------|------------|
| CES Labeling Algorithm | `compute_sat()` | O(|φ|*(|S|+|R|)) |
| EG Fixpoint (Greatest) | `ctl_label_eg()` | O(|S|*(|S|+|R|)) |
| EU Fixpoint (Least) | `ctl_label_eu()` | O(|S|*(|S|+|R|)) |
| Tarjan SCC | `ctl_compute_sccs()` | O(|S|+|R|) |
| BFS Reachability | `ctl_reachable_states()` | O(|S|+|R|) |
| CTL SAT (Tableau) | `ctl_is_satisfiable()` | EXPTIME |
| Bounded MC | `ctl_bounded_model_check()` | O(bound * branching^bound) |
| Fair MC | `ctl_fair_model_check()` | O(|φ|*(|S|+|R|)*|fair|) |
| Formula Simplification | `ctl_simplify()` | O(|φ|) |
| PNF Conversion | `ctl_to_pnf()` | O(|φ|) |

## L6: Canonical Problems (Complete ✅)

| Problem | Example | File |
|---------|---------|------|
| Mutual Exclusion | 2-process mutex protocol verification | examples/ex_mutex.c |
| Traffic Light Controller | Safety + liveness properties | examples/ex_traffic.c |
| Formula Equivalence | Duality, NNF, reduction to core | examples/ex_equiv.c |

## L7: Applications (Partial+ ✅ — 3 applications)

| Application | Description |
|-------------|-------------|
| **Formal Verification** | CTL model checking for reactive systems (mutual exclusion, traffic control) |
| **Safety-Critical Systems** | Safety properties (AG !bad) and liveness properties (AG (req -> AF ack)) |
| **Hardware Verification** | CTL used in industrial model checkers (SMV, NuSMV) — verified by CTL model checking |

## L8: Advanced Topics (Partial+ ✅ — 2 topics)

| Topic | Implementation |
|-------|---------------|
| **Bounded Model Checking** | `ctl_bounded_model_check()` — checks properties up to a bound |
| **Fair CTL Model Checking** | `ctl_fair_model_check()` — fairness-constrained path quantification |

## L9: Research Frontiers (Partial — documented)

| Topic | Status |
|-------|--------|
| CTL* (full branching-time logic) | Documented in course-tree.md |
| Symbolic CTL Model Checking (BDDs) | Documented; implementation deferred |
| Probabilistic CTL (PCTL) | Documented in gap-report.md |
