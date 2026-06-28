./build_pass.sh
echo '================================================================================'
opt-19 -load-pass-plugin build/libLoopPass.so -passes=loop-pass Loop.m2r.ll -disable-output
