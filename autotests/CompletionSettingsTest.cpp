/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: CompletionSettingsTest
*/

#include "settings/CompletionSettings.h"

#include <KConfig>
#include <KConfigGroup>

#include <QTemporaryDir>
#include <QtTest>

using KateAiInlineCompletion::CompletionSettings;

class CompletionSettingsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultsAreValid();
    void validationClampsBounds();
    void copilotForcesEndpoint();
    void roundTripConfig();
};

void CompletionSettingsTest::defaultsAreValid()
{
    const CompletionSettings d = CompletionSettings::defaults().validated();

    QVERIFY(d.debounceMs >= CompletionSettings::kDebounceMinMs);
    QVERIFY(d.debounceMs <= CompletionSettings::kDebounceMaxMs);

    QVERIFY(d.maxPrefixChars >= CompletionSettings::kPrefixMinChars);
    QVERIFY(d.maxPrefixChars <= CompletionSettings::kPrefixMaxChars);

    QVERIFY(d.maxSuffixChars >= CompletionSettings::kSuffixMinChars);
    QVERIFY(d.maxSuffixChars <= CompletionSettings::kSuffixMaxChars);

    QVERIFY(d.enableContextualPrompt);
    QVERIFY(d.maxContextItems >= CompletionSettings::kContextItemsMin);
    QVERIFY(d.maxContextItems <= CompletionSettings::kContextItemsMax);
    QVERIFY(d.maxContextChars >= CompletionSettings::kContextCharsMin);
    QVERIFY(d.maxContextChars <= CompletionSettings::kContextCharsMax);

    QVERIFY(d.enableRecentEditsContext);
    QCOMPARE(d.recentEditsMaxFiles, 20);
    QCOMPARE(d.recentEditsMaxEdits, 8);
    QCOMPARE(d.recentEditsDiffContextLines, 3);
    QCOMPARE(d.recentEditsMaxCharsPerEdit, 2000);
    QCOMPARE(d.recentEditsDebounceMs, 500);
    QCOMPARE(d.recentEditsMaxLinesPerEdit, 10);
    QCOMPARE(d.recentEditsActiveDocDistanceLimitFromCursor, 100);

    QVERIFY(d.enableOpenTabsContext);
    QVERIFY(d.enableDiagnosticsContext);
    QCOMPARE(d.diagnosticsMaxItems, 8);
    QCOMPARE(d.diagnosticsMaxChars, 3000);
    QCOMPARE(d.diagnosticsMaxLineDistance, 120);
    QVERIFY(d.diagnosticsIncludeWarnings);
    QVERIFY(!d.diagnosticsIncludeInformation);
    QVERIFY(!d.diagnosticsIncludeHints);
    QVERIFY(d.enableRelatedFilesContext);
    QCOMPARE(d.relatedFilesMaxFiles, 6);
    QCOMPARE(d.relatedFilesMaxChars, 12000);
    QCOMPARE(d.relatedFilesMaxCharsPerFile, 4000);
    QVERIFY(d.relatedFilesPreferOpenTabs);
    QVERIFY(d.contextExcludePatterns.isEmpty());

    QVERIFY(d.enableCompletionStrategy);
    QCOMPARE(d.singleLineMaxTokens, 64);
    QCOMPARE(d.multilineMaxTokens, 192);
    QCOMPARE(d.manualMultilineMaxTokens, 256);
    QCOMPARE(d.afterAcceptMaxTokens, 96);
    QCOMPARE(d.completionTemperature, 0.2);
    QVERIFY(d.singleLineStopAtNewline);

    QVERIFY(d.endpoint.isValid());
    QVERIFY(!d.endpoint.isRelative());
    QVERIFY(!d.model.trimmed().isEmpty());
    QVERIFY(!d.promptTemplate.trimmed().isEmpty());
    QVERIFY(!d.copilotClientId.trimmed().isEmpty());
    QVERIFY(!d.copilotNwo.trimmed().isEmpty());
}

