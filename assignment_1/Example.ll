define dso_local i32 @foo(i32 noundef %0, i32 noundef %1) #0 {
  %3 = add nsw i32 %1, 0
  %4 = mul nsw i32 %3, 1

  %5 = mul nsw i32 %0, 15
  %6 = mul nsw i32 %0, 17
  %7 = sdiv i32 %0, 8
  %8 = udiv i32 %0, 8

  ret i32 %4
}