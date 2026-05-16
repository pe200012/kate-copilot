/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: KateAiConfigPageTest
*/

#include "settings/KateAiConfigPage.h"

#include "plugin/KateAiInlineCompletionPlugin.h"
#include "settings/CompletionSettings.h"

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
#include <QTest>

class KateAiConfigPageTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void showsProviderRecommendationAndShortcutHint();
    void hiddenRecentEditsSettingsSurviveApply();
};

void KateAiConfigPageTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void KateAiConfigPageTest::showsProviderRecommendationAndShortcutHint()
{
    KateAiConfigPage page(nullptr, nullptr);
    page.show();
    QTest::qWait(50);

    auto *providerHint = page.findChild<QLabel *>(QStringLiteral("providerHintLabel"));
    auto *shortcutHint = page.findChild<QLabel *>(QStringLiteral("shortcutHintLabel"));
    auto *providerCombo = page.findChild<QComboBox *>(QStringLiteral("providerCombo"));
    auto *verifySession = page.findChild<QPushButton *>(QStringLiteral("copilotVerifySessionButton"));

    QVERIFY(providerHint);
    QVERIFY(shortcutHint);
    QVERIFY(providerCombo);
    QVERIFY(verifySession);

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
