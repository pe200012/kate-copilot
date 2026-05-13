/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: SuggestionPostProcessorTest
*/

#include "session/SuggestionPostProcessor.h"

#include <QtTest>

using KateAiInlineCompletion::ProcessedSuggestion;
using KateAiInlineCompletion::SuggestionPostProcessor;
using KateAiInlineCompletion::SuggestionProcessingContext;

namespace
{

SuggestionProcessingContext contextAtCursor(QString currentLineSuffix = {}, QString nextNonEmptyLine = {})
{
    SuggestionProcessingContext ctx;
    ctx.cursor = KTextEditor::Cursor(3, 6);
    ctx.currentLineSuffix = std::move(currentLineSuffix);
    ctx.nextNonEmptyLine = std::move(nextNonEmptyLine);
    return ctx;
}

} // namespace

class SuggestionPostProcessorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void suffixCoverageExpandsReplaceRange();
    void duplicateNextLineSuggestionIsFiltered();
    void duplicatedClosingBraceLineIsTrimmed();
    void markerOnlySuggestionIsInvalid();
    void fencedCodeIsSanitized();
};

void SuggestionPostProcessorTest::suffixCoverageExpandsReplaceRange()
{
    const ProcessedSuggestion suggestion = SuggestionPostProcessor::process(QStringLiteral("ghost()SUFFIX"),
                                                                            contextAtCursor(QStringLiteral("SUFFIX")));

    QVERIFY(suggestion.valid);
    QCOMPARE(suggestion.insertText, QStringLiteral("ghost()SUFFIX"));
    QCOMPARE(suggestion.displayText, QStringLiteral("ghost()"));
    QCOMPARE(suggestion.suffixCoverage, 6);
    QCOMPARE(suggestion.replaceRange.start(), KTextEditor::Cursor(3, 6));
    QCOMPARE(suggestion.replaceRange.end(), KTextEditor::Cursor(3, 12));
}

void SuggestionPostProcessorTest::duplicateNextLineSuggestionIsFiltered()
{
    const ProcessedSuggestion suggestion = SuggestionPostProcessor::process(QStringLiteral("return value;"),
                                                                            contextAtCursor(QString(), QStringLiteral("return value;")));

    QVERIFY(!suggestion.valid);
}

void SuggestionPostProcessorTest::duplicatedClosingBraceLineIsTrimmed()
{
    const ProcessedSuggestion suggestion = SuggestionPostProcessor::process(QStringLiteral("doWork();\n}"),
                                                                            contextAtCursor(QString(), QStringLiteral("}")));

    QVERIFY(suggestion.valid);
    QCOMPARE(suggestion.insertText, QStringLiteral("doWork();"));
    QCOMPARE(suggestion.displayText, QStringLiteral("doWork();"));
    QCOMPARE(suggestion.suffixCoverage, 0);
}

void SuggestionPostProcessorTest::markerOnlySuggestionIsInvalid()
{
    const ProcessedSuggestion suggestion = SuggestionPostProcessor::process(QStringLiteral("<|fim_middle|><|fim_suffix|>"), contextAtCursor());

    QVERIFY(!suggestion.valid);
}

void SuggestionPostProcessorTest::fencedCodeIsSanitized()
{
    const ProcessedSuggestion suggestion = SuggestionPostProcessor::process(QStringLiteral("```cpp\nreturn 42;\n```"), contextAtCursor());

    QVERIFY(suggestion.valid);
    QCOMPARE(suggestion.insertText, QStringLiteral("return 42;\n"));
    QCOMPARE(suggestion.displayText, QStringLiteral("return 42;\n"));
}

QTEST_MAIN(SuggestionPostProcessorTest)

#include "SuggestionPostProcessorTest.moc"
