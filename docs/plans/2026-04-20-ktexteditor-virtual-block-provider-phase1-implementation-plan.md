# VirtualBlockProvider Phase 1 Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add the first upstream KTextEditor patch for `VirtualBlockProvider`: public API, `View` registration methods, and `ViewPrivate` provider collection with validation and priority resolution.

**Architecture:** Phase 1 mirrors the existing `InlineNoteProvider` structure. Public API lives in `src/include/ktexteditor/`, `ViewPrivate` owns provider registration and invalidation in `src/view/`, and tests exercise the new provider via `ViewPrivate` in `autotests/src/`. This phase keeps rendering and layout unchanged and focuses on a stable API/data path.

**Tech Stack:** C++17, Qt6, KDE Frameworks 6, ECM test macros, QTest, KTextEditor internals (`ViewPrivate`, `DocumentPrivate`).

---

### Task 1: Add the failing Phase 1 test

**Files:**
- Create: `autotests/src/virtualblock_test.h`
- Create: `autotests/src/virtualblock_test.cpp`
- Modify: `autotests/CMakeLists.txt`

**Step 1: Write the failing test**

Create a provider that returns `VirtualBlock` values and a test that expects:
- registration on `ViewPrivate` compiles and stores a provider
- invalid blocks are filtered out
- the highest-priority valid block wins for a line
- unregister clears the collected block

Test shape:
```cpp
void VirtualBlockTest::testPriorityResolution()
{
    KTextEditor::DocumentPrivate doc;
    doc.setText(QStringLiteral("alpha\nbeta\n"));

    KTextEditor::ViewPrivate view(&doc, nullptr);
    view.show();

    LowPriorityProvider low;
    HighPriorityProvider high;
    view.registerVirtualBlockProvider(&low);
    view.registerVirtualBlockProvider(&high);

    const auto blocks = view.virtualBlocks(0);
    QCOMPARE(blocks.size(), 1);
    QCOMPARE(blocks[0].lines, QStringList({QStringLiteral("chosen") }));
}
```

**Step 2: Run test to verify it fails**

Run:
```bash
cd /tmp/ktexteditor-upstream
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j 8 --target virtualblock_test
ctest --test-dir build -R virtualblock_test --output-on-failure
```

Expected:
- compile failure because `VirtualBlock`, `VirtualBlockProvider`, and new `View` methods do not exist yet

**Step 3: Commit**

```bash
cd /tmp/ktexteditor-upstream
jj status
```

### Task 2: Add the public API

**Files:**
- Create: `src/include/ktexteditor/virtualblockprovider.h`
- Modify: `src/include/CMakeLists.txt`
- Modify: `src/include/ktexteditor/view.h`
- Modify: `src/utils/ktexteditor.cpp`

**Step 1: Write the minimal implementation**

Add:
- `enum class VirtualBlockCursorBehavior { Skip };`
- `class VirtualBlock`
- `class VirtualBlockProvider : public QObject`
- `View::registerVirtualBlockProvider()` / `unregisterVirtualBlockProvider()`
- out-of-line default destructor for `VirtualBlockProvider` in `src/utils/ktexteditor.cpp`
- `#include "moc_virtualblockprovider.cpp"` in `src/utils/ktexteditor.cpp`
- CamelCase header generation in `src/include/CMakeLists.txt`

**Step 2: Run test to move failure forward**

Run:
```bash
cd /tmp/ktexteditor-upstream
cmake --build build -j 8 --target virtualblock_test
ctest --test-dir build -R virtualblock_test --output-on-failure
```

Expected:
- failure now comes from missing `ViewPrivate` implementation or unresolved symbols for new methods

**Step 3: Commit**

```bash
cd /tmp/ktexteditor-upstream
jj status
```

### Task 3: Add `ViewPrivate` provider wiring

**Files:**
- Create: `src/view/virtualblockdata.h`
- Modify: `src/view/kateview.h`
- Modify: `src/view/kateview.cpp`

**Step 1: Write the minimal implementation**

Add:
- `m_virtualBlockProviders`
- `registerVirtualBlockProvider()` / `unregisterVirtualBlockProvider()`
- `virtualBlocks(int line) const`
- `virtualBlocksReset()` / `virtualBlocksLineRangeChanged(KTextEditor::LineRange)`

Validation in `virtualBlocks(int line) const`:
- keep only blocks where `block.isValid()`
- keep only blocks whose anchor line equals `line`
- keep only blocks whose anchor column equals `doc()->lineLength(line)`
- choose one block per line by higher `priority`, then earlier provider registration order

**Step 2: Run test to verify it passes**

Run:
```bash
cd /tmp/ktexteditor-upstream
cmake --build build -j 8 --target virtualblock_test
ctest --test-dir build -R virtualblock_test --output-on-failure
```

Expected:
- `virtualblock_test` passes

**Step 3: Commit**

```bash
cd /tmp/ktexteditor-upstream
jj status
```

### Task 4: Run focused regression coverage

**Files:**
- Reuse: `autotests/src/inlinenote_test.cpp`
- Reuse: `autotests/src/kateview_test.cpp`

**Step 1: Run neighboring tests**

Run:
```bash
cd /tmp/ktexteditor-upstream
cmake --build build -j 8 --target inlinenote_test kateview_test
ctest --test-dir build -R "(inlinenote_test|kateview_test|virtualblock_test)" --output-on-failure
```

Expected:
- all three tests pass

**Step 2: Commit**

```bash
cd /tmp/ktexteditor-upstream
jj status
```

### Task 5: Record implementation results

**Files:**
- Modify: `docs/plans/2026-04-20-ktexteditor-virtual-block-provider-design.md`
- Modify: `docs/plans/2026-04-20-ktexteditor-virtual-block-provider-phase1-implementation-plan.md`

**Step 1: Append implementation notes**

Record:
- exact files changed
- validation and priority semantics shipped in Phase 1
- verification commands and observed results

**Step 2: Run final verification**

Run:
```bash
cd /tmp/ktexteditor-upstream
ctest --test-dir build -R "(inlinenote_test|kateview_test|virtualblock_test)" --output-on-failure
```

Expected:
- 3/3 tests pass

**Step 3: Commit**

```bash
cd /tmp/ktexteditor-upstream
jj status
```
