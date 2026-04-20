# Kate AI InlineNote EOF Baseline A/B Design

## Background
当前多行 ghost text 已有两条可行路线：
1. 单个 InlineNote，`drawText(rect, AlignTop, fullText)`
2. 单个 InlineNote，`QTextLayout` 逐行绘制

用户要求直接比较这两条路线在 **Python + 文末位置** 场景下的观感，主判据选择为 **字形与基线一致性**。

## Problem
我们需要在同一份 `KTextEditor::View`、同一字体、同一颜色、同一锚点位置下，对比两种绘制方式的真实渲染结果，并给出清晰证据：截图、局部裁剪图、像素测量结果、滚动后重绘结果。

## Questions and Answers
### Q1. 场景固定为什么？
A1. 固定为 Python 文件、文末位置、三行缩进块 suggestion。这个场景最容易放大基线漂移、缩进观感、裁切与重绘问题。

### Q2. 本次对比的 suggestion 文本是什么？
A2. 固定为：
```python
if n <= 1:
    return n
return fib(n - 1) + fib(n - 2)
```

### Q3. 主判据是什么？
A3. 主判据是字形与基线一致性。次级判据是文末多行可见范围与滚动后的重绘稳定性。

## Design
新增 GUI 渲染测试：`autotests/InlineNoteRenderingABTest.cpp`。

测试文件内定义两种最小 provider：
- `AlignTopSingleNoteProvider`
  - 单个 note，挂在锚点行
  - `inlineNoteSize()` 返回最大行宽与总文本高度
  - `paintInlineNote()` 调用 `drawText(rect, AlignLeft | AlignTop, fullText)`
- `QTextLayoutSingleNoteProvider`
  - 单个 note，挂在锚点行
  - `inlineNoteSize()` 返回同样尺寸
  - `paintInlineNote()` 使用 `QTextLayout` 做 layout，并逐行绘制

测试 harness：
- 创建统一的 `RenderScenario { documentText, anchorLine, anchorColumn, suggestionText }`
- 建立 `KTextEditor::View`
- 注册 provider
- 抓取整图与锚点附近裁剪图
- 输出截图到 `/tmp/kate-ai-inline-note-rendering/`
- 记录像素指标：
  - 首行顶边偏移
  - 第二行缩进起点
  - 三行文本块总高度
- 做一次“离开锚点再回到锚点”的重绘复测

## Implementation Plan
1. 在 `autotests/CMakeLists.txt` 注册 `InlineNoteRenderingABTest.cpp`。
2. 先写失败测试，直接引用待实现的 provider 与截图产物，确认红灯。
3. 在测试文件中实现统一场景与两个 provider。
4. 生成 full/crop/scroll-back 截图与像素测量日志。
5. 运行单测并检查结果。
6. 运行全量构建与测试。
7. 在文档末尾追加实现结果与推荐路线。

## Examples
### ✅ AlignTop 路线
- 单个 note
- `drawText(rect, Qt::AlignLeft | Qt::AlignTop, fullText)`
- rect 高度为三行文本块总高度

### ✅ QTextLayout 路线
- 单个 note
- `QTextLayout` 按 `lineHeight` 逐行放置并绘制

### ❌ 混入其他变量
- 不同字体
- 不同颜色
- 不同锚点位置
- 不同 suggestion 文本

## Trade-offs
- A/B 测试成本主要在 GUI 渲染 harness，收益是可以直接用真实截图比较观感。
- `AlignTop` 方案实现更短。
- `QTextLayout` 方案对每一行 y 位置控制更强。
## Implementation Results
- 已新增 `autotests/InlineNoteRenderingABTest.cpp`。
- 已在 `autotests/CMakeLists.txt` 注册 `kateaiinlinecompletion_inline_note_rendering_ab_test`。
- 统一场景：
  - Python 风格文档，锚点位于文末空白行
  - suggestion 固定为三行：
    ```python
    if n <= 1:
        return n
    return fib(n - 1) + fib(n - 2)
    ```
- 已产出调试截图：
  - `/tmp/kate-ai-inline-note-rendering/ab_align_top_full.png`
  - `/tmp/kate-ai-inline-note-rendering/ab_align_top_crop.png`
  - `/tmp/kate-ai-inline-note-rendering/ab_align_top_return_crop.png`
  - `/tmp/kate-ai-inline-note-rendering/ab_qtextlayout_full.png`
  - `/tmp/kate-ai-inline-note-rendering/ab_qtextlayout_crop.png`
  - `/tmp/kate-ai-inline-note-rendering/ab_qtextlayout_return_crop.png`
- 实测结果：
  - `AlignTop + 高 rect` 与 `QTextLayout` 逐行绘制在这个 EOF 场景下生成了相同画面。
  - 两组 full/crop 截图逐像素对比结果一致。
  - 图像差分结果：`diff bbox = None`，说明两条路线在当前场景下没有可见差异。
  - 两条路线都只稳定显示首行 `if n <= 1:`，后两行没有进入可见区域。
- 测量日志：
  - `firstTopOffsetPx = 0`
  - `secondIndentStartPx = 0`
  - `blockHeightPx = 27`
  - `visibleLineCount = 2`（变化像素带覆盖到第二条 band，正文层面仍以首行为主）
- 推荐：
  - 在“Python + EOF + 字形与基线一致性优先”这一场景里，方案 1 与方案 2 没有拉开差距。
  - 若只在这两条里选主路径，方案 1 更合适，因为结果相同且实现更简单。
  - 若目标是继续改善多行观感，下一轮应比较“当前 `AlignVCenter` 单 note 路线”与“每行一个 note 路线”。
- 验证结果：
  - `cmake --build build -j 8 && ctest --test-dir build --output-on-failure`：8/8 passed
