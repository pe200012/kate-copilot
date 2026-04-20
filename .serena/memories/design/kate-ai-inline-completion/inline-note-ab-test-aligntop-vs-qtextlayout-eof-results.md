2026-04-20：A/B 渲染实验，对比单个 InlineNote 的两条路线：
1) `drawText(rect, AlignTop, fullText)`
2) `QTextLayout` 逐行绘制

场景固定：Python + 文末空白行 + 三行 suggestion，主判据是字形与基线一致性。

实现：
- 新增 `autotests/InlineNoteRenderingABTest.cpp`
- 新增测试目标 `kateaiinlinecompletion_inline_note_rendering_ab_test`
- 产出截图：
  - `/tmp/kate-ai-inline-note-rendering/ab_align_top_full.png`
  - `/tmp/kate-ai-inline-note-rendering/ab_align_top_crop.png`
  - `/tmp/kate-ai-inline-note-rendering/ab_align_top_return_crop.png`
  - `/tmp/kate-ai-inline-note-rendering/ab_qtextlayout_full.png`
  - `/tmp/kate-ai-inline-note-rendering/ab_qtextlayout_crop.png`
  - `/tmp/kate-ai-inline-note-rendering/ab_qtextlayout_return_crop.png`

结果：
- 两条路线在该 EOF 场景下生成完全相同的画面。
- full/crop 图逐像素对比一致，图像差分 `diff bbox = None`。
- 两条路线都只稳定显示首行 `if n <= 1:`，后两行没有进入可见区域。
- 测量日志：
  - `firstTopOffsetPx = 0`
  - `secondIndentStartPx = 0`
  - `blockHeightPx = 27`
  - `visibleLineCount = 2`

结论：
- 在“Python + EOF + 基线一致性优先”的场景里，方案 1 与方案 2 没有差距。
- 若只在二者中选主路径，方案 1 更合适，因为结果相同且实现更简单。
- 下一轮更有价值的比较对象是：当前 `AlignVCenter` 单 note 路线 vs 每行一个 note 路线。

验证：`cmake --build build -j 8 && ctest --test-dir build --output-on-failure` 8/8 passed。