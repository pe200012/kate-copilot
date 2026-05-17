# Kate AI Phase 3B CompletionCache and Typing Reuse Design

## Background

Phase 3A moved request shape decisions into `CompletionStrategyEngine`. `EditorSession` still clears the visible suggestion on every user text insertion and schedules a fresh debounced network request. This creates flicker and duplicate requests when the user types the exact prefix of the visible ghost text.

Recent inline-completion implementations preserve reusable suggestions across matching edits. The VS Code inline-completions model keeps reusable inline items when the text model and cursor still agree with the previous suggestion, and extension authors are encouraged to cache deterministic results by position/text version for stable UX.

## Problem

`EditorSession::onTextInserted()` currently calls `bumpGeneration()` for normal user typing. That clears `GhostTextState`, cancels the active request, and starts debounce. When the inserted text matches the front of the visible suggestion, the plugin already has enough local data to show the remaining suggestion immediately.

Phase 3B adds a local cache and a typing-as-suggested fast path so matching user input shrinks the suggestion locally and exact cached request states return immediately.

## Questions and Answers

### Q1: Where should the cache live?

Answer: Use one `CompletionCache` per `KateAiInlineCompletionPluginView`. Sessions for views under the same Kate main window share provider/model/template-local reuse while keeping lifetime simple and UI-thread-only.

### Q2: What goes into the key?

Answer: Use provider, model, prompt template, language, file path, SHA-256 hash of prefix tail, SHA-256 hash of suffix head, `requestMultiline`, and strategy mode name. This keeps prompt and credential data outside the key while making nearby equivalent states deterministic.

### Q3: What goes into the value?

Answer: Store only provider output and local post-processing results: raw completion, processed insert text, processed display text, suffix coverage, creation time, and hit count. Prompts, API keys, OAuth tokens, and headers stay outside the cache.

### Q4: How should typing-as-suggested interact with network requests?

Answer: `onTextInserted()` first checks the current visible suggestion. If the inserted text equals the beginning of `m_state.visibleText`, `EditorSession` appends it to the accepted/typed prefix, reprocesses the remaining raw completion against the new cursor, updates ghost state, and returns without scheduling debounce. Any other insert follows the existing generation bump and debounce path.

### Q5: How should cache hits interact with context providers?

Answer: Strategy and cache key construction happen before context collection. Exact cache hits populate the ghost state immediately and skip context collection plus provider start. Cache misses continue through the existing context, prompt, auth, and provider paths.

## Design

### Cache Model

New files:

- `src/session/CompletionCache.h`
- `src/session/CompletionCache.cpp`

Types:

```cpp
struct CompletionCacheKey {
    QString providerId;
    QString model;
    QString promptTemplate;
    QString languageId;
    QString filePath;
    QString prefixTailHash;
    QString suffixHeadHash;
    bool requestMultiline = false;
    QString strategyMode;

    bool operator==(const CompletionCacheKey &other) const;
};

struct CompletionCacheValue {
    QString rawCompletion;
    QString processedInsertText;
    QString processedDisplayText;
    int suffixCoverage = 0;
    QDateTime createdAt;
    int hitCount = 0;
};

struct CompletionCacheOptions {
    bool enabled = true;
    int maxEntries = 128;
    int ttlMs = 120000;
    int prefixTailChars = 1200;
    int suffixHeadChars = 600;
};
```

`CompletionCache` exposes:

```cpp
class CompletionCache final
{
public:
    explicit CompletionCache(CompletionCacheOptions options = {});

    void setOptions(CompletionCacheOptions options);
    CompletionCacheOptions options() const;

    void insert(const CompletionCacheKey &key, const CompletionCacheValue &value);
    std::optional<CompletionCacheValue> lookupExact(const CompletionCacheKey &key);
    std::optional<CompletionCacheValue> lookupTypingAsSuggested(const CompletionCacheKey &key, const QString &typedPrefixDelta);

    void clear();
    int size() const;

    static CompletionCacheKey makeKey(const CompletionSettings &settings,
                                      const CompletionStrategy &strategy,
                                      const PromptContext &promptCtx,
                                      const QString &prefix,
                                      const QString &suffix);
};
```

Implementation details:

- SHA-256 via `QCryptographicHash`.
- `prefixTailHash` hashes `prefix.right(prefixTailChars)`.
- `suffixHeadHash` hashes `suffix.left(suffixHeadChars)`.
- TTL pruning happens on lookup, insert, `setOptions()`, and `size()`.
- LRU eviction uses `lastAccess` tracked internally beside `CompletionCacheValue`.
- `maxEntries == 0` behaves as a disabled store.
- `lookupTypingAsSuggested()` returns a value with raw and processed strings advanced by `typedPrefixDelta` when the cached processed insert/display text starts with the typed prefix.

