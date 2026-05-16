# Kate AI DiagnosticsContextProvider Design

- Date: 2026-05-16
- Status: Planned
- Scope: Phase 2B only — diagnostic context store and provider for contextual prompts.

## Background
Phase 1 added deterministic context providers and prompt assembly. Phase 2A added recent edit summaries. Diagnostics are the next compact signal: errors and warnings near the cursor often describe the exact constraint the completion should satisfy.

KTextEditor public headers in the current KF6 install expose document text, marks, messages, and editing signals. They do not expose the Kate LSP client diagnostic list as a stable public KTextEditor API. Kate's LSP plugin keeps diagnostics inside the Kate application/addon layer. Phase 2B therefore adds a production diagnostic store plus provider API; a future Kate/LSP adapter can feed the store without changing prompt assembly.

## Problem
The completion request currently has no structured diagnostics context. Models can complete code that preserves an existing error or ignores a warning. We need bounded, deterministic diagnostic context that is safe to include in prompts.

## Questions and Answers
### Q1. Where do diagnostics live?
A1. `DiagnosticStore` owns diagnostics keyed by URI. It supports replace-per-file and clear operations. The store is synchronous and bounded.

### Q2. How does provider ranking work?
A2. `DiagnosticsContextProvider` reads the store, prefers active-file diagnostics, then cursor-near diagnostics, then severity order: error, warning, information, hint. It uses deterministic tie-breakers.

### Q3. How is prompt rendering shaped?
A3. The provider emits `ContextItem::Kind::DiagnosticBag` items with `providerId = "diagnostics"`. `PromptAssembler` renders them as comment-prefixed diagnostic blocks before ordinary snippets.

### Q4. How do settings work?
A4. New settings are persistence-first:
- `EnableDiagnosticsContext = true`
- `DiagnosticsMaxItems = 8`
- `DiagnosticsMaxChars = 3000`
- `DiagnosticsMaxLineDistance = 120`
- `DiagnosticsIncludeWarnings = true`
- `DiagnosticsIncludeInformation = false`
- `DiagnosticsIncludeHints = false`

## Design
### Data model
`src/context/DiagnosticItem.h`:
```cpp
struct DiagnosticItem {
    enum class Severity { Error, Warning, Information, Hint };
    QString uri;
    Severity severity = Severity::Information;
    int startLine = 0;
    int startColumn = 0;
    int endLine = 0;
    int endColumn = 0;
    QString source;
    QString code;
    QString message;
    QDateTime timestamp;
};
```

### Store
`DiagnosticStore`:
- `setDiagnostics(uri, diagnostics)` replaces diagnostics for a file.
- `clearDiagnostics(uri)` and `clear()` remove entries.
- `diagnostics(uri)` returns one file.
- `allDiagnostics()` returns deterministic flattened diagnostics.
- Empty messages are skipped.
- Invalid timestamps are normalized to UTC now.
- Lines/columns are clamped to non-negative values.

### Provider
`DiagnosticsContextProvider`:
- `id() == "diagnostics"`
- `matchScore() == 75` when enabled and store exists.
- Filters diagnostics by settings.
- Includes active-file diagnostics first.
- Keeps diagnostics within `DiagnosticsMaxLineDistance` when cursor is valid, while still allowing other active-file diagnostics after nearby ones if budget remains.
- Renders each item value as one diagnostic line: `42:13 - error CLANG-Wunused-variable: unused variable 'x'`.
- Emits one `DiagnosticBag` item per file so `PromptAssembler` can print a file header.

### PromptAssembler
Add diagnostic rendering:
```text
// Consider these diagnostics from src/foo.cpp:
// 42:13 - error CLANG: use of undeclared identifier 'bar'
// 57:9 - warning CLANG-Wunused-variable: unused variable 'x'
```

### Integration
`KateAiInlineCompletionPluginView` owns a `DiagnosticStore` per main window and passes it to each `EditorSession`. `EditorSession` adds `DiagnosticsContextProvider` to the registry when contextual prompts and diagnostics context are enabled.

## Implementation Plan
1. Add diagnostic model/store/provider files and CMake entries.
2. Add diagnostics settings validation, load, and save.
3. Preserve hidden diagnostics settings in `KateAiConfigPage::readUi()` through existing settings-copy behavior.
4. Extend `PromptAssembler` to render `DiagnosticBag` items.
5. Wire `DiagnosticStore` through `PluginView` into `EditorSession` and add provider to request context collection.
6. Add tests for store filtering, provider sorting/filtering/budgeting, prompt rendering, and settings persistence.
7. Run full build and CTest.

## Examples
✅ Near active-file error:
```text
// Consider these diagnostics from src/main.cpp:
// 42:13 - error CLANG: use of undeclared identifier 'bar'
```

✅ Warning with code:
```text
// 57:9 - warning CLANG-Wunused-variable: unused variable 'x'
```

❌ Empty diagnostic:
```text
// 10:1 - error CLANG:
```
Skipped because message is empty.

## Trade-offs
- The store-provider split keeps Phase 2B independent from Kate's LSP plugin internals.
- Prompt rendering is compact and deterministic for testing.
- A later adapter can populate `DiagnosticStore` from a stable public API or Kate-internal integration point.

## Implementation Results
- Added `src/context/DiagnosticItem.h`, `DiagnosticStore.{h,cpp}`, and `DiagnosticsContextProvider.{h,cpp}`.
- `DiagnosticStore` supports per-URI replacement, clearing, deterministic flattening, empty-message filtering, range normalization, and timestamp normalization.
- `DiagnosticsContextProvider` emits `ContextItem::Kind::DiagnosticBag` items with provider id `diagnostics`, active-file priority, cursor-distance ordering, severity ordering, total item limits, character budgets, and severity filters.
- `PromptAssembler` renders diagnostic bags as comment-prefixed diagnostic blocks before ordinary snippets.
- `KateAiInlineCompletionPluginView` owns a per-main-window `DiagnosticStore` and passes it to `EditorSession`.
- `EditorSession` adds `DiagnosticsContextProvider` to the context registry when contextual prompts and diagnostics context are enabled.
- `CompletionSettings` persists and validates diagnostics settings.
- Tests added: `DiagnosticStoreTest`, `DiagnosticsContextProviderTest`, prompt rendering coverage, settings round-trip/clamping, config-page hidden setting preservation, and updated integration wiring.
- Reviewer follow-up addressed diagnostics ranking under default context budgets and plugin-view/session teardown lifetime by deleting sessions before context services are destroyed.
- Verification: `cmake --build build -j 8 && ctest --test-dir build --output-on-failure` => 19/19 passed.

### Deviations from original design
- Phase 2B provides a functional store/provider path and plugin-owned store, while a live Kate LSP diagnostics adapter remains future work because current public KTextEditor headers do not expose Kate LSP diagnostics as a stable API.
