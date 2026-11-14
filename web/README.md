# PBRT Scene Lab · WebGPU

可选 SDK 消费者：C++ SDK → 单线程 WASM Worker → BLAS/TLAS → WGSL megakernel → HDR 累积与独立显示 pass。Tiny CPU Renderer 继续保持原范围。

目前 Cornell Box 已实际渲染，提供相机旋转、曝光、暂停、重新累积、128–512 预览最长边、采样钳制开关及原始参数 Inspector。Spaceship / Bathroom 可以通过同一 WASM 加载、构建几何、检查参数与缺口，尚不能渲染。

## 本机构建

需要 CMake ≥3.21、C++20、Ninja、Node 24、Python 3、Emscripten **7.0.0**。SDK 默认构建仍不需要 Node / Emscripten。浏览器需要 HTTPS 或 localhost、WebGPU；此版不使用 pthread / SharedArrayBuffer。

以下命令从仓库根执行，PowerShell：

```powershell
cmake -S . -B build-web-native -G 'Visual Studio 17 2022' -A x64 -DPBRT4_BUILD_WEB=ON
cmake --build build-web-native --config Release --parallel 4
ctest --test-dir build-web-native -C Release --output-on-failure
```

安装一次官方 Emscripten SDK（也可使用已有 SDK 并设置 `EMSDK`）：

```powershell
git clone https://github.com/emscripten-core/emsdk.git .reference/emsdk
git -C .reference/emsdk checkout 708dcbcc9ee23bc7d2dad44fc90db377e38b771c
python .reference/emsdk/emsdk.py install 7.0.0
python .reference/emsdk/emsdk.py activate 7.0.0
```

```powershell
npm ci --prefix web
npm run build:wasm --prefix web
npm run prepare:assets --prefix web
npm test --prefix web
npm run dev --prefix web -- --port 5180
```

打开终端打印的 localhost URL。`build:wasm` 优先读取 `EMSDK`，否则使用 `.reference/emsdk`；无需永久修改系统 PATH。`prepare:assets` 复制编译后的 WASM，并生成 Cornell 的 manifest / 原始文件包。只运行 `npm run build` 不会重新编译 C++ 或打包场景，修改 C++ 后必须依次重跑这两步。

Linux 的原生配置使用 `cmake -S . -B build-web-native -DCMAKE_BUILD_TYPE=Release -DPBRT4_BUILD_WEB=ON`，省略 Visual Studio 参数和 `--config Release`。自动比较程序可用 `PBRT4_WEB_PACK` 指定 native packer 路径。

## 可选大场景加载检查

先按 [场景准备说明](../scenes/README.md) 放置原始模型/纹理，然后：

```powershell
npm run prepare:assets --prefix web -- cornell-box spaceship bathroom
$env:PBRT4_WEB_LARGE_SCENES='1'
npm test --prefix web
Remove-Item Env:PBRT4_WEB_LARGE_SCENES
```

默认测试 Cornell、异常恢复、Include/Import 与共享实例；设置此变量后额外执行两个大场景的 native/WASM 元数据及四个 GPU 数组逐字节比较。请求未打包的场景会明确报“场景包未准备”。资源包保留 PBRT/PLY/TGA 原字节和 LICENSE，逐文件 SHA256；不包含 `.lt` / `.ivol`。`web/public/` 内的 WASM 与场景包均为生成物，不提交 Git。

## 生产子路径验证与发布边界

```powershell
$env:PAGES_BASE_PATH='/renamed-project/'
npm run build --prefix web
npm run preview --prefix web -- --port 5181 --strictPort
# 在另一终端运行：
node web/scripts/check-http.mjs http://127.0.0.1:5181/renamed-project/
# 停止 preview 后清理变量：
Remove-Item Env:PAGES_BASE_PATH
```

必须访问包含 `/renamed-project/` 的生产 URL。HTTP 检查涵盖 HTML、JS、CSS、module worker、内嵌 WGSL、WASM MIME/magic/实际编译以及 Cornell 包哈希；随后在真实浏览器里观察画面、GPU 自检、Console 与交互。

`.github/workflows/web-ci.yml` 固定工具链并在 Linux 配置上述工程与 HTTP 检查，尚未在远程执行。Pages 发布方法已完整引入 [.agents skill](../.agents/skills/webgpu-github-pages/SKILL.md)，当前没有 remote、发布工作流或线上 URL。大资产的固定下载地址/哈希与实际部署在后续 W5 处理。

## 当前语义与限制

- GPU layout v1：vertex 48 B、primitive 16 B、node 32 B、material 64 B、placement 160 B、emitter 16 B；矩阵为 row-major / column vectors。四数组顺序为 vertices / primitives / nodes / records。
- 每个网格只构建一份 BLAS；TLAS 保存 placement。GPU 循环遍历使用两个 64 槽栈，构建深度受限，溢出/非法材质/非有限贡献有计数并停止错误渲染。
- `GPU 自检` 在独立 buffer 发射 6 条射线；独立 CPU brute-force 世界空间求交对比命中 ID、t、重心坐标、几何与插值法线，容差 `1e-3`。它不是图像正确性或完整 BSDF 的证明。
- 只渲染静态透视、box/triangle filter、trianglemesh/PLY、常量 RGB diffuse、RGB diffuse 面积光；其他光源、介质、动画、图片/光谱、alpha、bump、复杂材质会显示诊断并阻止开始渲染。
- Film 分辨率保持纵横比，预览默认最长边 384；`maxdepth` 保留 Cornell 的 65。Sampler 原参数保留，但执行使用 PCG，Accelerator 使用自有 median BVH。没有 Sobol 或 PBRT 全部 Integrator / Film 语义。
- RR 概率 `.05–.95`，贡献钳制 `64/8` 默认开启，可关闭；HDR 累积不受曝光影响。显示使用 `1-exp(-exposure*RGB)` 与 gamma 2.2，没有声称 PBRT 色彩管线等价。
- 未提供 GPU 时间或性能承诺。`WASM 加载` 是下载/校验/解析/构建/复制的 CPU wall time；spp 是每像素样本数。
- Worker generation 防止迟到结果覆盖新场景；scene handle 显式 close，大数组复制后再释放 WASM，MEMFS 输入在完成后清理。WASM heap 会保留已扩展容量供复用，不声称切换后回到初始进程内存。

来源、许可边界与逐函数映射见 [THIRD_PARTY](THIRD_PARTY.md)，阶段证据与剩余工作见 [实施记录](../docs/webgpu-implementation.md)。
