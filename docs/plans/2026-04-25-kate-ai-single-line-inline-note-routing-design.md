# Kate AI Single-Line InlineNote Routing Design

- Date: 2026-04-25
- Status: Implemented
- Scope: Route single-line ghost suggestions through `InlineNoteProvider`, while multiline suggestions continue using overlay.

## Background
The current overlay path renders all ghost text above `editorWidget()`. This works for multiline suggestions, but a single-line ghost can overlap other editor overlays or status decorations. `KTextEditor::InlineNoteProvider` reserves horizontal space in the line layout, which fits single-line suggestions better.

KTextEditor API reference confirms:
- `InlineNoteProvider` adapts the text layout to create space for notes.
- `inlineNoteSize()` reserves width, with height clipped to `note.lineHeight()`.
- `View::registerInlineNoteProvider()` and `unregisterInlineNoteProvider()` are the correct lifecycle API.

Reference:
- https://api.kde.org/ktexteditor-view.html
- https://api.kde.org/frameworks/ktexteditor/html/classKTextEditor_1_1InlineNoteProvider.html

## Problem
Single-line ghost text drawn by overlay can visually collide with other editor-layer content. Multiline ghost text still needs overlay because `InlineNoteProvider` clips note height to one line and cannot create virtual lines.

## Questions and Answers
### Q1. What decides the renderer?
A1. The visible suggestion text decides the renderer:
- text with no `\n` -> `InlineNoteProvider`
- text containing `\n` -> `GhostTextOverlayWidget`

### Q2. How is state synchronized?
A2. `EditorSession::applyStateToOverlay()` becomes a generic render-state sync point. It updates a single-line state for InlineNote and a multiline state for overlay.

### Q3. How are stale notes cleared when switching renderer?
A3. Each sync emits a cleared state to the inactive renderer. This prevents overlay and inline note from painting the same suggestion.

### Q4. How does acceptance continue working?
A4. Acceptance logic stays in `EditorSession` and uses the same `GhostTextState`. Renderer selection affects view presentation only.

## Design
### EditorSession
- Add `std::unique_ptr<GhostTextInlineNoteProvider> m_inlineNoteProvider`.
- Register it in the constructor after view creation.
- Unregister it in the destructor.
- Add helpers:
  - `bool shouldRenderInlineNote(const GhostTextState &state) const`
  - `bool shouldRenderOverlay(const GhostTextState &state) const`
  - `GhostTextState clearedRenderState() const`
- `applyStateToOverlay()` routes:
  - single-line state -> inline note, cleared state -> overlay
  - multiline state -> overlay, cleared state -> inline note
  - invisible state -> cleared state to both

### GhostTextInlineNoteProvider
- Keep provider renderability state-driven so standalone rendering tests and experiments can set a raw `GhostTextState` directly.
- Keep single-line painting behavior: one note at `anchor.column` on `anchor.line`.
- Enforce `anchorTracked` at the `EditorSession` routing layer.

### Tests
- `EditorSessionIntegrationTest::singleLineSuggestionUsesInlineNoteInsteadOfOverlay()` verifies single-line suggestions remain visible at session level and keep overlay inactive.
- `EditorSessionIntegrationTest::multilineSuggestionUsesOverlay()` verifies multiline suggestions still activate overlay.
- Existing accept, dismiss, focus, and anchor tests use session visibility instead of overlay-only assumptions.

## Implementation Results
- `EditorSession` now registers both renderer paths and routes by visible text line count.
- `EditorSession` enforces `anchorTracked` before routing to either renderer.
- Integration tests cover single-line inline route and multiline overlay route.
- Verification: `cmake --build build -j 8 && ctest --test-dir build --output-on-failure` => 12/12 passed.
