# CTL — Nine-School Curriculum Alignment

| School | Course | Relevant Chapters | Our Coverage |
|--------|--------|-------------------|-------------|
| **MIT** | 6.045 Automata, Computability, Complexity | Temporal Logic, Model Checking | L1-L6 Complete |
| **Stanford** | CS256/CS358 Formal Methods / Circuit Complexity | CTL, Symbolic MC, BDDs | L1-L6 + L8 BMC |
| **Berkeley** | CS172/CS278 Complexity | Model Checking Complexity (EXPTIME) | L4 SAT EXPTIME |
| **CMU** | 15-414 Bug Catching / 15-814 Model Checking | CTL Model Checking, SMV | L1-L7 Applications |
| **Princeton** | COS 522 Complexity / COS 551 Advanced | Temporal Logic, Verification | L4-L5 Algorithms |
| **Caltech** | CS 151/154 Complexity / Limits | Branching Time Logic | L2-L4 Concepts |
| **Cambridge** | Part II Logic & Proof / Complexity | Temporal Logic Semantics | L3-L4 Structures |
| **Oxford** | Computational Complexity / Advanced | Model Checking, Automata | L5 Algorithms |
| **ETH** | 252-0400 Logic & Computation | CTL Semantics, Fixpoint Logics | L3-L4 Fixpoints |

## Chapter-by-Chapter Mapping

| Textbook Chapter | Topic | Implementation |
|-----------------|-------|---------------|
| Baier & Katoen §6.1 | CTL Syntax | `ctl_ast.h`, `ctl_ast.c` |
| Baier & Katoen §6.2 | CTL Semantics (Kripke semantics) | `ctl_kripke.c`, `ctl_modelcheck.c` |
| Baier & Katoen §6.3 | CTL Equivalences | `ctl_equiv.c` |
| Baier & Katoen §6.4 | CTL Model Checking (labeling algorithm) | `ctl_modelcheck.c` |
| Baier & Katoen §6.5 | Fairness in CTL | `ctl_modelcheck.c` (fair MC) |
| Baier & Katoen §6.6 | CTL* and extensions | Documented in gap-report |
| Clarke, Grumberg, Peled Ch.2 | Kripke Structures | `ctl_kripke.h`, `ctl_kripke.c` |
| Clarke, Grumberg, Peled Ch.3 | CTL Model Checking | `ctl_modelcheck.c` |
| Clarke, Grumberg, Peled Ch.10 | Counterexamples | `ctl_counterexample.c` |
| Emerson (1990) | Adequate Sets, Duality | `ctl_equiv.c` |
| Emerson, Jutla (1988) | CTL SAT EXPTIME | `ctl_sat.c` |
