# Kate AI Phase 3A CompletionStrategyEngine Design

## Background

Phases 1 and 2 built the context pipeline, prompt assembly, post-processing, recent edits, diagnostics, related files, path hardening, and context UI. `EditorSession::startRequest()` still shapes every request with mostly static generation settings: `maxTokens = 512`, `temperature = 0.2`, and provider/template stop sequences.

Inline completion quality benefits from matching request size to cursor intent. VS Code Copilot traces and user reports show strategy fields such as `blockMode`, `requestMultiline`, `stop`, and `maxTokens` because single-line ghost text and block continuations need different bounds.

```mermaid
flowchart LR
  Session[EditorSession::startRequest] --> Strategy[CompletionStrategyEngine]
  Strategy --> Request[CompletionRequest]
  Request --> Provider[OpenAI/Ollama/Copilot]
```

## Problem

Static request settings create two failure modes:

1. Automatic mid-line completions can be too long and noisy.
2. Empty-line block contexts need enough budget for useful multiline code.

The strategy decision currently lives implicitly inside `EditorSession`, which makes tuning and testing hard.

## Questions and Answers

### Q1: Should Phase 3A add caching or candidate cycling?

Answer: Phase 3A adds only strategy selection. Cache, typing-as-suggested, candidate cycling, speculative requests, and NES stay in later phases.

### Q2: Should the strategy alter prompts or context providers?

Answer: No. `PromptAssembler`, context providers, and `SuggestionPostProcessor` keep their current contracts. Strategy only controls request shape.

### Q3: Should settings UI be included?

Answer: Yes, add a compact Strategy group because settings are small and user-facing: enable toggle, token budgets, temperature, and newline stop toggle.

### Q4: How should legacy behavior be preserved?

Answer: When adaptive strategy is disabled, return a legacy-compatible strategy with `maxTokens=512`, `temperature=0.2`, multiline allowed, and no strategy stop sequences.

## Design

### Data model

Add `src/session/CompletionStrategy.{h,cpp}`:

```cpp
struct CompletionStrategy {
    enum class Mode { SingleLine, ParseBlock, MoreMultiline, AfterAccept };

    Mode mode = Mode::SingleLine;
    bool requestMultiline = false;
    int maxTokens = 64;
    double temperature = 0.2;
    QStringList stopSequences;
    QString reason;
};

struct CompletionStrategyRequest {
    QString providerId;
    QString languageId;
    QString filePath;
    QString prefix;
    QString suffix;
    QString currentLinePrefix;
    QString currentLineSuffix;
    QString previousLine;
    QString nextLine;
    KTextEditor::Cursor cursor;
    bool manualTrigger = false;
    bool afterPartialAccept = false;
    bool afterFullAccept = false;
};
```

`reason` is for tests/debugging and remains out of user-facing UI.

### Strategy engine

Add `src/session/CompletionStrategyEngine.{h,cpp}`:

```cpp
class CompletionStrategyEngine {
public:
    static CompletionStrategy choose(const CompletionStrategyRequest &request,
                                     const CompletionSettings &settings);
};
```

Rules:

- `AfterAccept`: `afterPartialAccept || afterFullAccept`; uses `afterAcceptMaxTokens`, multiline true, bounded stops.
- `MoreMultiline`: manual trigger on empty/whitespace-only current line; uses `manualMultilineMaxTokens`, multiline true.
- `ParseBlock`: automatic empty/whitespace-only current line after a cheap language-specific block opener; uses `multilineMaxTokens`, multiline true.
- `SingleLine`: default, mid-line suffix, long current line, or low-confidence context; uses `singleLineMaxTokens`, multiline false.

Block-start heuristics:

- C/C++/Java/JavaScript/TypeScript/Rust: previous trimmed line ends with `{` or `:`, or includes common openers such as `if (...)`, `for (...)`, `while (...)`, `switch (...)`, `class`, `struct`, `fn`.
- Python: previous trimmed line ends with `:` and begins with `def`, `class`, `if`, `for`, `while`, `try`, `except`, `with`, etc.
- Shell: previous trimmed line ends with `then`, `do`, or `{`.
- Haskell: previous trimmed line ends with `where`, `do`, or `of`, or contains `= do`.

Stop sequences:

