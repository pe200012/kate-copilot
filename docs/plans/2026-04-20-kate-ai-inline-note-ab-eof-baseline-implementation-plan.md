# Inline Note EOF Baseline A/B Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a GUI rendering experiment that compares `AlignTop + high rect` and `QTextLayout` for single-note multiline InlineNote rendering at Python EOF.

**Architecture:** Add one dedicated GUI test target under `autotests/`. The test owns both minimal providers, the shared render scenario, screenshot capture, crop generation, and pixel measurements. Production plugin code stays untouched for this comparison pass.

**Tech Stack:** C++, Qt6 Test/Widgets/Gui, KF6 TextEditor

---

### Task 1: Register the A/B test target

**Files:**
- Modify: `autotests/CMakeLists.txt`
- Create: `autotests/InlineNoteRenderingABTest.cpp`

**Step 1: Write the failing test skeleton**
Add a new `ecm_add_test` entry for `InlineNoteRenderingABTest.cpp`.
Create the test file with one test slot that references `AlignTopSingleNoteProvider` and `QTextLayoutSingleNoteProvider` before those types exist.

**Step 2: Run test to verify it fails**
Run: `cmake --build build -j 8 --target kateaiinlinecompletion_inline_note_rendering_ab_test`
Expected: compile failure because providers/helpers are missing.

**Step 3: Write minimal implementation**
Add the provider classes and minimal helper functions inside `InlineNoteRenderingABTest.cpp`.

**Step 4: Run test to verify it passes**
Run: `ctest --test-dir build -R kateaiinlinecompletion_inline_note_rendering_ab_test --output-on-failure`
Expected: PASS

### Task 2: Build the shared EOF scenario harness

**Files:**
- Modify: `autotests/InlineNoteRenderingABTest.cpp`

**Step 1: Write the failing assertions**
Add assertions that both providers produce:
- full screenshot
- crop screenshot
- measurable changed pixel bounds near the EOF anchor

**Step 2: Run test to verify it fails**
Run the single test target.
Expected: FAIL because the harness does not yet save or measure these artifacts.

**Step 3: Write minimal implementation**
Implement:
- `RenderScenario`
- `prepareView(...)`
- screenshot save helpers
- crop helpers
- changed pixel measurement helpers

**Step 4: Run test to verify it passes**
Run the single test target again.
Expected: PASS

### Task 3: Add the A/B comparison metrics

**Files:**
- Modify: `autotests/InlineNoteRenderingABTest.cpp`

**Step 1: Write the failing assertions**
Add assertions/logging for:
- first line top offset
- second line indent start
- total rendered block height

**Step 2: Run test to verify it fails**
Run the single test target.
Expected: FAIL because the metrics are not computed yet.

**Step 3: Write minimal implementation**
Compute and print the metrics from the captured crop images.

**Step 4: Run test to verify it passes**
Run the single test target again.
Expected: PASS

### Task 4: Add repaint stability evidence

**Files:**
- Modify: `autotests/InlineNoteRenderingABTest.cpp`

**Step 1: Write the failing assertions**
Add a second capture after moving away from the anchor and returning.
Assert that both providers still render a visible block in the crop area.

**Step 2: Run test to verify it fails**
Run the single test target.
Expected: FAIL because the scroll-back capture is missing.

**Step 3: Write minimal implementation**
Implement the away-and-back repaint cycle and save extra screenshots.

**Step 4: Run test to verify it passes**
Run the single test target.
Expected: PASS

### Task 5: Verify the whole project and document results

**Files:**
- Modify: `docs/plans/2026-04-20-kate-ai-inline-note-ab-eof-baseline-design.md`

**Step 1: Run full verification**
Run: `cmake --build build -j 8 && ctest --test-dir build --output-on-failure`
Expected: all tests pass.

**Step 2: Append implementation results**
Add screenshot paths, measured observations, and the recommended route to the design doc.

**Step 3: Commit**
Version control is expected through `jj` when available in a repo.
