# 设计系统与布局参考

## 视觉标尺

推荐起点，按既有品牌与真实可读性调整：

| 项目 | 默认值 / 用途 |
|---|---|
| 主背景 | `#121414`，承托真实渲染图像 |
| 面板 | `#181b1a`，主要内容容器 |
| 抬升层 | `#222624`，Popover / 工具栏控件 |
| 边框 | `#303532`，常用 1px；次级分隔更轻 |
| 主文字 / 次文字 | `#dce0de` / `#919c95`，不要用过低对比隐藏关键控制 |
| Sage / 选中底色 | `#b2c5a8` / `#2c372e`，只用于选中、焦点和状态重点 |
| 间距 | 4 / 8 / 12 / 16 / 24 / 32px |
| 圆角 | 控件 5px，面板 8px，悬浮层 10px |
| 字号 | 主要控制 11–13px；面板标题 13–15px；场景标题 22–26px；元数据约 10–11px |
| 数值与源数据 | 等宽字体 + tabular numerals；不能靠截断小数解决拥挤 |

少用边界框；按钮的 Hover、Active、Selected 和 Focus 使用同一套状态。图标线宽约 1.5px，常用图标尺寸 16–18px。桌面紧凑控件可为 32px；触摸端应留足可点击区域并减少同排动作，不能机械缩小。

## 布局骨架

```text
Header: Brand / Scene / [Showcase | Inspect] / Source
┌───────────────────────────────────────────────────────────┐
│ Showcase: full-film viewport                              │
│  scene caption                    optional popover        │
│                 real rendered image                      │
│           reset / pause / exposure / settings             │
└───────────────────────────────────────────────────────────┘
Status: real render state / samples & dimensions / Diagnostics

Inspect: [Explorer] | largest viewport | [Properties]
         search      same renderer      fields / Raw Data
         virtual                         optional drawer
Bottom: optional Scene / GPU checks / Activity drawer
```

工具栏、标题和 Popover 不成为另一个常驻面板。为标题与 Film 划定互不相交的区域；不要简单地把标题绝对定位到画面左上角后忽略方图。

## Film fit

针对已经提供正确比例 backbuffer 的 renderer，布局适配可遵循：

```ts
// area excludes the caption, toolbar and any open diagnostics drawer.
const aspect = filmWidth / filmHeight;
const displayWidth = Math.min(areaWidth, areaHeight * aspect);
const displayHeight = displayWidth / aspect;
// Apply positive, nonzero display dimensions only.
// Keep object-fit: contain as a safeguard; do not set cover.
```

实际 backbuffer 若有整数像素舍入，允许对应的小幅比例误差，但不要改变相机或 Film 以填满 CSS 容器。ResizeObserver 比只监听 window 更能覆盖面板调整；也要处理隐藏/零尺寸容器。UI 布局变化无需反复分配 GPU buffer。动态渲染分辨率项目则通过已有 renderer 接口更新，不能照抄固定分辨率策略。

## Explorer 与 Inspector

虚拟化只创建可视范围与少量 overscan 行。滚动高度与逻辑节点数相符，过滤后修正滚动位置与焦点。固定行高只是一个实现选择；嵌套和多行标签要有对应测量策略。过滤和选择只更新 UI 状态，不触发解析或渲染重建。

结构化字段使用文本节点而非插入原始 HTML。保留 Source、参数类型、色彩空间、继承值、引用 ID、变换端点和资源 literal/resolved 等可用信息；按当前 Scene IR 展示，不假设所有 SDK 都拥有相同字段。完整原始数据单独序列化，切回 Properties 不丢失选择。
