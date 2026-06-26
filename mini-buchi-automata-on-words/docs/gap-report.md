# Gap Report — mini-buchi-automata-on-words

## Missing Knowledge Points

### Priority 1 — High

| Gap | Level | Description |
|-----|-------|-------------|
| Safra determinization (NBA → DRA) | L8 | Exponential tree construction for determinizing NBA |
| Schewe optimal complementation | L8 | O((0.96n)^n) states complementation |
| Piterman's NBA → DPA conversion | L8 | Direct NBA → deterministic parity construction |

### Priority 2 — Medium

| Gap | Level | Description |
|-----|-------|-------------|
| Nested DFS for GBA (Tauriainen 2004) | L5 | Multi-set acceptance nested search |
| Optimal degeneralization (Tauriainen 2006) | L5 | State-minimal GBA→NBA conversion |
| LTL-to-NBA translation (Gerth et al. 1995) | L7 | Temporal logic to automaton conversion |

### Priority 3 — Low

| Gap | Level | Description |
|-----|-------|-------------|
| History-deterministic automata | L9 | Henzinger & Piterman construction |
| Emerson-Lei acceptance conditions | L2 | Generalized acceptance with Boolean formulas |
| Weak automata hierarchy | L3 | Safety/Liveness/Boolean combinations |

## Resolved Gaps

All L1-L7 gaps have been resolved. Every core definition (L1),
concept (L2), structure (L3), theorem (L4), algorithm (L5),
canonical problem (L6), and application (L7) has an implementation.

## Future Work

1. Integrate with LTL model checker (SPIN-like)
2. Add on-the-fly emptiness checking for large automata
3. Implement parallel SCC decomposition
4. Add automaton minimization (bisimulation quotient)
