; ModuleID = 'Example.ll'
source_filename = "Example.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@g = dso_local global i32 0, align 4

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @g_incr(i32 noundef %0) #0 {
  %2 = load i32, ptr @g, align 4
  %3 = add nsw i32 %2, %0
  store i32 %3, ptr @g, align 4
  %4 = load i32, ptr @g, align 4
  ret i32 %4
}

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @loop(i32 noundef %0, i32 noundef %1, i32 noundef %2) #0 {
  br label %4

4:                                                ; preds = %17, %3
  %.01 = phi i32 [ %0, %3 ], [ %18, %17 ]
  %5 = icmp slt i32 %.01, %1
  br i1 %5, label %6, label %19

6:                                                ; preds = %4
  %7 = add nsw i32 123, 456
  br label %8

8:                                                ; preds = %14, %6
  %.0 = phi i32 [ 0, %6 ], [ %15, %14 ]
  %9 = icmp slt i32 %.0, 5
  br i1 %9, label %10, label %16

10:                                               ; preds = %8
  %11 = add nsw i32 %7, 123
  %12 = add nsw i32 %.01, %11
  %13 = call i32 @g_incr(i32 noundef %2)
  br label %14

14:                                               ; preds = %10
  %15 = add nsw i32 %.0, 1
  br label %8, !llvm.loop !6

16:                                               ; preds = %8
  br label %17

17:                                               ; preds = %16
  %18 = add nsw i32 %.01, 1
  br label %4, !llvm.loop !8

19:                                               ; preds = %4
  %20 = load i32, ptr @g, align 4
  %21 = add nsw i32 0, %20
  ret i32 %21
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