- Single-line strategy adds `\n` when `singleLineStopAtNewline` is true.
- Multiline strategies add compact bounds such as `\n\n\n`.
- Copilot keeps its existing ` ``` ` stop and merges strategy stops by deduplication.
- OpenAI/Ollama keep PromptTemplate stop sequences and merge strategy stops by deduplication.

### Settings

Add to `CompletionSettings`:

```cpp
bool enableCompletionStrategy = true;
int singleLineMaxTokens = 64;
int multilineMaxTokens = 192;
int manualMultilineMaxTokens = 256;
int afterAcceptMaxTokens = 96;
double completionTemperature = 0.2;
bool singleLineStopAtNewline = true;
```

Validation:

- token settings: `8..1024`
- temperature: `0.0..1.5`
- all fields load/save through `KConfigGroup`

### EditorSession integration

`EditorSession::startRequest()` builds `CompletionStrategyRequest` from the current document/cursor:

- `currentLinePrefix`: text before cursor on the current line
- `currentLineSuffix`: text after cursor on the current line
- `previousLine`: nearest previous line text, or empty
- `nextLine`: nearest next line text, or empty
- `manualTrigger`: initially false for Phase 3A because current trigger path calls the same debounced request flow; a follow-up can preserve trigger origin.
- accept flags remain false in this phase; after-accept strategy is tested directly and used when later hooks mark it.

Apply strategy:

- `request.maxTokens = strategy.maxTokens`
- `request.temperature = strategy.temperature`
- merge `request.stopSequences` with strategy stops after provider/template stops

### Settings UI

Add a compact Strategy group to `KateAiConfigPage`:

- Enable adaptive completion strategy
- Single-line max tokens
- Multiline max tokens
- Manual multiline max tokens
- After-accept max tokens
- Temperature
- Stop single-line at newline

### Tests

Add `CompletionStrategyEngineTest`:

1. automatic mid-line request chooses `SingleLine`
2. empty Python block line after `def foo():` chooses `ParseBlock`
3. empty C++ line after `{` chooses `ParseBlock`
4. manual trigger on empty block chooses `MoreMultiline`
5. after accept chooses `AfterAccept`
6. long current line with suffix chooses `SingleLine`
7. disabled strategy returns legacy-compatible strategy
8. max token settings are applied
9. stop sequences are deterministic and deduplicated
10. Haskell `do` / `where` chooses multiline mode

Update `CompletionSettingsTest` for defaults, validation, and roundtrip.

Update `KateAiConfigPageTest` if Strategy UI is included.

## Implementation Plan

1. Add failing strategy engine tests and settings tests.
2. Add `CompletionStrategy` model and `CompletionStrategyEngine`.
3. Add settings fields, validation, load/save.
4. Add Strategy UI group and apply/load behavior.
5. Integrate strategy into `EditorSession::startRequest()`.
6. Wire CMake and run targeted tests.
7. Run full build and CTest.

## Examples

✅ `foo(|suffix)` automatic mid-line uses `SingleLine`, 64 tokens, newline stop.

✅ Python after `def foo():` on an empty line uses `ParseBlock`, 192 tokens, multiline.

✅ Manual trigger on an empty line uses `MoreMultiline`, 256 tokens.

✅ Disabled strategy sends legacy-compatible 512-token requests.

## Trade-offs

- Phase 3A uses cheap deterministic heuristics and avoids parser dependencies.
- Manual-trigger origin is represented in the request model; `EditorSession::triggerSuggestion()` now preserves it for the next request.
- After-accept mode is implemented and tested; full and partial accept paths mark the next request as an after-accept continuation.
- UI adds only compact controls and avoids advanced per-language strategy knobs.

## Implementation Results

Implemented files:

- `src/session/CompletionStrategy.{h,cpp}`
- `src/session/CompletionStrategyEngine.{h,cpp}`
- `src/session/EditorSession.{h,cpp}` strategy integration
- `src/settings/CompletionSettings.{h,cpp}` strategy persistence and validation
- `src/settings/KateAiConfigPage.{h,cpp}` compact Strategy UI
- `autotests/CompletionStrategyEngineTest.cpp`
- updated `autotests/CompletionSettingsTest.cpp`, `autotests/KateAiConfigPageTest.cpp`, and `autotests/EditorSessionIntegrationTest.cpp`

Behavior delivered:

- `SingleLine`, `ParseBlock`, `MoreMultiline`, and `AfterAccept` modes are chosen by deterministic cursor/context heuristics.
- Strategy settings control `max_tokens`, `temperature`, and bounded stop sequences.
- Provider/template stop sequences are preserved and strategy stops are deduplicated.
- Copilot keeps the fixed endpoint/auth path and receives only request-shape changes.
- `ProjectContextResolver::findProjectRoot()` ignores an ambient `/tmp/.git` while preserving project-local `.git` precedence; this keeps temp-dir tests deterministic on machines with `/tmp/.git`.

Verification:

- `ctest --test-dir build --output-on-failure -R 'completion_strategy_engine|completion_settings|config_page|editor_session_integration'` → 4/4 passed.
- `git diff --check && cmake --build build -j 8 && ctest --test-dir build --output-on-failure` → 25/25 passed.

Deviations from original design:

- Manual-trigger and after-accept flags were wired into `EditorSession` during Phase 3A because the existing action paths provided clear integration points.
- `triggerSuggestion()` preserves a pending after-accept intent across its generation bump so the follow-up request uses `afterAcceptMaxTokens`; ordinary cursor/text bumps clear stale intent.
- `startRequest()` consumes one-shot strategy intent before provider/auth early returns, which prevents stale after-accept state from carrying into later requests.
- Strategy stop sequences are merged under a four-stop cap for OpenAI-compatible requests; strategy newline stops replace the lower-value code-fence stop when the FIM template already contributes three FIM sentinel stops.
- A small project-root fix was included after full CTest exposed an ambient temp-directory `.git` interaction.
