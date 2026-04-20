/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: PromptTemplateTest
*/

#include "prompt/PromptTemplate.h"

#include "settings/CompletionSettings.h"

#include <QtTest>

using KateAiInlineCompletion::BuiltPrompt;
using KateAiInlineCompletion::CompletionSettings;
using KateAiInlineCompletion::PromptContext;
using KateAiInlineCompletion::PromptTemplate;

class PromptTemplateTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void fimV1DoesNotAddFimMiddle();
    void fimV2AddsCursorMetadataAndStops();
    void fimV3StartsWithPrefixTokenAndStops();
    void sanitizeExtractsFencedBlock();
    void sanitizeRespectsFimMarkers();
};

void PromptTemplateTest::fimV1DoesNotAddFimMiddle()
{
    PromptContext ctx;
    ctx.language = QStringLiteral("C++");
    ctx.prefix = QStringLiteral("int main() {\n");
    ctx.suffix = QStringLiteral("\n}\n");

    const BuiltPrompt p = PromptTemplate::build(QString::fromLatin1(CompletionSettings::kPromptTemplateFimV1), ctx);

    QVERIFY(p.userPrompt.contains(QStringLiteral("<|fim_prefix|>")));
    QVERIFY(p.userPrompt.contains(QStringLiteral("<|fim_suffix|>")));
    QVERIFY(!p.userPrompt.contains(QStringLiteral("<|fim_middle|>")));
    QVERIFY(p.stopSequences.isEmpty());
}

void PromptTemplateTest::fimV2AddsCursorMetadataAndStops()
{
    PromptContext ctx;
    ctx.filePath = QStringLiteral("/tmp/example.cpp");
    ctx.language = QStringLiteral("C++");
    ctx.cursorLine1 = 12;
    ctx.cursorColumn1 = 3;
    ctx.prefix = QStringLiteral("int add(int a, int b) {\n  return ");
    ctx.suffix = QStringLiteral(";\n}\n");

    const BuiltPrompt p = PromptTemplate::build(QString::fromLatin1(CompletionSettings::kPromptTemplateFimV2), ctx);

    QVERIFY(p.userPrompt.contains(QStringLiteral("// File: /tmp/example.cpp")));
    QVERIFY(p.userPrompt.contains(QStringLiteral("// Language: C++")));
    QVERIFY(p.userPrompt.contains(QStringLiteral("// Cursor: line 12, column 3")));
    QVERIFY(p.userPrompt.contains(QStringLiteral("<|fim_middle|>")));

    QVERIFY(p.stopSequences.contains(QStringLiteral("<|fim_prefix|>")));
    QVERIFY(p.stopSequences.contains(QStringLiteral("<|fim_suffix|>")));
    QVERIFY(p.stopSequences.contains(QStringLiteral("<|fim_middle|>")));
}

void PromptTemplateTest::fimV3StartsWithPrefixTokenAndStops()
{
    PromptContext ctx;
    ctx.filePath = QStringLiteral("/tmp/example.py");
    ctx.language = QStringLiteral("Python");
    ctx.cursorLine1 = 1;
    ctx.cursorColumn1 = 5;
    ctx.prefix = QStringLiteral("def f():\n    return ");
    ctx.suffix = QStringLiteral("\n");

    const BuiltPrompt p = PromptTemplate::build(QString::fromLatin1(CompletionSettings::kPromptTemplateFimV3), ctx);

    QVERIFY(p.userPrompt.startsWith(QStringLiteral("<|fim_prefix|>")));
    QVERIFY(p.userPrompt.contains(QStringLiteral("<|fim_suffix|>")));
    QVERIFY(p.userPrompt.contains(QStringLiteral("<|fim_middle|>")));

    QVERIFY(p.stopSequences.contains(QStringLiteral("<|fim_prefix|>")));
    QVERIFY(p.stopSequences.contains(QStringLiteral("<|fim_suffix|>")));
    QVERIFY(p.stopSequences.contains(QStringLiteral("<|fim_middle|>")));
    QVERIFY(p.stopSequences.contains(QStringLiteral("```")));
}

void PromptTemplateTest::sanitizeExtractsFencedBlock()
{
    const QString raw = QStringLiteral("```cpp\nreturn a + b;\n```\n");
    const QString cleaned = PromptTemplate::sanitizeCompletion(raw);
    QCOMPARE(cleaned, QStringLiteral("return a + b;\n"));
}

void PromptTemplateTest::sanitizeRespectsFimMarkers()
{
    const QString raw = QStringLiteral("noise <|fim_middle|>abc<|fim_suffix|> tail");
    const QString cleaned = PromptTemplate::sanitizeCompletion(raw);
    QCOMPARE(cleaned, QStringLiteral("abc"));
}

QTEST_MAIN(PromptTemplateTest)

#include "PromptTemplateTest.moc"