void CompletionSettingsTest::validationClampsBounds()
{
    CompletionSettings s = CompletionSettings::defaults();

    s.debounceMs = 1;
    s.maxPrefixChars = 1;
    s.maxSuffixChars = 999999;
    s.maxContextItems = 999999;
    s.maxContextChars = 999999;
    s.recentEditsMaxFiles = 999999;
    s.recentEditsMaxEdits = 999999;
    s.recentEditsDiffContextLines = 999999;
    s.recentEditsMaxCharsPerEdit = 999999;
    s.recentEditsDebounceMs = 1;
    s.recentEditsMaxLinesPerEdit = 999999;
    s.recentEditsActiveDocDistanceLimitFromCursor = 999999;
    s.diagnosticsMaxItems = 999999;
    s.diagnosticsMaxChars = 999999;
    s.diagnosticsMaxLineDistance = 999999;
    s.relatedFilesMaxFiles = 999999;
    s.relatedFilesMaxChars = 999999;
    s.relatedFilesMaxCharsPerFile = 999999;
    s.singleLineMaxTokens = 1;
    s.multilineMaxTokens = 999999;
    s.manualMultilineMaxTokens = 999999;
    s.afterAcceptMaxTokens = 1;
    s.completionTemperature = 99.0;
    s.contextExcludePatterns = {QStringLiteral(" *.tmp "), QString(), QStringLiteral("build/generated/*")};
    s.provider = QStringLiteral("unknown");
    s.endpoint = QUrl(QStringLiteral("relative/path"));
    s.model = QString();
    s.promptTemplate = QStringLiteral("unknown");
    s.copilotClientId = QString();
    s.copilotNwo = QString();

    const CompletionSettings v = s.validated();

    QCOMPARE(v.debounceMs, CompletionSettings::kDebounceMinMs);
    QCOMPARE(v.maxPrefixChars, CompletionSettings::kPrefixMinChars);
    QCOMPARE(v.maxSuffixChars, CompletionSettings::kSuffixMaxChars);
    QCOMPARE(v.maxContextItems, CompletionSettings::kContextItemsMax);
    QCOMPARE(v.maxContextChars, CompletionSettings::kContextCharsMax);
    QCOMPARE(v.recentEditsMaxFiles, CompletionSettings::kRecentEditsMaxFilesMax);
    QCOMPARE(v.recentEditsMaxEdits, CompletionSettings::kRecentEditsMaxEditsMax);
    QCOMPARE(v.recentEditsDiffContextLines, CompletionSettings::kRecentEditsDiffContextLinesMax);
    QCOMPARE(v.recentEditsMaxCharsPerEdit, CompletionSettings::kRecentEditsMaxCharsPerEditMax);
    QCOMPARE(v.recentEditsDebounceMs, CompletionSettings::kRecentEditsDebounceMinMs);
    QCOMPARE(v.recentEditsMaxLinesPerEdit, CompletionSettings::kRecentEditsMaxLinesPerEditMax);
    QCOMPARE(v.recentEditsActiveDocDistanceLimitFromCursor, CompletionSettings::kRecentEditsActiveDocDistanceLimitMax);
    QCOMPARE(v.diagnosticsMaxItems, CompletionSettings::kDiagnosticsMaxItemsMax);
    QCOMPARE(v.diagnosticsMaxChars, CompletionSettings::kDiagnosticsMaxCharsMax);
    QCOMPARE(v.diagnosticsMaxLineDistance, CompletionSettings::kDiagnosticsMaxLineDistanceMax);
    QCOMPARE(v.relatedFilesMaxFiles, CompletionSettings::kRelatedFilesMaxFilesMax);
    QCOMPARE(v.relatedFilesMaxChars, CompletionSettings::kRelatedFilesMaxCharsMax);
    QCOMPARE(v.relatedFilesMaxCharsPerFile, CompletionSettings::kRelatedFilesMaxCharsPerFileMax);
    QCOMPARE(v.singleLineMaxTokens, CompletionSettings::kStrategyMaxTokensMin);
    QCOMPARE(v.multilineMaxTokens, CompletionSettings::kStrategyMaxTokensMax);
    QCOMPARE(v.manualMultilineMaxTokens, CompletionSettings::kStrategyMaxTokensMax);
    QCOMPARE(v.afterAcceptMaxTokens, CompletionSettings::kStrategyMaxTokensMin);
    QCOMPARE(v.completionTemperature, CompletionSettings::kCompletionTemperatureMax);
    QCOMPARE(v.contextExcludePatterns, QStringList({QStringLiteral("*.tmp"), QStringLiteral("build/generated/*")}));
    QCOMPARE(v.provider, QString::fromLatin1(CompletionSettings::kProviderOpenAICompatible));
    QVERIFY(v.endpoint.isValid());
    QVERIFY(!v.endpoint.isRelative());
    QVERIFY(!v.model.isEmpty());
    QCOMPARE(v.promptTemplate, QString::fromLatin1(CompletionSettings::kPromptTemplateFimV3));
    QCOMPARE(v.copilotClientId, CompletionSettings::defaults().copilotClientId);
    QCOMPARE(v.copilotNwo, CompletionSettings::defaults().copilotNwo);
}

