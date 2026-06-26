# Knowledge Graph — mini-grg1-synthesis-algorithm

## Nine-Layer Knowledge Coverage

| Level | Name | Status | Items Covered |
|-------|------|--------|---------------|
| **L1** | Definitions | **Complete** | Game arena, state, valuation, transition, strategy, winning region, GR(1) formula, justice condition, safety condition, controllable predecessor, fixpoint, region |
| **L2** | Core Concepts | **Complete** | Two-player determinacy, memoryless strategies, safety vs liveness, realizability, environment assumptions vs system guarantees, monotonicity of CPre |
| **L3** | Mathematical Structures | **Complete** | Complete lattice (P(S),⊆), bit-vector representation, BDD/ROBDD, strongly connected components, game graph, transition system |
| **L4** | Fundamental Laws | **Complete** | Knaster-Tarski fixpoint theorem, CPre monotonicity, soundness of nested fixpoint, determinacy of finite-state games, safety game winning region characterization |
| **L5** | Algorithms/Methods | **Complete** | Kleene iteration (LFP/GFP), nested fixpoint ν.μ, attractor computation, worklist algorithm, BDD operations (ite/apply/restrict), strategy extraction, counterstrategy extraction |
| **L6** | Canonical Problems | **Complete** | Mutual exclusion arbiter, traffic light controller, reachability game, safety game, Büchi game |
| **L7** | Applications | **Partial+** | Bounded strategy verification, DOT export for visualization, BDD-based symbolic state representation |
| **L8** | Advanced Topics | **Partial+** | BDD symbolic synthesis, widening/narrowing (abstract interpretation), worklist acceleration |
| **L9** | Research Frontiers | **Partial** | Documented: bounded synthesis, timed GR(1), distributed GR(1), probabilistic GR(1), GR(1) modulo theories |

## Knowledge Items Detail

### L1 — Definitions
- `grg1_state_t` — State in game arena with valuation and turn
- `grg1_valuation_t` — Complete variable assignment
- `grg1_variable_t` — Variable with finite domain
- `grg1_transition_t` — Labeled edge in game graph
- `grg1_game_t` — Complete two-player game arena
- `grg1_spec_t` — GR(1) specification structure
- `grg1_justice_t` — Justice/liveness condition
- `grg1_region_t` — Bit-vector state set
- `grg1_strategy_t` — Memoryless strategy
- `grg1_bdd_node_t` — BDD node for symbolic representation
- `grg1_realizability_t` — Realizability classification

### L2 — Core Concepts
- Safety vs liveness decomposition (Alpern & Schneider, 1985)
- GR(1) as implication: assumptions → guarantees
- Environment assumptions as fairness constraints
- System guarantees as justice requirements
- Memoryless strategies suffice for GR(1) (Piterman et al., 2006)
- Realizability as game winning

### L3 — Mathematical Structures
- Complete lattice (P(S), ⊆) with ∪/∩/complement
- Bit-vector encoding for O(1) membership testing
- BDD unique table for canonical Boolean function representation
- Transition system semantics
- Game graph as labeled directed graph

### L4 — Fundamental Laws
- Knaster-Tarski fixpoint theorem (monotone F has LFP and GFP)
- cpre_sys_monotone: CPre_sys is monotone (proved in Lean)
- cpre_env_monotone: CPre_env is monotone (proved in Lean)
- Safety game GFP: W = νX. CPre_sys(X) ∩ CPre_env(X)
- Nested fixpoint convergence in ≤|S| iterations

### L5 — Algorithms
- Kleene LFP iteration from ∅
- Kleene GFP iteration from S
- Nested fixpoint νX.μY.F(X,Y) for GR(1) justice
- CPre_sys computation (branching on player turn)
- Attractor computation (repeated CPre)
- Tarjan's SCC algorithm
- BFS reachability
- Worklist-based fixpoint
- Widening/narrowing (abstract interpretation)
- Strategy extraction from winning region
- Counterstrategy extraction for debugging

### L6 — Canonical Problems
- Mutual exclusion arbiter (2 clients, 1 grant)
- Traffic light controller (NS/EW lights with pedestrians)
- Reachability game solving
- Safety game solving
- Büchi game solving (single justice)

### L7 — Applications
- Bounded trace verification of strategies
- Game visualization via DOT/Graphviz export
- BDD-to-explicit-region conversion
- CSV statistics export for benchmarking

### L8 — Advanced Topics
- BDD-based symbolic synthesis (ROBDD with unique table)
- Compute table for BDD operation caching
- Worklist algorithm for fixpoint acceleration
- Post-fixpoint narrowing for precision

### L9 — Research Frontiers (Documented)
- Bounded synthesis (Finkbeiner & Schewe, 2013)
- Timed GR(1) with deadlines
- Distributed GR(1) for multi-process synthesis
- Probabilistic GR(1) for stochastic environments
- GR(1) modulo theories (SMT-based synthesis)
- Incremental GR(1) synthesis
