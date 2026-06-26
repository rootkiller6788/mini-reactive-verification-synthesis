# CTL Gap Report

## Missing Topics (Priority Order)

### High Priority
- None — all L1-L6 topics covered completely.

### Medium Priority
1. **Symbolic CTL Model Checking (BDD-based)** — OBDD-based state representation would enable verification of systems with >10^20 states. The explicit-state implementation limits scalability. Reference: Burch, Clarke, McMillan, Dill (1992).
2. **CTL* Model Checking** — The full branching-time logic CTL* subsumes both CTL and LTL. Model checking CTL* is PSPACE-complete. Our implementation covers CTL only.

### Low Priority (L9 Research Frontiers)
3. **Probabilistic CTL (PCTL)** — Extending CTL with probability operators for quantitative verification of stochastic systems.
4. **Timed CTL (TCTL)** — Adding real-time constraints to CTL operators for timed automata.
5. **Parametric CTL** — CTL with parameters in temporal operators for robustness analysis.

## Partial Coverage

- **Counterexample Minimization** — Counterexample generation exists but could be improved with minimal counterexample computation (shortest path to violation).
- **CTL SAT Performance** — The SAT solver uses brute-force enumeration. A proper tableau-based or automata-theoretic decision procedure would be more efficient and scale better.

## No Coverage

- CTL* (superset of CTL)
- PCTL (probabilistic extension)
- TCTL (timed extension)
- Abstract CTL model checking (CEGAR loop)
