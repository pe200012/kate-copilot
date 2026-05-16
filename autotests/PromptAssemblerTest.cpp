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
    void diagnosticsRenderAsCommentBlock();
    void relatedFilesRenderWithSpecificHeader();
    void oversizedRelatedFileBlockKeepsTruncatedUsefulPrefix();
    void copilotContextPrefixUsesCommentBoundaries();
    void recentEditsKeepFittingFirstEditAndEndMarkerUnderTightBudget();
    void truncatedRecentEditsKeepEndMarker();
    void recentEditsSkipUntruncatableLargeEditAndKeepLaterFittingEdit();
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

void PromptAssemblerTest::diagnosticsRenderAsCommentBlock()
{
    PromptAssemblyOptions options;
    options.maxContextChars = 2000;

    ContextItem item;
    item.kind = ContextItem::Kind::DiagnosticBag;
    item.providerId = QStringLiteral("diagnostics");
    item.id = QStringLiteral("src/foo.cpp");
    item.importance = 65;
    item.name = QStringLiteral("src/foo.cpp");
    item.value = QStringLiteral("42:13 - error CLANG: use of undeclared identifier 'bar'\n57:9 - warning CLANG-Wunused-variable: unused variable 'x'");

    const BuiltPrompt prompt = PromptAssembler::build(QString::fromLatin1(CompletionSettings::kPromptTemplateFimV3),
                                                      baseContext(),
                                                      QVector<ContextItem>{item},
                                                      options);

    QVERIFY(prompt.userPrompt.contains(QStringLiteral("// Consider these diagnostics from src/foo.cpp:")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("// 42:13 - error CLANG: use of undeclared identifier 'bar'")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("// 57:9 - warning CLANG-Wunused-variable: unused variable 'x'")));
    QVERIFY(prompt.userPrompt.indexOf(QStringLiteral("Consider these diagnostics")) < prompt.userPrompt.indexOf(QStringLiteral("<|fim_prefix|>")));
}

void PromptAssemblerTest::relatedFilesRenderWithSpecificHeader()
{
    PromptAssemblyOptions options;
    options.maxContextChars = 2000;

    ContextItem item = snippet(QStringLiteral("src/foo.h"), QStringLiteral("class Foo {};"));
    item.providerId = QStringLiteral("related-files");
    item.importance = 80;

    const BuiltPrompt prompt = PromptAssembler::build(QString::fromLatin1(CompletionSettings::kPromptTemplateFimV3),
                                                      baseContext(),
                                                      QVector<ContextItem>{item},
                                                      options);

    QVERIFY(prompt.userPrompt.contains(QStringLiteral("// Compare this related file from src/foo.h:")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("class Foo {};")));
}

void PromptAssemblerTest::oversizedRelatedFileBlockKeepsTruncatedUsefulPrefix()
{
    PromptAssemblyOptions options;
    options.maxContextChars = 420;

    const QString body = QStringLiteral("class Foo {\n") + QString(520, QLatin1Char('x')) + QStringLiteral("\n};\nTAIL_MARKER\n");
    ContextItem item = snippet(QStringLiteral("src/foo.h"), body, 90);
    item.providerId = QStringLiteral("related-files");

    const BuiltPrompt prompt = PromptAssembler::build(QString::fromLatin1(CompletionSettings::kPromptTemplateFimV3),
                                                      baseContext(),
                                                      QVector<ContextItem>{item},
                                                      options);

    QVERIFY(prompt.userPrompt.contains(QStringLiteral("// Compare this related file from src/foo.h:")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("class Foo")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("...")));
    QVERIFY(!prompt.userPrompt.contains(QStringLiteral("TAIL_MARKER")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("<|fim_prefix|>")));
}

