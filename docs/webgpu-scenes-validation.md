# Spaceship / Bathroom 实施与验证

本记录接续 [首批 Cornell / WASM 基线](webgpu-implementation.md)。日期：2026-11-15，Asia/Shanghai。发布状态与线上证据将在实际部署完成后补齐；本机证据不代替线上或用户人工验收。

## 当前实现

两个原始 `scene-v4.pbrt` 均由同一 C++ SDK / WASM 解析，原文件没有为 renderer 改写。Tiny 仍保持 Cornell 范围；新增能力只进入可选 `web/` 消费者。

| 项目 | Spaceship | Bathroom |
|---|---:|---:|
| 原始 triangles | 457,560 | 592,188 |
| vertices | 368,377 | 362,068 |
| placements | 91 | 856 |
| BVH nodes / 最大深度 | 273,954 / 16 | 364,427 / 15 |
| materials | 15 | 33 |
| 原始 texture 声明 / GPU 角色副本 | 0 / 0 | 12 / 11 |
| image mip 数据字节 | 0 | 87,013,388 |
| 解析/适配诊断 | 0 | 0 |

字节数是传输数据，不是驱动实际显存统计。Bathroom 的 floor_tiles 同时作为颜色与数据；wood 重复声明可以复用相同角色。10 个 TGA 因此不是 10 个 GPU texture。原始 rug 的 A 通道为 opaque，不从它代替 `rug_mask`；alpha 绑定独立 float texture。

