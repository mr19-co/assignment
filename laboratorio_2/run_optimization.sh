opt-19 -load-pass-plugin build/libLocalOpts.so -passes=local-opts Foo.ll -o Foo.optimized.bc
llvm-dis-19 Foo.optimized.bc -o Foo.optimized.ll
