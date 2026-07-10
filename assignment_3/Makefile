SHELL=/usr/bin/bash
FILE=Example.c
FILE_NO_EXT=$(basename ${FILE})

.PHONY: test/${FILE_NO_EXT}.optimized.ll test/${FILE_NO_EXT}.optimized.bc

test/${FILE_NO_EXT}.optimized.ll: test/${FILE_NO_EXT}.optimized.bc
	llvm-dis test/${FILE_NO_EXT}.optimized.bc -o test/${FILE_NO_EXT}.optimized.ll

test/${FILE_NO_EXT}.optimized.bc: build/libLoopPass.so test/${FILE_NO_EXT}.m2r.ll
	opt \
		-load-pass-plugin build/libLoopPass.so \
		-passes=loop-pass \
		test/${FILE_NO_EXT}.m2r.ll \
		-o test/${FILE_NO_EXT}.optimized.bc

build/libLoopPass.so: LoopPass.cpp
	mkdir -p build && \
	cd build && \
	export LLVM_DIR=/usr/bin && \
	cmake -DLT_LLVM_INSTALL_DIR=$$LLVM_DIR .. && \
	make

test/${FILE_NO_EXT}.m2r.ll: test/${FILE_NO_EXT}.ll
	opt -p mem2reg test/${FILE_NO_EXT}.ll -So test/${FILE_NO_EXT}.m2r.ll

test/${FILE_NO_EXT}.ll: test/${FILE}
	clang -S -O0 -emit-llvm -Xclang -disable-O0-optnone test/${FILE} -o test/${FILE_NO_EXT}.ll