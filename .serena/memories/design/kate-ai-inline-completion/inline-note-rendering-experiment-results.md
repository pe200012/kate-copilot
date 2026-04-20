2026-04-20 InlineNote 渲染实证结果：

- 新增 GUI 像素实验测试 `autotests/InlineNoteRenderingExperimentTest.cpp`，直接创建 KTextEditor::View，注册自定义 InlineNoteProvider，抓取真实渲染截图并做颜色断言。
- 截图路径：`/tmp/kate-ai-inline-note-rendering/oversize_single_note.png`、`multiline_single_note.png`、`per_line_notes.png`。
- 三个实验：
  1. OversizeSingleNote：单个 note 请求 `height = 3 * lineHeight`，paint 填充整块洋红。结果只看到单行高度的洋红带，说明超高区域被裁剪。
  2. MultilineSingleNote：单个 note 在 y=0..3*lineHeight 画三条色带（洋红/橙/青）。结果只有首条洋红可见，第二三条完全被裁剪。
  3. PerLineNotes：分别在连续三个真实行返回 note，画洋红/绿/蓝三条色带。结果三条色带分别出现在三个真实行内。
- 结论：
  - InlineNoteProvider 的实际可见绘制区域受单行高度约束。
  - 单个 note 无法制造多行虚拟块，也无法把后文整体下推。
  - 通过多个真实行分别返回 note，可以得到“多行都有附注”的视觉效果；该效果依附现有真实行，不提供虚拟插入行布局。
- 验证：`ctest --test-dir build --output-on-failure` 5/5 passed。