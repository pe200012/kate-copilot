# Kate AI Context-Aware Completion Phase 1 Design

- Date: 2026-05-16
- Status: In progress
- Scope: Add synchronous context providers, prompt assembly, and single-range suggestion post-processing for current-line overlap replacement.

## Background
The plugin already has a per-view `EditorSession`, FIM prompt templates, OpenAI-compatible streaming, Ollama compatibility, Copilot Codex completion prompts, and ghost rendering through InlineNote/overlay routing. Current requests use only current-file prefix and suffix. Better completion quality needs small deterministic context that can later grow into diagnostics, recent edits, related files, and symbols.

KTextEditor API reference confirms `View::document()`, `Document::views()`, and `MainWindow::views()` expose the open editor surface needed for a cheap open-tabs provider. `Document::replaceText(KTextEditor::Range, QString)` provides the single-range writeback path for rest-of-line overlap replacement.

References:
- https://api.kde.org/ktexteditor-view.html
- https://api.kde.org/frameworks/ktexteditor/html/ktexteditor_8cpp_source.html
- https://api.kde.org/ktexteditor.html

## Problem
Current prompts lack repository/editor context. Current acceptance always inserts at the cursor, which duplicates text when a completion includes text already present after the cursor on the same line.

## Questions and Answers
### Q1. How much context should Phase 1 collect?
A1. Phase 1 collects lightweight deterministic context only: current file traits, project traits from nearby files, and snippets from same-language open tabs.

### Q2. How does provider compatibility stay stable?
A2. OpenAI-compatible and Ollama requests keep chat-completions with system/user prompts. The context prefix is prepended to the existing user prompt. Copilot Codex keeps `prompt` plus `suffix`; the same context prefix is prepended to the completion prompt.

### Q3. How does replacement work?
A3. `SuggestionPostProcessor` computes how many characters at the end of the suggestion overlap the current line text after the cursor. Full accept uses `Document::replaceText(range, insertText)` for the cursor-to-overlap range. Rendering shows only `displayText`, which omits the overlapped tail already visible in the document.

### Q4. What remains Phase 2 work?
A4. Recent edits, diagnostics, related-file scanning, symbol extraction, project exclusion rules, UI controls for every provider, and multi-range inline edits remain Phase 2+ work.

## Design
### Context Engine
Add:
- `src/context/ContextItem.h`: `ContextItem` and `ContextResolveRequest` data structs.
- `src/context/ContextProvider.h`: synchronous provider interface.
- `src/context/ContextProviderRegistry.h/.cpp`: provider ownership, match-score ordering, deterministic item ordering, and max item limiting.
- `src/context/CurrentFileContextProvider.h/.cpp`: emits traits for file path, language, cursor line, and cursor column.
- `src/context/ProjectTraitsContextProvider.h/.cpp`: walks upward from the current local file, detects `.git`, build markers, and cheap framework hints from marker file names/content.
- `src/context/OpenTabsContextProvider.h/.cpp`: reads same-language documents from `KTextEditor::MainWindow::views()`, excludes the active/current URI, limits document size and snippet budget, and emits code snippets with relative paths when a project root is available.

### PromptAssembler
Add `src/prompt/PromptAssembler.h/.cpp`:
- `PromptAssemblyOptions { enabled, maxContextItems, maxContextChars }`.
- `PromptAssembler::build(templateId, PromptContext, ContextItem[], options)` returns the existing `BuiltPrompt` shape with a context prefix prepended to `userPrompt`.
- `PromptAssembler::renderContextPrefix(...)` exposes the same prefix for Copilot Codex prompt construction.
- Context blocks use deterministic ordering, line comments appropriate for common language names, and a hard character budget.

### SuggestionPostProcessor
Add `src/session/SuggestionPostProcessor.h/.cpp`:
- Reuse `PromptTemplate::sanitizeCompletion()`.
- Filter empty/marker-only completions.
- Filter completions duplicating the next non-empty line.
- Trim trailing duplicated close-token lines when the next non-empty line is `}`, `)`, `]`, or `end`.
- Compute `suffixCoverage`, `displayText`, `insertText`, and `replaceRange`.

