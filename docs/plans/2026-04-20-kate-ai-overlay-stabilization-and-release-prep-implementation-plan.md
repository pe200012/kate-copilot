# Kate AI Overlay Stabilization and Release Prep Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stabilize the overlay-based ghost text path for daily use and add release-facing docs plus configuration hints.

**Architecture:** Keep runtime anchor tracking in `EditorSession` through a dedicated `SuggestionAnchorTracker`, render ghost text with baseline-aware point drawing in `GhostTextOverlayWidget`, and verify behavior end-to-end with GUI integration tests backed by a local SSE test server. Publish-facing improvements stay in `README.md`, `docs/assets/`, and `KateAiConfigPage` labels.

**Tech Stack:** C++17, Qt 6, KDE Frameworks 6 / KTextEditor, QTest, QTcpServer, CMake

---

### Task 1: Add failing tests for runtime anchor tracking

**Files:**
- Create: `autotests/SuggestionAnchorTrackerTest.cpp`
- Modify: `autotests/CMakeLists.txt`
- Create: `src/session/SuggestionAnchorTracker.h`
- Create: `src/session/SuggestionAnchorTracker.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
void SuggestionAnchorTrackerTest::movesOnInsertBeforeAndAtAnchor()
{
    QScopedPointer<KTextEditor::Document> doc(KTextEditor::Editor::instance()->createDocument());
    QVERIFY(doc);
    doc->setText(QStringLiteral("abc\ndef\n"));

    KateAiInlineCompletion::SuggestionAnchorTracker tracker;
    tracker.attach(doc.data(), KTextEditor::Cursor(1, 1));

    QCOMPARE(tracker.position(), KTextEditor::Cursor(1, 1));

    QVERIFY(doc->insertText(KTextEditor::Cursor(0, 0), QStringLiteral("XX")));
    QCOMPARE(tracker.position(), KTextEditor::Cursor(1, 1));

    QVERIFY(doc->insertText(KTextEditor::Cursor(1, 1), QStringLiteral("YY")));
    QCOMPARE(tracker.position(), KTextEditor::Cursor(1, 3));
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --test-dir build -R kateaiinlinecompletion_suggestion_anchor_tracker_test --output-on-failure`
Expected: FAIL because `SuggestionAnchorTracker` target and implementation are missing.

- [ ] **Step 3: Write minimal implementation**

```cpp
class SuggestionAnchorTracker final {
public:
    void attach(KTextEditor::Document *document,
                const KTextEditor::Cursor &cursor,
                KTextEditor::MovingCursor::InsertBehavior insertBehavior = KTextEditor::MovingCursor::MoveOnInsert);
    void clear();
    bool isValid() const;
    KTextEditor::Cursor position() const;
private:
    std::unique_ptr<KTextEditor::MovingCursor> m_cursor;
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `ctest --test-dir build -R kateaiinlinecompletion_suggestion_anchor_tracker_test --output-on-failure`
Expected: PASS

### Task 2: Add failing overlay rendering tests for EOF and scroll return

**Files:**
- Modify: `autotests/GhostTextOverlayWidgetRenderingTest.cpp`

- [ ] **Step 1: Write the failing tests**

```cpp
void GhostTextOverlayWidgetRenderingTest::multilineOverlayRendersAtEofCursor();
void GhostTextOverlayWidgetRenderingTest::overlayDisappearsWhenAnchorScrollsAwayAndReturnsAfterScrollBack();
```

Each test should:
- create a real `KTextEditor::View`
- attach a `GhostTextOverlayWidget`
- set a multiline `GhostTextState`
- grab the overlay widget image
- assert non-transparent pixels appear for visible anchor states
- assert transparent output when the anchor is fully out of viewport
- assert pixels return after scrolling back

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --test-dir build -R kateaiinlinecompletion_ghost_text_overlay_widget_rendering_test --output-on-failure`
Expected: FAIL on the new assertions.

- [ ] **Step 3: Write minimal implementation**

Implement baseline drawing, per-line clipping, line elision, and visibility skipping in `src/render/GhostTextOverlayWidget.cpp`.

- [ ] **Step 4: Run test to verify it passes**

Run: `ctest --test-dir build -R kateaiinlinecompletion_ghost_text_overlay_widget_rendering_test --output-on-failure`
Expected: PASS

### Task 3: Add failing end-to-end session tests for Tab, Esc, and focus out

**Files:**
- Create: `autotests/EditorSessionIntegrationTest.cpp`
- Modify: `autotests/CMakeLists.txt`

