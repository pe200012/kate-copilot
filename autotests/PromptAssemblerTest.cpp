/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: PromptAssemblerTest
*/

#include "prompt/PromptAssembler.h"

#include "settings/CompletionSettings.h"

#include <QtTest>

using KateAiInlineCompletion::BuiltPrompt;
using KateAiInlineCompletion::CompletionSettings;
using KateAiInlineCompletion::ContextItem;
using KateAiInlineCompletion::PromptAssembler;
using KateAiInlineCompletion::PromptAssemblyOptions;
using KateAiInlineCompletion::PromptContext;

namespace
{

ContextItem trait(QString name, QString value, int importance = 50)
{
    ContextItem item;
    item.kind = ContextItem::Kind::Trait;
    item.providerId = QStringLiteral("test");
    item.id = name;
    item.importance = importance;
    item.name = std::move(name);
    item.value = std::move(value);
    return item;
}

ContextItem snippet(QString path, QString code, int importance = 50)
{
    ContextItem item;
    item.kind = ContextItem::Kind::CodeSnippet;
    item.providerId = QStringLiteral("open-tabs");
    item.id = path;
    item.importance = importance;
    item.uri = path;
    item.name = std::move(path);
    item.value = std::move(code);
    return item;
}

PromptContext baseContext()
{
    PromptContext ctx;
    ctx.filePath = QStringLiteral("/repo/src/main.cpp");
    ctx.language = QStringLiteral("C++");
    ctx.cursorLine1 = 10;
    ctx.cursorColumn1 = 5;
    ctx.prefix = QStringLiteral("int main() {\n    return ");
    ctx.suffix = QStringLiteral(";\n}\n");
    return ctx;
}

} // namespace

class PromptAssemblerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void traitsRenderBeforeFimMarkers();
    void codeSnippetsIncludePathHeaders();
    void recentEditsRenderAsEditPatternBlock();
    void budgetDropsLowerImportanceItemsFirst();
    void disabledContextKeepsFimPromptValid();
};

void PromptAssemblerTest::traitsRenderBeforeFimMarkers()
{
    PromptAssemblyOptions options;
    options.maxContextChars = 2000;

    const BuiltPrompt prompt = PromptAssembler::build(QString::fromLatin1(CompletionSettings::kPromptTemplateFimV3),
                                                      baseContext(),
                                                      QVector<ContextItem>{trait(QStringLiteral("build_system"), QStringLiteral("CMake"))},
                                                      options);

    QVERIFY(prompt.userPrompt.contains(QStringLiteral("// Related project information:")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("// build_system: CMake")));
    QVERIFY(prompt.userPrompt.indexOf(QStringLiteral("build_system")) < prompt.userPrompt.indexOf(QStringLiteral("<|fim_prefix|>")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("<|fim_suffix|>")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("<|fim_middle|>")));
}

void PromptAssemblerTest::codeSnippetsIncludePathHeaders()
{
    PromptAssemblyOptions options;
    options.maxContextChars = 2000;

    const BuiltPrompt prompt = PromptAssembler::build(QString::fromLatin1(CompletionSettings::kPromptTemplateFimV3),
                                                      baseContext(),
                                                      QVector<ContextItem>{snippet(QStringLiteral("src/foo.h"), QStringLiteral("class Foo {};"))},
                                                      options);

    QVERIFY(prompt.userPrompt.contains(QStringLiteral("// Compare this snippet from src/foo.h:")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("class Foo {};")));
}

void PromptAssemblerTest::recentEditsRenderAsEditPatternBlock()
{
    PromptAssemblyOptions options;
    options.maxContextChars = 2000;

    ContextItem item = snippet(QStringLiteral("src/foo.cpp"), QStringLiteral("File: src/foo.cpp\n@@ lines 40-47\n- oldName();\n+ newName();"), 95);
    item.providerId = QStringLiteral("recent-edits");

    const BuiltPrompt prompt = PromptAssembler::build(QString::fromLatin1(CompletionSettings::kPromptTemplateFimV3),
                                                      baseContext(),
                                                      QVector<ContextItem>{item},
                                                      options);

    QVERIFY(prompt.userPrompt.contains(QStringLiteral("// Recently edited files. Continue the user's current edit pattern.")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("// File: src/foo.cpp")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("@@ lines 40-47")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("+ newName();")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("// End of recent edits")));
    QVERIFY(prompt.userPrompt.indexOf(QStringLiteral("Recently edited files")) < prompt.userPrompt.indexOf(QStringLiteral("<|fim_prefix|>")));
}

void PromptAssemblerTest::budgetDropsLowerImportanceItemsFirst()
{
    PromptAssemblyOptions options;
    options.maxContextItems = 2;
    options.maxContextChars = 90;

    const QVector<ContextItem> items{
        trait(QStringLiteral("low_importance"), QStringLiteral("low-value-that-should-be-dropped"), 1),
        trait(QStringLiteral("high_importance"), QStringLiteral("high-value"), 100),
    };

    const BuiltPrompt prompt = PromptAssembler::build(QString::fromLatin1(CompletionSettings::kPromptTemplateFimV3), baseContext(), items, options);

    QVERIFY(prompt.userPrompt.contains(QStringLiteral("high_importance")));
    QVERIFY(!prompt.userPrompt.contains(QStringLiteral("low_importance")));
}

void PromptAssemblerTest::disabledContextKeepsFimPromptValid()
{
    PromptAssemblyOptions options;
    options.enabled = false;

    const BuiltPrompt prompt = PromptAssembler::build(QString::fromLatin1(CompletionSettings::kPromptTemplateFimV3),
                                                      baseContext(),
                                                      QVector<ContextItem>{trait(QStringLiteral("build_system"), QStringLiteral("CMake"))},
                                                      options);

    QVERIFY(prompt.userPrompt.startsWith(QStringLiteral("<|fim_prefix|>")));
    QVERIFY(prompt.userPrompt.contains(baseContext().prefix));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("<|fim_suffix|>")));
    QVERIFY(prompt.userPrompt.contains(baseContext().suffix));
    QVERIFY(prompt.userPrompt.endsWith(QStringLiteral("<|fim_middle|>")));
    QVERIFY(!prompt.userPrompt.contains(QStringLiteral("build_system")));
}

QTEST_MAIN(PromptAssemblerTest)

#include "PromptAssemblerTest.moc"
