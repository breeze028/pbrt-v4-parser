# Bathroom / Spaceship 示例资产

这两个场景目前是 **Parser / Scene IR / PLY Geometry 加载回归样例**。当前 Tiny CPU Renderer 不能按原始参数正常渲染，不能将本目录标为渲染验收通过。

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

默认来源为 `F:\code\LightTransport\scenes`。脚本只复制两个场景的 `scene-v4.pbrt`、`LICENSE.txt`、`models/` 和 `textures/`，**排除所有 `.lt` 文件**，也不复制已有 Tungsten 渲染图。源目录只读。每份复制件均经过 SHA-256 校验，结果写入被忽略的 `asset-manifest.json`；已有不同文件会报错并保留，不直接覆盖。

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

两者以原始文件运行 Tiny 均返回 exit 1，首先报告 `Tiny renderer uses a box pixel filter`。两份文件声明了 triangle filter 及 x/y radius；当前示例拒绝这些参数。

- 两者均使用 `coateddiffuse`、`conductor`、`dielectric`，Tiny 当前只实现 diffuse；单独解决滤波器不足以完成渲染。
- Bathroom 有 12 个 imagemap 声明、10 个唯一 TGA 文件、一个 Shape 的纹理 alpha、两个材质的 displacement，以及银的具名光谱。当前 Tiny 没有这些求值实现。
- Spaceship 无图片纹理，使用常量 infinite 与 4 个面积光声明；这些光源类型已有基础支持。但其 `maxdepth=65` 还超过当前 Tiny 的 64 上限。

没有修改两份源描述、替换材质或去掉纹理来制造“通过”结果。详细证据与修正记录见 [场景检查报告](../docs/example-scenes-validation.md)。