- [ ] **Step 1: Write the failing tests**

```cpp
void EditorSessionIntegrationTest::tabAcceptsStreamedSuggestion();
void EditorSessionIntegrationTest::escapeClearsStreamedSuggestion();
void EditorSessionIntegrationTest::focusOutClearsStreamedSuggestion();
```

Test harness:
- start a local `QTcpServer`
- respond with SSE frames carrying `"ghost()"` or multiline text
- instantiate `KateAiInlineCompletionPlugin`, `EditorSession`, real `Document`, and real `View`
- configure plugin settings to `openai-compatible` with local endpoint and short debounce
- wait for overlay state to become visible
- drive `Tab`, `Esc`, and focus changes through `QTest`

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --test-dir build -R kateaiinlinecompletion_editor_session_integration_test --output-on-failure`
Expected: FAIL because current session code lacks the new test harness expectations.

- [ ] **Step 3: Write minimal implementation**

Implement tracker-backed anchor syncing and transaction-wrapped accept logic in `src/session/EditorSession.{h,cpp}`.

- [ ] **Step 4: Run test to verify it passes**

Run: `ctest --test-dir build -R kateaiinlinecompletion_editor_session_integration_test --output-on-failure`
Expected: PASS

### Task 4: Add failing settings-page hint test

**Files:**
- Create: `autotests/KateAiConfigPageTest.cpp`
- Modify: `autotests/CMakeLists.txt`
- Modify: `src/settings/KateAiConfigPage.h`
- Modify: `src/settings/KateAiConfigPage.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
void KateAiConfigPageTest::showsProviderRecommendationAndShortcutHint()
{
    KateAiConfigPage page(nullptr, nullptr);
    auto *providerHint = page.findChild<QLabel *>(QStringLiteral("providerHintLabel"));
    auto *shortcutHint = page.findChild<QLabel *>(QStringLiteral("shortcutHintLabel"));
    auto *providerCombo = page.findChild<QComboBox *>(QStringLiteral("providerCombo"));

    QVERIFY(providerHint);
    QVERIFY(shortcutHint);
    QVERIFY(providerCombo);
    QVERIFY(providerHint->text().contains(QStringLiteral("qwen3-coder-q4:latest")));
    QVERIFY(shortcutHint->text().contains(QStringLiteral("Tab")));

    providerCombo->setCurrentIndex(providerCombo->findData(QStringLiteral("github-copilot-codex")));
    QVERIFY(providerHint->text().contains(QStringLiteral("GitHub Copilot")));
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --test-dir build -R kateaiinlinecompletion_config_page_test --output-on-failure`
Expected: FAIL because the labels and object names do not exist yet.

- [ ] **Step 3: Write minimal implementation**

Add provider and shortcut hint labels, update them in `updateCredentialsUi()`, and set stable object names used by the test.

- [ ] **Step 4: Run test to verify it passes**

Run: `ctest --test-dir build -R kateaiinlinecompletion_config_page_test --output-on-failure`
Expected: PASS

### Task 5: Publish-facing docs and screenshot asset

**Files:**
- Create: `README.md`
- Create: `docs/assets/ghost-overlay-midline.png`
- Create: `docs/assets/ghost-overlay-eof.png`
- Modify: `docs/plans/2026-04-20-kate-ai-overlay-stabilization-and-release-prep-design.md`

- [ ] **Step 1: Write README content**

README must include:
- plugin overview
- build command
- development run command with `QT_PLUGIN_PATH`
- install command with `cmake --install`
- Ollama setup example
- GitHub Copilot OAuth setup summary
- shortcut behavior: `Tab`, `Esc`, popup suppression
- benchmark and smoke-test entry points

- [ ] **Step 2: Generate and copy screenshot assets**

Run rendering tests, copy generated PNGs into `docs/assets/`, and reference them from README.

- [ ] **Step 3: Append implementation results to the design log**

Add touched files, behavior delivered, and final verification command output summary.

### Task 6: Full verification

**Files:**
- Modify: `src/CMakeLists.txt`
- Modify: `autotests/CMakeLists.txt`
- Modify: any file touched by earlier tasks

- [ ] **Step 1: Build everything**

Run: `cmake --build build -j 8`
Expected: build succeeds

- [ ] **Step 2: Run all tests**

Run: `ctest --test-dir build --output-on-failure`
Expected: all tests pass

- [ ] **Step 3: Verify screenshot assets exist**

Run: `ls docs/assets`
Expected: overlay PNG assets are present
