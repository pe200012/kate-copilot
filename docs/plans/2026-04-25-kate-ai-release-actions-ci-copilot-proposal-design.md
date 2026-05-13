# Kate AI Release Actions, CI, Copilot Stabilization, and Virtual Block Proposal Design

- Date: 2026-04-25
- Status: Approved by user request
- Scope: Tasks 1, 3, 4, 5, 6, and 8 from the project next-step list

## Background
The plugin now has a usable overlay-based ghost text path, partial accept, Gentoo packaging, and LGPL license coverage. The next batch moves it toward a first public release and a more native Kate user experience.

## Problem
The current plugin still has six release-facing gaps:
1. Inline-completion commands live primarily in `EditorSession::eventFilter()`.
2. v0.1.0 release material exists only as scattered README/design notes.
3. GitHub Actions CI is absent.
4. Plugin runtime messages are noisy for daily use.
5. Copilot OAuth status lacks a direct session verification path and categorized failures.
6. The long-term KTextEditor virtual block proposal needs a self-contained public document.

## Questions and Answers
### Q1. Which commands become Kate-native actions?
A1. Add actions for: accept full suggestion, accept next word, accept next line, dismiss suggestion, and trigger suggestion.

### Q2. How should shortcuts work?
A2. Keep current event-filter shortcuts as immediate defaults and expose actions through `KActionCollection` so users can rebind them in Kate's shortcut settings. Default shortcuts are assigned for partial accept and trigger commands. `Tab` and `Esc` remain event-filter defaults because they are highly context-sensitive inside the editor.

### Q3. How should action state update?
A3. `EditorSession` emits `suggestionVisibilityChanged(bool)`. `KateAiInlineCompletionPluginView` enables accept/dismiss actions when the active session has a visible suggestion. Trigger remains enabled when an active view exists.

### Q4. What counts as v0.1.0 release work?
A4. Add `CHANGELOG.md`, `docs/releases/v0.1.0.md`, README badges/links, and packaging notes. Tagging and GitHub Release creation are kept as explicit maintainer commands.

### Q5. What CI should do first?
A5. Add a GitHub Actions workflow that runs the build inside an Ubuntu 26.04 Docker image with Qt6/KF6 development packages, configures CMake with tests, builds, and runs CTest with the Qt offscreen platform.

### Q6. How should Copilot verification work?
A6. Settings page gains **Verify session**. It reads the GitHub OAuth token from KWallet and calls `https://api.github.com/copilot_internal/v2/token`. A successful session-token response updates status with expiry. Failures map to authentication, entitlement, rate-limit, and generic provider categories.

### Q7. What is the UX cleanup boundary?
A7. Remove routine plugin loaded / active-view messages from `PluginView`. Keep real errors in `EditorSession::showError()`.

### Q8. What should the KTextEditor proposal include?
A8. A standalone proposal with motivation, API sketch, lifecycle, rendering model, implementation phases, and evidence gathered from InlineNote/overlay experiments.

## Design
### 1. Native actions
Files:
- `src/plugin/KateAiInlineCompletionPluginView.{h,cpp}`
- `src/session/EditorSession.{h,cpp}`

`EditorSession` adds:
```cpp
bool hasVisibleSuggestion() const;
void acceptFullSuggestion();
void acceptNextWord();
void acceptNextLine();
void dismissSuggestion();
void triggerSuggestion();
Q_SIGNAL void suggestionVisibilityChanged(bool visible);
```

`KateAiInlineCompletionPluginView` adds:
```cpp
void setupActions();
EditorSession *activeSession() const;
void updateActionState();
```

Action names:
- `kate_ai_inline_completion_accept_full`
- `kate_ai_inline_completion_accept_next_word`
- `kate_ai_inline_completion_accept_next_line`
- `kate_ai_inline_completion_dismiss`
- `kate_ai_inline_completion_trigger`

### 2. Release docs
Files:
- `CHANGELOG.md`
- `docs/releases/v0.1.0.md`
- `README.md`

Release notes include features, install options, Gentoo live ebuild path, known limitations, and verification command.

### 3. CI
Files:
- `.github/workflows/ci.yml`
- `.github/ci/Dockerfile`

