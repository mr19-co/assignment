; ModuleID = 'Example.c'
source_filename = "Example.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@g = dso_local global i32 0, align 4

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @g_incr(i32 noundef %0) #0 {
  %2 = alloca i32, align 4
  store i32 %0, ptr %2, align 4
  %3 = load i32, ptr %2, align 4
  %4 = load i32, ptr @g, align 4
  %5 = add nsw i32 %4, %3
  store i32 %5, ptr @g, align 4
  %6 = load i32, ptr @g, align 4
  ret i32 %6
}

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @loop(i32 noundef %0, i32 noundef %1, i32 noundef %2) #0 {
  %4 = alloca i32, align 4
  %5 = alloca i32, align 4
  %6 = alloca i32, align 4
  %7 = alloca i32, align 4
  %8 = alloca i32, align 4
  %9 = alloca i32, align 4
  %10 = alloca i32, align 4
  %11 = alloca i32, align 4
  %12 = alloca i32, align 4
  %13 = alloca i32, align 4
  %14 = alloca i32, align 4
  store i32 %0, ptr %4, align 4
  store i32 %1, ptr %5, align 4
  store i32 %2, ptr %6, align 4
  store i32 0, ptr %8, align 4
  store i32 123, ptr %9, align 4
  store i32 456, ptr %10, align 4
  %15 = load i32, ptr %4, align 4
  store i32 %15, ptr %7, align 4
  br label %16

16:                                               ; preds = %40, %3
  %17 = load i32, ptr %7, align 4
  %18 = load i32, ptr %5, align 4
  %19 = icmp slt i32 %17, %18
  br i1 %19, label %20, label %43

20:                                               ; preds = %16
  %21 = load i32, ptr %9, align 4
  %22 = load i32, ptr %10, align 4
  %23 = add nsw i32 %21, %22
  store i32 %23, ptr %11, align 4
  store i32 0, ptr %14, align 4
  br label %24

24:                                               ; preds = %36, %20
  %25 = load i32, ptr %14, align 4
  %26 = icmp slt i32 %25, 5
  br i1 %26, label %27, label %39

27:                                               ; preds = %24
  %28 = load i32, ptr %11, align 4
  %29 = load i32, ptr %9, align 4
  %30 = add nsw i32 %28, %29
  store i32 %30, ptr %12, align 4
  %31 = load i32, ptr %7, align 4
  %32 = load i32, ptr %12, align 4
  %33 = add nsw i32 %31, %32
  store i32 %33, ptr %13, align 4
  %34 = load i32, ptr %6, align 4
  %35 = call i32 @g_incr(i32 noundef %34)
  br label %36

36:                                               ; preds = %27
  %37 = load i32, ptr %14, align 4
  %38 = add nsw i32 %37, 1
  store i32 %38, ptr %14, align 4
  br label %24, !llvm.loop !6

39:                                               ; preds = %24
  br label %40

40:                                               ; preds = %39
  %41 = load i32, ptr %7, align 4
  %42 = add nsw i32 %41, 1
  store i32 %42, ptr %7, align 4
  br label %16, !llvm.loop !8

43:                                               ; preds = %16
  %44 = load i32, ptr %8, align 4
  %45 = load i32, ptr @g, align 4
  %46 = add nsw i32 %44, %45
  ret i32 %46
}

attributes #0 = { noinline nounwind uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Ubuntu clang version 19.1.7 (++20250114103320+cd708029e0b2-1~exp1~20250114103432.75)"}
!6 = distinct !{!6, !7}
!7 = !{!"llvm.loop.mustprogress"}
!8 = distinct !{!8, !7}
