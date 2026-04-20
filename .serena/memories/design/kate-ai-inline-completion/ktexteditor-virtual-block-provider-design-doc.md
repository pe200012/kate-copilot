2026-04-20 新增设计文档 `docs/plans/2026-04-20-ktexteditor-virtual-block-provider-design.md`。

核心内容：
- 提议在 KTextEditor frameworks 层新增 `VirtualBlockProvider` public API，而非复用 InlineNoteProvider。
- 新 public types：`VirtualBlock`、`VirtualBlockProvider`、`VirtualBlockCursorBehavior::Skip`。
- 新 View API：`registerVirtualBlockProvider()` / `unregisterVirtualBlockProvider()`。
- Phase 1 范围：行尾锚点、view-local、ephemeral、non-buffer、多行虚拟块；`lines[0]` 作为 inline continuation，`lines[1..]` 作为虚拟行插入到锚点真实行之后。
- Validation rules：anchor 有效、`QStringList` 非空、元素内无 `\n`、行尾锚点限定、同锚点按 priority 决议。
- Upstream target files：
  - `src/include/ktexteditor/virtualblockprovider.h`
  - `src/include/ktexteditor/view.h`
  - `src/view/kateview.{h,cpp}`
  - `src/view/virtualblockdata.h`
  - `src/render/katelayoutcache.{h,cpp}`
  - `src/render/katerenderer.{h,cpp}`
  - `src/view/kateviewinternal.{h,cpp}`
- 内部策略：保持 `KateTextLayout` 主体模型，Phase 1 通过“每个真实行追加 virtual block line count”接入 layout cache 与 renderer。
- 后续 phases：先 public API 与 provider wiring，再 layout mapping，再 renderer，再 screenshot tests。