调研（2026-04-20）：KTextEditor/Kate 多行 ghost text 的可行技术路径。

1) InlineNoteProvider 的边界（来自 KDE API 文档）
- InlineNoteProvider 以“按行查询 + 单行绘制”为模型：`inlineNotes(int line)` 返回某行的列位置。
- `inlineNoteSize()` 文档明确高度约束：高度以 `note.lineHeight()` 为上限，渲染层会对超出行高的绘制进行裁剪。
  - 引用：KTextEditor::InlineNoteProvider 文档（inlineNoteSize 注释）
    https://api.kde.org/frameworks/ktexteditor/html/classKTextEditor_1_1InlineNoteProvider.html

2) KDE 生态对 InlineNote 的典型用法
- Kate 官方博客展示 InlineNote 主要用于单行内联对象（颜色预览方块），示例里 `inlineNoteSize` 返回 `QSize(note.lineHeight(), note.lineHeight())`。
  - https://kate-editor.org/2018/08/17/kate-gains-support-for-inline-notes
- KDevelop 博客展示 InlineNote 用于“在出错行旁边显示短文本诊断”，同样落在单行范围。
  - https://blog.david-redondo.de/kde/2020/02/28/problems.html

3) 其他编辑器的多行 ghost text 依赖“编辑器核心的虚拟文本/虚拟行能力”
- VS Code Copilot：ghost text 可能包含多行代码块，属于编辑器内联补全系统能力（多行 insertion 由核心渲染）。
  - https://code.visualstudio.com/docs/copilot/ai-powered-suggestions
- Neovim nvim-cmp：多行 ghost text 由 Neovim 的 virtual text/相关 API 支撑。
  - https://github.com/hrsh7th/nvim-cmp/issues/1862

4) Kate 插件（仅使用 public API）的可行实现路线
A) 单行 InlineNote + 其余行预览浮层（QWidget/QFrame 或 KTextEditor::Message/TextHint）
- 渲染与交互保持稳定，虚拟行观感较弱。

B) 像素下推 overlay（当前实现）
- 用 editorWidget 快照 pixmap + clip + 平移实现“后文下推”，再绘制第 2..N 行 ghost。
- 质量关键点：HiDPI DPR 对齐、像素对齐、与 KateRenderer 文本 hinting 差异。

C) 文档注入式 ghost（插入文本 + MovingRange/Attribute 渲染幽灵样式）
- 文本渲染与原生一致，虚拟行观感自然。
- 需要处理：modified 状态、undo/redo 分组、任意编辑导致建议失效时的清理策略。

D) Frameworks 级别扩展
- 设计一个“可变高度 inline note / block note / virtual lines”接口，进入 KTextEditor 渲染管线，获得 VS Code 风格能力；实现成本在 frameworks/渲染层。