/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: CompletionStrategyEngineTest
*/

#include "session/CompletionStrategyEngine.h"

#include "settings/CompletionSettings.h"

#include <QtTest>

#include <utility>

using KateAiInlineCompletion::CompletionSettings;
using KateAiInlineCompletion::CompletionStrategy;
using KateAiInlineCompletion::CompletionStrategyEngine;
using KateAiInlineCompletion::CompletionStrategyRequest;

namespace
{
CompletionStrategyRequest baseRequest()
{
    CompletionStrategyRequest request;
    request.providerId = QString::fromLatin1(CompletionSettings::kProviderOpenAICompatible);
    request.languageId = QStringLiteral("C++");
    request.filePath = QStringLiteral("/repo/src/main.cpp");
    request.prefix = QStringLiteral("int main() {\n    return ");
    request.suffix = QStringLiteral(";\n}\n");
    request.currentLinePrefix = QStringLiteral("    return ");
    request.currentLineSuffix = QStringLiteral(";\n");
    request.previousLine = QStringLiteral("int main() {");
    request.nextLine = QStringLiteral("}");
    request.cursor = KTextEditor::Cursor(1, 11);
    return request;
}

CompletionStrategyRequest emptyLineAfter(QString language, QString previousLine)
{
    CompletionStrategyRequest request = baseRequest();
    request.languageId = std::move(language);
    request.currentLinePrefix.clear();
    request.currentLineSuffix.clear();
    request.previousLine = std::move(previousLine);
    request.nextLine.clear();
    request.cursor = KTextEditor::Cursor(10, 0);
    return request;
}
}

class CompletionStrategyEngineTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void automaticMidLineChoosesSingleLine();
    void emptyPythonBlockLineChoosesParseBlock();
    void emptyCppLineAfterBraceChoosesParseBlock();
    void manualTriggerOnEmptyBlockChoosesMoreMultiline();
    void afterAcceptChoosesAfterAccept();
    void longCurrentLineWithSuffixChoosesSingleLine();
    void disabledStrategyReturnsLegacyCompatibleStrategy();
    void maxTokenSettingsAreApplied();
    void stopSequencesAreDeterministicAndDeduplicated();
    void haskellDoAndWhereChooseMultilineMode();
    void extensionFallbackChoosesLanguageSpecificBlockMode();
};

void CompletionStrategyEngineTest::automaticMidLineChoosesSingleLine()
{
    CompletionSettings settings = CompletionSettings::defaults().validated();
    const CompletionStrategy strategy = CompletionStrategyEngine::choose(baseRequest(), settings);

    QCOMPARE(strategy.mode, CompletionStrategy::Mode::SingleLine);
    QCOMPARE(strategy.requestMultiline, false);
    QCOMPARE(strategy.maxTokens, settings.singleLineMaxTokens);
    QCOMPARE(strategy.temperature, settings.completionTemperature);
    QCOMPARE(strategy.stopSequences, QStringList({QStringLiteral("\n")}));
    QVERIFY(strategy.reason.contains(QStringLiteral("single"), Qt::CaseInsensitive));
}

void CompletionStrategyEngineTest::emptyPythonBlockLineChoosesParseBlock()
{
    CompletionSettings settings = CompletionSettings::defaults().validated();
    const CompletionStrategy strategy = CompletionStrategyEngine::choose(emptyLineAfter(QStringLiteral("Python"), QStringLiteral("def foo():")), settings);

    QCOMPARE(strategy.mode, CompletionStrategy::Mode::ParseBlock);
    QCOMPARE(strategy.requestMultiline, true);
    QCOMPARE(strategy.maxTokens, settings.multilineMaxTokens);
    QVERIFY(strategy.stopSequences.contains(QStringLiteral("\n\n\n")));
}

void CompletionStrategyEngineTest::emptyCppLineAfterBraceChoosesParseBlock()
{
    CompletionSettings settings = CompletionSettings::defaults().validated();
    const CompletionStrategy strategy = CompletionStrategyEngine::choose(emptyLineAfter(QStringLiteral("C++"), QStringLiteral("if (ok) {")), settings);

    QCOMPARE(strategy.mode, CompletionStrategy::Mode::ParseBlock);
    QCOMPARE(strategy.requestMultiline, true);
    QCOMPARE(strategy.maxTokens, settings.multilineMaxTokens);
}

void CompletionStrategyEngineTest::manualTriggerOnEmptyBlockChoosesMoreMultiline()
{
    CompletionSettings settings = CompletionSettings::defaults().validated();
    CompletionStrategyRequest request = emptyLineAfter(QStringLiteral("C++"), QStringLiteral("if (ok) {"));
    request.manualTrigger = true;

    const CompletionStrategy strategy = CompletionStrategyEngine::choose(request, settings);

    QCOMPARE(strategy.mode, CompletionStrategy::Mode::MoreMultiline);
    QCOMPARE(strategy.requestMultiline, true);
    QCOMPARE(strategy.maxTokens, settings.manualMultilineMaxTokens);
}

void CompletionStrategyEngineTest::afterAcceptChoosesAfterAccept()
{
    CompletionSettings settings = CompletionSettings::defaults().validated();
    CompletionStrategyRequest request = baseRequest();
    request.afterPartialAccept = true;

    const CompletionStrategy strategy = CompletionStrategyEngine::choose(request, settings);

    QCOMPARE(strategy.mode, CompletionStrategy::Mode::AfterAccept);
    QCOMPARE(strategy.requestMultiline, true);
    QCOMPARE(strategy.maxTokens, settings.afterAcceptMaxTokens);
    QVERIFY(strategy.stopSequences.contains(QStringLiteral("\n\n")));
}

