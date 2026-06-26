# Course Alignment — mini-buchi-automata-on-words

## Nine-School Curriculum Mapping

### MIT
| Course | Topic | Implementation |
|--------|-------|---------------|
| 6.045/18.400 Automata, Computability, Complexity | Omega-automata, NBA definition | `buchi.h` definitions |
| 6.841 Advanced Complexity Theory | Omega-regular languages, complementation | `buchi_complement_kv()` |
| 6.845 Quantum Complexity Theory | (prerequisite concepts) | — |

### Stanford
| Course | Topic | Implementation |
|--------|-------|---------------|
| CS254 Computational Complexity | Rabin/Streett/Parity conditions | `acceptance_conditions.c` |
| CS358 Circuit Complexity | (circuit-to-automaton connections) | — |

### Berkeley
| Course | Topic | Implementation |
|--------|-------|---------------|
| CS172 Computability and Complexity | NBA emptiness | `buchi_is_empty()` |
| CS278 Computational Complexity | Omega-regular closure properties | `omega_operations.c` |

### CMU
| Course | Topic | Implementation |
|--------|-------|---------------|
| 15-455 Undergraduate Complexity Theory | SCC-based emptiness | `buchi_scc_decompose()` |
| 15-855 Graduate Complexity Theory | Nested DFS, complementation | `nested_dfs_check()`, `buchi_complement_kv()` |

### Princeton
| Course | Topic | Implementation |
|--------|-------|---------------|
| COS 522 Computational Complexity | Model checking product | `buchi_product_ks()` |
| COS 551 Advanced Complexity | GBA degeneralization | `gba_degeneralize()` |

### Caltech
| Course | Topic | Implementation |
|--------|-------|---------------|
| CS 151 Complexity Theory | Omega-word representations | `OmegaWord` struct |
| CS 154 Limits of Computation | Language containment | `buchi_language_contained()` |

### Cambridge
| Course | Topic | Implementation |
|--------|-------|---------------|
| Part II Complexity Theory | Buchi acceptance condition | `buchi_accepts_lasso()` |
| Part III Advanced Complexity | Rabin-Streett duality | `rabin_complement()` |

### Oxford
| Course | Topic | Implementation |
|--------|-------|---------------|
| Computational Complexity | Parity games connection | `parity_check()` |
| Advanced Complexity Theory | Determinization issues | Documented in headers |

### ETH
| Course | Topic | Implementation |
|--------|-------|---------------|
| 263-4650 Advanced Complexity | Omega-automata hierarchy | `omega_automata.h` |
| 252-0400 Logic and Computation | LTL/NBA connection | `example_mutex_verification.c` |

## Reference Textbooks

| Textbook | Chapters | Coverage |
|----------|---------|----------|
| Baier & Katoen — Principles of Model Checking | Ch. 4 (Omega-Automata) | Full coverage |
| Gradel, Thomas, Wilke — Automata, Logics, and Infinite Games | Ch. 1-3 | Core definitions + theory |
| Thomas — Automata on Infinite Objects (Handbook) | Complete | Expressiveness results |
| Clarke, Grumberg, Peled — Model Checking | Ch. 2, 9 | Model checking product |
| Holzmann — The SPIN Model Checker | Ch. 4-6 | Nested DFS implementation |
