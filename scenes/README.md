# 示例场景与消费者范围

**Tiny Renderer 的主要验收场景是 `cornell-box/scene-v4.pbrt`**，复制自用户指定的 LightTransport 场景目录，原描述和许可证保留字节内容。它仅需 8 个内联 TriangleMesh（36 个三角形）、diffuse 材质和面积光，没有外部资源。只复制 PBRT 和许可证，不复制 `.ivol` 缓存或已有渲染图。

```powershell
build-shared/Release/tiny_pathtracer.exe scenes/cornell-box/scene-v4.pbrt out/cornell-box.ppm 64 256
```

参数为 spp 和预览图像最长边；省略最长边时使用原 Film 的 1024×1024。Tiny 实现该场景的 triangle filter，接受原始 `maxdepth=65`，仍使用自有独立 RNG 和线性 sRGB 输出近似，不复现 PBRT 的 Sobol 或完整输出管线。原来的 `testdata/cornell.pbrt` 保留为小型合成回归。

**Bathroom / Spaceship 的渲染支持留到 WebGPU 阶段考虑，不再扩展 Tiny 来覆盖它们。** 两者目前是 Parser / Scene IR / PLY Geometry 加载回归样例，不能标为渲染验收通过。

| 场景 | 本地路径 | 原作者（随包许可证记载） | 实际检查 |
|---|---|---|---|
| Contemporary Bathroom | `bathroom/scene-v4.pbrt` | Mareck | 856 个 Shape，592,188 个三角形，865 个唯一输入资源，全部存在 |
| 4060.b Spaceship | `spaceship/scene-v4.pbrt` | thecali | 91 个 Shape，457,560 个三角形，88 个唯一输入资源，全部存在 |

PBRT 描述和原 `LICENSE.txt` 保留字节内容。PLY/图片资源在本地放入 `models/`、`textures/`，约 77 MiB，默认被 Git 忽略；从 Git 克隆并不会自动获得这些依赖。随包 LICENSE 记载原场景为 CC0，但原许可证仍按原文保留，不将它们改标成 SDK 的 Apache-2.0。

## 从本机 LightTransport 复制

PowerShell 7：

```powershell
./tools/copy_example_scenes.ps1
# 或指定其他资产来源：
./tools/copy_example_scenes.ps1 -SourceRoot 'D:\assets\pbrt-scenes'
```

默认来源为 `F:\code\LightTransport\scenes`。脚本复制 Cornell Box、Bathroom、Spaceship 的 `scene-v4.pbrt`、`LICENSE.txt` 及适用的 `models/`、`textures/`，**排除所有 `.lt` 和 `.ivol` 文件**，也不复制已有 Tungsten 渲染图。源目录只读。每份复制件均经过 SHA-256 校验，结果写入被忽略的 `asset-manifest.json`；已有不同文件会报错并保留，不直接覆盖。

## 检查入口

```powershell
cmake --preset msvc
cmake --build --preset msvc
build-msvc/Debug/pbrt4_info.exe --details --check-resources --geometry scenes/bathroom/scene-v4.pbrt
build-msvc/Debug/pbrt4_info.exe --details --check-resources --geometry scenes/spaceship/scene-v4.pbrt
```

`--check-resources` 只检查已解析的输入路径是否存在，不对图片解码；不会把 Film 输出路径当作缺少输入。Spectrum 字符串保留“预定义名称或文件”的歧义并单列计数。`--geometry` 实际读取全部被使用的 PLY 和内联 TriangleMesh，不会渲染。

可选启用 CTest 加载回归（需要先复制资产）：

```powershell
cmake --preset msvc -DPBRT4_TEST_LARGE_SCENES=ON
cmake --build --preset msvc
ctest --preset msvc -L example_scene
```

## 当前渲染缺口

初次检查时，两者以原始文件运行 Tiny 均首先在 triangle filter 处退出（历史证据见报告）。为 Cornell Box 新增 triangle filter 后，这一限制已消除，但下列材质等缺口仍在，因此它们不属于 Tiny 的渲染目标。

- 两者均使用 `coateddiffuse`、`conductor`、`dielectric`，Tiny 当前只实现 diffuse；单独解决滤波器不足以完成渲染。
- Bathroom 有 12 个 imagemap 声明、10 个唯一 TGA 文件、一个 Shape 的纹理 alpha、两个材质的 displacement，以及银的具名光谱。当前 Tiny 没有这些求值实现。
- Spaceship 无图片纹理，使用常量 infinite 与 4 个面积光声明；这些光源类型已有基础支持。其 `maxdepth=65` 现可接受（当前上限 1024），材质仍未支持。

没有修改两份源描述、替换材质或去掉纹理来制造“通过”结果。详细证据与修正记录见 [场景检查报告](../docs/example-scenes-validation.md)。
