/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: KateAiConfigPageTest
*/

#include "settings/KateAiConfigPage.h"

#include "plugin/KateAiInlineCompletionPlugin.h"
#include "settings/CompletionSettings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTest>

class KateAiConfigPageTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void showsProviderRecommendationShortcutHintAndContextControls();
    void contextualSettingsApplyFromUi();
    void contextControlsFollowMasterAndRelatedFileToggles();
    void strategySettingsApplyFromUi();
    void cacheSettingsApplyFromUi();
    void candidateSettingsApplyFromUi();
    void inlineEditSettingsApplyFromUi();
    void hiddenRecentEditsSettingsSurviveApply();
};

void KateAiConfigPageTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void KateAiConfigPageTest::showsProviderRecommendationShortcutHintAndContextControls()
{
    KateAiConfigPage page(nullptr, nullptr);
    page.show();
    QTest::qWait(50);

    auto *providerHint = page.findChild<QLabel *>(QStringLiteral("providerHintLabel"));
    auto *shortcutHint = page.findChild<QLabel *>(QStringLiteral("shortcutHintLabel"));
    auto *providerCombo = page.findChild<QComboBox *>(QStringLiteral("providerCombo"));
    auto *verifySession = page.findChild<QPushButton *>(QStringLiteral("copilotVerifySessionButton"));
    auto *contextualPrompt = page.findChild<QCheckBox *>(QStringLiteral("contextualPromptCheckBox"));
    auto *openTabs = page.findChild<QCheckBox *>(QStringLiteral("openTabsContextCheckBox"));
    auto *recentEdits = page.findChild<QCheckBox *>(QStringLiteral("recentEditsContextCheckBox"));
    auto *diagnostics = page.findChild<QCheckBox *>(QStringLiteral("diagnosticsContextCheckBox"));
    auto *relatedFiles = page.findChild<QCheckBox *>(QStringLiteral("relatedFilesContextCheckBox"));
    auto *excludePatterns = page.findChild<QLineEdit *>(QStringLiteral("contextExcludePatternsEdit"));
    auto *strategyEnabled = page.findChild<QCheckBox *>(QStringLiteral("adaptiveStrategyCheckBox"));
    auto *singleLineMaxTokens = page.findChild<QSpinBox *>(QStringLiteral("singleLineMaxTokensSpinBox"));
    auto *temperature = page.findChild<QDoubleSpinBox *>(QStringLiteral("completionTemperatureSpinBox"));
    auto *cacheEnabled = page.findChild<QCheckBox *>(QStringLiteral("completionCacheCheckBox"));
    auto *typingReuse = page.findChild<QCheckBox *>(QStringLiteral("typingAsSuggestedCheckBox"));
    auto *cacheMaxEntries = page.findChild<QSpinBox *>(QStringLiteral("completionCacheMaxEntriesSpinBox"));
    auto *candidateCycling = page.findChild<QCheckBox *>(QStringLiteral("candidateCyclingCheckBox"));
    auto *manualCandidateCount = page.findChild<QSpinBox *>(QStringLiteral("manualCandidateCountSpinBox"));
    auto *speculativeRequests = page.findChild<QCheckBox *>(QStringLiteral("speculativeRequestsCheckBox"));
    auto *inlineEdits = page.findChild<QCheckBox *>(QStringLiteral("inlineEditsCheckBox"));
    auto *inlineEditMaxNewText = page.findChild<QSpinBox *>(QStringLiteral("inlineEditMaxNewTextCharsSpinBox"));

    QVERIFY(providerHint);
    QVERIFY(shortcutHint);
    QVERIFY(providerCombo);
    QVERIFY(verifySession);
    QVERIFY(contextualPrompt);
    QVERIFY(openTabs);
    QVERIFY(recentEdits);
    QVERIFY(diagnostics);
    QVERIFY(relatedFiles);
    QVERIFY(excludePatterns);
    QVERIFY(strategyEnabled);
    QVERIFY(singleLineMaxTokens);
    QVERIFY(temperature);
    QVERIFY(cacheEnabled);
    QVERIFY(typingReuse);
    QVERIFY(cacheMaxEntries);
    QVERIFY(candidateCycling);
    QVERIFY(manualCandidateCount);
    QVERIFY(speculativeRequests);
    QVERIFY(inlineEdits);
    QVERIFY(inlineEditMaxNewText);

    QVERIFY(providerHint->text().contains(QStringLiteral("qwen3-coder-q4:latest")));
    QVERIFY(shortcutHint->text().contains(QStringLiteral("Tab")));
    QVERIFY(shortcutHint->text().contains(QStringLiteral("Esc")));
    QVERIFY(shortcutHint->text().contains(QStringLiteral("Ctrl+Alt+Shift+Right")));
    QVERIFY(shortcutHint->text().contains(QStringLiteral("Ctrl+Alt+Shift+L")));
    QVERIFY(shortcutHint->text().contains(QStringLiteral("Ctrl+Alt+Shift+Space")));

    const int copilotIndex = providerCombo->findData(QStringLiteral("github-copilot-codex"));
    QVERIFY(copilotIndex >= 0);
    providerCombo->setCurrentIndex(copilotIndex);

    QVERIFY(providerHint->text().contains(QStringLiteral("GitHub Copilot")));
}

