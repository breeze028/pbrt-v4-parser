# Web 消费者的来源与归属

核心 C++ Parser / Scene IR / GeometryUtils 保持独立实现；以下记录适用于可选 `web/` 消费者。

## LightTransport

- 作者与来源：breeze028，用户提供的 `F:\code\LightTransport`。
- 固定 commit：`47a461d267ed61c2c30c5054d98290ab107da581`。
- 用户明确要求复用 CUDA megakernel，并继续要求完成两场景后公开发布。当前发布账号与来源作者均为 `breeze028`。本仓库的 WGSL 移植/新增文件随本项目以 Apache-2.0 发布，保留 LightTransport 来源与作者归属；不为 LightTransport 全仓库或它的第三方代码另行授予许可。
- 未复制 CUDA headers、runtime、场景 Loader 或 TinyBVH；核心 SDK 保持独立。

| 原函数/文件 | 当前对应文件与修改 |
|---|---|
| `src/gpu/math.cuh` 中 PCG 随机数 | `src/shaders/pathtrace.wgsl::random`：u32 溢出与移位；取高 24 bit 构造 `[0,1)`，避免 f32 舍入到 1 |
| `kernel.cuh::render_kernel` | `main`：每像素 seed、整路径；改为 8×8 dispatch，每次 1 spp，累积与显示独立 |
| `shading.cuh::trace_gpu` | `integrator.wgsl::trace`：throughput、emitter 命中、NEE、MIS、RR、delta 与 transmission 状态；最多 12 个额外透射事件 |
| `evaluate_brdf_gpu/material_pdf_gpu/sample_material_gpu` | `bsdf.wgsl`：Lambertian、eta/k Fresnel、GGX conductor、Principled+coat、delta dielectric；另补 rough dielectric 的求值/采样/PDF |
| `estimate_direct_gpu/light_pdf_solid_angle_gpu/mis_weight_gpu` | `trace/light_pdf/power`：均匀选发光三角形、面积转立体角 PDF、NEE 与 BSDF-hit MIS |
| `intersection.cuh`、`ray_query.cuh` | `bounds/triangle/intersect/offset`：自有 BLAS/TLAS 数据结构与迭代栈；局部 ray 不归一化，保留 world t；独立错误计数 |

保留 RR 生存概率 `[.05,.95]`，第 4 个表面事件起启用；透射增加 eta² 的 RR 补偿。保留 primary/secondary 贡献钳制 `64/8`，界面可关闭。MIS 使用缩放比值避免大 PDF 的平方溢出。RNG 消耗顺序、光源选择分布、BVH、偏移、矩阵与显示管线已经重写；不宣称 CUDA 随机序列或逐像素等价。CUDA HDR reference 的真实配置与剩余差异记录在实施报告。

coateddiffuse 沿用 LT 的 Principled + clearcoat 近似：底层 Principled roughness 固定 1，coat roughness 至少 .02；它不是 PBRT layered BSDF，白色材质可能有小幅能量增益。具名银 eta/k 沿用 LT 的 650/550/450 nm RGB 近似。透明阴影采用直线穿越与 Fresnel 衰减，不是折射阴影/焦散。

`cpp/scene.cpp`、TGA/mip decoder、CPU BVH、布局协议、WASM bridge、Worker、Inspector、CPU brute-force probes 是本仓库的新实现。独立 Shape alpha、材质 displacement/bump 和纹理角色由此实现，不从颜色图 A 通道代替 rug_mask。颜色 TGA 默认按 LT 风格 sRGB，float 数据默认 linear；原 encoding 参数保留并优先求值。这与官方 PBRT 对 TGA 的默认 linear 有差异。

## PBRT-v4 微表面公式

rough dielectric、各向异性 GGX visible normal / Jacobian、法线微分与 bump 方程参考固定 PBRT-v4 `b0bdf461ba5dd6504b70ebf0932acc0fe7aa744f` 的 `src/pbrt/bxdfs.cpp`、`util/scattering.h`、`materials.h`，重新写成 WGSL。pbrt is Copyright(c) 1998-2020 Matt Pharr, Wenzel Jakob, and Greg Humphreys；[原 Apache-2.0 许可](licenses/pbrt-v4-Apache-2.0.txt)一并保留。

## webgpu-pages-starter

- 来源：`F:\code\webgpu-pages-starter`，commit `468bee0d5f3527d7523f826cf4c64872f70d3385`。
- Copyright (c) 2026 breeze028；[完整 MIT 许可](licenses/webgpu-pages-starter-MIT.txt)。
- `.agents/skills/webgpu-github-pages/` 完整保留原 skill、参考和模板，另附原 MIT LICENSE；不包含 starter 的立方体 renderer。
- 本仓库采用其 base / Worker / WASM 资源路径，以及工程、静态 HTTP、真实 GPU、线上分层验证方法。`web-ci.yml` 与 `deploy-pages.yml` 分开，发布只上传 `web/dist`。

## 场景与工具链

场景为原作者发布的 CC0，原始 LICENSE 随资源包保留，见 [scenes/README](../scenes/README.md)。打包白名单明确排除 `.lt` 与 `.ivol`。Emscripten 7.0.0、其 libc++ / musl notices 随静态产物保留在 `licenses/`。Node、Vite、TypeScript 等构建工具源码不打入站点；SDK 默认构建不依赖这些工具。
