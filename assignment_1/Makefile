SHELL=/usr/bin/bash
FILE=Example.ll
FILE_NO_EXT=$(basename ${FILE})

.PHONY: test/${FILE_NO_EXT}.optimized.ll test/${FILE_NO_EXT}.optimized.bc build_passes

test/${FILE_NO_EXT}.optimized.ll: test/${FILE_NO_EXT}.optimized.bc
	llvm-dis test/${FILE_NO_EXT}.optimized.bc -o test/${FILE_NO_EXT}.optimized.ll

test/${FILE_NO_EXT}.optimized.bc: \
build/libAlgebraicIdentity.so \
build/libStrengthReduction.so \
build/libMultiInstructionOptimization.so \
test/${FILE}
	opt \
		-load-pass-plugin build/libAlgebraicIdentity.so \
		-load-pass-plugin build/libStrengthReduction.so \
		-load-pass-plugin build/libMultiInstructionOptimization.so \
		-passes=algebraic-identity,strength-reduction,multi-instruction-optimization \
		test/${FILE} \
		-o test/${FILE_NO_EXT}.optimized.bc

build/libAlgebraicIdentity.so: AlgebraicIdentity.cpp
	make build_passes

build/libStrengthReduction.so: StrengthReduction.cpp
	make build_passes

build/libMultiInstructionOptimization.so: MultiInstructionOptimization.cpp
	make build_passes

build_passes:
	mkdir -p build && \
	cd build && \
	export LLVM_DIR=/usr/bin && \
	cmake -DLT_LLVM_INSTALL_DIR=$$LLVM_DIR .. && \
	make