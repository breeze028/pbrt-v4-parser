# PBRT Scene Lab · WebGPU

独立 C++ SDK 的浏览器消费者：**原始 PBRT/PLY/TGA → 同一 C++ SDK / 单线程 WASM Worker → BLAS/TLAS → WGSL megakernel → HDR 累积与显示**。没有另写 JS Parser，也不把预解析 JSON 当作场景入口。

Cornell、Spaceship、Bathroom 均已在本机真实 WebGPU 中渲染。Spaceship 包括 conductor、coat、rough dielectric 和常量环境；Bathroom 包括 TGA、trilinear mip、独立 rug mask、银的 RGB 近似及材质 bump。原始参数、引用、资源路径、变换、对象/实例、材质与纹理可在 Inspector 中检查。

## 运行

需要 CMake ≥3.21、C++20、Ninja、Node **24**、Python 3、Emscripten **7.0.0**。浏览器需要 HTTPS / localhost 与 WebGPU。核心 SDK 默认构建仍不需要 Node、Emscripten 或图片库。

从仓库根执行，Windows / PowerShell：

```powershell
python tools/web_scene_assets.py fetch
cmake -S . -B build-web-native -G 'Visual Studio 17 2022' -A x64 -DPBRT4_BUILD_WEB=ON -DPBRT4_TEST_LARGE_SCENES=ON
cmake --build build-web-native --config Release --parallel 4
ctest --test-dir build-web-native -C Release --output-on-failure
```

若已有原始模型/纹理，资源脚本验证后跳过匹配文件，不覆盖改动过的资产。也可按 [场景说明](../scenes/README.md) 从本地 LightTransport 准备。固定下载包与 SHA256 见 [assets.lock.json](assets.lock.json)。每个包包含 CC0 LICENSE；不包含 `.lt` / `.ivol`。

安装一次官方 SDK，或设置 `EMSDK` 指向已有的 7.0.0 SDK：

```powershell
git clone https://github.com/emscripten-core/emsdk.git .reference/emsdk
git -C .reference/emsdk checkout 708dcbcc9ee23bc7d2dad44fc90db377e38b771c
python .reference/emsdk/emsdk.py install 7.0.0
python .reference/emsdk/emsdk.py activate 7.0.0
npm ci --prefix web
npm run build:wasm --prefix web
npm run prepare:assets --prefix web -- cornell-box spaceship bathroom
$env:PBRT4_WEB_LARGE_SCENES='1'
npm test --prefix web
Remove-Item Env:PBRT4_WEB_LARGE_SCENES
npm run dev --prefix web -- --port 5180
```

打开终端打印的 URL。Linux 原生配置改用 `-DCMAKE_BUILD_TYPE=Release` 并省略 Visual Studio / `--config` 参数。`PBRT4_WEB_PACK` 可指定 native packer 路径。只准备 Cornell 时，省略 `prepare:assets` 的三个名称及大场景测试变量；默认 WASM 测试为 4 项，全部资产齐备后为 6 项。

修改 C++ 后必须重跑 `build:wasm` 和 `prepare:assets`，再构建前端；单独 `npm run build` 只做类型检查与 Vite 打包。WASM 与原始场景包、许可证目录均由脚本生成，不将大模型、node_modules 或工具链放进 Git。

## 操作

- 拖动画布旋转，滚轮前后移动；聚焦画布后 WASD / QE 移动、方向键转头，Shift 加速。重置相机恢复原 PBRT 构图。
- 预览最长边 128 / 256 / 384 / 512，保持 Film 纵横比。曝光只影响显示；相机、分辨率、深度、seed、LOD / alpha / bump 开关会清空累积。
- 渲染设置可指定目标 spp（0 持续）、深度与随机种子。达到目标自动暂停。PFM 导出包含按每像素计数归一化后的线性 RGB，不含曝光或 tonemap。
- `GPU 自检` 比较 GPU BVH 与 CPU brute-force 命中/距离/重心/法线，执行 BSDF 解析值、PDF/能量/Fresnel/TIR、MIS 极端值、纹理编码/wrap/mip、bump 法线检查，并读取实际渲染诊断。CPU 求交在另一个 Worker 中运行。
- Texture Inspector 展示原始声明与解码后预览、尺寸、mip、颜色/数据角色。Geometry / Material / Light / Object / Instance 参数保持只读，不用简化表单替换原始 PBRT。

