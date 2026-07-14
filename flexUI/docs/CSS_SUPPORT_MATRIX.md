# flexUI CSS Support Matrix

本文件定义 `flexUI` 当前 CSS 能力边界，以及与 WPT 对齐的测试引入顺序。

## 测试源

官方主源统一采用 WPT GitHub 仓库：

- WPT 仓库: https://github.com/web-platform-tests/wpt
- CSS 根目录: https://github.com/web-platform-tests/wpt/tree/master/css
- Selectors: https://github.com/web-platform-tests/wpt/tree/master/css/selectors
- CSS Variables: https://github.com/web-platform-tests/wpt/tree/master/css/css-variables
- Flexbox: https://github.com/web-platform-tests/wpt/tree/master/css/css-flexbox
- Transitions: https://github.com/web-platform-tests/wpt/tree/master/css/css-transitions
- Animations: https://github.com/web-platform-tests/wpt/tree/master/css/css-animations

## 当前支持

### Selectors

状态: 部分支持

已支持：

- type selector
- `#id`
- `.class`
- `*`
- 属性选择器 `[*]` / `[name=value]` 的基础子集
- 单个 pseudo-class，如 `:hover` / `:active` / `:focus` / `:disabled`
- 函数伪类基础子集：`:not()` / `:is()` / `:where()`
- 关系伪类 `:has()` 的基础子集
- 后代 / 子代 / 相邻兄弟 / 通用兄弟组合器
- 多段复杂 selector chain 的基础子集
- selector list，如 `label, .pill`
- specificity + source order + declaration-level `!important`
- `Box::query_selector()` / `query_selector_all()` 使用同一 selector matcher，并按文档树 preorder
  返回应用节点和 Widget-owned semantic parts

不支持：

- 浏览器级 selector invalidation 全量边界

### CSS Variables

状态: 部分支持，但足够开始引入基础用例

已支持：

- custom property 声明 `--foo`
- `var(--foo)`
- `var(--foo, fallback)`
- 递归变量解析
- 父到子的变量继承
- cascade 覆盖

当前限制：

- 未对“计算值阶段 invalid”做完整浏览器级语义对齐
- 主要按 `flexUI` 的 `ComputedStyle` 消费路径生效

### Layout / Flexbox

状态: 部分支持

已支持：

- `display: flex`
- `flex-direction`
- `justify-content`
- `align-items`
- `gap`
- `flex-grow`
- `flex-shrink`
- `flex-basis`
- `flex` 简写的常见形式
- `inline-size` / `block-size` 与 min/max 逻辑尺寸别名，当前按水平 writing-mode 映射到 width/height

当前限制：

- 不是浏览器级完整 flexbox 实现
- logical size 暂不实现垂直 writing-mode 轴交换
- 不应先引入复杂 intrinsic sizing、baseline、anonymous item、writing mode 相关用例

### Layout / Grid

状态: UI 常用子集可用

已支持：

- 显式与隐式行列
- `repeat()` / `minmax()` / `auto-fit` / `auto-fill`
- named template areas
- row/column placement、span 与 dense auto-flow
- grid gap、track distribution、item self alignment

当前限制：

- 不覆盖浏览器级 intrinsic track sizing、subgrid 与 masonry
- 与 Flexbox 相同，vertical writing mode 尚未形成完整轴语义

### Queries

状态: UI 常用子集可用

- `@media` 支持 viewport、orientation、aspect ratio、pointer/hover、颜色方案、对比度与 reduced motion 的已测试子集
- `@container` 支持 inline-size 与 named container 的 min/max 查询
- `@supports` 支持声明查询、boolean operator 与 `selector(...)`

### Visual Effects

状态: 部分支持

已支持：

- `color`
- `background-color`
- `background-image` 的 `linear-gradient()` / `radial-gradient()` / `url(...)` 基础层
- `background-position` / `background-position-x` / `background-position-y`
  - 支持常见 1/2/3/4 值位置语法，例如 `center`、`left top`、`right 10px bottom 20px`
  - `background-position-x/y` 会合并为每层的二维 `background-position`
- `background-size` / `background-repeat`
  - `background-repeat` 支持 `repeat` / `no-repeat` / `repeat-x` / `repeat-y` / `space` / `round`
- `background-clip` / `background-origin`
  - 支持 `border-box` / `padding-box` / `content-box`
  - 支持多层 metadata；per-layer `background-clip` 已作用于背景填充、渐变与 image layer
