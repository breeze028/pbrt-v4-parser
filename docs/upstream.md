# 上游来源、版本与许可证

调研日期：2026-11-15。早期下载的参考仓库保存在忽略的 `.reference/` 中；LightTransport 和 webgpu-pages-starter 使用各自本机工作树只读调研，均不是构建依赖。

| 来源 | 本次读取版本 | 重点读取 | 处理与许可证 |
|---|---|---|---|
| [LightTransport](https://github.com/breeze028/LightTransport) | `47a461d267ed61c2c30c5054d98290ab107da581`（本机） | `src/pbrt/pbrt_loader.cpp`, `include/lt/scene.h`, `include/lt/material.h`, `src/scene/`, CMake，后续 GPU megakernel 路径 | 核心 SDK 独立实现；WebGPU 消费者按用户本轮明确要求计划抽取，尚未执行。根目录未见 LICENSE，移植文件的声明/归属与第三方许可须逐项记录 |
| [pbrt-v4](https://github.com/mmp/pbrt-v4) | `b0bdf461ba5dd6504b70ebf0932acc0fe7aa744f` | `parser.h/cpp`, `parser_test.cpp`, `paramdict.h`, `scene.h/cpp`, `util/file.cpp`, `util/transform.cpp` | Apache-2.0；参考语义，独立实现。未移植官方源文件或测试代码 |
| [ingowald/pbrt-parser](https://github.com/ingowald/pbrt-parser) | `26571dae2a6dec05655116c9c3a5ac70421fee0b`（GitHub API/raw，clone 网络失败） | README, `pbrtParser/impl/syntactic/Scene.h`, `pbrtParser/include/pbrtParser/Scene.h` | Apache-2.0；主要针对 v3/Moana，不作为 v4 语法依据；无代码复制 |
| [miniScene](https://github.com/ingowald/miniScene) | `2259c635f22db97ac9e1bd132990e063efef7c49` | README, `miniScene/Scene.h` | 文件头 Apache-2.0；参考共享 Mesh/Object/Instance 层次，无代码复制 |
| [mxpv/pbrt4](https://github.com/mxpv/pbrt4) | `c4bb00e659978e6396b638cc1aa8b96e813a9cd3` | README, Cargo.toml, `src/scene.rs`, `src/param.rs` | Apache-2.0；Rust + glam + thiserror，参考 API 与参数表达，无代码复制 |

语法权威：[PBRT-v4 文件格式](https://pbrt.org/fileformat-v4)和上述固定 commit 的官方源码。文档含部分历史拼写示例，类型以 v4 源码及当前 ParameterDictionary 为准。

2026-11-15 本地 Bathroom/Spaceship 回归纠正了此前的法线拼写判断：`paramdict.cpp:116` 使用官方类型 `normal`；之前只接受 `normal3` 的实现有误，现已修复，`normal3` 仅保留为显式 warning 的扩展。源场景未改写。

同日 WebGPU 新增来源：[webgpu-pages-starter](https://github.com/breeze028/webgpu-pages-starter) 固定 `468bee0d5f3527d7523f826cf4c64872f70d3385`，MIT；完整 `.agents/skills/webgpu-github-pages/` 已复制并附原 MIT LICENSE，构建/base/验证流程据此适配。LightTransport CUDA 的 diffuse megakernel 已部分移植至独立 WGSL 消费者，逐函数来源、修改与许可边界见 [web/THIRD_PARTY.md](../web/THIRD_PARTY.md)。核心 SDK 仍不链接 LightTransport。

## 官方内部依赖与抽取边界

```text
parser.cpp -> ParserTarget / ParsedParameter / FileLoc
           -> pstd / util containers / allocators / logging / file mapping
           -> BasicSceneBuilder (Import 特殊路径) / parallel jobs
scene.cpp  -> ParameterDictionary / spectra / RGBColorSpace / TransformCache
           -> Shape / Material / Texture / Light factories / CPU/GPU allocator
LightTransport Loader -> lt::Scene / Material / Texture / Mesh / log / future
                      -> 光谱 RGB 近似 / UV 生成 / 几何烘焙 / 渲染器材质转换
```

核心 SDK 没有复制这些内部依赖实现，采用标准库与自有 IR。可选 Web shader 的算法移植另有上述归属记录；不能将核心 SDK 的 Apache-2.0 声明视为未知上游代码或资产的授权。

## 已核对的关键语义

- 后乘 CTM；输入的 16 个矩阵值按列优先解释。
- Camera 保存 CTM 的逆；WorldBegin 仅重置变换与 active transform，不清空颜色空间/介质等状态。
- Attribute 保存图形状态；命名材质/纹理定义不随 AttributeEnd 消失。
- ObjectBegin 保存状态，不隐式 Identity；ObjectEnd 恢复状态。官方禁止在对象定义中 ObjectInstance。
- Import 继承 graphics state，但官方 CopyForImport 不复制命名坐标系；状态修改不返回父文件，可向当前对象定义贡献 Shape。
- Import 新定义名称在父文件不可引用，按格式文档执行；官方全局工厂存在更宽松的延后查找路径，本库不将其当作可依赖的格式保证。
- 官方路径解析会在根目录找不到时尝试原路径；本库固定使用根目录，避免依赖进程 cwd。gzip 暂明确报错。