void KateAiConfigPageTest::contextualSettingsApplyFromUi()
{
    KateAiInlineCompletionPlugin plugin(nullptr, {});
    KateAiConfigPage page(nullptr, &plugin);

    auto *contextualPrompt = page.findChild<QCheckBox *>(QStringLiteral("contextualPromptCheckBox"));
    auto *maxContextItems = page.findChild<QSpinBox *>(QStringLiteral("maxContextItemsSpinBox"));
    auto *maxContextChars = page.findChild<QSpinBox *>(QStringLiteral("maxContextCharsSpinBox"));
    auto *openTabs = page.findChild<QCheckBox *>(QStringLiteral("openTabsContextCheckBox"));
    auto *recentEdits = page.findChild<QCheckBox *>(QStringLiteral("recentEditsContextCheckBox"));
    auto *diagnostics = page.findChild<QCheckBox *>(QStringLiteral("diagnosticsContextCheckBox"));
    auto *relatedFiles = page.findChild<QCheckBox *>(QStringLiteral("relatedFilesContextCheckBox"));
    auto *relatedFilesMaxFiles = page.findChild<QSpinBox *>(QStringLiteral("relatedFilesMaxFilesSpinBox"));
    auto *relatedFilesMaxChars = page.findChild<QSpinBox *>(QStringLiteral("relatedFilesMaxCharsSpinBox"));
    auto *relatedFilesMaxCharsPerFile = page.findChild<QSpinBox *>(QStringLiteral("relatedFilesMaxCharsPerFileSpinBox"));
    auto *excludePatterns = page.findChild<QLineEdit *>(QStringLiteral("contextExcludePatternsEdit"));

    QVERIFY(contextualPrompt);
    QVERIFY(maxContextItems);
    QVERIFY(maxContextChars);
    QVERIFY(openTabs);
    QVERIFY(recentEdits);
    QVERIFY(diagnostics);
    QVERIFY(relatedFiles);
    QVERIFY(relatedFilesMaxFiles);
    QVERIFY(relatedFilesMaxChars);
    QVERIFY(relatedFilesMaxCharsPerFile);
    QVERIFY(excludePatterns);

    contextualPrompt->setChecked(false);
    maxContextItems->setValue(9);
    maxContextChars->setValue(9000);
    openTabs->setChecked(false);
    recentEdits->setChecked(false);
    diagnostics->setChecked(false);
    relatedFiles->setChecked(false);
    relatedFilesMaxFiles->setValue(4);
    relatedFilesMaxChars->setValue(8000);
    relatedFilesMaxCharsPerFile->setValue(2000);
    excludePatterns->setText(QStringLiteral("*.secret; generated/*"));

    page.apply();

    const KateAiInlineCompletion::CompletionSettings out = plugin.settings().validated();
    QCOMPARE(out.enableContextualPrompt, false);
    QCOMPARE(out.maxContextItems, 9);
    QCOMPARE(out.maxContextChars, 9000);
    QCOMPARE(out.enableOpenTabsContext, false);
    QCOMPARE(out.enableRecentEditsContext, false);
    QCOMPARE(out.enableDiagnosticsContext, false);
    QCOMPARE(out.enableRelatedFilesContext, false);
    QCOMPARE(out.relatedFilesMaxFiles, 4);
    QCOMPARE(out.relatedFilesMaxChars, 8000);
    QCOMPARE(out.relatedFilesMaxCharsPerFile, 2000);
    QCOMPARE(out.contextExcludePatterns, QStringList({QStringLiteral("*.secret"), QStringLiteral("generated/*")}));
}