### Settings

Add fields to `CompletionSettings`:

```cpp
bool enableCompletionCache = true;
int completionCacheMaxEntries = 128;
int completionCacheTtlMs = 120000;
int completionCachePrefixTailChars = 1200;
int completionCacheSuffixHeadChars = 600;
bool enableTypingAsSuggested = true;
```

Validation bounds:

- entries: `0..1000`
- ttl: `1000..600000`
- prefix tail: `100..10000`
- suffix head: `0..5000`

KConfig keys:

- `EnableCompletionCache`
- `CompletionCacheMaxEntries`
- `CompletionCacheTtlMs`
- `CompletionCachePrefixTailChars`
- `CompletionCacheSuffixHeadChars`
- `EnableTypingAsSuggested`

### Settings UI

Add a `Cache` group after `Strategy`:

- `Enable completion cache`
- `Enable typing-as-suggested reuse`
- `Cache max entries`
- `Cache TTL`
- `Prefix tail hash characters`
- `Suffix head hash characters`

`updateCacheControlsUi()` disables numeric cache controls when cache is disabled. Typing-as-suggested remains independently configurable because it can reuse the currently visible suggestion without storing cache entries.

### PluginView Integration

`KateAiInlineCompletionPluginView` owns:

```cpp
KateAiInlineCompletion::CompletionCache *m_completionCache = nullptr;
QString m_cacheSettingsSignature;
```

It creates the cache in the constructor, applies options from settings, and passes the pointer to `EditorSession`. On settings changes it updates cache options and clears the cache when provider, model, prompt template, cache hash windows, or enabled state changes.

### EditorSession Integration

Add fields:

```cpp
CompletionCache *m_completionCache = nullptr;
CompletionCacheKey m_activeCacheKey;
bool m_hasActiveCacheKey = false;
QString m_activeRawCompletion;
QString m_typedPrefixFromSuggestion;
enum class SuggestionSource { None, Network, Cache, TypingAsSuggested };
SuggestionSource m_suggestionSource = SuggestionSource::None;
```

#### Typing-as-suggested fast path

`onTextInserted()` uses `tryReuseVisibleSuggestionForTypedText(text)` before `bumpGeneration()`:

1. Validate settings enablement, visible suggestion, no active ignore count, and inserted text prefix match.
2. Append the text to `m_typedPrefixFromSuggestion` and `m_acceptedFromSuggestion`.
3. Compute remaining raw completion by removing the accepted typed prefix from sanitized raw completion.
4. Sync the anchor tracker to the new cursor.
5. Re-run `SuggestionPostProcessor::process(remaining, suggestionProcessingContext(doc, newCursor))`.
6. Update `m_state` and render.
7. Mark source as `TypingAsSuggested`.
8. Return without scheduling debounce.

This supports character-by-character input and pasted chunks.

#### Cache lookup before network

`startRequest()` flow becomes:

1. Validate view/document/settings/selection/popup/provider/auth as today.
2. Compute prefix/suffix/language/file and `PromptContext`.
3. Choose `CompletionStrategy`.
4. Build `CompletionCacheKey`.
5. Lookup exact cache hit.
6. On hit, process cached raw completion at the current cursor, populate ghost state, mark streaming false, mark source `Cache`, and skip context/prompt/provider.
7. On miss, collect context and proceed through the provider path.

#### Cache insertion

On valid processed network suggestions:

- `onDeltaReceived()` updates `m_activeRawCompletion` and processed state.
- `onRequestFinished()` inserts the active key and current valid processed state when request id and generation still match.
- Invalid or empty suggestions clear display state and skip insertion.

### Cache Invalidation

- TTL expiry prunes stale entries.
- LRU eviction caps memory.
- Settings changes in `PluginView` clear the cache when core cache identity settings change.
- Provider/model/template differences miss through the key.
- Strategy mode changes miss through the key.

### Tests

Add `autotests/CompletionCacheTest.cpp`:

1. exact lookup returns inserted value
2. TTL expiry removes entries
3. LRU max entries evicts the oldest entry
4. provider/model/template differences miss
5. prefix tail hash differences miss
6. suffix head hash differences miss
7. disabled cache stores no values and returns no hits
8. clear removes entries
9. typing-as-suggested lookup returns remaining completion
10. typing-as-suggested rejects nonmatching prefix

