# LTL Gap Report — Missing Knowledge Points & Priorities

## Current Gaps

| # | Gap | Priority | Effort | Level |
|---|-----|----------|--------|-------|
| 1 | LTL+Past operators (Y, O, H, S) | Low | Medium | L8 |
| 2 | Full LTL-to-Büchi translation (separate module) | Low | High | L9 |
| 3 | BDD-based symbolic LTL model checking | Medium | High | L9 |
| 4 | GR(1) synthesis algorithm | Medium | High | L9 |
| 5 | Probabilistic LTL (PLTL) semantics | Low | High | L9 |
| 6 | LTL modulo theories (LTL-MT) | Low | High | L9 |
| 7 | Stutter-invariant LTL | Low | Medium | L8 |
| 8 | LTL learning from traces | Low | High | L9 |
| 9 | OBDD implementation for symbolic checking | Medium | High | L9 |

## Addressed Gaps (from initial state)

All core L1-L6 gaps have been resolved:
- [x] LTL AST construction: all 14 node types implemented
- [x] Trace-based semantics with lasso support
- [x] NNF conversion with all duality laws
- [x] Formula simplification with 30+ rewrite rules
- [x] Tableau construction for LTL
- [x] Explicit-state model checking
- [x] Specification pattern library (13 patterns)
- [x] Safety/Liveness/Obligation classification
- [x] Fischer-Ladner closure
- [x] S-expression parser/serializer

## Dependencies

This module provides the LTL foundation used by:
- `mini-ltl-to-buchi-translation` — translates LTL formulas to Büchi automata
- `mini-reactive-synthesis-pnueli` — GR(1) synthesis requires LTL specifications
- `mini-omega-regular-properties` — LTL ⊆ ω-regular properties

## Recommendations

Priority order for future work:
1. BDD-based symbolic model checking (highest impact for scalability)
2. GR(1) synthesis algorithm (connects to reactive synthesis track)
3. LTL+Past operators (completes temporal logic coverage)
