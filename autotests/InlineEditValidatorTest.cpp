/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: InlineEditValidatorTest
*/

#include "inlineedit/InlineEditValidator.h"

#include <KTextEditor/Document>
#include <KTextEditor/Editor>

#include <QTest>

#include <memory>
#include <utility>

using KateAiInlineCompletion::InlineEditSuggestion;
using KateAiInlineCompletion::InlineEditValidationOptions;
using KateAiInlineCompletion::InlineEditValidator;
using KateAiInlineCompletion::ProposedEdit;

namespace
{
std::unique_ptr<KTextEditor::Document> makeDocument(const QString &text)
{
    auto *editor = KTextEditor::Editor::instance();
    if (!editor) {
        QTest::qFail("KTextEditor editor instance is unavailable", __FILE__, __LINE__);
        return {};
    }

    std::unique_ptr<KTextEditor::Document> doc(editor->createDocument(nullptr));
    if (!doc) {
        QTest::qFail("Failed to create KTextEditor document", __FILE__, __LINE__);
        return {};
    }

    doc->setText(text);
    return doc;
}

InlineEditSuggestion suggestion(QVector<ProposedEdit> edits)
{
    InlineEditSuggestion out;
    out.edits = std::move(edits);
    out.valid = true;
    return out;
}

ProposedEdit edit(int startLine, int startColumn, int endLine, int endColumn, QString newText)
{
    return ProposedEdit{KTextEditor::Range(startLine, startColumn, endLine, endColumn), std::move(newText)};
}
} // namespace

class InlineEditValidatorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void acceptsNonOverlappingSortedEdits();
    void acceptsNonOverlappingUnsortedEditsAfterNormalization();
    void rejectsOverlappingEdits();
    void allowsAdjacentEdits();
    void rejectsDuplicateSamePositionInsertions();
    void rejectsSameStartInsertionAndReplacement();
    void rejectsOutOfBoundsRanges();
    void rejectsStaleDocumentRevision();
    void rejectsTooManyEdits();
};

void InlineEditValidatorTest::acceptsNonOverlappingSortedEdits()
{
    auto doc = makeDocument(QStringLiteral("alpha\nbeta\ngamma\n"));
    const auto result = InlineEditValidator::validate(doc.get(), suggestion({edit(0, 0, 0, 5, QStringLiteral("ALPHA")), edit(2, 0, 2, 5, QStringLiteral("GAMMA"))}));

    QVERIFY(result.ok);
    QCOMPARE(result.edits.size(), 2);
    QCOMPARE(result.edits.at(0).newText, QStringLiteral("ALPHA"));
}

void InlineEditValidatorTest::acceptsNonOverlappingUnsortedEditsAfterNormalization()
{
    auto doc = makeDocument(QStringLiteral("alpha\nbeta\ngamma\n"));
    const auto result = InlineEditValidator::validate(doc.get(), suggestion({edit(2, 0, 2, 5, QStringLiteral("GAMMA")), edit(0, 0, 0, 5, QStringLiteral("ALPHA"))}));

    QVERIFY(result.ok);
    QCOMPARE(result.edits.size(), 2);
    QCOMPARE(result.edits.at(0).range, KTextEditor::Range(0, 0, 0, 5));
    QCOMPARE(result.edits.at(1).range, KTextEditor::Range(2, 0, 2, 5));
}

void InlineEditValidatorTest::rejectsOverlappingEdits()
{
    auto doc = makeDocument(QStringLiteral("abcdef\n"));
    const auto result = InlineEditValidator::validate(doc.get(), suggestion({edit(0, 0, 0, 4, QStringLiteral("one")), edit(0, 3, 0, 6, QStringLiteral("two"))}));

    QVERIFY(!result.ok);
}

void InlineEditValidatorTest::allowsAdjacentEdits()
{
    auto doc = makeDocument(QStringLiteral("abcdef\n"));
    const auto result = InlineEditValidator::validate(doc.get(), suggestion({edit(0, 0, 0, 3, QStringLiteral("ABC")), edit(0, 3, 0, 6, QStringLiteral("DEF"))}));

    QVERIFY(result.ok);
}

void InlineEditValidatorTest::rejectsDuplicateSamePositionInsertions()
{
    auto doc = makeDocument(QStringLiteral("alpha\n"));
    const auto result = InlineEditValidator::validate(doc.get(), suggestion({edit(0, 0, 0, 0, QStringLiteral("one")), edit(0, 0, 0, 0, QStringLiteral("two"))}));

    QVERIFY(!result.ok);
}

void InlineEditValidatorTest::rejectsSameStartInsertionAndReplacement()
{
    auto doc = makeDocument(QStringLiteral("alpha\n"));
    const auto result = InlineEditValidator::validate(doc.get(), suggestion({edit(0, 0, 0, 0, QStringLiteral("// ")), edit(0, 0, 0, 5, QStringLiteral("ALPHA"))}));

    QVERIFY(!result.ok);
}

void InlineEditValidatorTest::rejectsOutOfBoundsRanges()
{
    auto doc = makeDocument(QStringLiteral("alpha\n"));
    const auto result = InlineEditValidator::validate(doc.get(), suggestion({edit(5, 0, 5, 1, QStringLiteral("bad"))}));

    QVERIFY(!result.ok);
}

void InlineEditValidatorTest::rejectsStaleDocumentRevision()
{
    auto doc = makeDocument(QStringLiteral("alpha\n"));
    InlineEditValidationOptions options;
    options.expectedDocumentRevision = doc->revision();
    doc->insertText(KTextEditor::Cursor(0, 0), QStringLiteral("// "));

    const auto result = InlineEditValidator::validate(doc.get(), suggestion({edit(0, 0, 0, 2, QStringLiteral("xx"))}), options);

    QVERIFY(!result.ok);
}

void InlineEditValidatorTest::rejectsTooManyEdits()
{
    auto doc = makeDocument(QStringLiteral("alpha\nbeta\n"));
    InlineEditValidationOptions options;
    options.maxEdits = 1;

    const auto result = InlineEditValidator::validate(doc.get(), suggestion({edit(0, 0, 0, 5, QStringLiteral("ALPHA")), edit(1, 0, 1, 4, QStringLiteral("BETA"))}), options);

    QVERIFY(!result.ok);
}

QTEST_MAIN(InlineEditValidatorTest)

#include "InlineEditValidatorTest.moc"