- `border-color`，包含 1-4 值展开与物理 / 逻辑分边颜色；`border-width` 支持长度与 `thin` / `medium` / `thick`；`border-style` 支持 `solid` / `dashed` / `dotted` / `double` / `groove` / `ridge` / `inset` / `outset`
  - 支持 `border-inline-*` / `border-block-*` 的 width / style / color 逻辑分边 longhand，并按 `direction` 映射 inline start/end
- `object-fit` / `object-position` 供 `ImageWidget` 消费
  - `object-fit` 支持 `contain` / `cover` / `fill` / `none`
  - `object-position` 支持关键字、百分比 / 长度，以及 `right 10px bottom 5px` 这类边距定位语法
- `opacity`
- `filter` 子集：`blur(...)` / `drop-shadow(...)` / `opacity(...)` / `none`
- `backdrop-filter` / `-webkit-backdrop-filter` 子集：`blur(...)` / `none`
  - blur/backdrop blur 依赖 renderer `caps.blur`；后端未声明能力时不会发出 blur command
- `visibility: visible` / `hidden` / `collapse` 会进入布局与绘制可见性判断
- `overflow` / `overflow-x` / `overflow-y` 支持 `visible` / `hidden` / `clip` / `scroll` / `auto` 的绘制裁剪映射
- `clip-path: inset(...)` 的矩形裁剪子集
- `transform` 子集:
  - `translate(x, y)` / `translateX(x)` / `translateY(y)`
  - `translate3d(x, y, z)`（忽略 z）
  - `scale(s)` / `scaleX(s)` / `scaleY(s)`
  - `scale3d(x, y, z)`（忽略 z）
  - `rotate()` / `rotateZ()`，支持 `deg` / `rad` / `turn` / `grad`
  - `matrix(a, b, c, d, e, f)` / `skew()` / `skewX()` / `skewY()` 会进入通用 affine transform
- individual transform properties:
  - `translate: x [y]`，`none` 重置为无平移
  - `scale: x [y]`，`none` 重置为 `1`
  - `rotate: <angle>`，支持同 `rotate()` 的角度单位
- `transform-origin` 支持关键字、长度、百分比与 `var(...)` 引用
- `box-shadow` 基础形式
- `text-shadow` 基础形式（支持多层偏移与 blur command）
- `font-family` 支持 CSS 字体列表并取第一个 family 供当前文本后端使用；`font-size` 支持长度、视口 / rem / em 基础单位，以及 `xx-small` 到 `xxx-large`、`larger` / `smaller` 关键字；`font-weight` 支持常见数值、`bold` / `normal` 与 `bolder` / `lighter`；`font-style` 支持 `normal` / `italic` / `oblique`
- `letter-spacing` / `word-spacing` / `text-indent` 会进入文本测量、首行缩进与绘制位置
- `text-decoration-line` 支持 `underline` / `line-through` / `overline`，`text-decoration-style` 支持 `solid` / `dashed` / `dotted` / `double` / `wavy`
- `text-align` 支持 `left` / `center` / `right` / `start` / `end`，`justify` 支持非最后一行空白分配
- `text-align-last` 支持最后一行 `left` / `center` / `right` / `start` / `end` / `justify` / `auto`
- `vertical-align: top` / `middle` / `bottom` 会进入普通文本和 label 文本块的垂直定位
- `tab-size` 数值会进入 tab 字符的测量与绘制推进
- `white-space: normal` / `nowrap` / `pre` / `pre-line` / `pre-wrap` / `break-spaces` 会进入文本归一化与换行
- `text-wrap` / `text-wrap-mode: nowrap` 会在不改变空白折叠规则的情况下关闭文本自动换行
- `text-overflow: ellipsis` / `clip` 会进入 `nowrap` 文本的宽度截断
- `overflow-wrap: break-word` / `anywhere` 与 `word-break: break-all` 会进入文本换行
- `max-lines` / `line-clamp` / `-webkit-line-clamp` 会进入文本行数裁剪
- `outline` / `outline-offset`，`outline-style` 支持 `solid` / `dashed` / `dotted` / `double` / `groove` / `ridge` / `inset` / `outset`，`outline-width` 支持长度与 `thin` / `medium` / `thick`
- `ring` / `ring-offset`（flexUI utility 扩展，用于 primitive focus ring）
- `caret-color` / `accent-color`
  - `caret-color` 会驱动 input / textarea 光标色
  - `accent-color` 会驱动 checkbox / radio / switch / progress / slider 的选中或填充色
- `cursor` / `user-select` / `touch-action` 等交互策略值会进入 `ComputedStyle` 变量，供 widget 或 host bridge 读取