The workflow builds an Ubuntu 26.04 CI image with Qt6/KF6 packages, then runs:
```bash
cmake -S . -B /tmp/kate-copilot-build -GNinja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build /tmp/kate-copilot-build -j"$(nproc)"
ctest --test-dir /tmp/kate-copilot-build --output-on-failure --timeout 120
```

### 4. UX cleanup
File:
- `src/plugin/KateAiInlineCompletionPluginView.cpp`

Remove informational `showMessage()` calls for plugin load and active-view changes. Errors stay visible.

### 5. Copilot verification and categorized errors
Files:
- `src/settings/KateAiConfigPage.{h,cpp}`
- `src/network/CopilotCodexProvider.cpp`
- `src/auth/CopilotAuthManager.cpp`

Settings page adds:
```cpp
void slotCopilotVerifySession();
QPushButton *m_copilotVerifySession = nullptr;
```

Provider failure formatting:
- 401: authentication
- 403: entitlement/access
- 429: rate limit
- quota / billing / entitlement keywords: quota/access

### 6. KTextEditor proposal
File:
- `docs/proposals/2026-04-25-ktexteditor-virtual-block-provider.md`

Proposal is written for KDE/KTextEditor maintainers and references public API concepts without relying on private symbols.

## Implementation Plan
1. Add action/state tests for `EditorSession` public command behavior.
2. Implement `EditorSession` public commands and visibility signal.
3. Implement `KActionCollection` actions in `KateAiInlineCompletionPluginView`.
4. Add Copilot verify button and categorized status text.
5. Add CI workflow.
6. Add release docs and proposal.
7. Run full build and CTest.

## Examples
### Good action state
✅ Accept Next Word action enabled only while ghost text is visible.

### Good Copilot verification status
✅ `Status: Copilot session verified, expires 2026-04-25 18:40 UTC`

### Good release note
✅ Explicit install path, provider setup, shortcuts, known limitations.

## Trade-offs
- Event-filter shortcuts remain because `Tab` and `Esc` are editor-context shortcuts.
- KActionCollection adds rebindable commands without forcing menu placement.
- Copilot verification uses the same token-exchange path as runtime inference, which validates entitlement and token freshness in one request.

## Implementation Results
- Native actions implemented in `src/plugin/KateAiInlineCompletionPluginView.{h,cpp}`.
- `EditorSession` command API and `suggestionVisibilityChanged(bool)` implemented in `src/session/EditorSession.{h,cpp}`.
- KF6 XmlGui dependency added to CMake and Gentoo ebuild.
- Routine plugin-loaded / active-view messages removed from `PluginView`.
- Copilot Verify session UI implemented in `src/settings/KateAiConfigPage.{h,cpp}`.
- Copilot exchange and inference failures now include categorized messages in `src/auth/CopilotAuthManager.cpp` and `src/network/CopilotCodexProvider.cpp`.
- CI workflow added at `.github/workflows/ci.yml`.
- CI now builds and runs tests inside `.github/ci/Dockerfile`, based on `ubuntu:26.04`, to provide recent KF6 dev packages unavailable on plain Ubuntu 24.04 runners.
- Release docs added: `CHANGELOG.md`, `docs/releases/v0.1.0.md`.
- KTextEditor proposal added: `docs/proposals/2026-04-25-ktexteditor-virtual-block-provider.md`.
- `.gitignore` now excludes local `.serena/` memory storage.
- Static helper libraries now build with `POSITION_INDEPENDENT_CODE` so Ubuntu's linker can link them into the plugin module.
- `EditorSession` avoids unregistering inline notes from a view while the view is already being destroyed.
- Inline-note rendering experiment assertions now verify rendering without requiring version-specific multiline spill behavior.
- Verification: `docker build -f .github/ci/Dockerfile -t kate-copilot-ci:ubuntu-26.04 . && docker run ... ctest --test-dir /tmp/kate-copilot-build --output-on-failure --timeout 120` => 12/12 passed.
- Local verification: `cmake --build build -j 8 && ctest --test-dir build --output-on-failure` => 12/12 passed.
