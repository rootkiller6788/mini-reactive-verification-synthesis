# Knowledge Graph ¡ª mini-reactive-synthesis-pnueli

## L1: Definitions
- Reactive system, reactive module (Mealy/Moore machine)
- LTL formula syntax and semantics
- Game arena, two-player game
- Parity game, safety game, reachability game
- Winning strategy, winning region
- Realizability problem
- Synthesis problem
- Nondeterministic B¨¹chi automaton (NBA)
- Deterministic parity automaton (DPA)
- Deterministic Rabin automaton (DRA)
- GR(1) fragment of LTL
- Fischer-Ladner closure
- Attractor set in games

## L2: Core Concepts
- Open vs closed systems
- Environment assumptions vs system guarantees (A -> G)
- Safety vs liveness properties
- Game determinacy (Martin's theorem)
- Automaton complementation and determinization
- Polynomial-time fragment (GR(1))
- Tableau-based LTL-to-automaton translation
- Controllable predecessor operator
- Symbolic vs explicit-state synthesis

## L3: Mathematical Structures
- Game graph G = (V, V0, V1, E)
- Parity condition c: V -> {0,...,k}
- LTL formula DAG with sharing
- NBA: (Q, Sigma, delta, q0, F)
- DPA: (Q, Sigma, delta, q0, c)
- Valuation bit-vectors (up to 128 variables)
- Symbolic region (bitmask representation)
- GR(1) state space: S = 2^I x 2^O

## L4: Fundamental Laws
- Pnueli-Rosner Theorem: LTL synthesis is 2EXPTIME-complete
- GR(1) synthesis is polynomial-time O(m n^3)
- LTL model checking is PSPACE-complete
- NBA complementation is EXPSPACE-complete
- Parity game solving is in NP intersect co-NP
- Emptiness of NBA is NLOGSPACE-complete
- Safety games are solvable in linear time
- Dualities: safety/reachability, until/release, always/eventually

## L5: Algorithms/Methods
- Zielonka's recursive parity game solver
- Jurdzinski's small progress measures
- Safety game greatest-fixed-point iteration
- Reachability game attractor computation
- Gerth-Peled-Vardi-Wolper LTL-to-NBA tableau
- Safra's NBA determinization (2^{O(n log n)})
- Piterman's compact determinization
- GR(1) nested fixed-point algorithm
- Bounded synthesis via enumeration
- Nested DFS for NBA emptiness
- LTL NNF transformation
- LTL expansion laws (tableau decomposition)

## L6: Canonical Problems
- Bus arbiter synthesis
- Traffic light controller
- Mutual exclusion protocol (Pnueli's classic)
- Robot mission planning from LTL
- Elevator controller
- Producer-consumer coordination

## L7: Applications
- Hardware controller synthesis (arbiter)
- Robot mission planning (LTL-based)
- Protocol synthesis (mutual exclusion)
- Formal verification of reactive systems

## L8: Advanced Topics
- Bounded synthesis (Finkbeiner & Schewe)
- Symbolic synthesis with BDDs
- GR(1) polynomial fragment
- Simulation-based NBA minimization
- Assume-guarantee decomposition

## L9: Research Frontiers
- Synthesis from probabilistic LTL
- Hyperproperties synthesis
- Neural-guided synthesis
- Infinite-state synthesis
- Runtime enforcement of LTL
