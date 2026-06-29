clang-19 -S -O0 -emit-llvm -Xclang -disable-O0-optnone Example.c -o Example.ll
opt-19 -p mem2reg Example.ll -So Example.m2r.ll