void PromptAssemblerTest::copilotContextPrefixUsesCommentBoundaries()
{
    PromptAssemblyOptions options;
    options.maxContextChars = 2000;

    ContextItem item = snippet(QStringLiteral("src/foo.h"), QStringLiteral("class Foo {\npublic:\n    void run();\n};\n"), 90);
    item.providerId = QStringLiteral("related-files");

    const QString prefix = PromptAssembler::renderCopilotContextPrefix(baseContext(), QVector<ContextItem>{item}, options);

    QVERIFY(prefix.startsWith(QStringLiteral("// BEGIN RELATED CONTEXT\n")));
    QVERIFY(prefix.contains(QStringLiteral("// File: src/foo.h\n")));
    QVERIFY(prefix.contains(QStringLiteral("// class Foo {\n")));
    QVERIFY(prefix.contains(QStringLiteral("// public:\n")));
    QVERIFY(prefix.contains(QStringLiteral("// END RELATED CONTEXT\n")));
    QVERIFY(!prefix.contains(QStringLiteral("\nclass Foo")));
}

void PromptAssemblerTest::recentEditsKeepFittingFirstEditAndEndMarkerUnderTightBudget()
{
    PromptAssemblyOptions options;
    options.maxContextItems = 3;
    options.maxContextChars = 230;

    ContextItem first = snippet(QStringLiteral("src/first.cpp"), QStringLiteral("File: src/first.cpp\n@@ lines 1-1\n- oldName();\n+ newName();"), 95);
    first.providerId = QStringLiteral("recent-edits");
    ContextItem second = snippet(QStringLiteral("src/second.cpp"), QStringLiteral("File: src/second.cpp\n@@ lines 2-20\n") + QString(500, QLatin1Char('x')), 94);
    second.providerId = QStringLiteral("recent-edits");

    const BuiltPrompt prompt = PromptAssembler::build(QString::fromLatin1(CompletionSettings::kPromptTemplateFimV3),
                                                      baseContext(),
                                                      QVector<ContextItem>{first, second},
                                                      options);

    QVERIFY(prompt.userPrompt.contains(QStringLiteral("src/first.cpp")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("+ newName();")));
    QVERIFY(!prompt.userPrompt.contains(QStringLiteral("src/second.cpp")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("// End of recent edits")));
}

void PromptAssemblerTest::truncatedRecentEditsKeepEndMarker()
{
    PromptAssemblyOptions options;
    options.maxContextChars = 420;

    ContextItem item = snippet(QStringLiteral("src/large.cpp"), QStringLiteral("File: src/large.cpp\n@@ lines 1-60\n") + QString(900, QLatin1Char('x')), 95);
    item.providerId = QStringLiteral("recent-edits");

    const BuiltPrompt prompt = PromptAssembler::build(QString::fromLatin1(CompletionSettings::kPromptTemplateFimV3),
                                                      baseContext(),
                                                      QVector<ContextItem>{item},
                                                      options);

    QVERIFY(prompt.userPrompt.contains(QStringLiteral("src/large.cpp")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("...")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("// End of recent edits")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("<|fim_prefix|>")));
}

void PromptAssemblerTest::recentEditsSkipUntruncatableLargeEditAndKeepLaterFittingEdit()
{
    PromptAssemblyOptions options;
    options.maxContextItems = 3;
    options.maxContextChars = 145;

    ContextItem large = snippet(QStringLiteral("src/large.cpp"), QStringLiteral("File: src/large.cpp\n@@ lines 1-80\n") + QString(900, QLatin1Char('x')), 95);
    large.providerId = QStringLiteral("recent-edits");
    ContextItem small = snippet(QStringLiteral("src/small.cpp"), QStringLiteral("File: src/small.cpp\n@@ lines 4-4\n+ ok();"), 94);
    small.providerId = QStringLiteral("recent-edits");

    const BuiltPrompt prompt = PromptAssembler::build(QString::fromLatin1(CompletionSettings::kPromptTemplateFimV3),
                                                      baseContext(),
                                                      QVector<ContextItem>{large, small},
                                                      options);

    QVERIFY(!prompt.userPrompt.contains(QStringLiteral("src/large.cpp")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("src/small.cpp")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("+ ok();")));
    QVERIFY(prompt.userPrompt.contains(QStringLiteral("// End of recent edits")));
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
