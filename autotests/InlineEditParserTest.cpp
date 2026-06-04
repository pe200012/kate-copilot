/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: InlineEditParserTest
*/

#include "inlineedit/InlineEditParser.h"

#include <KTextEditor/Document>
#include <KTextEditor/Editor>

#include <QTest>

#include <memory>

using KateAiInlineCompletion::InlineEditParser;
using KateAiInlineCompletion::InlineEditParserOptions;

namespace
{
std::unique_ptr<KTextEditor::Document> makeDocument(const QString &text)
{
    auto *editor = KTextEditor::Editor::instance();
    Q_ASSERT(editor);
    std::unique_ptr<KTextEditor::Document> doc(editor->createDocument(nullptr));
    Q_ASSERT(doc);
    doc->setText(text);
    return doc;
}
}

class InlineEditParserTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesPlainJsonEdit();
    void parsesFencedJsonEdit();
    void convertsOneBasedPositionsToZeroBasedRange();
    void rejectsInvalidJson();
    void rejectsMissingEdits();
    void rejectsOutOfBoundsRange();
    void rejectsEmptyNewTextWhenDeletionDisabled();
    void rejectsNoOpReplacement();
    void rejectsNewTextAboveMax();
    void rejectsMultipleEdits();
    void rejectsRangeDifferentFromExpectedRange();
    void normalizesCrLf();
};

void InlineEditParserTest::parsesPlainJsonEdit()
{
    auto doc = makeDocument(QStringLiteral("alpha\nbeta\n"));
    const QString response = QStringLiteral(R"({"edits":[{"startLine":1,"startColumn":1,"endLine":1,"endColumn":6,"newText":"omega"}]})");

    const auto suggestion = InlineEditParser::parse(response, doc.get());

    QVERIFY(suggestion.valid);
    QCOMPARE(suggestion.edits.size(), 1);
    QCOMPARE(suggestion.edits.constFirst().range, KTextEditor::Range(0, 0, 0, 5));
    QCOMPARE(suggestion.edits.constFirst().newText, QStringLiteral("omega"));
    QCOMPARE(suggestion.rawResponse, response);
    QCOMPARE(suggestion.source, QStringLiteral("manual"));
}

void InlineEditParserTest::parsesFencedJsonEdit()
{
    auto doc = makeDocument(QStringLiteral("alpha\nbeta\n"));
    const QString response = QStringLiteral("```json\n{\"edits\":[{\"startLine\":2,\"startColumn\":1,\"endLine\":2,\"endColumn\":5,\"newText\":\"BETA\"}]}\n```");

    const auto suggestion = InlineEditParser::parse(response, doc.get());

    QVERIFY(suggestion.valid);
    QCOMPARE(suggestion.edits.constFirst().range, KTextEditor::Range(1, 0, 1, 4));
    QCOMPARE(suggestion.edits.constFirst().newText, QStringLiteral("BETA"));
}

void InlineEditParserTest::convertsOneBasedPositionsToZeroBasedRange()
{
    auto doc = makeDocument(QStringLiteral("0123456789\nabcdefghij\n"));
    const QString response = QStringLiteral(R"({"edits":[{"startLine":2,"startColumn":3,"endLine":2,"endColumn":8,"newText":"CDEFG"}]})");

    const auto suggestion = InlineEditParser::parse(response, doc.get());

    QVERIFY(suggestion.valid);
    QCOMPARE(suggestion.edits.constFirst().range.start(), KTextEditor::Cursor(1, 2));
    QCOMPARE(suggestion.edits.constFirst().range.end(), KTextEditor::Cursor(1, 7));
}

void InlineEditParserTest::rejectsInvalidJson()
{
    auto doc = makeDocument(QStringLiteral("alpha\n"));
    QVERIFY(!InlineEditParser::parse(QStringLiteral("not json"), doc.get()).valid);
}

void InlineEditParserTest::rejectsMissingEdits()
{
    auto doc = makeDocument(QStringLiteral("alpha\n"));
    QVERIFY(!InlineEditParser::parse(QStringLiteral(R"({"message":"none"})"), doc.get()).valid);
}

void InlineEditParserTest::rejectsOutOfBoundsRange()
{
    auto doc = makeDocument(QStringLiteral("alpha\n"));
    const QString response = QStringLiteral(R"({"edits":[{"startLine":99,"startColumn":1,"endLine":99,"endColumn":2,"newText":"x"}]})");
    QVERIFY(!InlineEditParser::parse(response, doc.get()).valid);
}

void InlineEditParserTest::rejectsEmptyNewTextWhenDeletionDisabled()
{
    auto doc = makeDocument(QStringLiteral("alpha\n"));
    const QString response = QStringLiteral(R"({"edits":[{"startLine":1,"startColumn":1,"endLine":1,"endColumn":6,"newText":""}]})");
    QVERIFY(!InlineEditParser::parse(response, doc.get()).valid);
}

void InlineEditParserTest::rejectsNoOpReplacement()
{
    auto doc = makeDocument(QStringLiteral("alpha\n"));
    const QString response = QStringLiteral(R"({"edits":[{"startLine":1,"startColumn":1,"endLine":1,"endColumn":6,"newText":"alpha"}]})");
    QVERIFY(!InlineEditParser::parse(response, doc.get()).valid);
}

void InlineEditParserTest::rejectsNewTextAboveMax()
{
    auto doc = makeDocument(QStringLiteral("alpha\n"));
    InlineEditParserOptions options;
    options.maxNewTextChars = 4;
    const QString response = QStringLiteral(R"({"edits":[{"startLine":1,"startColumn":1,"endLine":1,"endColumn":6,"newText":"omega"}]})");
    QVERIFY(!InlineEditParser::parse(response, doc.get(), options).valid);
}

void InlineEditParserTest::rejectsMultipleEdits()
{
    auto doc = makeDocument(QStringLiteral("alpha\nbeta\n"));
    const QString response = QStringLiteral(
        R"({"edits":[{"startLine":1,"startColumn":1,"endLine":1,"endColumn":6,"newText":"omega"},{"startLine":2,"startColumn":1,"endLine":2,"endColumn":5,"newText":"BETA"}]})");

    QVERIFY(!InlineEditParser::parse(response, doc.get()).valid);
}

void InlineEditParserTest::rejectsRangeDifferentFromExpectedRange()
{
    auto doc = makeDocument(QStringLiteral("alpha\nbeta\n"));
    InlineEditParserOptions options;
    options.expectedRange = KTextEditor::Range(1, 0, 1, 4);
    const QString response = QStringLiteral(R"({"edits":[{"startLine":1,"startColumn":1,"endLine":1,"endColumn":6,"newText":"omega"}]})");

    QVERIFY(!InlineEditParser::parse(response, doc.get(), options).valid);
}

void InlineEditParserTest::normalizesCrLf()
{
    auto doc = makeDocument(QStringLiteral("alpha\nbeta\n"));
    const QString response = QStringLiteral("{\"edits\":[{\"startLine\":1,\"startColumn\":1,\"endLine\":2,\"endColumn\":5,\"newText\":\"one\\r\\ntwo\\rthree\"}]}");

    const auto suggestion = InlineEditParser::parse(response, doc.get());

    QVERIFY(suggestion.valid);
    QCOMPARE(suggestion.edits.constFirst().newText, QStringLiteral("one\ntwo\nthree"));
}

QTEST_MAIN(InlineEditParserTest)

#include "InlineEditParserTest.moc"