Extend `GhostTextState`:
- Keep `visibleText` as the rendering field for current renderer compatibility.
- Add `insertText`, `KTextEditor::Range replaceRange`, and `int suffixCoverage`.

Update `EditorSession`:
- Build context items after `PromptContext` creation when `EnableContextualPrompt` is true.
- Use `PromptAssembler` for OpenAI/Ollama prompts.
- Prefix Copilot Codex prompt with `PromptAssembler::renderContextPrefix(...)`.
- Process streamed suggestions before updating `GhostTextState`.
- Full accept calls `Document::replaceText(m_state.replaceRange, m_state.insertText)` when the state is valid.
- Partial accept remains insert-based and uses `visibleText` chunks.

### Settings
Add persistence-only settings:
- `enableContextualPrompt`, default `true`.
- `maxContextItems`, default `6`.
- `maxContextChars`, default `6000`.
Validation clamps count and character budgets to bounded values.

### Tests
Add:
- `autotests/ContextProviderRegistryTest.cpp`
- `autotests/PromptAssemblerTest.cpp`
- `autotests/SuggestionPostProcessorTest.cpp`

Update:
- `autotests/CompletionSettingsTest.cpp`
- `autotests/EditorSessionIntegrationTest.cpp`
- `src/CMakeLists.txt`
- `autotests/CMakeLists.txt`

## Implementation Plan
1. Add tests for settings, registry ordering/limits, prompt rendering/budgeting, and suggestion post-processing.
2. Add context model, providers, and registry.
3. Add prompt assembler and hook OpenAI/Ollama/Copilot request construction.
4. Add suggestion post-processing and replace-range acceptance.
5. Update CMake integration.
6. Run `cmake -S . -B build -DBUILD_TESTING=ON`, `cmake --build build -j`, and `ctest --test-dir build --output-on-failure`.

## Examples
✅ Context block before existing FIM prompt:
```text
// Related project information:
// build_system: CMake
// framework: KDE Frameworks 6 / Qt 6

// Compare this snippet from src/foo.h:
class Foo {};

<|fim_prefix|>...
<|fim_suffix|>...
<|fim_middle|>
```

✅ Rest-of-line overlap:
```text
Document line: prefixSUFFIX
Cursor: after prefix
Model text: ghost()SUFFIX
Display text: ghost()
Replace range: cursor through SUFFIX
Insert text: ghost()SUFFIX
```

## Trade-offs
- Synchronous providers keep Phase 1 simple and testable. The small limits keep UI latency bounded.
- The context prefix is plain text and comment-oriented. This preserves provider compatibility and avoids provider-specific message schemas.
- Open-tabs snippets use open documents only. This provides useful context while avoiding repository scans and ignore-file complexity in Phase 1.

## Implementation Results
- Added `src/context/` with `ContextItem`, `ContextProvider`, `ContextProviderRegistry`, `CurrentFileContextProvider`, `ProjectTraitsContextProvider`, and `OpenTabsContextProvider`.
- Added `src/prompt/PromptAssembler.*` and integrated it into OpenAI-compatible, Ollama, and Copilot Codex request construction in `EditorSession`.
- Added `src/session/SuggestionPostProcessor.*`, extended `GhostTextState`, and changed full accept to use `Document::replaceText()` for valid single-line overlap ranges.
- Added persistence settings for `EnableContextualPrompt`, `MaxContextItems`, and `MaxContextChars`.
- Added tests: `ContextProviderRegistryTest`, `PromptAssemblerTest`, `SuggestionPostProcessorTest`, settings coverage, and an integration regression for accepting a completion that overlaps text after the cursor.
- Verification: `cmake --build build -j 8 && ctest --test-dir build --output-on-failure` => 15/15 passed.

### Deviations from original design
- Open-tabs ordering uses `KTextEditor::MainWindow::views()` order because the available public API does not expose a richer recent-activity history.
- Settings are persistence-only in this phase. A full contextual prompt UI remains Phase 2 work.
