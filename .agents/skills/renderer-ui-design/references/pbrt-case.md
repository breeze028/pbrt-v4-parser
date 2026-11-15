# PBRT Scene Lab 实例

这份 skill 从 pbrt-v4-parser 的 UI 改版提炼，参考实现版本 `ebc186ad1ecf1e428002a95d2508264357d50357`。案例帮助理解设计，不能代替后续项目的现场检查。

- [源码](https://github.com/breeze028/pbrt-v4-parser/tree/ebc186ad1ecf1e428002a95d2508264357d50357/web/src)：`main.ts` 集成状态；`ui/explorer.ts` 虚拟列表；`ui/properties.ts` 结构化字段；`style.css` 设计系统。
- [实施与证据](https://github.com/breeze028/pbrt-v4-parser/blob/ebc186ad1ecf1e428002a95d2508264357d50357/docs/webgpu-ui-redesign.md)
- [线上页面](https://breeze028.github.io/pbrt-v4-parser/)：可能随仓库更新，不视为固定版本。

需要视觉对照时，用可用的图像查看工具打开下面的本地截图；不要只根据文件名推断布局。

| 截图 | 用途 |
|---|---|
| [showcase.jpg](showcase.jpg) | 1920×1080，完整横向 Film、顶栏、悬浮操作与真实 HUD |
| [inspect.jpg](inspect.jpg) | 1440×900，搜索结果、方形 Film、只读纹理属性 |
| [drawer.jpg](drawer.jpg) | 1050px 宽时按需展示 Properties |
| [mobile.jpg](mobile.jpg) | 390px 宽的两行顶栏和完整方形 Film |

案例中 Bathroom 的 908 个场景条目约挂载 24 行，证明可见范围虚拟化有效；这些数字不是新项目要伪造或达到的固定指标。目标 16 spp 自动暂停、手动深度 35、PFM 导出与 resize 不清空累积均实际检查过。窗口改变只重新 fit 固定预览分辨率，这是该 renderer 的策略，不能硬套到其他 renderer。

视觉模式的外部参考为 [PlayCanvas](https://developer.playcanvas.com/user-manual/editor/interface/)、[Spline V2](https://spline.design/blog/spline-v2)、[SuperSplat](https://developer.playcanvas.com/user-manual/supersplat/editor/interface/) 与 [Babylon Inspector](https://doc.babylonjs.com/legacy/inspector/)。需要继续研究它们时再查看官方页面；不复制其品牌、Logo 或将历史截图当作最新产品状态。

随附截图来自该项目真实渲染，场景资产为 CC0（Spaceship: thecali；Bathroom: Mareck）。它们仅作为设计参考，不是供后续项目伪装成实时渲染结果的贴图。
