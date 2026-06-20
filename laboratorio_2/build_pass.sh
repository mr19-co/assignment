cd build
export LLVM_DIR=/usr/bin
cmake -DLT_LLVM_INSTALL_DIR=$LLVM_DIR ..
make