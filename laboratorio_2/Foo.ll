; int foo(int e, int a) {
;   int b = a + 1;
;   int c = b * 2;
;   b = e << 1;
;   int d = b / 4;
;   return c * d;
; }

define dso_local i32 @foo(i32 noundef %0, i32 noundef %1) #0 {
  %3 = add nsw i32 %1, 1    ; V = %1 + 1
  %4 = mul nsw i32 %3, 2    ; Y = V * 2

  %5 = shl i32 %0, 1        ; W = %0 << 1
  %6 = sdiv i32 %5, 4       ; X = W / 4
  %7 = mul nsw i32 %4, %6   ; Z = Y * X
  ret i32 %7                ; return Z
}