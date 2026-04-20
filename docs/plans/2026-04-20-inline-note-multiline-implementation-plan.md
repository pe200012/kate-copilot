# InlineNote Multiline Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Render multiline ghost suggestions entirely through `KTextEditor::InlineNoteProvider`, with later lines displayed on subsequent real lines using the suggestion's own indentation.

**Architecture:** `GhostTextInlineNoteProvider` becomes the single rendering path. It splits `GhostTextState.visibleText` into logical lines, maps line 0 to the anchor column, maps later lines to column 0 on subsequent real lines, and emits repaint signals for every affected line. `EditorSession` stops creating and syncing `GhostTextPushDownOverlay`.

**Tech Stack:** C++17, Qt6, KF6::TextEditor, QtTest

---

### Task 1: Add a failing provider test

**Files:**
- Create: `autotests/GhostTextInlineNoteProviderTest.cpp`
- Modify: `autotests/CMakeLists.txt`
- Test: `build/bin/kateaiinlinecompletion_ghost_text_inline_note_provider_test`

**Step 1: Write the failing test**
Create tests for:
- multiline suggestions returning notes on anchor line and following real lines
- `setState()` emitting `inlineNotesChanged` for the full affected range

**Step 2: Run test to verify it fails**
Run: `ctest --test-dir build -R kateaiinlinecompletion_ghost_text_inline_note_provider_test --output-on-failure`
Expected: FAIL because provider only exposes the first line and only refreshes the anchor line.

**Step 3: Write minimal implementation**
Update `GhostTextInlineNoteProvider` to:
- split `visibleText` with `Qt::KeepEmptyParts`
- return `{anchor.column}` for line 0 and `{0}` for later lines
- choose note text per `note.position().line()`
- emit `inlineNotesChanged` for all lines in the old/new visible range union

**Step 4: Run test to verify it passes**
Run: `ctest --test-dir build -R kateaiinlinecompletion_ghost_text_inline_note_provider_test --output-on-failure`
Expected: PASS

### Task 2: Remove overlay from EditorSession main path

**Files:**
- Modify: `src/session/EditorSession.h`
- Modify: `src/session/EditorSession.cpp`
- Test: `ctest --test-dir build --output-on-failure`

**Step 1: Write the failing test**
Use the provider test from Task 1 as the rendering contract. Keep it green while removing overlay wiring.

**Step 2: Run targeted verification before editing**
Run: `ctest --test-dir build -R kateaiinlinecompletion_ghost_text_inline_note_provider_test --output-on-failure`
Expected: PASS

**Step 3: Write minimal implementation**
Remove:
- `GhostTextPushDownOverlay` include and forward declaration from `EditorSession`
- overlay member
- overlay construction, scroll sync, and state sync calls

Keep:
- inline note provider registration
- request lifecycle
- `Tab` / `Esc`
- `clearSuggestion()` and `setSuppressed()` state updates through the note provider

**Step 4: Run full verification**
Run: `cmake --build build -j 8 && ctest --test-dir build --output-on-failure`
Expected: build success, all tests pass

### Task 3: Document implementation results

**Files:**
- Modify: `docs/plans/2026-04-20-kate-ai-inline-note-multiline-design.md`

**Step 1: Append implementation results**
Record:
- files changed
- test results
- any deviation from the design

**Step 2: Verification**
Reuse the full verification output from Task 2.

**Step 3: Commit**
Use jj to record the change after verification.