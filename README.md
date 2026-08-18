# Kaleidoscope Compiler Frontend

参考 LLVM Kaleidoscope 教程开发的编译器前端，支持将 Kaleidoscope toy 语言源码编译为 LLVM IR，并可选生成目标文件。

## 技术栈

- C++20 / CMake 4.2+
- LLVM 22
- 递归下降语法分析 + Pratt 运算符优先级解析

## 已实现特性

- [x] 基础表达式：变量、数字、加减乘除以及比较运算符
- [x] 控制流：`if/then/else`、`for/in` 循环
- [x] 函数定义与调用：`def`、`extern`、匿名顶层表达式
- [x] 局部变量：`var` 块级作用域
- [x] 自定义运算符优先级：`binary` / `unary` 声明
- [x] LLVM IR 生成与优化（InstCombine / GVN / SimplifyCFG）
- [x] DWARF Debug 信息生成
- [x] 目标文件生成（.o）

## 语言示例

```kaleidoscope
# 函数定义
def add(a b) a + b;

# 自定义运算符
# 自定义一元按位取反（近似）
def unary ~ (v) -v - 1;

# 局部变量
def testVar()
    var x = 3, y = 4 in x + y;
```

## 项目结构

```
src/
├── main.cpp              # 入口：初始化 LLVM Context、注册内置算符、启动主循环
├── common.hpp            # 跨模块工具（debug 宏等）
├── lexer/
│   ├── char_stream.hpp   # 字符流抽象接口
│   ├── input_char_stream.{hpp,cpp} # 封装 stdin 实现的charStream接口
│   ├── token.hpp         # Token 类型定义（std::variant）
│   └── token_stream.{hpp,cpp}  # 词法分析器
├── parser/
│   ├── ast.hpp           # AST 节点声明（ExprAst / PrototypeAst / DefinitionAst）
│   └── parser.{hpp,cpp}  # 递归下降解析器
└── codegen/
    ├── context.{hpp,cpp} # CodeGenContext：LLVM 核心对象、符号表、Debug 信息
    └── codegen.cpp       # AST → LLVM IR 翻译实现
```

## 构建

```bash
# 预先安装llvm库，archlinux上可以使用`pacman -S llvm`
cmake -B build -S .
cmake --build build
```

## 运行示例

```bash
$ echo 'def fib(x) if x < 3 then 1 else fib(x-1)+fib(x-2)' | ./build/practice_Kaleidoscope
```

输出 LLVM IR

<pre>
<details>
<summary> 查看完整的 LLVM IR 输出 </summary>

; ModuleID = 'code.txt'
source_filename = "code.txt"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

define double @fib(double %x) #0 !dbg !4 {
entry:
  %x1 = alloca double, align 8
    #dbg_declare(ptr %x1, !9, !DIExpression(), !10)
  store double %x, ptr %x1, align 8
  %0 = load double, ptr %x1, align 8, !dbg !11
  %cmptmp = fcmp ult double %0, 3.000000e+00, !dbg !12
  %booltmp = uitofp i1 %cmptmp to double, !dbg !12
  %ifcond = fcmp one double %booltmp, 0.000000e+00, !dbg !12
  br i1 %ifcond, label %thenBB, label %elseBB, !dbg !12

thenBB:                                           ; preds = %entry
  br label %ifcont, !dbg !13

elseBB:                                           ; preds = %entry
  %1 = load double, ptr %x1, align 8, !dbg !14
  %2 = fsub double %1, 1.000000e+00, !dbg !15
  %calltmp = call double @fib(double %2), !dbg !16
  %3 = load double, ptr %x1, align 8, !dbg !17
  %4 = fsub double %3, 2.000000e+00, !dbg !18
  %calltmp2 = call double @fib(double %4), !dbg !19
  %5 = fadd double %calltmp, %calltmp2, !dbg !19
  br label %ifcont, !dbg !19

ifcont:                                           ; preds = %elseBB, %thenBB
  %iftemp = phi double [ 1.000000e+00, %thenBB ], [ %5, %elseBB ], !dbg !19
  ret double %iftemp, !dbg !19
}

attributes #0 = { "frame-pointer"="all" }

!llvm.module.flags = !{!0, !1}
!llvm.dbg.cu = !{!2}

!0 = !{i32 2, !"Debug Info Version", i32 3}
!1 = !{i32 2, !"Dwarf Version", i32 4}
!2 = distinct !DICompileUnit(language: DW_LANG_C, file: !3, producer: "Kaleidoscope Compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!3 = !DIFile(filename: "code.txt", directory: ".")
!4 = distinct !DISubprogram(name: "fib", scope: !3, file: !3, line: 1, type: !5, scopeLine: 1, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !2, retainedNodes: !8)
!5 = !DISubroutineType(types: !6)
!6 = !{!7, !7}
!7 = !DIBasicType(name: "double", size: 64, encoding: DW_ATE_float)
!8 = !{!9}
!9 = !DILocalVariable(name: "x", arg: 1, scope: !4, file: !3, line: 1, type: !7)
!10 = !DILocation(line: 1, column: 9, scope: !4)
!11 = !DILocation(line: 1, column: 15, scope: !4)
!12 = !DILocation(line: 1, column: 19, scope: !4)
!13 = !DILocation(line: 1, column: 26, scope: !4)
!14 = !DILocation(line: 1, column: 37, scope: !4)
!15 = !DILocation(line: 1, column: 39, scope: !4)
!16 = !DILocation(line: 1, column: 33, scope: !4)
!17 = !DILocation(line: 1, column: 46, scope: !4)
!18 = !DILocation(line: 1, column: 48, scope: !4)
!19 = !DILocation(line: 1, column: 42, scope: !4)

</details>
</pre>
## 待改进

- [ ] 支持从文件读取源码，替代硬编码的 stdin 与 "code.txt"
- [ ] 引入多种类型支持。
- [ ] 引入单元测试框架对 Parser 做单元测试