void KateAiConfigPageTest::contextControlsFollowMasterAndRelatedFileToggles()
{
    KateAiInlineCompletionPlugin plugin(nullptr, {});
    KateAiConfigPage page(nullptr, &plugin);

    auto *contextualPrompt = page.findChild<QCheckBox *>(QStringLiteral("contextualPromptCheckBox"));
    auto *maxContextItems = page.findChild<QSpinBox *>(QStringLiteral("maxContextItemsSpinBox"));
    auto *openTabs = page.findChild<QCheckBox *>(QStringLiteral("openTabsContextCheckBox"));
    auto *relatedFiles = page.findChild<QCheckBox *>(QStringLiteral("relatedFilesContextCheckBox"));
    auto *relatedFilesMaxFiles = page.findChild<QSpinBox *>(QStringLiteral("relatedFilesMaxFilesSpinBox"));
    auto *excludePatterns = page.findChild<QLineEdit *>(QStringLiteral("contextExcludePatternsEdit"));

    QVERIFY(contextualPrompt);
    QVERIFY(maxContextItems);
    QVERIFY(openTabs);
    QVERIFY(relatedFiles);
    QVERIFY(relatedFilesMaxFiles);
    QVERIFY(excludePatterns);

    contextualPrompt->setChecked(false);
    QVERIFY(!maxContextItems->isEnabled());
    QVERIFY(!openTabs->isEnabled());
    QVERIFY(!relatedFiles->isEnabled());
    QVERIFY(!relatedFilesMaxFiles->isEnabled());
    QVERIFY(!excludePatterns->isEnabled());

    contextualPrompt->setChecked(true);
    relatedFiles->setChecked(false);
    QVERIFY(maxContextItems->isEnabled());
    QVERIFY(openTabs->isEnabled());
    QVERIFY(relatedFiles->isEnabled());
    QVERIFY(!relatedFilesMaxFiles->isEnabled());
    QVERIFY(!excludePatterns->isEnabled());

    relatedFiles->setChecked(true);
    QVERIFY(relatedFilesMaxFiles->isEnabled());
    QVERIFY(excludePatterns->isEnabled());
}

void KateAiConfigPageTest::strategySettingsApplyFromUi()
{
    KateAiInlineCompletionPlugin plugin(nullptr, {});
    KateAiConfigPage page(nullptr, &plugin);

    auto *strategyEnabled = page.findChild<QCheckBox *>(QStringLiteral("adaptiveStrategyCheckBox"));
    auto *singleLineMaxTokens = page.findChild<QSpinBox *>(QStringLiteral("singleLineMaxTokensSpinBox"));
    auto *multilineMaxTokens = page.findChild<QSpinBox *>(QStringLiteral("multilineMaxTokensSpinBox"));
    auto *manualMultilineMaxTokens = page.findChild<QSpinBox *>(QStringLiteral("manualMultilineMaxTokensSpinBox"));
    auto *afterAcceptMaxTokens = page.findChild<QSpinBox *>(QStringLiteral("afterAcceptMaxTokensSpinBox"));
    auto *temperature = page.findChild<QDoubleSpinBox *>(QStringLiteral("completionTemperatureSpinBox"));
    auto *singleLineStopAtNewline = page.findChild<QCheckBox *>(QStringLiteral("singleLineStopAtNewlineCheckBox"));

    QVERIFY(strategyEnabled);
    QVERIFY(singleLineMaxTokens);
    QVERIFY(multilineMaxTokens);
    QVERIFY(manualMultilineMaxTokens);
    QVERIFY(afterAcceptMaxTokens);
    QVERIFY(temperature);
    QVERIFY(singleLineStopAtNewline);

    strategyEnabled->setChecked(false);
    singleLineMaxTokens->setValue(37);
    multilineMaxTokens->setValue(211);
    manualMultilineMaxTokens->setValue(333);
    afterAcceptMaxTokens->setValue(55);
    temperature->setValue(0.6);
    singleLineStopAtNewline->setChecked(false);

    page.apply();

    const KateAiInlineCompletion::CompletionSettings out = plugin.settings().validated();
    QCOMPARE(out.enableCompletionStrategy, false);
    QCOMPARE(out.singleLineMaxTokens, 37);
    QCOMPARE(out.multilineMaxTokens, 211);
    QCOMPARE(out.manualMultilineMaxTokens, 333);
    QCOMPARE(out.afterAcceptMaxTokens, 55);
    QCOMPARE(out.completionTemperature, 0.6);
    QCOMPARE(out.singleLineStopAtNewline, false);
}

