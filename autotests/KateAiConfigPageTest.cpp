/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: KateAiConfigPageTest
*/

#include "settings/KateAiConfigPage.h"

#include <QComboBox>
#include <QLabel>
#include <QTest>

class KateAiConfigPageTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void showsProviderRecommendationAndShortcutHint();
};

void KateAiConfigPageTest::showsProviderRecommendationAndShortcutHint()
{
    KateAiConfigPage page(nullptr, nullptr);
    page.show();
    QTest::qWait(50);

    auto *providerHint = page.findChild<QLabel *>(QStringLiteral("providerHintLabel"));
    auto *shortcutHint = page.findChild<QLabel *>(QStringLiteral("shortcutHintLabel"));
    auto *providerCombo = page.findChild<QComboBox *>(QStringLiteral("providerCombo"));

    QVERIFY(providerHint);
    QVERIFY(shortcutHint);
    QVERIFY(providerCombo);

    QVERIFY(providerHint->text().contains(QStringLiteral("qwen3-coder-q4:latest")));
    QVERIFY(shortcutHint->text().contains(QStringLiteral("Tab")));
    QVERIFY(shortcutHint->text().contains(QStringLiteral("Esc")));

    const int copilotIndex = providerCombo->findData(QStringLiteral("github-copilot-codex"));
    QVERIFY(copilotIndex >= 0);
    providerCombo->setCurrentIndex(copilotIndex);

    QVERIFY(providerHint->text().contains(QStringLiteral("GitHub Copilot")));
}

QTEST_MAIN(KateAiConfigPageTest)

#include "KateAiConfigPageTest.moc"
