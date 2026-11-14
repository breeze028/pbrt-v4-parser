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

## 2026-11-15 本轮交付

| 阶段 | 状态 | 实际结果 |
|---|---|---|
| M0 | 完成 | 五个参考项目与官方格式已研究；固定 commit、内部依赖和许可证处理记录在 upstream.md；独立 Git、CMake、Apache-2.0 |
| M1 | 完成当前格式基础层 | 覆盖当前 v4 指令语法和 typed parameters；公开 Tokenizer/Callback；内存/文件/VFS、源位置、错误、UTF-8、Include、Import 边界 |
| M2 | 完成初版核心 | Graphics State、五类 Attribute 默认参数、后乘/相机/动画端点、对象定义、共享实例、具名引用、介质、面积光、Import 隔离、原始参数与资源路径 |
| M3 | 完成基础验收 | 独立 geometry target；ASCII/LE/BE PLY、TriangleMesh、显式整球离散化、共享 placement、静态烘焙；约 220 行 CPU 示例、BVH、面积光/常量环境光、PPM、CLI |
| M4 | **未实现** | 没有用网页占位或 TypeScript parser 冒充 WASM/WebGPU；当前环境未配置 emcc，也未执行浏览器验收 |
| M5 | 部分完成 | README/API/兼容性/来源文档、安装导出、官方回归脚本、Windows/Linux + sanitizer CI 配置；远程 CI、Linux 实跑、sanitizer 实跑、发布均未完成 |

所有“完成”均限于 Compatibility 文档注明的层次与范围，不表示所有 PBRT 实体工厂或渲染算法已经实现。

## 构建、测试与证据

以下全部在 Windows 本机执行，不把 MinGW/GCC 当作 Linux 验证。

| 配置 | 命令 | 结果 |
|---|---|---|
| MSVC 19.38 Debug 静态库 | `cmake --preset msvc` / `cmake --build --preset msvc` / `ctest --preset msvc` | 30/30 通过 |
| GCC 10.3 Debug 静态库 | Ninja + `CMAKE_CXX_COMPILER=C:/TDM-GCC-64/bin/g++.exe` | 30/30 通过 |
| MSVC Release 共享库 | `BUILD_SHARED_LIBS=ON`, `ctest --test-dir build-shared -C Release` | 30/30 通过 |
| GCC Release 核心独立配置 | `PBRT4_BUILD_GEOMETRY=OFF` | 20/20 核心测试通过；未链接 geometry |
| 安装包消费 | 安装到 `out/install` 后配置 `tests/consumer` 的 find_package 模式 | consumer 编译、链接、运行通过 |
| add_subdirectory 消费 | `tests/consumer -DPBRT4_SOURCE=...` | consumer 编译、链接、运行通过 |
| FetchContent 消费 | 同上另加 `-DPBRT4_FETCHCONTENT=ON` | 本地 SOURCE_DIR 消费通过，未宣称远程下载已测试 |

完整本机 CTest 日志在各构建目录的 `verification.log` 与 `Testing/Temporary/LastTest.log`，不纳入 Git。

测试覆盖 Tokenizer/转义/位置/非法 UTF-8、全部指令事件、全部参数类别、错误类型/数组/矩阵、CTM 顺序、相机求逆、Attribute、动画端点、对象共享、具名引用、根目录路径、Include 环与深度、Import 状态/名称/坐标系隔离、导入当前对象、介质与光谱、12 次并发加载、固定种子的 1,500 个变异输入。几何测试覆盖属性不伪造、大小端 PLY、截断与坏索引、球绕序、逆转置法线、负缩放、静态烘焙与动画拒绝。Tiny 的测试包含 Cornell、外部 PLY 实例 + 环境光，以及不支持材质的失败路径。

### 官方描述回归

`tools/official_regression.py` 固定 `mmp/pbrt-v4-scenes@30cf4a0346ae5a80a2d7a530a3ef7d0fa4f70572`，下载 5 个文件，总计 292,899 字节。四个根描述通过，诊断均为 0：

| 场景 | Shape 数 | 额外检查 |
|---|---:|---|
| killeroo-simple | 5 | Include 的 loopsubdiv 完整参数保留 |
| killeroo-moving | 5 | 2 个 animated Shape 端点保留 |
| ganesha | 4 | imagemap、coateddiffuse、PLY.gz 引用保留 |
| head | 1 | subsurface、纹理引用与相机参数保留 |

这是 Parser/Scene IR 证据。没有下载这些场景的外部图片/PLY，没有把它们宣称为 Tiny 的渲染通过场景。gzip **描述文件**读取仍报错；PLY.gz **资源路径**能保留，不代表能够解压。

### 实际图像

