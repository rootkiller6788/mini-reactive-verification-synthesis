# Course Tree ¡ª mini-reactive-synthesis-pnueli

## Prerequisites
```
mini-linear-temporal-logic-ltl
  ©¸©¤©¤ mini-buchi-automata-on-words
        ©¸©¤©¤ mini-omega-regular-properties
              ©¸©¤©¤ mini-ltl-to-buchi-translation
                    ©¸©¤©¤ mini-reactive-synthesis-pnueli (this module)
                          ©¸©¤©¤ mini-grg1-synthesis-algorithm
                          ©¸©¤©¤ mini-symbolic-model-checking-obdd
```

## Dependencies Within This Module
```
reactive_types.h
  ©À©¤©¤ ltl_syntax.h
  ©¦     ©¸©¤©¤ automaton.h
  ©¦           ©¸©¤©¤ game_structure.h
  ©¦                 ©¸©¤©¤ synthesis.h
  ©¦                       ©¸©¤©¤ gr1_synthesis.h
  ©¸©¤©¤ (all src files depend on corresponding headers)
```

## Knowledge Dependencies
- LTL syntax and semantics (L1-L3)
- Omega-automata theory (L2-L4)
- Game theory / two-player games (L2-L4)
- Fixed-point theory (L3-L4)
- Complexity theory (2EXPTIME, PSPACE, NP) (L4)
