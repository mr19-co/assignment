./build_passes.sh
echo '================================================================================'
opt-19 \
    -load-pass-plugin build/libAlgebraicIdentity.so\
    -load-pass-plugin build/libStrengthReduction.so \
    -load-pass-plugin build/libMultiInstructionOptimization.so \
    -passes=algebraic-identity,strength-reduction,multi-instruction-optimization \
    Example.ll -o Example.optimized.bc
llvm-dis-19 Example.optimized.bc -o Example.optimized.ll