`testdata/cornell.pbrt` 通过公开 API 加载为 7 个 Shape、1 个共享对象、2 个实例；6 个世界 Shape + 两个球体 placement，显式离散化后消费者构建 1,932 个三角形、1,023 个 BVH 节点。实际输出并查看了 96×96 / 16 spp，以及 192×192 / 64 spp 的 PPM 转 PNG 图像，能观察到房间、面积光及两球。后者位于 `out/cornell-preview.*`，属于本地预览，不进入 Git。

## 下一轮明确入口

当前以 [WebGPU Renderer 计划](webgpu-renderer-plan.md) 为准：用户指定复用 LightTransport CUDA megakernel，发布参考 `webgpu-pages-starter` 的 skill。顺序更新为 W0 移植契约 → W1 单线程 WASM/数据布局/资源 → W2 Cornell Compute Path Tracer → W3 Spaceship → W4 Bathroom → W5 Inspector 完善与 Pages 验收；Inspector 自 W1 起逐步接入，不再要求先完成独立 Raster Viewer。

原有核心库与 Tiny 的已完成项保持关闭。此轮完成源码调研和计划文档，没有启动 WASM/WebGPU 实现、运行浏览器验收或部署。

当前未创建远程仓库、未公开发布、未设置版本 tag；LightTransport 工作树保持原状。Git 仅包含 SDK 源码、文档、CI 和小型原创测试数据。

## 后续：Bathroom / Spaceship 本地示例检查

按用户指定，已将两个 PBRT 场景及 models/textures/LICENSE 复制到 `scenes/`，不复制任何 `.lt`。大资源在本机存在但被 Git 忽略；根描述、许可证和复制工具进入版本控制。

真实场景发现并修复了此前只支持 `normal3` 而遗漏官方 `normal` 的错误。修正后两场景均完整解析并加载全部几何，分别为 592,188 / 457,560 个三角形，缺少资源 0；Tiny 原样执行仍因滤波器参数拒绝渲染，另有材质、纹理等消费者缺口。不能将加载成功当作渲染验收。

本次默认测试 MSVC Debug / MSVC Release shared / Windows GCC Debug 均为 33/33；两项可选大型场景 CTest 加载回归均通过。关闭 Geometry 的配置为 22/22。完整分层结论、来源哈希、渲染缺口和复现命令见 [检查报告](example-scenes-validation.md)。

## 当前范围：Tiny 验收 Cornell Box

用户将 Tiny Renderer 的主要验收目标限定为 `F:\code\LightTransport\scenes\cornell-box`；Bathroom / Spaceship 的渲染支持留到 WebGPU 阶段考虑，当前仍只做加载回归。无需继续为 Tiny 扩展复杂材质和纹理。

已将 Cornell 的原始 `scene-v4.pbrt` 和 `LICENSE.txt` 复制至 `scenes/cornell-box/`（共 7,763 字节），未复制 `.ivol`、已有 EXR/PNG 或任何 `.lt`。源与复制件 PBRT SHA-256 均为 `B2FC6654C966468119187D169A76ECEC82E169A1058F3344E27E5103189D4E43`。随包许可证记载原作者 Benedikt Bitterli、CC0，原文保留。

原场景全部为 diffuse、内联 TriangleMesh 和面积光，无外部资源。实际加载得到 8 个 mesh、72 个顶点、36 个三角形、9 个材质（含默认）、1 个面积光声明，诊断 0。

Tiny 为该场景新增 separable triangle filter：以逆 CDF 重要性采样 `max(0, radius - abs(offset))` 核，支持 x/y radius，不改写 Parser/IR。深度上限从 64 提高到 1024，接受原始 maxdepth=65，不进行截断。新增可选最长边参数，只改变消费者输出分辨率，不改源文件。

```powershell
build-shared/Release/tiny_pathtracer.exe scenes/cornell-box/scene-v4.pbrt out/cornell-box.ppm 64 256
```

实际输出 256×256 / 64 spp 图像并查看 PNG 转换件 `out/cornell-box.png`：可观察到红绿墙、两个方盒、面积光及间接照明。消费者构建 36 个三角形、23 个 BVH 节点。仍使用独立 RNG、sRGB 近似和 2.2 gamma，不宣称复现 PBRT Sobol 或输出管线；已有缺省 filter 预览行为保留。

新增 `tiny_cornell_box` CTest，直接加载未修改的原场景，使用原 triangle filter/maxdepth、1 spp、32×32 冒烟渲染。Windows GCC 默认测试 34/34，MSVC Release shared 含两项本地大场景加载测试为 36/36。原合成 Cornell、球体/PLY 小样例继续作为回归保留。复制脚本、README 与兼容性文档已同步范围约定。