void KateAiConfigPageTest::cacheSettingsApplyFromUi()
{
    KateAiInlineCompletionPlugin plugin(nullptr, {});
    KateAiConfigPage page(nullptr, &plugin);

    auto *cacheEnabled = page.findChild<QCheckBox *>(QStringLiteral("completionCacheCheckBox"));
    auto *typingReuse = page.findChild<QCheckBox *>(QStringLiteral("typingAsSuggestedCheckBox"));
    auto *maxEntries = page.findChild<QSpinBox *>(QStringLiteral("completionCacheMaxEntriesSpinBox"));
    auto *ttl = page.findChild<QSpinBox *>(QStringLiteral("completionCacheTtlSpinBox"));
    auto *prefixTail = page.findChild<QSpinBox *>(QStringLiteral("completionCachePrefixTailSpinBox"));
    auto *suffixHead = page.findChild<QSpinBox *>(QStringLiteral("completionCacheSuffixHeadSpinBox"));

    QVERIFY(cacheEnabled);
    QVERIFY(typingReuse);
    QVERIFY(maxEntries);
    QVERIFY(ttl);
    QVERIFY(prefixTail);
    QVERIFY(suffixHead);

    cacheEnabled->setChecked(false);
    typingReuse->setChecked(false);
    maxEntries->setValue(77);
    ttl->setValue(45000);
    prefixTail->setValue(900);
    suffixHead->setValue(300);

    QVERIFY(typingReuse->isEnabled());
    QVERIFY(!maxEntries->isEnabled());

    page.apply();

    const KateAiInlineCompletion::CompletionSettings out = plugin.settings().validated();
    QCOMPARE(out.enableCompletionCache, false);
    QCOMPARE(out.enableTypingAsSuggested, false);
    QCOMPARE(out.completionCacheMaxEntries, 77);
    QCOMPARE(out.completionCacheTtlMs, 45000);
    QCOMPARE(out.completionCachePrefixTailChars, 900);
    QCOMPARE(out.completionCacheSuffixHeadChars, 300);
}

void KateAiConfigPageTest::candidateSettingsApplyFromUi()
{
    KateAiInlineCompletionPlugin plugin(nullptr, {});
    KateAiConfigPage page(nullptr, &plugin);

    auto *candidateCycling = page.findChild<QCheckBox *>(QStringLiteral("candidateCyclingCheckBox"));
    auto *manualCandidateCount = page.findChild<QSpinBox *>(QStringLiteral("manualCandidateCountSpinBox"));
    auto *maxStoredCandidates = page.findChild<QSpinBox *>(QStringLiteral("maxStoredCandidatesSpinBox"));
    auto *speculativeRequests = page.findChild<QCheckBox *>(QStringLiteral("speculativeRequestsCheckBox"));
    auto *speculativeDelay = page.findChild<QSpinBox *>(QStringLiteral("speculativeRequestDelaySpinBox"));
    auto *speculativeMaxTokens = page.findChild<QSpinBox *>(QStringLiteral("speculativeRequestMaxTokensSpinBox"));

    QVERIFY(candidateCycling);
    QVERIFY(manualCandidateCount);
    QVERIFY(maxStoredCandidates);
    QVERIFY(speculativeRequests);
    QVERIFY(speculativeDelay);
    QVERIFY(speculativeMaxTokens);

    candidateCycling->setChecked(false);
    manualCandidateCount->setValue(5);
    maxStoredCandidates->setValue(6);
    speculativeRequests->setChecked(true);
    speculativeDelay->setValue(400);
    speculativeMaxTokens->setValue(88);

    QVERIFY(!manualCandidateCount->isEnabled());
    QVERIFY(speculativeDelay->isEnabled());
    QVERIFY(speculativeMaxTokens->isEnabled());

    page.apply();

    const KateAiInlineCompletion::CompletionSettings out = plugin.settings().validated();
    QCOMPARE(out.enableCandidateCycling, false);
    QCOMPARE(out.manualCandidateCount, 5);
    QCOMPARE(out.maxStoredCandidates, 6);
    QCOMPARE(out.enableSpeculativeRequests, true);
    QCOMPARE(out.speculativeRequestDelayMs, 400);
    QCOMPARE(out.speculativeRequestMaxTokens, 88);
}

