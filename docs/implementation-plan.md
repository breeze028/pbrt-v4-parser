# 实施与验收记录

按 M0 → M1 → M2 → M3 → M4 → M5 实施，每阶段以实际构建与测试记录更新。

| 阶段 | 交付 | 验收 |
|---|---|---|
| M0 | 调研、独立 Git/CMake、架构与来源记录 | 基础工程可配置构建 |
| M1 | Tokenizer、全部 v4 指令语法、typed parameters、callback、Include/Import 文件事件 | 正例、类型/词法/语法错误测试 |
| M2 | Scene IR、图形状态、动画端点、引用、Import、实例 | 状态/引用/路径集成测试 |
| M3 | 独立 Geometry target、PLY、Tiny CPU path tracer | SDK 加载和 PPM 图像 |
| M4 | Emscripten、Inspector、WebGPU raster | WASM 构建和真实浏览器观测 |
| M5 | 官方场景回归、安装消费、Windows/Linux CI、发布检查 | 不能用配置文件代替 CI 运行证据 |

本执行周期优先交付经过验证的核心。最终阶段状态与具体证据见本文末尾；没有实现或没有运行的项明确记录为未完成。
