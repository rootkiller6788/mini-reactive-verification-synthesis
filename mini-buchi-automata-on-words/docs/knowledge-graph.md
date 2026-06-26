# Knowledge Graph — mini-buchi-automata-on-words

## L1 — Definitions (Complete ✅)

| Entry | C Implementation | Lean Formalization |
|-------|-----------------|-------------------|
| Nondeterministic Buchi Automaton (NBA) 5-tuple (Q,Σ,δ,q₀,F) | `BuchiAutomaton` struct, `buchi_create()` | `NBA` structure |
| ω-word (infinite word over Σ) | `OmegaWord` struct | `OmegaWord` type |
| Lasso representation (prefix·cycle^ω) | `omega_make_lasso()` | `Lasso` constructor |
| Periodic ω-word (period^ω) | `omega_make_periodic()` | `Periodic` constructor |
| Transition relation δ ⊆ Q×Σ×Q (nondeterministic) | `BuchiTransition`, `BuchiTransitionSet` | `delta : Q -> Sigma -> Set Q` |
| Run (infinite sequence of states) | `BuchiRun` struct | `Run` type |
| Accepting run: Inf(ρ) ∩ F ≠ ∅ | `buchi_run_is_accepting()` | `accepting : Run -> Prop` |
| Acceptance condition types (Büchi, co-Büchi, Rabin, Streett, Parity, Muller) | `AccConditionType` enum | `AccCondition` inductive |
| Generalized Büchi Automaton (GBA): k acceptance sets | `GBA` struct | `GBA` structure |

## L2 — Core Concepts (Complete ✅)

| Entry | Implementation |
|-------|---------------|
| Büchi acceptance: Inf(ρ) ∩ F ≠ ∅ | `buchi_accepts_lasso()`, `buchi_accepts()` |
| co-Büchi acceptance: Inf(ρ) ∩ F = ∅ | `acc_condition_is_dual()` |
| Rabin acceptance: ∃i (Inf∩E_i=∅ ∧ Inf∩F_i≠∅) | `rabin_check()` |
| Streett acceptance (strong fairness): ∀i (Inf∩G_i≠∅ → Inf∩R_i≠∅) | `streett_check()` |
| Parity acceptance (min-even / max-even / min-odd / max-odd) | `parity_check()` |
| Muller acceptance: Inf(ρ) ∈ F | `muller_check()` |
| Generalized Büchi acceptance: ∀i (Inf∩F_i≠∅) | `gbuchi_check()` |
| ω-regular languages | closure under union/intersection/projection |
| Deterministic vs nondeterministic ω-automata | DBA ⊊ NBA expressiveness gap |
| Language emptiness: L(A)=∅ | `buchi_is_empty()` |

## L3 — Mathematical Structures (Complete ✅)

| Entry | Implementation |
|-------|---------------|
| NBA transition graph as directed graph with edge labels | `buchi_add_transition()`, `buchi_has_transition()` |
| SCC decomposition of directed graph (Tarjan) | `buchi_scc_decompose()`, `BuchiSCCDecomp` |
| Nested DFS data structure (blue/red coloring) | `NestedDFS` struct |
| Subset construction for complementation | `buchi_complement_kv()` |
| Product construction with Kripke structure | `buchi_product_ks()` |
| Acceptance condition data types (RabinPair, StreettPair, etc.) | All condition structs |
| DOT graph output for visualization | `buchi_print_dot()` |
| ω-word indexing with modular arithmetic | `omega_get()` |

## L4 — Fundamental Laws (Complete ✅)

| Theorem | Status | Implementation |
|---------|--------|---------------|
| NBA emptiness is NLOGSPACE-complete (decidable in O(n+m)) | ✅ Complete | `buchi_is_empty()` |
| ω-regular languages closed under union | ✅ Complete | `buchi_union()` |
| ω-regular languages closed under intersection | ✅ Complete | `buchi_intersect()` |
| ω-regular languages closed under projection | ✅ Complete | `buchi_project()` |
| ω-regular languages closed under complement (Büchi 1962) | ✅ Complete | `buchi_complement_kv()` |
| Rabin-Streett duality | ✅ Complete | `rabin_complement()` |
| Büchi = Rabin with 1 pair ({},F) | ✅ Complete | `buchi_to_rabin()` |
| Parity-to-Rabin conversion (O(k) pairs) | ✅ Complete | `parity_to_rabin()` |
| GBA degeneralization (GBA → NBA, O(k·|Q|) states) | ✅ Complete | `gba_degeneralize()` |

## L5 — Algorithms/Methods (Complete ✅)

| Algorithm | Complexity | Implementation |
|-----------|-----------|---------------|
| Tarjan's SCC algorithm | O(|Q|+|δ|) | `buchi_scc_decompose()` |
| SCC-based emptiness | O(|Q|+|δ|) | `buchi_is_empty()` |
| Counterexample extraction via BFS + SCC | O(|Q|+|δ|) | `buchi_find_accepting_lasso()` |
| Nested DFS (Courcoubetis et al. 1992) | O(|Q|+|δ|) | `nested_dfs_check()` |
| Standard GBA degeneralization | O(k·|Q|²) | `gba_degeneralize()` |
| Subset-construction-based complementation | O(2^|Q|) | `buchi_complement_kv()` |
| Union construction (disjoint union + new q₀) | O(|Q₁|+|Q₂|) | `buchi_union()` |
| Intersection construction (product with flag) | O(|Q₁|×|Q₂|) | `buchi_intersect()` |
| Trimming (forward + backward reachability) | O(|Q|+|δ|) | `buchi_trim()` |

## L6 — Canonical Problems (Complete ✅)

| Problem | Recognition | Implementation |
|---------|------------|---------------|
| Eventually-a: {w | ∃i: w[i]=a} | NBA with 2 states | `example_nba_operations.c` |
| Infinitely-often-a: {w | Inf(w)∩{a}≠∅} | NBA with 2 states | `test_buchi_core.c` |
| Eventually-always-a: {w | ∃i∀j≥i: w[j]=a} | NBA with 2 states | `test_buchi_core.c` |
| Mutual exclusion verification | Product + emptiness | `example_mutex_verification.c` |
| Ladder automaton family | Parameterized NBA | `example_emptiness_demo.c` |

## L7 — Applications (Complete ✅)

| Application | Implementation |
|-------------|---------------|
| Model checking: Kripke structure product | `buchi_product_ks()` |
| Safety property verification (mutual exclusion) | `example_mutex_verification.c` |
| Liveness property verification | `example_mutex_verification.c` |
| Counterexample visualization (DOT output) | `demo_dot_visualizer.c` |

## L8 — Advanced Topics (Partial ⚠️)

| Topic | Status | Notes |
|-------|--------|-------|
| Rank-based complementation (Kupferman-Vardi 2001) | ✅ Complete | `buchi_complement_kv()` |
| Nested DFS for GBA (Tauriainen 2004) | ⚠️ Partial | Documented, not fully implemented |
| Optimal degeneralization (Tauriainen 2006) | ⚠️ Partial | Falls back to standard |
| Safra determinization | ❌ Missing | Exponential construction |
| Schewe optimal complementation | ❌ Missing | Research-level |

## L9 — Research Frontiers (Documented Only)

| Topic | Reference |
|-------|-----------|
| History-deterministic automata | Henzinger & Piterman 2006 |
| Limit-deterministic Büchi automata | Sickert et al. 2016 |
| Index appearance records for parity games | Jurdzinski 2000 |
| Complementation lower bound: (0.96n)^n | Schewe 2009 |
| Target discounted-sum automata | Boker & Henzinger 2014 |
