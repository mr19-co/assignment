define dso_local i32 @foo(i32 noundef %0, i32 noundef %1) #0 {
  %3 = add nsw i32 %1, 0
  %4 = mul nsw i32 %3, 1

  %5 = mul nsw i32 %0, 15
  %6 = mul nsw i32 %0, 16
  %7 = mul nsw i32 %0, 17
  %8 = sdiv i32 %0, 8
  %9 = udiv i32 %0, 8
  %a = udiv i32 %0, 7

  %10 = add nsw i32 %1, 5
  %11 = sub nsw i32 %10, 7
  %12 = add nsw i32 %1, 7
  %13 = sub nsw i32 %12, 5

  ret i32 %13
}