./build_passes.sh
echo '================================================================================'
opt-19 -load-pass-plugin build/libAlgebraicIdentity.so -passes=algebraic-identity Example.ll -o Example.optimized.bc
llvm-dis-19 Example.optimized.bc -o Example.optimized.ll