当前限制：

- `background-position-x/y` 当前按 flexUI 已支持的两轴定位模型合并，不宣称覆盖浏览器级全部边界
- `background-clip` 支持 per-layer `border-box` / `padding-box` / `content-box`；`background-clip: text` 暂不支持
- `clip-path` 暂只支持 `inset(...)` 的矩形裁剪，`round` 半径会保留解析边界但不生成圆角 clip
- `filter` 不宣称覆盖浏览器级函数顺序、颜色矩阵、混合与复杂 compositing；当前只把已列子集映射到中立渲染命令
- text-shadow blur 依赖 backend blur 能力；无 blur backend 时仍会保留偏移 shadow 文本
- transform 仍是 2D 子集；`perspective()` / 非 z 轴 3D 旋转暂不支持，3D 参数仅保留基础兼容路径或忽略
- transform transition 对非均匀缩放仅覆盖基础路径
- `touch-action` 目前只存储策略值，不等同于浏览器级 touch gesture negotiation

### Transitions

状态: shadcn 所需基础子集可用，非浏览器级完整实现

已具备：

- `transition` / `transition-property` / `transition-duration` 等常见声明解析
- `TransitionManager`
- `Box` 更新路径可为部分数值 / 颜色样式变化启动 transition

当前限制：

- 不覆盖浏览器级 transition 全语义
- 当前重点是 shadcn 高频交互所需的 opacity / transform / color / ring 等路径

### Animations

状态: shadcn 所需基础子集可用，非浏览器级完整实现

已具备：

- `@keyframes` / `@-webkit-keyframes` 解析
- `animation-name`
- `animation-duration`
- `animation-delay`
- `animation-fill-mode`
- `animation-iteration-count`
- keyframe runtime 的常见属性子集
- keyframe `transform` 覆盖 2D 平移 / 缩放 / 旋转，并兼容 `translate3d` / `scale3d` / `rotateZ` 的 2D 降级路径

当前限制：

- 不覆盖浏览器级 CSS Animations 全语义
- runtime 重点覆盖 shadcn 常见的 opacity、background、transform、ring、shadow 等属性
- complex interpolation、timeline、composition、direction 等高级语义仍不作为 ready 范围

### Designer Workflow

状态: 稳定基础接口可用

- `Box::load_stylesheet()` 返回 `StylesheetId` 与结构化 `CssDiagnostic`
- `Box::replace_stylesheet()` / `remove_stylesheet()` 支持主题和实时预览的原子更新
- `CssLoadOptions::strict` 在存在 warning/error 时拒绝整次 load/replace
- 未支持属性和无效 size 值会报告来源、selector、property、value 与原因
- `Box::register_font()` / `unregister_font()` 通过 backend-neutral renderer facade 管理字体
- `width` / `height` 的 computed state 区分 `auto`、length、percentage 与 expression

当前限制：

- 嵌套 at-rule 被拆成 parser fragment 时，诊断仍保留 source 名，但部分 Lexbor 日志不能提供原文件精确行列
- 尚未实现 CSS cascade layer、origin 与 `@font-face`
- 文件 watch 属于宿主/Designer 职责；StyleEngine 只提供稳定 stylesheet lifecycle

最小使用方式：

```cpp
flexUI::CssLoadOptions options;
options.source = "theme.css";
options.strict = true;
auto loaded = box.load_stylesheet(css, options);
if (!loaded.applied) {
  // Present loaded.diagnostics in the designer diagnostics panel.
}

box.replace_stylesheet(loaded.stylesheet_id, edited_css, options);
box.register_font("Inter", "assets/Inter.ttf");
```

## 下一阶段优先级

- Designer inspector 展示 matched rules、specificity、source order、important 与 computed value provenance
- 诊断补齐原文件绝对 offset/行列和修复建议
- `@font-face` 适配到 renderer font facade
- cascade layers 与 origin，仅在设计系统迁移用例明确需要时引入
- 按 Stable / Partial / Unsupported 由 conformance tests 校验本矩阵

## 明确不建议现在直接引入的主题

- selector invalidation 的浏览器级复杂场景
- 完整 css-transitions 套件
- 完整 css-animations 套件
- 依赖 DOM / browser event loop / Web Animations API 的测试

## 落地原则

- 不全量搬运 WPT
- 每次只引入一小批与当前实现严格匹配的主题
- 每条对齐用例都要注明：
  - WPT 来源目录
  - 对应的 `flexUI` 能力点
  - 是否为裁剪版语义
  - 明确未覆盖的标准边界
