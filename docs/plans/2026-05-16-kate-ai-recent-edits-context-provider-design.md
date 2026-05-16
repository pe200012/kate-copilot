# Kate AI Recent Edits Context Provider Design

- Date: 2026-05-16
- Status: Planned
- Scope: Phase 2A only, recent edit context for contextual prompts.

## Background
Phase 1 added synchronous context providers, `PromptAssembler`, and range-aware suggestion post-processing. Completion requests already collect bounded context in `EditorSession` and prepend it to OpenAI-compatible/Ollama prompts, with a Copilot Codex prefix path.

KTextEditor exposes document edit signals suitable for line-based tracking:
- `Document::textChanged(Document *)`
- `Document::textInserted(Document *, Cursor, QString)`
- `Document::textRemoved(Document *, Range, QString)`

Reference:
- https://api.kde.org/ktexteditor-document.html

## Problem
The model sees current-file prefix/suffix and static project/open-tab context. It does not see the user's recent edit pattern across files, so repeated transformations require more manual prompting.

## Questions and Answers
### Q1. Where should recent edit state live?
A1. `KateAiInlineCompletionPluginView` owns one `RecentEditsTracker` per main window. It observes open documents from that window and passes the tracker to per-view `EditorSession` objects.

### Q2. What context item kind should recent edits use?
A2. `RecentEditsContextProvider` emits `ContextItem::Kind::CodeSnippet` with provider id `recent-edits`. `PromptAssembler` renders this provider with a dedicated recent-edits block.

### Q3. How are active-file edits near the cursor handled?
A3. The provider filters edits whose URI matches the active request URI and whose line range is within `RecentEditsActiveDocDistanceLimitFromCursor` lines of the cursor.

### Q4. How are edits bounded?
A4. The tracker caps document size, history size, files, lines per edit, and characters per edit. The provider caps emitted edits through the settings values and the existing `MaxContextItems` budget.

## Design
### Data model
`src/context/RecentEdit.h` defines:
```cpp
struct RecentEdit {
    QString uri;
    QDateTime timestamp;
    int startLine = 0;
    int endLine = 0;
    QString beforeText;
    QString afterText;
    QString summary;
};
```
Lines are zero-based internally. Rendered prompt line ranges are one-based.

### RecentEditsTracker
`src/context/RecentEditsTracker.{h,cpp}` is a `QObject` that:
- tracks `KTextEditor::Document *` objects,
- stores line snapshots per document,
- debounces `textChanged` with per-document timers,
- computes compact before/after line summaries,
- merges close edits in the same URI,
- prunes history by max edits and max files,
- exposes `recentEdits()` and `flushPendingEdits()` for tests and deterministic lifecycle handling.

### RecentEditsContextProvider
`src/context/RecentEditsContextProvider.{h,cpp}` reads from `RecentEditsTracker`, filters active-file cursor-near edits, renders compact values:
```text
File: src/foo.cpp
@@ lines 40-47
- old line
+ new line
```
The provider emits high-importance items so recent edit context appears before static open-tab snippets under tight budgets.

### PromptAssembler
`PromptAssembler` gains a provider-specific rendering path:
```text
// Recently edited files. Continue the user's current edit pattern.
// File: src/foo.cpp
@@ lines 40-47
- old line
+ new line
// End of recent edits
```
The comment prefix follows the active language.

### Settings
Add persistence-only settings:
- `EnableRecentEditsContext = true`
- `RecentEditsMaxFiles = 20`
- `RecentEditsMaxEdits = 8`
- `RecentEditsDiffContextLines = 3`
- `RecentEditsMaxCharsPerEdit = 2000`
- `RecentEditsDebounceMs = 500`
- `RecentEditsMaxLinesPerEdit = 10`
- `RecentEditsActiveDocDistanceLimitFromCursor = 100`

### Integration
- `KateAiInlineCompletionPluginView` owns `RecentEditsTracker` and tracks documents as views are seen.
- `EditorSession` receives the tracker pointer and adds `RecentEditsContextProvider` to the per-request registry when enabled.
- Provider auth, network payload shape, and existing renderer behavior stay unchanged.

## Implementation Plan
1. Add tests for `RecentEditsTracker`, `RecentEditsContextProvider`, settings persistence, and recent-edit rendering in `PromptAssemblerTest`.
2. Add data model and tracker with deterministic flush API.
3. Add provider and PromptAssembler recent-edit block rendering.
4. Wire settings, CMake, PluginView ownership, and EditorSession registry integration.
5. Run build and CTest.

## Examples
✅ Recent cross-file edit:
```text
// Recently edited files. Continue the user's current edit pattern.
// File: src/foo.cpp
@@ lines 12-14
- oldName();
+ newName();
// End of recent edits
```

✅ Active file edit far from cursor can be included when it represents a file-wide pattern.

✅ Active file edit near cursor is filtered because current prefix/suffix already carries that context.

## Trade-offs
- Line snapshots keep the tracker simple and deterministic. This is adequate for compact prompt summaries.
- Per-main-window tracking avoids global cross-window leakage.
- The first implementation tracks documents after the plugin sees their views. A later repository context layer can add project-wide edit history.

## Implementation Results
- Added `RecentEdit`, `RecentEditsTracker`, and `RecentEditsContextProvider` under `src/context/`.
- `KateAiInlineCompletionPluginView` owns one `RecentEditsTracker` per main window, tracks known view documents, updates tracker options from settings, and clears tracking when recent-edit context is disabled.
- `EditorSession` receives the tracker pointer and adds `RecentEditsContextProvider` to the context registry when contextual prompts and recent edits are enabled.
- `PromptAssembler` renders `recent-edits` items as a dedicated edit-pattern block before ordinary snippets.
- `CompletionSettings` persists and validates the recent-edit context settings.
- `KateAiConfigPage::readUi()` now preserves persistence-only settings while applying visible UI fields.
- Tests added for tracker line edits, insertion/deletion summaries, pending flush on untrack, provider filtering, prompt rendering, settings persistence, config-page preservation, and existing integration behavior.
- Verification: `cmake -S . -B build -DBUILD_TESTING=ON && cmake --build build -j 8 && ctest --test-dir build --output-on-failure` => 17/17 passed.

### Deviations from original design
- The tracker uses `Document::textChanged` plus line snapshots instead of individual insert/remove signal reconstruction. This gives deterministic summaries and avoids depending on KTextEditor edit transaction internals.
- Tracking is per main window and begins after the plugin sees a document view. Project-wide historical edit mining remains future work.
