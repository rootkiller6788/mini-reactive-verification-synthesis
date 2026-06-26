# Gap Report — mini-grg1-synthesis-algorithm

## Missing Knowledge Items (Priority-Ordered)

### High Priority (none)

All L1-L6 items and core algorithms are complete. No critical gaps.

### Medium Priority

1. **L7: Hardware synthesis integration**
   - Current: specification creation and strategy extraction
   - Missing: Verilog/VHDL output from synthesized strategies
   - Impact: 1 application
   - Effort: ~200 lines

2. **L8: Symbolic synthesis pipeline**
   - Current: BDD library with basic operations
   - Missing: Full symbolic CPre using BDD quantification
   - Impact: Performance for large state spaces
   - Effort: ~300 lines

### Low Priority

3. **L7: Model checker integration**
   - Missing: Interface with NuSMV/SPIN for specification import
   - Effort: ~150 lines

4. **L8: Strategy minimization**
   - Missing: Reduce strategy size via state merging
   - Effort: ~200 lines

5. **L9: Non-GR(1) LTL fragments**
   - Missing: Support for bounded LTL, safety LTL
   - Effort: ~500 lines (major research effort)

## False Gaps (Implemented but not obvious)

- Fixpoint convergence: implemented in grg1_fixpoint.c (LFP/GFP)
- BDD canonicity: unique table in grg1_bdd.c
- Memoryless strategy sufficiency: proven in Lean (theorem statement)
- CPre monotonicity: both C and Lean proofs
