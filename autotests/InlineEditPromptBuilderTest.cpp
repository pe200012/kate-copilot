/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: InlineEditPromptBuilderTest
*/

#include "inlineedit/InlineEditPromptBuilder.h"

#include "context/ContextItem.h"

#include <QtTest>

using KateAiInlineCompletion::ContextItem;
using KateAiInlineCompletion::InlineEditPromptBuilder;
using KateAiInlineCompletion::InlineEditPromptOptions;
using KateAiInlineCompletion::InlineEditRequestContext;

namespace
{
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
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("Return exactly one edit using the exact target range above.")));
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
