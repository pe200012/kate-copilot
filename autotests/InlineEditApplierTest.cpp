/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: InlineEditApplierTest
*/

#include "inlineedit/InlineEditApplier.h"

#include <KTextEditor/Document>
#include <KTextEditor/Editor>

#include <QTest>

#include <memory>
#include <utility>

using KateAiInlineCompletion::InlineEditApplier;
using KateAiInlineCompletion::InlineEditSuggestion;
using KateAiInlineCompletion::InlineEditValidationOptions;
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

ProposedEdit edit(int startLine, int startColumn, int endLine, int endColumn, QString newText)
{
    return ProposedEdit{KTextEditor::Range(startLine, startColumn, endLine, endColumn), std::move(newText)};
}

InlineEditSuggestion suggestion(QVector<ProposedEdit> edits)
{
    InlineEditSuggestion out;
    out.edits = std::move(edits);
    out.valid = true;
    return out;
}
} // namespace

class InlineEditApplierTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void appliesMultipleReplacementsBottomToTop();
    void appliesInsertionAndReplacementInOneTransaction();
    void appliesDeletionWhenEnabled();
    void preservesLineEndingsAsLf();
    void returnsFailureForInvalidEditsWithoutChangingDocument();
};

void InlineEditApplierTest::appliesMultipleReplacementsBottomToTop()
{
    auto doc = makeDocument(QStringLiteral("alpha\nbeta\ngamma\n"));
    const auto result = InlineEditApplier::apply(doc.get(), suggestion({edit(0, 0, 0, 5, QStringLiteral("ALPHA")), edit(2, 0, 2, 5, QStringLiteral("GAMMA"))}));

    QVERIFY(result.ok);
    QCOMPARE(doc->text(), QStringLiteral("ALPHA\nbeta\nGAMMA\n"));
}

void InlineEditApplierTest::appliesInsertionAndReplacementInOneTransaction()
{
    auto doc = makeDocument(QStringLiteral("alpha\nbeta\n"));
    const auto result = InlineEditApplier::apply(doc.get(), suggestion({edit(0, 0, 0, 5, QStringLiteral("ALPHA")), edit(1, 4, 1, 4, QStringLiteral("!"))}));

    QVERIFY(result.ok);
    QCOMPARE(doc->text(), QStringLiteral("ALPHA\nbeta!\n"));
}

void InlineEditApplierTest::appliesDeletionWhenEnabled()
{
    auto doc = makeDocument(QStringLiteral("alpha\nbeta\n"));
    InlineEditValidationOptions options;
    options.allowDeletion = true;

    const auto result = InlineEditApplier::apply(doc.get(), suggestion({edit(0, 0, 0, 5, QString())}), options);

    QVERIFY(result.ok);
    QCOMPARE(doc->text(), QStringLiteral("\nbeta\n"));
}

void InlineEditApplierTest::preservesLineEndingsAsLf()
{
    auto doc = makeDocument(QStringLiteral("alpha\nbeta\n"));
    const auto result = InlineEditApplier::apply(doc.get(), suggestion({edit(0, 0, 0, 5, QStringLiteral("one\ntwo"))}));

    QVERIFY(result.ok);
    QCOMPARE(doc->text(), QStringLiteral("one\ntwo\nbeta\n"));
}

void InlineEditApplierTest::returnsFailureForInvalidEditsWithoutChangingDocument()
{
    auto doc = makeDocument(QStringLiteral("alpha\nbeta\n"));
    const QString before = doc->text();
    const auto result = InlineEditApplier::apply(doc.get(), suggestion({edit(0, 0, 0, 5, QStringLiteral("ALPHA")), edit(0, 4, 1, 2, QStringLiteral("overlap"))}));

    QVERIFY(!result.ok);
    QCOMPARE(doc->text(), before);
}

QTEST_MAIN(InlineEditApplierTest)

#include "InlineEditApplierTest.moc"
