./build_pass.sh
echo '================================================================================'
opt-19 -load-pass-plugin build/libLoopPass.so -passes=loop-pass Example.m2r.ll -o Example.optimized.bc
llvm-dis-19 Example.optimized.bc -o Example.optimized.ll
