# Mini Reactive Verification & Synthesis

A collection of **from-scratch, zero-dependency C implementations** of university-level theory for reactive system verification and synthesis. Each module maps to MIT (and other top-tier university) courses, bridging theory and practice by translating textbook algorithms into runnable C code — from LTL model checking and Büchi automata to GR(1) synthesis and symbolic verification with OBDDs.

## Sub-Modules

| Sub-Module | Topics | Key Courses |
|-----------|--------|-------------|
| [mini-buchi-automata-on-words](mini-buchi-automata-on-words/) | NBA on infinite words, emptiness checking (SCC/Nested DFS), ω-automata acceptance conditions (Büchi, Rabin, Streett, Parity, Muller), closure under union/intersection/projection/complement | MIT 6.841, Stanford CS254 |
| [mini-computation-tree-logic-ctl](mini-computation-tree-logic-ctl/) | CTL syntax/AST, formula equivalences & normal forms, Kripke structure representation, explicit-state model checking (Clarke-Emerson-Sistla algorithm), satisfiability & validity checking | MIT 6.841, CMU 15-414 |
| [mini-grg1-synthesis-algorithm](mini-grg1-synthesis-algorithm/) | GR(1) specification (initial/safety/justice), two-player game graph construction, nested fixpoint computation (νX.μY iteration), winning strategy extraction | MIT 6.841, Stanford CS254 |
| [mini-linear-temporal-logic-ltl](mini-linear-temporal-logic-ltl/) | LTL syntax/AST (Next, Until, Release, Weak Until), algebraic equivalence & expansion laws, specification patterns catalog (Dwyer et al. 1999), trace & Kripke semantics | MIT 6.841, Stanford CS254 |
| [mini-ltl-to-buchi-translation](mini-ltl-to-buchi-translation/) | LTL-to-NBA translation algorithms (tableau-based, Gerth et al., on-the-fly), Büchi automaton construction and optimization | MIT 6.841, Stanford CS254 |
| [mini-omega-regular-properties](mini-omega-regular-properties/) | ω-words & ω-regular languages, ω-regular expressions, Büchi automata on infinite words, LTL formula representation, closure properties of ω-regular languages | MIT 6.841, Stanford CS254 |
| [mini-reactive-synthesis-pnueli](mini-reactive-synthesis-pnueli/) | Full LTL synthesis pipeline (LTL→NBA→DPA→parity game→strategy), GR(1) synthesis, safety/reachability/Büchi/parity game solving, reactive module extraction as Mealy machine | MIT 6.841, Stanford CS254 |
| [mini-symbolic-model-checking-obdd](mini-symbolic-model-checking-obdd/) | ROBDD canonical representation (Bryant 1986), CTL symbolic model checking, least/greatest fixpoint computation over state-set lattice, Kripke structure symbolic encoding | MIT 6.841, CMU 18-760 |

## Design Philosophy

- **Zero external dependencies** — pure C (C99/C11), only `libc` and `libm`
- **Self-contained modules** — each directory has its own `Makefile`, `include/`, `src/`, `examples/`, `demos/`, `tests/`
- **Theory-to-code mapping** — every module includes `docs/` with course-alignment notes and proof sketches
- **Practical demos** — model checkers, emptiness checkers, synthesis engines, specification pattern validators

## Building

Each module is standalone. Navigate to a module directory and run:

```bash
cd mini-buchi-automata-on-words
make all    # build everything
make test   # run tests
```

Requires **GCC** and **GNU Make**.

## Project Structure

```
mini-reactive-verification-synthesis/
├── mini-buchi-automata-on-words/     # Büchi Automata on Infinite Words
├── mini-computation-tree-logic-ctl/  # Computation Tree Logic (CTL)
├── mini-grg1-synthesis-algorithm/    # GR(1) Reactive Synthesis
├── mini-linear-temporal-logic-ltl/   # Linear Temporal Logic (LTL)
├── mini-ltl-to-buchi-translation/    # LTL-to-Büchi Translation
├── mini-omega-regular-properties/    # ω-Regular Properties & Languages
├── mini-reactive-synthesis-pnueli/   # Reactive Synthesis (Pnueli Framework)
└── mini-symbolic-model-checking-obdd/ # Symbolic Model Checking with OBDDs
```

## License

MIT
