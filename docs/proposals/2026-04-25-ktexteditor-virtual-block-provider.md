# Proposal: View-Local Virtual Blocks for KTextEditor

## Summary
KTextEditor would benefit from a public virtual block API that lets plugins display ephemeral, view-local text blocks anchored to document positions. The first target use case is Copilot-style multi-line ghost text: the preview should use Kate's native text layout and rendering while staying outside the document buffer.

## Motivation
Kate plugins can currently use public APIs such as:
- `KTextEditor::InlineNoteProvider`
- `KTextEditor::View::cursorToCoordinate()`
- `KTextEditor::View::editorWidget()`
- `KTextEditor::MovingCursor`

These APIs allow overlays and inline notes, but a multi-line ghost suggestion has stronger layout requirements:
- first line appears at the cursor
- following lines appear as virtual document lines
- surrounding text shifts visually while the buffer stays unchanged
- font shaping, bidi, tabs, wrapping, folding, and HiDPI rendering match Kate's renderer
- accept/dismiss remain plugin-owned actions

The current plugin implementation uses an overlay as the practical public-API solution. It works for daily use, and it still draws above existing text. A framework-level virtual block API would let KTextEditor render suggestions through its own layout pipeline.

## Evidence from experiments
This project tested several public-API approaches:

1. **Single InlineNote with oversized height**
   - The visible paint area is constrained to one line height.

2. **Single InlineNote with newline text**
   - Qt can draw multiple lines, but the view clips and aligns as an inline note.

3. **Per-line InlineNote mapping**
   - Works only when real host lines already exist.
   - EOF and virtual-line insertion remain out of scope.

4. **Overlay attached to `editorWidget()`**
   - Works today.
   - Supports mid-line and EOF preview.
   - Provides view-local rendering and clean buffer semantics.
   - Lacks layout participation and native text reflow.

The screenshots and tests live under:
- `autotests/InlineNoteRenderingExperimentTest.cpp`
- `autotests/InlineNoteRenderingABTest.cpp`
- `autotests/GhostTextOverlayWidgetRenderingTest.cpp`
- `docs/assets/ghost-overlay-midline.png`
- `docs/assets/ghost-overlay-eof.png`

## Proposed API
Names are illustrative.

```cpp
namespace KTextEditor {

class VirtualBlock {
public:
    KTextEditor::Cursor anchor() const;
    QStringList lines() const;
    KTextEditor::Attribute::Ptr attribute() const;
    int priority() const;
    bool cursorSkipsBlock() const;
};

class VirtualBlockProvider : public QObject {
    Q_OBJECT
public:
    virtual QList<VirtualBlock> virtualBlocks(KTextEditor::View *view,
                                              const KTextEditor::Range &visibleRange) const = 0;

Q_SIGNALS:
    void virtualBlocksChanged(KTextEditor::View *view);
};

class View {
public:
    void registerVirtualBlockProvider(KTextEditor::VirtualBlockProvider *provider);
    void unregisterVirtualBlockProvider(KTextEditor::VirtualBlockProvider *provider);
};

} // namespace KTextEditor
```

## Semantics
- Virtual blocks are **view-local**.
- Virtual blocks are **ephemeral**.
- Virtual blocks are **absent from the document buffer**.
- Search, save, undo, redo, and line numbers continue to operate on real document text.
- Providers own accept/dismiss behavior.
- KTextEditor owns layout, painting, scrolling, wrapping, folding interaction, and coordinate mapping.

## Rendering model
A virtual block has two display parts:

1. **Inline head**
   - First line starts at the anchor cursor x position.
   - This supports FIM insertion at the cursor.

2. **Virtual continuation lines**
   - Remaining lines participate in vertical layout after the anchor visual line.
   - They use the same renderer, font, tab width, bidi handling, and theme attributes as normal text.

## Cursor and selection behavior
For ghost text, the default cursor behavior should skip virtual blocks:
- vertical movement steps over virtual lines
- mouse hit tests can return the anchor cursor plus virtual-block metadata
- selection excludes virtual text by default

Future API can allow richer behavior for diagnostics, code lenses, or generated preview blocks.

## Implementation phases
### Phase 1: Ghost-text MVP
- View-local providers
- Anchor cursor
- Plain `QStringList` lines
- Single attribute for the whole block
- Cursor skip behavior
- Visible-range invalidation

### Phase 2: Layout integration
- Dynamic wrap support
- Folding interaction
- Scrollbar extent updates
- Coordinate mapping for virtual lines

### Phase 3: Rich blocks
- Attribute spans
- Mouse hover metadata
- Accessibility text
- Conflict priority between providers

## Compatibility
This API can coexist with `InlineNoteProvider`:
- Inline notes remain ideal for short inlay annotations.
- Virtual blocks serve multi-line preview and ephemeral generated text.

## Kate Copilot integration sketch
The plugin would replace overlay painting with:

```cpp
class GhostTextVirtualBlockProvider final : public KTextEditor::VirtualBlockProvider {
public:
    QList<KTextEditor::VirtualBlock> virtualBlocks(KTextEditor::View *view,
                                                   const KTextEditor::Range &visibleRange) const override;
};
```

`EditorSession` would keep using `MovingCursor`, `GhostTextState`, and explicit accept/dismiss actions. The rendering path would become native KTextEditor layout instead of widget overlay painting.

## Expected benefit
The API gives Kate plugins a principled way to display generated, reviewable, ephemeral text. It also avoids editor-specific hacks for ghost completions, code transformation previews, generated tests, inline diffs, and AI explanations.
