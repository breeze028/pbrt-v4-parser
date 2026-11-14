# Bathroom / Spaceship 当前状态检查

日期：2026-11-15。检查目标为当前独立 SDK 与 Tiny CPU Renderer，未调用 LightTransport 或官方 PBRT 渲染器。

**后续范围调整：** 用户指定 Tiny 仅需支持 `scenes/cornell-box`，Bathroom / Spaceship 的渲染支持留到 WebGPU 阶段考虑。为 Cornell 新增了 triangle filter，并将深度上限提高到 1024，所以本文记录的滤波器 / 深度失败是当时的历史证据；两场景的复杂材质、纹理等仍未实现，不属于 Tiny 后续目标。

## 结论与证据层次

**原始场景目前都不能由 Tiny 正常渲染。修正法线参数兼容性后，解析、引用、资源存在性与全部几何读取均通过。** 没有生成这两个场景的新渲染图，也没有把源目录中的 TungstenRender.png 当作本项目结果。

| 项目 | Bathroom | Spaceship |
|---|---:|---:|
| Parser / Scene IR 诊断 | 0 | 0 |
| Shape | 856（855 PLY + 1 TriangleMesh） | 91（88 PLY + 3 TriangleMesh） |
| Materials（含默认 diffuse） | 33 | 15 |
| Textures | 12（imagemap） | 0 |
| 面积光声明 / 其他光源 | 1 / 0 | 4 / 1 constant infinite |
| 唯一输入文件 | 865 | 88 |
| 缺少输入文件 | 0 | 0 |
| 几何顶点 | 362,068 | 368,377 |
| 几何三角形 | 592,188 | 457,560 |
| 有法线 / UV 的 mesh | 808 / 856 | 48 / 91 |
| 几何读取诊断 | 0 | 0 |
| Tiny 原样执行 | exit 1：不支持该像素滤波器参数 | exit 1：不支持该像素滤波器参数 |

没有法线的 mesh 仍保留为空；SDK 未为通过检查而生成法线或 UV。当前文件没有 ObjectInstance，geometry placements 数分别为 856 / 91。

## 发现并修复的 SDK 错误

初始执行分别在 Bathroom 第 236 行与 Spaceship 第 315 行失败：`Unknown v4 parameter type: normal`。之前只实现 `normal3`，这是此前兼容性核对的遗漏。

本机固定上游 `mmp/pbrt-v4@b0bdf461ba5dd6504b70ebf0932acc0fe7aa744f` 的 `src/pbrt/paramdict.cpp:116` 明确使用 `ParameterTypeTraits<ParameterType::Normal3f>::typeName = "normal"`。本次修正如下：

- Syntax 接受 `normal`，按 3 个数值组成一个法线，保留原始类型拼写。
- Geometry adapter 正确读取 `normal N`。
- 已有 `normal3` 保留为 SDK 扩展并给出 `parameter.extension` warning，不能继续冒称它是官方拼写。
- 添加正常法线、错误组件数量、扩展诊断与内联法线提取的回归断言。

MSVC Release 共享库与 Windows GCC Debug 的默认测试均为 **33/33 通过**，包含新增 CLI 资源与几何检查。原有 Cornell、PLY 示例渲染和不支持材质的失败测试也通过。大型本地场景测试通过 `PBRT4_TEST_LARGE_SCENES` 单独启用，不要求默认 CI 拉取资产。

## 渲染器尚缺的能力

实际执行的第一处拒绝为带参数的 triangle pixel filter。其余缺口通过完整 IR 类型统计及 Tiny 当前代码检查确认，尚未运行到相应分支：

| 功能 | Bathroom 声明数量 | Spaceship 声明数量 | Tiny 当前情况 |
|---|---:|---:|---|
| coateddiffuse | 7 | 6 | 未实现 |
| conductor | 5 | 2 | 未实现 |
| dielectric | 1 | 1 | 未实现 |
| imagemap | 12 | 0 | 未实现图像解码/采样 |
| 纹理 alpha | 1 Shape | 0 | 未实现 |
| displacement | 2 材质 | 0 | 未实现 |
| 银的 eta/k 具名光谱 | 2 参数 | 0 | 保留在 IR，Tiny 不求值 |
| maxdepth | 17 | 65 | Tiny 设置上限为 64，Spaceship 另有此限制 |

这些问题属于示例消费者的功能覆盖，不能通过把全部材质变 diffuse、删除参数或在 Parser 中丢弃数据来解决。

## 复制、完整性与复现

复制范围：PBRT 描述 + LICENSE + models + textures；未复制 `.lt`、已有 EXR/PNG 渲染图。Bathroom 867 个文件 / 64,517,898 字节；Spaceship 90 个文件 / 16,268,468 字节。两者全部复制项已在复制脚本中逐项核对 SHA-256。目标 `scenes/` 中 `.lt` 数为 **0**，LightTransport 工作树无改动。

根文件 SHA-256（源与复制件一致）：

```text
bathroom  7E3A3A438FA811E757C13B8DB8B0FF415E2EBE943B5D1FBC4B7DA0598F8E5324
spaceship 4195F5F3F265C58B92E80E363F3D545711EEDC2FE0181A2D3D4C2713B6EF1853
```

原始执行日志位于被忽略的 `out/scene-validation/`：`*-info-original.txt` / `*-tiny-original.txt` 为修正前，`*-info.txt` / `*-tiny.txt` 为修正后，`*-audit.txt` 为全部资源与实际几何读取。默认单测日志为 `unit-tests.log` / `gcc-unit-tests.log`。本报告保存可阅读结果；日志与大资产不纳入 Git。

```powershell
./tools/copy_example_scenes.ps1
cmake --preset msvc
cmake --build --preset msvc
build-msvc/Debug/pbrt4_info.exe --details --check-resources --geometry scenes/bathroom/scene-v4.pbrt
build-msvc/Debug/pbrt4_info.exe --details --check-resources --geometry scenes/spaceship/scene-v4.pbrt
build-msvc/Debug/tiny_pathtracer.exe scenes/bathroom/scene-v4.pbrt out/bathroom.ppm 1
build-msvc/Debug/tiny_pathtracer.exe scenes/spaceship/scene-v4.pbrt out/spaceship.ppm 1
```

最后两条当前应失败并给出明确提示，不作为成功渲染测试。两个原始场景现由独立 WebGPU 消费者渲染，见 [实施与验证](webgpu-scenes-validation.md)；Tiny 继续限定 Cornell。
