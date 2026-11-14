# Web 消费者的来源与归属

核心 C++ Parser / Scene IR / GeometryUtils 保持独立实现；以下记录适用于可选 `web/` 消费者。

## LightTransport

- 作者与来源：breeze028，用户提供的 `F:\code\LightTransport`。
- 固定 commit：`47a461d267ed61c2c30c5054d98290ab107da581`。
- 用户明确要求复用 CUDA megakernel。当前只移植 diffuse 路径所需算法，未复制 CUDA headers、runtime、场景 Loader 或 TinyBVH。
- 本地来源根目录没有 LICENSE。此记录保留作者与来源，并记录本次复用指令；不替 LightTransport 或它的第三方代码声明 Apache-2.0。公开分发派生 shader 前，应由权利人明确派生文件的许可；当前没有推送或发布。

| 原函数/文件 | 当前对应文件与修改 |
|---|---|
| `src/gpu/math.cuh` 中 PCG 随机数 | `src/shaders/pathtrace.wgsl::random`：u32 溢出与移位；取高 24 bit 构造 `[0,1)`，避免 f32 舍入到 1 |
| `kernel.cuh::render_kernel` | `main`：每像素 seed、整路径；改为 8×8 dispatch，每次 1 spp，累积与显示独立 |
| `shading.cuh::trace_gpu` | `trace`：throughput、emitter 命中、NEE、power MIS、RR；当前只有 diffuse，没有 delta / transmission 分支 |
| `evaluate_brdf_gpu/material_pdf_gpu/sample_material_gpu` 的 diffuse 分支 | Lambertian 与 cosine hemisphere sampling；暂未移植 GGX、coat、glass |
| `estimate_direct_gpu/light_pdf_solid_angle_gpu/mis_weight_gpu` | `trace/light_pdf/power`：均匀选发光三角形、面积转立体角 PDF、NEE 与 BSDF-hit MIS |
| `intersection.cuh`、`ray_query.cuh` | `bounds/triangle/intersect/offset`：自有 BLAS/TLAS 数据结构与迭代栈；局部 ray 不归一化，保留 world t；独立错误计数 |

保留 RR 生存概率 `[.05,.95]`，第 4 个表面事件起启用；保留 primary/secondary 贡献钳制 `64/8`，界面可关闭并重置累积。RNG 消耗顺序、光源选择分布、BVH、偏移、矩阵与显示管线已经重写；没有宣称 CUDA 随机序列或逐像素等价。尚未采集同设置的 CUDA HDR reference。

`cpp/scene.cpp`、CPU BVH、布局协议、WASM bridge、Worker、Inspector、CPU brute-force probes 是本仓库的新实现。PBRT Sampler / Accelerator 原参数保留在 Inspector，但执行使用 PCG / 自有 BVH；Film 分辨率按比例降低为可选预览最长边。PBRT 色谱与其他材质参数尚未求值，遇到这些输入会阻止开始渲染。

## webgpu-pages-starter

- 来源：`F:\code\webgpu-pages-starter`，commit `468bee0d5f3527d7523f826cf4c64872f70d3385`。
- Copyright (c) 2026 breeze028；[完整 MIT 许可](licenses/webgpu-pages-starter-MIT.txt)。
- `.agents/skills/webgpu-github-pages/` 完整保留原 skill、参考和模板，另附原 MIT LICENSE；不包含 starter 的立方体 renderer。
- 本仓库采用其 base / Worker / WASM 资源路径与工程、静态 HTTP、真实 GPU、线上分层验证方法。新增 `web-ci.yml` 只做工程和静态产物检查；没有启用线上发布。

## 场景与工具链

场景的原始 LICENSE 随资源包保留，见 [scenes/README](../scenes/README.md)。打包白名单明确排除 `.lt` 与 `.ivol`。Emscripten 7.0.0 及 npm 包按各自许可证使用；SDK 默认构建不依赖这些工具。
