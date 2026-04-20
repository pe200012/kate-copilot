2026-04-20 KTextEditor frameworks 级 virtual-lines 扩展概念设计：

- 目标：在 KTextEditor frameworks 层提供“view-local、ephemeral、non-buffer”的虚拟行/虚拟块能力，让插件能像 VS Code Copilot 一样显示多行 ghost text，同时复用 Kate 自身字体 shaping、layout、scroll、selection、HiDPI 渲染。
- 现状依据：InlineNoteProvider 文档明确 `inlineNoteSize()` 高度以 `note.lineHeight()` 为上限，超出部分会被裁剪；因此 public API 现有能力适合单行附注，无法表达插入多条虚拟行。
- 推荐 API 方向：新增 `KTextEditor::VirtualBlockProvider`（名称可调整），在 `KTextEditor::View` 上注册/注销，返回锚定到某个 `Cursor` 的 `VirtualBlock` 列表。`VirtualBlock` 至少包含：anchor、`QStringList lines`、样式/attribute、priority、view-local flags、cursor behavior。
- 对 Copilot 最关键的语义：
  1. 第一行仍可与当前行联排；
  2. 第 2..N 行作为虚拟行参与垂直布局；
  3. 不进入文档 buffer；
  4. 不污染 undo/save/search；
  5. 接受时由插件显式插入真实文本；
  6. 清理时只撤销 view presentation。
- 需要改动的内部层级：
  - `KateLayoutCache`：把虚拟块纳入 view line 计数与缓存失效；
  - `KateTextLayout` / layout structures：承载虚拟行片段；
  - `KateRenderer`：按 Kate 原生文本路径绘制虚拟块；
  - `KateViewInternal`：坐标映射、滚动条高度、鼠标 hit-test、cursor 上下移动、可见区更新。
- 设计重点：
  - per-view 生效，多个 view 可有不同虚拟块；
  - 跟随 folding / dyn-wrap / tab width / font zoom 更新；
  - 默认 cursor 行为建议为 skip，适合 ghost text；
  - 支持 priority 解决多个 provider 冲突。
- 推荐落地顺序：
  1. MVP：只支持 after-cursor 的只读虚拟块，view-local，cursor skip；
  2. 接通 Kate plugin 端实验；
  3. 再扩展点击、hover、attribute spans、RTL/BiDi、accessibility。