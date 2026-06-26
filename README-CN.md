# Mini Reactive Verification & Synthesis（迷你反应式验证与综合）

**从零开始、零依赖的 C 语言实现**，涵盖反应式系统验证与综合的大学级理论。每个模块对应 MIT（及其他顶尖大学）的一门或多门课程，将教科书中的算法转化为可运行的 C 代码，实现理论与实践的桥接——从 LTL 模型检测、Büchi 自动机到 GR(1) 综合与基于 OBDD 的符号化验证。

## 子模块总览

| 子模块 | 主题 | 参考课程 |
|-----------|--------|-------------|
| [mini-buchi-automata-on-words](mini-buchi-automata-on-words/) | 无穷字上的非确定性 Büchi 自动机、空虚性检测（SCC/嵌套 DFS）、ω-自动机接收条件（Büchi, Rabin, Streett, Parity, Muller）、并/交/投影/补运算下的封闭性 | MIT 6.841, Stanford CS254 |
| [mini-computation-tree-logic-ctl](mini-computation-tree-logic-ctl/) | CTL 语法与 AST、公式等价性与范式、Kripke 结构表示、显式状态模型检测（Clarke-Emerson-Sistla 算法）、可满足性与有效性判定 | MIT 6.841, CMU 15-414 |
| [mini-grg1-synthesis-algorithm](mini-grg1-synthesis-algorithm/) | GR(1) 规范（初始/安全/公平性）、二人博弈图构造、嵌套不动点计算（νX.μY 迭代）、获胜策略提取 | MIT 6.841, Stanford CS254 |
| [mini-linear-temporal-logic-ltl](mini-linear-temporal-logic-ltl/) | LTL 语法与 AST（Next, Until, Release, Weak Until）、代数等价与展开律、规范模式库（Dwyer 等 1999）、迹与 Kripke 语义 | MIT 6.841, Stanford CS254 |
| [mini-ltl-to-buchi-translation](mini-ltl-to-buchi-translation/) | LTL 到 NBA 的翻译算法（基于 tableau、Gerth 等、即时构造）、Büchi 自动机构造与优化 | MIT 6.841, Stanford CS254 |
| [mini-omega-regular-properties](mini-omega-regular-properties/) | ω-词与 ω-正则语言、ω-正则表达式、无穷字上的 Büchi 自动机、LTL 公式表示、ω-正则语言的封闭性质 | MIT 6.841, Stanford CS254 |
| [mini-reactive-synthesis-pnueli](mini-reactive-synthesis-pnueli/) | 完整 LTL 综合流水线（LTL→NBA→DPA→奇偶博弈→策略）、GR(1) 综合、安全/可达性/Büchi/奇偶博弈求解、反应式模块 Mealy 机提取 | MIT 6.841, Stanford CS254 |
| [mini-symbolic-model-checking-obdd](mini-symbolic-model-checking-obdd/) | ROBDD 规范表示（Bryant 1986）、CTL 符号化模型检测、状态集格上的最小/最大不动点计算、Kripke 结构的符号化编码 | MIT 6.841, CMU 18-760 |

## 设计理念

- **零外部依赖** — 纯 C（C99/C11），仅使用 `libc` 和 `libm`
- **模块自包含** — 每个目录自带 `Makefile`、`include/`、`src/`、`examples/`、`demos/`、`tests/`
- **理论到代码的映射** — 每个模块包含 `docs/` 目录，内有课程对齐说明与证明概要
- **实用演示程序** — 模型检测器、空虚性检测器、综合引擎、规范模式验证器

## 构建方式

每个模块相互独立。进入模块目录后运行：

```bash
cd mini-buchi-automata-on-words
make all    # 构建全部
make test   # 运行测试
```

需要 **GCC** 和 **GNU Make**。

## 项目结构

```
mini-reactive-verification-synthesis/
├── mini-buchi-automata-on-words/     # 无穷字上的 Büchi 自动机
├── mini-computation-tree-logic-ctl/  # 计算树逻辑（CTL）
├── mini-grg1-synthesis-algorithm/    # GR(1) 反应式综合
├── mini-linear-temporal-logic-ltl/   # 线性时序逻辑（LTL）
├── mini-ltl-to-buchi-translation/    # LTL 到 Büchi 的翻译
├── mini-omega-regular-properties/    # ω-正则性质与语言
├── mini-reactive-synthesis-pnueli/   # 反应式综合（Pnueli 框架）
└── mini-symbolic-model-checking-obdd/ # 基于 OBDD 的符号化模型检测
```

## 许可证

MIT
