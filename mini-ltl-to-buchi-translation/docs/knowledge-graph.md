# Knowledge Graph: LTL-to-Buchi Translation

## L1: Definitions [COMPLETE]
- LTL formula syntax (16 operators)
- Buchi automaton tuple (Q, Sigma, delta, Q0, F)
- Generalized Buchi automaton
- Transition labels (conjunctive literal form)
- Acceptance condition (Inf(rho) cap F != empty)
- omega-words and omega-regular languages

## L2: Core Concepts [COMPLETE]
- Formula construction API
- Reference counting memory management
- Formula metrics (size, depth, subformulas, atoms)
- Automaton construction API (states, transitions)
- Label algebra (conjunction, implication, compatibility)
- Property class hierarchy (Manna-Pnueli)

## L3: Mathematical Structures [COMPLETE]
- Negation Normal Form (NNF)
- Label representation (pos/neg bitmasks)
- Fischer-Ladner closure structure
- Tableau graph (nodes + edges)
- GPVW node structure (old/new/next sets)
- SCC decomposition

## L4: Fundamental Laws [COMPLETE]
- Fixed-point expansion: phi U psi = psi or (phi and X(phi U psi))
- Duality: not(phi U psi) = (not phi) R (not psi)
- NNF conversion correctness
- Fischer-Ladner theorem: |FL(phi)| = O(|phi|)
- Buchi closure properties (union, intersection, complement)
- Emptiness: L(B) != empty iff reachable accepting cycle

## L5: Algorithms [COMPLETE]
- NNF push-negation
- Formula simplification (Boolean + temporal identities)
- Fischer-Ladner closure computation
- Classic tableau construction (Wolper-Vardi-Sistla)
- GPVW on-the-fly construction
- Degeneralization (TGBA -> NBA)
- Nested DFS emptiness checking (CVWY)
- Tarjan SCC algorithm
- Buchi product and union
- Reachability analysis

## L6: Canonical Problems [COMPLETE]
- G(p -> F q): Response property
- G F p: Recurrence (strong fairness)
- F G p: Persistence (stabilization)
- p U q: Until property
- G(!p || !q): Mutual exclusion
- G(p -> X q): Next-step response
- Property classification (safety/liveness/obligation/recurrence/persistence/reactivity)

## L7: Applications [COMPLETE]
- Automata-theoretic model checking (Sys |= phi iff L(B_Sys) cap L(B_not_phi) = empty)
- Satisfiability checking
- Validity checking
- Counterexample generation (lasso traces)
- DOT format visualization
- Pattern-based translation

## L8: Advanced Topics [PARTIAL]
- Safra complementation (documented, not implemented)
- Alternating Buchi automata
- LTL simplification heuristics
- Simulation-based reduction

## L9: Research Frontiers [PARTIAL]
- Limit-deterministic Buchi automata
- Probabilistic LTL model checking
- Runtime monitoring via LTL_3 semantics
- LTL synthesis (reactive synthesis)