void CompletionStrategyEngineTest::longCurrentLineWithSuffixChoosesSingleLine()
{
    CompletionSettings settings = CompletionSettings::defaults().validated();
    CompletionStrategyRequest request = baseRequest();
    request.currentLinePrefix = QString(180, QLatin1Char('x'));
    request.currentLineSuffix = QStringLiteral(" + tail");
    request.previousLine = QStringLiteral("if (ok) {");

    const CompletionStrategy strategy = CompletionStrategyEngine::choose(request, settings);

    QCOMPARE(strategy.mode, CompletionStrategy::Mode::SingleLine);
    QCOMPARE(strategy.requestMultiline, false);
}

void CompletionStrategyEngineTest::disabledStrategyReturnsLegacyCompatibleStrategy()
{
    CompletionSettings settings = CompletionSettings::defaults().validated();
    settings.enableCompletionStrategy = false;

    const CompletionStrategy strategy = CompletionStrategyEngine::choose(emptyLineAfter(QStringLiteral("Python"), QStringLiteral("def foo():")), settings);

    QCOMPARE(strategy.mode, CompletionStrategy::Mode::MoreMultiline);
    QCOMPARE(strategy.requestMultiline, true);
    QCOMPARE(strategy.maxTokens, 512);
    QCOMPARE(strategy.temperature, 0.2);
    QVERIFY(strategy.stopSequences.isEmpty());
    QVERIFY(strategy.reason.contains(QStringLiteral("legacy"), Qt::CaseInsensitive));
}

void CompletionStrategyEngineTest::maxTokenSettingsAreApplied()
{
    CompletionSettings settings = CompletionSettings::defaults().validated();
    settings.singleLineMaxTokens = 33;
    settings.multilineMaxTokens = 222;
    settings.manualMultilineMaxTokens = 333;
    settings.afterAcceptMaxTokens = 44;

    QCOMPARE(CompletionStrategyEngine::choose(baseRequest(), settings).maxTokens, 33);
    QCOMPARE(CompletionStrategyEngine::choose(emptyLineAfter(QStringLiteral("Python"), QStringLiteral("def foo():")), settings).maxTokens, 222);

    CompletionStrategyRequest manual = emptyLineAfter(QStringLiteral("C++"), QStringLiteral("if (ok) {"));
    manual.manualTrigger = true;
    QCOMPARE(CompletionStrategyEngine::choose(manual, settings).maxTokens, 333);

    CompletionStrategyRequest after = baseRequest();
    after.afterFullAccept = true;
    QCOMPARE(CompletionStrategyEngine::choose(after, settings).maxTokens, 44);
}

void CompletionStrategyEngineTest::stopSequencesAreDeterministicAndDeduplicated()
{
    CompletionSettings settings = CompletionSettings::defaults().validated();
    CompletionStrategyRequest request = emptyLineAfter(QStringLiteral("Haskell"), QStringLiteral("main = do"));

    const CompletionStrategy strategy = CompletionStrategyEngine::choose(request, settings);

    QCOMPARE(strategy.stopSequences, QStringList({QStringLiteral("\n\n\n")}));
    QStringList unique = strategy.stopSequences;
    QCOMPARE(unique.removeDuplicates(), 0);
}

void CompletionStrategyEngineTest::haskellDoAndWhereChooseMultilineMode()
{
    CompletionSettings settings = CompletionSettings::defaults().validated();

    const CompletionStrategy doStrategy = CompletionStrategyEngine::choose(emptyLineAfter(QStringLiteral("Haskell"), QStringLiteral("main = do")), settings);
    const CompletionStrategy whereStrategy = CompletionStrategyEngine::choose(emptyLineAfter(QStringLiteral("Haskell"), QStringLiteral("foo x = y where")), settings);

    QCOMPARE(doStrategy.mode, CompletionStrategy::Mode::ParseBlock);
    QCOMPARE(doStrategy.requestMultiline, true);
    QCOMPARE(whereStrategy.mode, CompletionStrategy::Mode::ParseBlock);
    QCOMPARE(whereStrategy.requestMultiline, true);
}

void CompletionStrategyEngineTest::extensionFallbackChoosesLanguageSpecificBlockMode()
{
    CompletionSettings settings = CompletionSettings::defaults().validated();

    CompletionStrategyRequest haskell = emptyLineAfter(QString(), QStringLiteral("main = do"));
    haskell.filePath = QStringLiteral("/repo/app/Main.hs");

    const CompletionStrategy haskellStrategy = CompletionStrategyEngine::choose(haskell, settings);
    QCOMPARE(haskellStrategy.mode, CompletionStrategy::Mode::ParseBlock);
    QCOMPARE(haskellStrategy.requestMultiline, true);

    CompletionStrategyRequest shell = emptyLineAfter(QString(), QStringLiteral("do"));
    shell.filePath = QStringLiteral("/repo/scripts/build.sh");

    const CompletionStrategy shellStrategy = CompletionStrategyEngine::choose(shell, settings);
    QCOMPARE(shellStrategy.mode, CompletionStrategy::Mode::ParseBlock);
    QCOMPARE(shellStrategy.requestMultiline, true);
}

QTEST_MAIN(CompletionStrategyEngineTest)

#include "CompletionStrategyEngineTest.moc"