Extend existing tests:

- `CompletionSettingsTest`: defaults, validation, roundtrip.
- `KateAiConfigPageTest`: cache controls exist and apply values.
- `EditorSessionIntegrationTest`:
  - typing matching prefix keeps remaining suggestion visible and request count stable
  - typing mismatch clears current suggestion and schedules normal completion
  - exact cached suggestion displays without a second fake-provider request

## Implementation Plan

1. Write failing `CompletionCacheTest` and CMake target.
2. Implement `CompletionCache` model, hashing, TTL, LRU, exact lookup, and typing lookup.
3. Add settings fields, validation, load/save, defaults tests.
4. Add Cache UI controls and UI tests.
5. Add cache ownership/options to `KateAiInlineCompletionPluginView`.
6. Add cache pointer and active cache metadata to `EditorSession`.
7. Add typing-as-suggested fast path in `onTextInserted()`.
8. Add exact cache lookup before context collection/provider start.
9. Add network insertion on finish for valid suggestions.
10. Add integration tests and run targeted CTest.
11. Run full verification and request code review.

## Examples

### Typing through ghost text

Current document:

```cpp
return pre▮
```

Visible suggestion:

```cpp
fixValue();
```

User types `fix`:

```cpp
return prefix▮Value();
```

The session consumes `fix`, reuses the local raw suggestion, and displays `Value();` immediately.

### Exact cache hit

A request key for `src/foo.cpp`, model `test-model`, strategy `SingleLine`, prefix tail hash `A`, suffix head hash `B` stores raw completion `Value();`. A later identical key reprocesses `Value();` at the current cursor and renders it with no provider request.

## Trade-offs

- Prefix/suffix hashes keep keys compact and safer for logs. A future Phase 3 hardening can include a context fingerprint.
- Per-main-window cache balances reuse and lifetime clarity. A future plugin-wide cache can share results across Kate windows.
- Cache hits re-run `SuggestionPostProcessor`, which preserves suffix-overlap behavior after cursor movement and document edits.
- Typing-as-suggested uses the current visible suggestion as the primary source of truth. Candidate cycling stays in Phase 3C.

## Deferred Work

- Candidate cycling and multiple cached candidates.
- Speculative requests.
- NES and inline edits.
- Context fingerprinting from final prompt assembly.
- Cross-main-window shared cache.

## Implementation Results

Implemented Phase 3B in the planned scope.

Files added:

- `src/session/CompletionCache.h`
- `src/session/CompletionCache.cpp`
- `autotests/CompletionCacheTest.cpp`

Files updated:

- `src/session/EditorSession.{h,cpp}`
- `src/plugin/KateAiInlineCompletionPluginView.{h,cpp}`
- `src/settings/CompletionSettings.{h,cpp}`
- `src/settings/KateAiConfigPage.{h,cpp}`
- `src/CMakeLists.txt`
- `autotests/CMakeLists.txt`
- `autotests/CompletionSettingsTest.cpp`
- `autotests/KateAiConfigPageTest.cpp`
- `autotests/EditorSessionIntegrationTest.cpp`

Implemented behavior:

- Completion cache with SHA-256 prefix-tail and suffix-head hashes, TTL expiry, LRU eviction, exact lookup, clear, disabled-cache behavior, and typing-as-suggested lookup API.
- Cache key includes provider, model, prompt template, language, file path, endpoint hash, Copilot NWO hash, prefix tail hash, suffix head hash, multiline flag, and strategy mode.
- One cache per `KateAiInlineCompletionPluginView`, shared by sessions in a Kate main window.
- Settings and UI for cache enablement, max entries, TTL, prefix/suffix hash window sizes, and typing-as-suggested.
- `EditorSession` exact cache lookup before context collection and provider start.
- Network suggestion insertion into cache after a valid completed streaming request.
- Typing-as-suggested fast path for character input and pasted chunks. Active streaming requests stay alive so later deltas continue updating the remaining suggestion.

Review-driven fixes:

- Kept active streaming requests alive during typing-as-suggested, relying on the existing `m_acceptedFromSuggestion` delta trimming path.
- Added endpoint and Copilot NWO cache identity to avoid cross-backend reuse.
- Added integration coverage for typing while an SSE stream is still active.

Verification:

```bash
git diff --check && cmake --build build -j 8 && ctest --test-dir build --output-on-failure
# 26/26 passed
```
