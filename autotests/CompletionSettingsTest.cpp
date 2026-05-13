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
    QCOMPARE(out.suppressWhenCompletionPopupVisible, in.suppressWhenCompletionPopupVisible);
    QCOMPARE(out.copilotClientId, in.copilotClientId);
    QCOMPARE(out.copilotNwo, in.copilotNwo);
}

QTEST_MAIN(CompletionSettingsTest)

#include "CompletionSettingsTest.moc"