void KateAiConfigPageTest::inlineEditSettingsApplyFromUi()
{
    KateAiInlineCompletionPlugin plugin(nullptr, {});
    KateAiConfigPage page(nullptr, &plugin);

    auto *inlineEdits = page.findChild<QCheckBox *>(QStringLiteral("inlineEditsCheckBox"));
    auto *maxNewText = page.findChild<QSpinBox *>(QStringLiteral("inlineEditMaxNewTextCharsSpinBox"));
    auto *maxPrefix = page.findChild<QSpinBox *>(QStringLiteral("inlineEditMaxPrefixCharsSpinBox"));
    auto *maxSuffix = page.findChild<QSpinBox *>(QStringLiteral("inlineEditMaxSuffixCharsSpinBox"));
    auto *useContext = page.findChild<QCheckBox *>(QStringLiteral("inlineEditUseContextCheckBox"));
    auto *copilotExperimental = page.findChild<QCheckBox *>(QStringLiteral("inlineEditCopilotExperimentalCheckBox"));

    QVERIFY(inlineEdits);
    QVERIFY(maxNewText);
    QVERIFY(maxPrefix);
    QVERIFY(maxSuffix);
    QVERIFY(useContext);
    QVERIFY(copilotExperimental);

    inlineEdits->setChecked(false);
    maxNewText->setValue(12345);
    maxPrefix->setValue(2345);
    maxSuffix->setValue(1234);
    useContext->setChecked(false);
    copilotExperimental->setChecked(true);

    QVERIFY(!maxNewText->isEnabled());
    QVERIFY(!maxPrefix->isEnabled());
    QVERIFY(!maxSuffix->isEnabled());

    page.apply();

    const KateAiInlineCompletion::CompletionSettings out = plugin.settings().validated();
    QCOMPARE(out.enableInlineEdits, false);
    QCOMPARE(out.inlineEditMaxNewTextChars, 12345);
    QCOMPARE(out.inlineEditMaxPrefixChars, 2345);
    QCOMPARE(out.inlineEditMaxSuffixChars, 1234);
    QCOMPARE(out.inlineEditUseContext, false);
    QCOMPARE(out.inlineEditCopilotExperimental, true);
}

void KateAiConfigPageTest::hiddenRecentEditsSettingsSurviveApply()
{
    KateAiInlineCompletionPlugin plugin(nullptr, {});

    KateAiInlineCompletion::CompletionSettings settings = KateAiInlineCompletion::CompletionSettings::defaults();
    settings.enableRecentEditsContext = false;
    settings.recentEditsMaxFiles = 7;
    settings.recentEditsMaxEdits = 5;
    settings.recentEditsDiffContextLines = 2;
    settings.recentEditsMaxCharsPerEdit = 1500;
    settings.recentEditsDebounceMs = 250;
    settings.recentEditsMaxLinesPerEdit = 6;
    settings.recentEditsActiveDocDistanceLimitFromCursor = 60;
    settings.enableDiagnosticsContext = false;
    settings.diagnosticsMaxItems = 4;
    settings.diagnosticsMaxChars = 1234;
    settings.diagnosticsMaxLineDistance = 42;
    settings.diagnosticsIncludeWarnings = false;
    settings.diagnosticsIncludeInformation = true;
    settings.diagnosticsIncludeHints = true;
    plugin.setSettings(settings);

    KateAiConfigPage page(nullptr, &plugin);
    page.apply();

    const KateAiInlineCompletion::CompletionSettings out = plugin.settings().validated();
    QCOMPARE(out.enableRecentEditsContext, settings.enableRecentEditsContext);
    QCOMPARE(out.recentEditsMaxFiles, settings.recentEditsMaxFiles);
    QCOMPARE(out.recentEditsMaxEdits, settings.recentEditsMaxEdits);
    QCOMPARE(out.recentEditsDiffContextLines, settings.recentEditsDiffContextLines);
    QCOMPARE(out.recentEditsMaxCharsPerEdit, settings.recentEditsMaxCharsPerEdit);
    QCOMPARE(out.recentEditsDebounceMs, settings.recentEditsDebounceMs);
    QCOMPARE(out.recentEditsMaxLinesPerEdit, settings.recentEditsMaxLinesPerEdit);
    QCOMPARE(out.recentEditsActiveDocDistanceLimitFromCursor, settings.recentEditsActiveDocDistanceLimitFromCursor);
    QCOMPARE(out.enableDiagnosticsContext, settings.enableDiagnosticsContext);
    QCOMPARE(out.diagnosticsMaxItems, settings.diagnosticsMaxItems);
    QCOMPARE(out.diagnosticsMaxChars, settings.diagnosticsMaxChars);
    QCOMPARE(out.diagnosticsMaxLineDistance, settings.diagnosticsMaxLineDistance);
    QCOMPARE(out.diagnosticsIncludeWarnings, settings.diagnosticsIncludeWarnings);
    QCOMPARE(out.diagnosticsIncludeInformation, settings.diagnosticsIncludeInformation);
    QCOMPARE(out.diagnosticsIncludeHints, settings.diagnosticsIncludeHints);
}

QTEST_MAIN(KateAiConfigPageTest)

#include "KateAiConfigPageTest.moc"
