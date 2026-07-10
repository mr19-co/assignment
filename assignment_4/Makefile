.PHONY: Example.optimized.ll Example.optimized.bc

Example.optimized.ll: Example.optimized.bc
	llvm-dis-19 Example.optimized.bc -o Example.optimized.ll

Example.optimized.bc: build/libLoopFusionPass.so Example.m2r.ll
	opt-19 \
		-load-pass-plugin build/libLoopFusionPass.so \
		-passes=loop-fusion-pass \
		Example.m2r.ll \
		-o Example.optimized.bc

build/libLoopFusionPass.so: LoopFusionPass.cpp
	mkdir -p build && \
	cd build && \
	export LLVM_DIR=/usr/bin && \
	cmake -DLT_LLVM_INSTALL_DIR=$LLVM_DIR .. && \
	make

Example.m2r.ll: Example.ll
	opt-19 -p mem2reg Example.ll -So Example.m2r.ll

Example.ll: Example.c
	clang-19 -S -O0 -emit-llvm -Xclang -disable-O0-optnone Example.c -o Example.ll