材质已覆盖 diffuse、conductor、coateddiffuse、dielectric。新增 rough dielectric，保留原 .01 roughness，区分连续与 delta PDF、Fresnel/TIR、反射/折射 Jacobian，保留最多 12 个额外透射事件。coateddiffuse 采用明确的 LT 近似，银用具名 RGB 近似；限制见 [Web README](../web/README.md#明确的近似与边界)。

## 工程与数据

- Windows / MSVC 原生 **38/38**：原 SDK / Geometry / Tiny 回归、两个大型场景、web scene 与 image contract。
- Emscripten **7.0.0** 实际编译；三场景 native/WASM **7/7** 通过。虚拟根目录规范化；typed parameters、引用、资源路径、scope、诊断均严格比较。派生 double Transform 允许 `32×epsilon×max(1,abs(a),abs(b))` 的跨 libm 舍入差；五个 GPU 数组仍逐字节比较。另有比较器回归，确保不能把该容差用于原始数值或引用 ID。
- 异常检查包含坏 PBRT / 缺文件 / 作用域错误、坏 TGA 后恢复、Import 隔离、共享 BLAS 与 handle 关闭后不可访问。TGA 测试涵盖 top/bottom/right origin、BGR、RLE、越界/截断、RGB scalar 与 alpha、linear/sRGB mip、NPOT 边缘。
- 类型检查、生产构建，以及 `/renamed-project/` 的三个包、两个 Worker、WGSL、WASM MIME/compile、每文件 SHA256、LICENSE 检查已实际通过。
- 可重建原始资产包已在空目录完整校验：Spaceship **90 文件 / 6,968,024 B gzip**，Bathroom **867 文件 / 36,309,416 B gzip**。锁与下载脚本保留根 PBRT / LICENSE hash，拒绝覆盖修改过的资产，排除 `.lt` / `.ivol`。

## GPU 检查的范围

浏览器实际报告 NVIDIA / Ampere adapter。自检使用真实 compute dispatch 与异步回读：

- 6 条射线：GPU BVH 与独立 CPU 世界空间 brute-force 的命中 ID、t、重心坐标、几何及原始插值法线，容差 `1e-3`。CPU 检查在单独 Worker，不阻塞 Inspector。
- 28,681 个 BSDF 查询：解析 Lambert/Fresnel、delta/TIR、rough glass、各向异性、f*cos/pdf、PDF、Fresnel 事件频率、数值积分和 4×4096 个能量抽样；另有大/小/零 PDF 的 MIS 数值检查。涂层测试明确允许 LT 近似的能量增益，不称为严格能量守恒。
- Bathroom 220 个图像取样：CPU 从原 mip 字节独立做 sRGB 解码、repeat、bilinear/trilinear，比较实际 GPU texture。另有 3 个解析 bump 法线输入；最大颜色取样误差 `0.004113`，阈值 `0.006`。
- 自检同时读取实际累积的 stack / material / nonfinite / transparency 计数，不用隔离测试 buffer 冒充渲染诊断。

早期长时间累积捕获非有限贡献。原 MIS `a²/(a²+b²)` 对极大 PDF 可溢出，现先除以最大 PDF 再计算比值，另处理退化法线；极端数值加入 GPU 检查。修复后 Spaceship 在生产子路径以 128×72、10,000 spp 完成累积，四项诊断均为 0，Console 无警告/错误。384×216、256 spp 的三个独立 seed 均导出有限、非负的线性 HDR。实际画面见 [Spaceship](assets/webgpu-spaceship.jpg)。

Bathroom 在 256×256 / 1024 spp、depth=17 完成原始视角累积，保留全部纹理、alpha 和 bump，见 [实际画面](assets/webgpu-bathroom.jpg)。256 spp HDR 导出全部有限、非负；与 CUDA 64 spp 对照的线性均值为 Web `(2.19290, 2.16030, 2.14518)`、CUDA `(2.21773, 2.19129, 2.17898)`，RMSE `1.18897`、MAE `0.47971`，受噪声和下述差异影响，不作等价性阈值。

## LightTransport CUDA 对照

从只读源码 `47a461d267ed61c2c30c5054d98290ab107da581` 在本仓库忽略的 `.reference/lt-build` 构建，CUDA **12.4.99 / sm86**，MSVC **19.38.33141**。run-manifest 确认 actual backend=`cuda-megakernel`、无 fallback，设备为 RTX 3060 Laptop GPU。

- Spaceship：384×216，256 spp，depth=32，MIS，denoiser/AA/IVol/ReSTIR off，成功输出线性 PFM。
- Bathroom：256×256，64 spp，depth=17，同上；bounded=8 的透明阴影预算触发 15,902 次耗尽，strict 正确失败。改用来源自带 reference=64 后成功，stack / transparency / queue 为 0。失败输出未作参考图。

另做 Spaceship 三个 Web seed（各 256 spp）与 CUDA 三帧（共 768 spp）的均值对照：Web RGB 均值 `(0.08833, 0.08508, 0.08884)`，CUDA `(0.08669, 0.08319, 0.08740)`，线性 RMSE `0.05074`、MAE `0.01024`。此为带噪描述性对照，不是等价性通过门槛；CUDA box/AA-off 与 Web PBRT triangle filter、材质和阴影策略均有已知差别。

参考数据与 manifest 位于本机 `out/webgpu/reference/`，原 LightTransport 工作树未修改。Web 的 rough dielectric、独立 alpha / bump、角色 mip、采样与数值保护是新增/修改路径；不能因为图像相似便声称 CUDA 或官方 PBRT 等价。PFM 用线性 RGB 检查，显示图采用相同曝光/tonemap；噪声和已记录的估计器差异不作逐像素确定性门槛。

相机移动、材质开关和 reset 会恢复累积，即使上一轮达到目标 spp 自动暂停。浴室地面视角以同一 seed / 64 spp 导出开关对照，开启和关闭 alpha+bump 的 HDR 均有限，65,536 像素中 33,518 个发生变化；这是功能开关证据，不是像素真值或两项效果的独立误差证明。Inspector 已实际展开原始 float 贴图与线性数据缩略图。

## 发布与验收边界

Pages 使用 [指定 skill](../.agents/skills/webgpu-github-pages/SKILL.md)，完整 MIT 归属保留。生成物只上传 `web/dist`，大型原始资产以固定 Release + SHA256 分发；不提交 WASM、模型、node_modules 或本地参考工程。

本机画面、自检、工程/HTTP 与线上分别记录，不以 CI 或 HTTP 绿灯代替 GPU 画面。以上是 Agent 的观察与自动验证，不是用户人工验收，没有跨设备性能或完整 PBRT 工厂覆盖承诺。