void CompletionSettingsTest::copilotForcesEndpoint()
{
    CompletionSettings s = CompletionSettings::defaults();
    s.provider = QString::fromLatin1(CompletionSettings::kProviderGitHubCopilotCodex);
    s.endpoint = QUrl(QStringLiteral("https://api.openai.com/v1/chat/completions"));

    const CompletionSettings v = s.validated();
    QCOMPARE(v.endpoint, QUrl(QStringLiteral("https://copilot-proxy.githubusercontent.com/v1/engines/copilot-codex/completions")));
}

void CompletionSettingsTest::roundTripConfig()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = dir.filePath(QStringLiteral("kateaiinlinecompletion_testrc"));
    KConfig config(path, KConfig::SimpleConfig);
    KConfigGroup group(&config, QString::fromLatin1(CompletionSettings::kConfigGroupName));

    CompletionSettings in = CompletionSettings::defaults();
    in.enabled = false;
    in.debounceMs = CompletionSettings::kDebounceMaxMs;
    in.maxPrefixChars = CompletionSettings::kPrefixMaxChars;
    in.maxSuffixChars = CompletionSettings::kSuffixMinChars;
    in.provider = QString::fromLatin1(CompletionSettings::kProviderOllama);
    in.endpoint = QUrl(QStringLiteral("http://localhost:11434/v1/chat/completions"));
    in.model = QStringLiteral("qwen2.5");
    in.promptTemplate = QString::fromLatin1(CompletionSettings::kPromptTemplateFimV1);
    in.enableContextualPrompt = false;
    in.maxContextItems = 3;
    in.maxContextChars = 2048;
    in.enableOpenTabsContext = false;
    in.enableRecentEditsContext = false;
    in.recentEditsMaxFiles = 7;
    in.recentEditsMaxEdits = 5;
    in.recentEditsDiffContextLines = 2;
    in.recentEditsMaxCharsPerEdit = 1500;
    in.recentEditsDebounceMs = 250;
    in.recentEditsMaxLinesPerEdit = 6;
    in.recentEditsActiveDocDistanceLimitFromCursor = 60;
    in.enableDiagnosticsContext = false;
    in.diagnosticsMaxItems = 4;
    in.diagnosticsMaxChars = 1234;
    in.diagnosticsMaxLineDistance = 42;
    in.diagnosticsIncludeWarnings = false;
    in.diagnosticsIncludeInformation = true;
    in.diagnosticsIncludeHints = true;
    in.enableRelatedFilesContext = false;
    in.relatedFilesMaxFiles = 4;
    in.relatedFilesMaxChars = 9000;
    in.relatedFilesMaxCharsPerFile = 2500;
    in.relatedFilesPreferOpenTabs = false;
    in.contextExcludePatterns = {QStringLiteral("*.secret"), QStringLiteral("generated/*")};
    in.enableCompletionStrategy = false;
    in.singleLineMaxTokens = 33;
    in.multilineMaxTokens = 222;
    in.manualMultilineMaxTokens = 333;
    in.afterAcceptMaxTokens = 44;
    in.completionTemperature = 0.7;
    in.singleLineStopAtNewline = false;
    in.suppressWhenCompletionPopupVisible = false;
    in.copilotClientId = QStringLiteral("Iv1.testclient");
    in.copilotNwo = QStringLiteral("example/org");

    in.save(group);

    const CompletionSettings out = CompletionSettings::load(group);

    QCOMPARE(out.enabled, in.enabled);
    QCOMPARE(out.debounceMs, in.debounceMs);
    QCOMPARE(out.maxPrefixChars, in.maxPrefixChars);
    QCOMPARE(out.maxSuffixChars, in.maxSuffixChars);
    QCOMPARE(out.provider, in.provider);
    QCOMPARE(out.endpoint, in.endpoint);
    QCOMPARE(out.model, in.model);
    QCOMPARE(out.promptTemplate, in.promptTemplate);
    QCOMPARE(out.enableContextualPrompt, in.enableContextualPrompt);
    QCOMPARE(out.maxContextItems, in.maxContextItems);
    QCOMPARE(out.maxContextChars, in.maxContextChars);
    QCOMPARE(out.enableOpenTabsContext, in.enableOpenTabsContext);
    QCOMPARE(out.enableRecentEditsContext, in.enableRecentEditsContext);
    QCOMPARE(out.recentEditsMaxFiles, in.recentEditsMaxFiles);
    QCOMPARE(out.recentEditsMaxEdits, in.recentEditsMaxEdits);
    QCOMPARE(out.recentEditsDiffContextLines, in.recentEditsDiffContextLines);
    QCOMPARE(out.recentEditsMaxCharsPerEdit, in.recentEditsMaxCharsPerEdit);
    QCOMPARE(out.recentEditsDebounceMs, in.recentEditsDebounceMs);
    QCOMPARE(out.recentEditsMaxLinesPerEdit, in.recentEditsMaxLinesPerEdit);
    QCOMPARE(out.recentEditsActiveDocDistanceLimitFromCursor, in.recentEditsActiveDocDistanceLimitFromCursor);
    QCOMPARE(out.enableDiagnosticsContext, in.enableDiagnosticsContext);
    QCOMPARE(out.diagnosticsMaxItems, in.diagnosticsMaxItems);
    QCOMPARE(out.diagnosticsMaxChars, in.diagnosticsMaxChars);
    QCOMPARE(out.diagnosticsMaxLineDistance, in.diagnosticsMaxLineDistance);
    QCOMPARE(out.diagnosticsIncludeWarnings, in.diagnosticsIncludeWarnings);
    QCOMPARE(out.diagnosticsIncludeInformation, in.diagnosticsIncludeInformation);
    QCOMPARE(out.diagnosticsIncludeHints, in.diagnosticsIncludeHints);
    QCOMPARE(out.enableRelatedFilesContext, in.enableRelatedFilesContext);
    QCOMPARE(out.relatedFilesMaxFiles, in.relatedFilesMaxFiles);
    QCOMPARE(out.relatedFilesMaxChars, in.relatedFilesMaxChars);
    QCOMPARE(out.relatedFilesMaxCharsPerFile, in.relatedFilesMaxCharsPerFile);
    QCOMPARE(out.relatedFilesPreferOpenTabs, in.relatedFilesPreferOpenTabs);
    QCOMPARE(out.contextExcludePatterns, in.contextExcludePatterns);
    QCOMPARE(out.enableCompletionStrategy, in.enableCompletionStrategy);
    QCOMPARE(out.singleLineMaxTokens, in.singleLineMaxTokens);
    QCOMPARE(out.multilineMaxTokens, in.multilineMaxTokens);
    QCOMPARE(out.manualMultilineMaxTokens, in.manualMultilineMaxTokens);
    QCOMPARE(out.afterAcceptMaxTokens, in.afterAcceptMaxTokens);
    QCOMPARE(out.completionTemperature, in.completionTemperature);
    QCOMPARE(out.singleLineStopAtNewline, in.singleLineStopAtNewline);
    QCOMPARE(out.suppressWhenCompletionPopupVisible, in.suppressWhenCompletionPopupVisible);
    QCOMPARE(out.copilotClientId, in.copilotClientId);
    QCOMPARE(out.copilotNwo, in.copilotNwo);
}

QTEST_MAIN(CompletionSettingsTest)

#include "CompletionSettingsTest.moc"
