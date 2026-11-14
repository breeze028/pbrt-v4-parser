# 兼容性与已知限制

支持语法和保存 IR 不等于能渲染该特性。以下是 0.1 开发版的实际边界。

| 功能 | Syntax / Scene IR | Geometry Utilities | Tiny CPU |
|---|---|---|---|
| v4 指令集合 | 全部当前 ParserTarget 指令，另有 Include/Import/Option | 不解析语法 | 只使用 SDK |
| Token / typed params | 转义、注释、数组、整数/浮点/布尔/字符串、SourceLocation、类型与组件数检查 | 消费 P/N/uv/indices | 消费明确子集 |
| 法线参数 | 官方拼写 `normal`；`normal3` 为带 warning 的 SDK 扩展，均保留拼写 | 两种输入均可提取 N | 通过 SDK |
| 未知指令 / v3 WorldEnd | error | — | — |
| v3 TransformBegin/End | 接受并 warning，按 Attribute 语义 | — | — |
| 未知合法实体类型 | 完整保留 type/params；不做工厂 schema 验证 | 无 adapter 则 error | 未支持则 error |
| 光谱 / blackbody | 原始数值或字符串保留，不转 RGB | — | 只接受线性 sRGB 的 rgb 值，其他明确拒绝 |
| ColorSpace | srgb / rec2020 / dci-p3 / aces2065-1；记录参数声明时颜色空间 | 不转换颜色 | 仅 linear sRGB |
| Transform / LookAt / 坐标系 | 后乘、PBRT 列优先矩阵、相机逆矩阵、作用域 | 仿射静态烘焙，逆转置法线 | 静态相机与静态实例 |
| ActiveTransform / TransformTimes | 两端矩阵和时间保留；坐标系保存两端 | 保留两端，不实现插值 | 动画拒绝 |
| Attribute | 状态栈 + 五类参数默认值，显式参数优先 | 消费有效参数 | 支持所需参数子集 |
| Object / Instance | 共享定义、ID 引用；禁止对象内嵌套实例 | 共享 mesh、独立 placement；默认不烘焙 | 显式静态烘焙到三角形 BVH |
| Include | 根目录相对路径、同状态展开、环/深度/文件大小诊断 | — | 通过 SDK |
| Import | 继承状态、独立命名分支与坐标系、作用域检查、顺序执行 | 同 Scene IR | 通过 SDK |
| NamedMaterial / Texture / Medium | 引用解析、未定义与重复定义报错；纹理双名称空间 | 材质 ID、面积光 ID | diffuse，无纹理、无介质 |
| AreaLight / Light | 原始参数、Shape 关联、变换与介质引用 | 关联随实例保留 | diffuse area + 常量 infinite |
| TriangleMesh | 原始参数，不强制三角化 | 索引/法线/UV 校验，不生成缺失 UV | 支持 |
| PLY | 原始路径引用，默认不读取外部资源 | ASCII、binary LE/BE；三角形/四边形，其他 polygon 报错 | 通过 SDK 读取 |
| Sphere | 原始解析形状 | 整球显式离散化；裁剪球报错 | 离散化预览 |
| curve / loopsubdiv / disk 等 | 完整声明与状态保留 | 尚无 adapter | 拒绝 |
| 图片 / Ptex / SPD / NanoVDB | 路径与参数保留，未解码 | 不解码 | 拒绝需要它们的渲染输入 |
| gzip | 明确报错 | 未实现 | 未实现 |
| WASM / WebGPU | 可选单线程 WASM，三个本地场景已比较 | BLAS/TLAS 四数组 native/WASM 一致 | 独立 Web consumer 可渲染 Cornell；复杂场景仅加载检查 |

## 尚需完善

- 不承诺与所有官方工厂相同的参数 schema 检查：例如负球半径在 IR 中可以保留，几何 adapter 才会拒绝；拼错实体参数也不会在核心自动删除。消费者负责处理实体实现语义。
- 具名纹理类型推断、材质/纹理引用循环的完整验证、所有文件资源参数名的注册表、Unicode NFC 规范化尚未提供。非 filename/mapname 的自定义资源参数仍在原始参数中，不能视为已消失。
- 数值采用 double 和 int64 保存，比官方默认 float/int 更宽；错误条件不保证逐字逐码一致。全局 Option 的 typed scalar 被保存，不尝试执行渲染器开关或未知选项。
- Import 的名称可见性遵循格式文档；官方的全局延后工厂路径可能容许更宽松的交叉引用，本库明确拒绝。尚未执行大型并行 Import 场景回归。
- 默认路径固定到根文件目录，与官方 ResolveFilename 找不到后尝试 cwd 的 fallback 不同。
- PLY 四边形按 0-2 对角线拆分，复杂凹多边形不支持。未读取 face-varying UV、颜色或任意额外属性；额外属性按 PLY 类型正确跳过。缺失的法线/UV 不生成。
- Tiny 支持 separable triangle filter 的重要性采样，其余无参数滤波器声明仍按已提示的盒滤波预览；不复现 PBRT 的 Sobol、Integrator 或色彩输出管线。使用独立 RNG、基本 diffuse 估计器和 2.2 显示 gamma。只适合 SDK 教学与小场景预览。
- 已写 Windows/Linux CI 配置，未运行远程 CI；本机两套编译器都运行在 Windows，不能作为 Linux 运行证据。

官方描述文件回归覆盖：killeroo-simple、killeroo-moving（包含 loopsubdiv Include）、ganesha、head（纹理、subsurface 与材质参数）。这些是语法/IR 证据，不是这些场景已由 Tiny 正确渲染的证据。

另已检查本地 Bathroom / Spaceship：修复 `normal` 拼写遗漏后，全部解析、资源检查及 PLY 几何加载通过；Tiny 原始场景渲染失败。统计与缺口见 [检查报告](example-scenes-validation.md)。

当前范围约定：Tiny 以 `scenes/cornell-box/scene-v4.pbrt` 为主要渲染验收场景；Bathroom / Spaceship 留待 WebGPU 消费者考虑。Cornell 原始 triangle filter 和 maxdepth=65 已支持；不因此新增复杂 BSDF、图片纹理、alpha 或 displacement 到 Tiny。

WebGPU 的首批实施证据见 [webgpu-implementation.md](webgpu-implementation.md)。Web 消费者与 Tiny 独立：它已在浏览器运行 Cornell 的 diffuse / 面积光 NEE/MIS；不支持的复杂材质、图片/光谱、alpha、bump、介质等会拦截渲染。GPU 时间、CUDA 图像对齐、复杂场景出图及线上部署仍未验证。