## 生产构建与 Pages

```powershell
$env:PAGES_BASE_PATH='/renamed-project/'
npm run build --prefix web
npm run preview --prefix web -- --port 5181 --strictPort
# 另一终端：
node web/scripts/check-http.mjs http://127.0.0.1:5181/renamed-project/ cornell-box spaceship bathroom
# 停止预览后：
Remove-Item Env:PAGES_BASE_PATH
```

HTTP 检查验证 HTML/JS/CSS、两个 Worker、内嵌 WGSL、WASM MIME/magic/实际编译，以及三个原始包的每文件 SHA256 和 LICENSE。随后仍需真实浏览器观察画面和 Console；HTTP 200 与 CI 绿灯不等于 GPU 已渲染。

部署采用指定 [webgpu-github-pages skill](../.agents/skills/webgpu-github-pages/SKILL.md)。公共 Release 提供固定原始资产，CI 按 lock 校验后生成 Pages 本站包。`web-ci.yml` 检查改名子路径，`deploy-pages.yml` 从 Pages 元数据取 base 并只上传 `web/dist`；核心 C++ CI 保留。

## 明确的近似与边界

这是 LightTransport 风格的 RGB renderer，**不是完整 PBRT-v4 渲染器**：

- coateddiffuse 为 LT 的 Principled + coat 近似，底层 roughness=1、coat roughness 至少 .02；不是 PBRT layered BSDF，白色涂层可能产生小幅能量增益。
- conductor 使用 RGB eta/k 与 GGX；银的具名光谱采用 LT 的 650/550/450 nm 近似。rough dielectric 的求值、采样、连续 PDF、Fresnel/TIR 和折射 Jacobian 已实现，未忽略 Spaceship 的 .01 roughness。
- 默认颜色 TGA 按 LT 风格 sRGB、float 数据按 linear，显式 encoding 优先；官方 PBRT 对 TGA 的默认 linear 与此不同。颜色与数据角色分开生成 mip。
- compute 显式取 LOD，使用 ray cone + UV 密度近似做 trilinear；不是完整 ray differentials / EWA。材质 displacement 扰动 shading normal，包含 UV 与法线微分，不改变网格轮廓。
- alpha 在主路径和阴影均独立求值。玻璃阴影采用直线穿越及 Fresnel 衰减，是 LT 近似；没有折射阴影/焦散保证。透明穿越最多 128 步、额外透射最多 12 次，超限会诊断并停止错误渲染。
- PCG、自有 median BVH、独立 tonemap 与 gamma 2.2；不实现 PBRT Sobol / 全部 Integrator / Film 色彩语义。原始参数始终保留。默认贡献钳制 64/8、RR .05–.95，可关闭钳制做线性 HDR 比较。
- 仅实现目标场景所需的四类材质、常量环境、diffuse 面积光、静态透视、box/triangle filter、三角网格/PLY 和 TGA。其他工厂、图片格式、介质、动画等会拦截并报诊断。
- 不要求硬件 RT、subgroups、f16、bindless、pthreads 或跨源隔离。每 dispatch 1 spp，不提供未经测量的 GPU 时间/帧率或全移动设备支持承诺。

GPU layout v2：vertex 48 B、primitive 16 B、node 32 B、material 64 B、placement 160 B、emitter 16 B、texture record 32 B、alpha record 16 B；矩阵 row-major / column vectors。五个传输数组为 vertices / primitives / nodes / records / image-mip-bytes。图像使用 12 个固定 slots + WGSL switch；当前 Bathroom 实际占 11 个角色副本。

Worker 以 generation 丢弃旧结果，显式 close scene handle、清理 MEMFS 输入；重新获取增长后的 HEAPU8，复制大数组后才释放来源。切场景销毁 GPU buffer/texture。WASM 会保留已扩展的 heap 容量供复用，不能把它当成未释放的完整旧场景。

来源与许可见 [THIRD_PARTY](THIRD_PARTY.md)，阶段证据见 [实施记录](../docs/webgpu-scenes-validation.md)。
