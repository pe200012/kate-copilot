/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: InlineEditPromptBuilderTest
*/

#include "inlineedit/InlineEditPromptBuilder.h"

#include "context/ContextItem.h"
#include "inlineedit/InlineEditTrigger.h"

#include <QtTest>

using KateAiInlineCompletion::ContextItem;
using KateAiInlineCompletion::InlineEditPromptBuilder;
using KateAiInlineCompletion::InlineEditPromptOptions;
using KateAiInlineCompletion::InlineEditRequestContext;
using KateAiInlineCompletion::InlineEditTrigger;
using KateAiInlineCompletion::InlineEditTriggerKind;

namespace
{
InlineEditTrigger trigger(InlineEditTriggerKind kind, QString reason)
{
    InlineEditTrigger out;
    out.kind = kind;
    out.reason = std::move(reason);
    out.diagnosticMessage = QStringLiteral("expected ';'");
    out.recentEditSummary = QStringLiteral("Renamed oldName to newName");
    return out;
}

InlineEditRequestContext baseContext()
{
    InlineEditRequestContext ctx;
    ctx.filePath = QStringLiteral("/repo/src/foo.cpp");
    ctx.languageId = QStringLiteral("C++");
    ctx.cursor = KTextEditor::Cursor(41, 12);
    ctx.targetRange = KTextEditor::Range(41, 0, 41, 79);
    ctx.selectedText = QStringLiteral("int value = oldName();");
    ctx.prefixExcerpt = QStringLiteral("void f() {\n");
    ctx.suffixExcerpt = QStringLiteral("}\n");

    ContextItem item;
    item.kind = ContextItem::Kind::CodeSnippet;
    item.providerId = QStringLiteral("related-files");
    item.name = QStringLiteral("foo.h");
    item.uri = QStringLiteral("/repo/src/foo.h");
    item.value = QStringLiteral("int oldName();\nint newName();");
    ctx.contextItems.push_back(item);
    return ctx;
}
}

class InlineEditPromptBuilderTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void includesFileLanguageCursorAndJsonOnlyInstruction();
    void includesSelectionTargetAndOneBasedRange();
    void includesCurrentLineTargetTextWhenNoSelection();
    void includesContextItemsWhenEnabled();
    void omitsContextItemsWhenDisabled();
    void includesConfiguredMaxEditCount();
    void includesDiagnosticTriggerReason();
    void includesRecentEditTriggerReason();
    void includesSelectionTriggerReason();
    void outputIsDeterministic();
};

void InlineEditPromptBuilderTest::includesFileLanguageCursorAndJsonOnlyInstruction()
{
    const auto prompt = InlineEditPromptBuilder::build(baseContext());

    QVERIFY(prompt.systemPrompt.contains(QStringLiteral("inline code edit engine")));
    QVERIFY(prompt.systemPrompt.contains(QStringLiteral("Return JSON only")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("File: /repo/src/foo.cpp")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("Language: C++")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("Cursor: line 42, column 13")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("Return 1 to 4 non-overlapping edits inside the target range above.")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("validates and applies ranges transactionally")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("\"edits\"")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("\"newText\"")));
}

void InlineEditPromptBuilderTest::includesSelectionTargetAndOneBasedRange()
{
    const auto prompt = InlineEditPromptBuilder::build(baseContext());

    QVERIFY(prompt.userPrompt.contains(QStringLiteral("Target range:")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("startLine: 42")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("startColumn: 1")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("endLine: 42")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("endColumn: 80")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("Current target text:")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("int value = oldName();")));
}

void InlineEditPromptBuilderTest::includesCurrentLineTargetTextWhenNoSelection()
{
    InlineEditRequestContext ctx = baseContext();
    ctx.selectedText.clear();
    ctx.currentTargetText = QStringLiteral("return oldValue; ");

    const auto prompt = InlineEditPromptBuilder::build(ctx);

    QVERIFY(prompt.userPrompt.contains(QStringLiteral("return oldValue; ")));
}

void InlineEditPromptBuilderTest::includesContextItemsWhenEnabled()
{
    const auto prompt = InlineEditPromptBuilder::build(baseContext());

    QVERIFY(prompt.userPrompt.contains(QStringLiteral("Relevant context:")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("related-files")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("foo.h")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("int newName();")));
}

void InlineEditPromptBuilderTest::omitsContextItemsWhenDisabled()
{
    InlineEditPromptOptions options;
    options.useContext = false;

    const auto prompt = InlineEditPromptBuilder::build(baseContext(), options);

    QVERIFY(!prompt.userPrompt.contains(QStringLiteral("int newName();")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("Relevant context:")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("None")));
}

void InlineEditPromptBuilderTest::includesConfiguredMaxEditCount()
{
    InlineEditPromptOptions options;
    options.maxEdits = 7;

    const auto prompt = InlineEditPromptBuilder::build(baseContext(), options);

    QVERIFY(prompt.userPrompt.contains(QStringLiteral("Return 1 to 7 non-overlapping edits inside the target range above.")));
}

void InlineEditPromptBuilderTest::includesDiagnosticTriggerReason()
{
    InlineEditPromptOptions options;
    options.trigger = trigger(InlineEditTriggerKind::DiagnosticRepair, QStringLiteral("Fix the diagnostic near the cursor:\nexpected ';'"));

    const auto prompt = InlineEditPromptBuilder::build(baseContext(), options);

    QVERIFY(prompt.userPrompt.contains(QStringLiteral("Trigger reason: DiagnosticRepair")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("Diagnostic:")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("expected ';'")));
}

void InlineEditPromptBuilderTest::includesRecentEditTriggerReason()
{
    InlineEditPromptOptions options;
    options.trigger = trigger(InlineEditTriggerKind::RecentEditContinuation, QStringLiteral("Continue the recent edit pattern:\nRenamed oldName to newName"));

    const auto prompt = InlineEditPromptBuilder::build(baseContext(), options);

    QVERIFY(prompt.userPrompt.contains(QStringLiteral("Trigger reason: RecentEditContinuation")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("Recent edit pattern:")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("Renamed oldName to newName")));
}

void InlineEditPromptBuilderTest::includesSelectionTriggerReason()
{
    InlineEditPromptOptions options;
    options.trigger = trigger(InlineEditTriggerKind::SelectionRepair, QStringLiteral("Improve or repair the selected code."));

    const auto prompt = InlineEditPromptBuilder::build(baseContext(), options);

    QVERIFY(prompt.userPrompt.contains(QStringLiteral("Trigger reason: SelectionRepair")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("Selected code:")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("int value = oldName();")));
}

void InlineEditPromptBuilderTest::outputIsDeterministic()
{
    const InlineEditRequestContext ctx = baseContext();
    const auto first = InlineEditPromptBuilder::build(ctx);
    const auto second = InlineEditPromptBuilder::build(ctx);

    QCOMPARE(first.systemPrompt, second.systemPrompt);
    QCOMPARE(first.userPrompt, second.userPrompt);
}

QTEST_MAIN(InlineEditPromptBuilderTest)

#include "InlineEditPromptBuilderTest.moc"
